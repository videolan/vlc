/*****************************************************************************
 * chorus_flanger: Basic chorus/flanger/delay audio filter
 *****************************************************************************
 * Copyright (C) 2009-12 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Srikanth Raju < srikiraju at gmail dot com >
 *          Sukrit Sangwan < sukritsangwan at gmail dot com >
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <math.h>

#include <vlc_common.h>
#include <vlc_plugin.h>

#include <vlc_aout.h>
#include <vlc_filter.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static int  Open     ( vlc_object_t * );
static void Close    ( vlc_object_t * );
static block_t *DoWork( filter_t *, block_t * );
static int paramCallback( vlc_object_t *, char const *, vlc_value_t ,
                          vlc_value_t , void * );
static int reallocate_buffer( filter_t *, filter_sys_t * );

struct filter_sys_t
{
    /* TODO: Cleanup and optimise */
    int i_cumulative;
    int i_channels, i_sampleRate;
    float f_delayTime, f_feedbackGain;  /* delayTime is in milliseconds */
    float f_wetLevel, f_dryLevel;
    float f_sweepDepth, f_sweepRate;

    float f_offset;
    int i_step;
    float f_temp;
    float f_sinMultiplier;

    /* This data is for the the circular queue which stores the samples. */
    int i_bufferLength;
    float * p_delayLineStart, * p_delayLineEnd;
    float * p_write;
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/


vlc_module_begin ()
    set_description( N_("Sound Delay") )
    set_shortname( N_("Delay") )
    set_help( N_("Add a delay effect to the sound") )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    add_shortcut( "delay" )
    add_float( "delay-time", 20, N_("Delay time"),
        N_("Time in milliseconds of the average delay. Note average"), true )
    add_float( "sweep-depth", 6, N_("Sweep Depth"),
        N_("Time in milliseconds of the maximum sweep depth. Thus, the sweep "
            "range will be delay-time +/- sweep-depth."), true )
    add_float( "sweep-rate", 6, N_("Sweep Rate"),
        N_("Rate of change of sweep depth in milliseconds shift per second "
           "of play"), true )
    add_float_with_range( "feedback-gain", 0.5, -0.9, 0.9,
        N_("Feedback gain"), N_("Gain on Feedback loop"), true )
    add_float_with_range( "wet-mix", 0.4, -0.999, 0.999,
        N_("Wet mix"), N_("Level of delayed signal"), true )
    add_float_with_range( "dry-mix", 0.4, -0.999, 0.999,
        N_("Dry Mix"), N_("Level of input signal"), true )
    set_capability( "audio filter", 0 )
    set_callbacks( Open, Close )
vlc_module_end ()

/**
 * small_value: Helper function
 * return high pass cutoff
 */
static inline float small_value()
{
    /* allows for 2^-24, should be enough for 24-bit DACs at least */
    return ( 1.0 / 16777216.0 );
}

/**
 * Open: initialize and create stuff
 * @param p_this
 */
static int Open( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys = p_filter->p_sys = malloc( sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->i_channels       = aout_FormatNbChannels( &p_filter->fmt_in.audio );
    p_sys->f_delayTime      = var_CreateGetFloat( p_this, "delay-time" );
    p_sys->f_sweepDepth     = var_CreateGetFloat( p_this, "sweep-depth" );
    p_sys->f_sweepRate      = var_CreateGetFloat( p_this, "sweep-rate" );
    p_sys->f_feedbackGain   = var_CreateGetFloat( p_this, "feedback-gain" );
    p_sys->f_dryLevel       = var_CreateGetFloat( p_this, "dry-mix" );
    p_sys->f_wetLevel       = var_CreateGetFloat( p_this, "wet-mix" );
    var_AddCallback( p_this, "delay-time", paramCallback, p_sys );
    var_AddCallback( p_this, "sweep-depth", paramCallback, p_sys );
    var_AddCallback( p_this, "sweep-rate", paramCallback, p_sys );
    var_AddCallback( p_this, "feedback-gain", paramCallback, p_sys );
    var_AddCallback( p_this, "dry-mix", paramCallback, p_sys );
    var_AddCallback( p_this, "wet-mix", paramCallback, p_sys );

    if( p_sys->f_delayTime < 0.0)
    {
        msg_Err( p_filter, "Delay Time is invalid" );
        free(p_sys);
        return VLC_EGENERIC;
    }

    if( p_sys->f_sweepDepth > p_sys->f_delayTime || p_sys->f_sweepDepth < 0.0 )
    {
        msg_Err( p_filter, "Sweep Depth is invalid" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    if( p_sys->f_sweepRate < 0.0 )
    {
        msg_Err( p_filter, "Sweep Rate is invalid" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Max delay = delay + depth. Min = delay - depth */
    p_sys->i_bufferLength = p_sys->i_channels * ( (int)( ( p_sys->f_delayTime
                + p_sys->f_sweepDepth ) * p_filter->fmt_in.audio.i_rate/1000 ) + 1 );

    msg_Dbg( p_filter , "Buffer length:%d, Channels:%d, Sweep Depth:%f, Delay "
            "time:%f, Sweep Rate:%f, Sample Rate: %d", p_sys->i_bufferLength,
            p_sys->i_channels, p_sys->f_sweepDepth, p_sys->f_delayTime,
            p_sys->f_sweepRate, p_filter->fmt_in.audio.i_rate );
    if( p_sys->i_bufferLength <= 0 )
    {
        msg_Err( p_filter, "Delay-time, Sample rate or Channels was incorrect" );
        free(p_sys);
        return VLC_EGENERIC;
    }

    p_sys->p_delayLineStart = calloc( p_sys->i_bufferLength, sizeof( float ) );
    if( !p_sys->p_delayLineStart )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }

    p_sys->i_cumulative = 0;
    p_sys->i_step = p_sys->f_sweepRate > 0 ? 1 : 0;
    p_sys->f_offset = 0;
    p_sys->f_temp = 0;

    p_sys->p_delayLineEnd = p_sys->p_delayLineStart + p_sys->i_bufferLength;
    p_sys->p_write = p_sys->p_delayLineStart;

    if( p_sys->f_sweepDepth < small_value() ||
            p_filter->fmt_in.audio.i_rate < small_value() ) {
        p_sys->f_sinMultiplier = 0.0;
    }
    else {
        p_sys->f_sinMultiplier = 11 * p_sys->f_sweepRate /
            ( 7 * p_sys->f_sweepDepth * p_filter->fmt_in.audio.i_rate ) ;
    }
    p_sys->i_sampleRate = p_filter->fmt_in.audio.i_rate;

    p_filter->fmt_in.audio.i_format = VLC_CODEC_FL32;
    p_filter->fmt_out.audio = p_filter->fmt_in.audio;
    p_filter->pf_audio_filter = DoWork;

    return VLC_SUCCESS;
}

/**
 * sanitize: Helper function to eliminate small amplitudes
 * @param f_value pointer to value to clean
 */
static inline void sanitize( float * f_value )
{
    if ( fabs( *f_value ) < small_value() )
        *f_value = 0.0f;
}


/**
 * DoWork : delays and finds the value of the current frame
 * @param p_filter This filter object
 * @param p_in_buf Input buffer
 * @return Output buffer
 */
static block_t *DoWork( filter_t *p_filter, block_t *p_in_buf )
{
    struct filter_sys_t *p_sys = p_filter->p_sys;
    int i_chan;
    unsigned i_samples = p_in_buf->i_nb_samples; /* number of samples */
    /* maximum number of samples to offset in buffer */
    int i_maxOffset = floor( p_sys->f_sweepDepth * p_sys->i_sampleRate / 1000 );
    float *p_out = (float*)p_in_buf->p_buffer;
    float *p_in =  (float*)p_in_buf->p_buffer;

    float *p_ptr, f_temp = 0;/* f_diff = 0, f_frac = 0;*/

    /* Process each sample */
    for( unsigned i = 0; i < i_samples ; i++ )
    {
        /* Sine function as a oscillator wave to calculate sweep */
        p_sys->i_cumulative += p_sys->i_step;
        p_sys->f_offset = sinf( (p_sys->i_cumulative) * p_sys->f_sinMultiplier )
                * floorf(p_sys->f_sweepDepth * p_sys->i_sampleRate / 1000);
        if( abs( p_sys->i_step ) > 0 )
        {
            if( p_sys->i_cumulative >=  floor( p_sys->f_sweepDepth *
                        p_sys->i_sampleRate / p_sys->f_sweepRate ))
            {
                p_sys->f_offset = i_maxOffset;
                p_sys->i_step = -1 * ( p_sys->i_step );
            }
            if( p_sys->i_cumulative <= floor( -1 * p_sys->f_sweepDepth *
                        p_sys->i_sampleRate / p_sys->f_sweepRate ) )
            {
                p_sys->f_offset = -i_maxOffset;
                p_sys->i_step = -1 * ( p_sys->i_step );
            }
        }
        /* Calculate position in delay */
        int offset = floor( p_sys->f_offset );
        p_ptr = p_sys->p_write + ( i_maxOffset - offset ) * p_sys->i_channels;

        /* Handle Overflow */
        if( p_ptr < p_sys->p_delayLineStart )
        {
            p_ptr += p_sys->i_bufferLength - p_sys->i_channels;
        }
        if( p_ptr > p_sys->p_delayLineEnd - 2*p_sys->i_channels )
        {
            p_ptr -= p_sys->i_bufferLength - p_sys->i_channels;
        }
        /* For interpolation */
/*        f_frac = ( p_sys->f_offset - (int)p_sys->f_offset );*/
        for( i_chan = 0; i_chan < p_sys->i_channels; i_chan++ )
        {
/*            if( p_ptr <= p_sys->p_delayLineStart + p_sys->i_channels )
                f_diff = *(p_sys->p_delayLineEnd + i_chan) - p_ptr[i_chan];
            else
                f_diff = *( p_ptr - p_sys->i_channels + i_chan )
                            - p_ptr[i_chan];*/
            f_temp = ( *( p_ptr + i_chan ) );//+ f_diff * f_frac;
            /*Linear Interpolation. FIXME. This creates LOTS of noise */
            sanitize(&f_temp);
            p_out[i_chan] = p_sys->f_dryLevel * p_in[i_chan] +
                p_sys->f_wetLevel * f_temp;
            *( p_sys->p_write + i_chan ) = p_in[i_chan] +
                p_sys->f_feedbackGain * f_temp;
        }
        if( p_sys->p_write == p_sys->p_delayLineStart )
            for( i_chan = 0; i_chan < p_sys->i_channels; i_chan++ )
                *( p_sys->p_delayLineEnd - p_sys->i_channels + i_chan )
                    = *( p_sys->p_delayLineStart + i_chan );

        p_in += p_sys->i_channels;
        p_out += p_sys->i_channels;
        p_sys->p_write += p_sys->i_channels;
        if( p_sys->p_write == p_sys->p_delayLineEnd - p_sys->i_channels )
        {
            p_sys->p_write = p_sys->p_delayLineStart;
        }

    }
    return p_in_buf;
}

/**
 * Close: Destructor
 * @param p_this pointer to this filter object
 */
static void Close( vlc_object_t *p_this )
{
    filter_t *p_filter = ( filter_t* )p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    var_DelCallback( p_this, "delay-time", paramCallback, p_sys );
    var_DelCallback( p_this, "sweep-depth", paramCallback, p_sys );
    var_DelCallback( p_this, "sweep-rate", paramCallback, p_sys );
    var_DelCallback( p_this, "feedback-gain", paramCallback, p_sys );
    var_DelCallback( p_this, "wet-mix", paramCallback, p_sys );
    var_DelCallback( p_this, "dry-mix", paramCallback, p_sys );
    var_Destroy( p_this, "delay-time" );
    var_Destroy( p_this, "sweep-depth" );
    var_Destroy( p_this, "sweep-rate" );
    var_Destroy( p_this, "feedback-gain" );
    var_Destroy( p_this, "wet-mix" );
    var_Destroy( p_this, "dry-mix" );

    free( p_sys->p_delayLineStart );
    free( p_sys );
}

/******************************************************************************
 * Callback to update parameters on the fly
 ******************************************************************************/
static int paramCallback( vlc_object_t *p_this, char const *psz_var,
                          vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = (filter_sys_t *) p_data;

    if( !strncmp( psz_var, "delay-time", 10 ) )
    {
        /* if invalid value pretend everything is OK without updating value */
        if( newval.f_float < 0 )
            return VLC_SUCCESS;
        p_sys->f_delayTime = newval.f_float;
        if( !reallocate_buffer( p_filter, p_sys ) )
        {
            p_sys->f_delayTime = oldval.f_float;
            p_sys->i_bufferLength = p_sys->i_channels * ( (int)
                            ( ( p_sys->f_delayTime + p_sys->f_sweepDepth ) * 
                              p_filter->fmt_in.audio.i_rate/1000 ) + 1 );
        }
    }
    else if( !strncmp( psz_var, "sweep-depth", 11 ) )
    {
        if( newval.f_float < 0 || newval.f_float > p_sys->f_delayTime)
            return VLC_SUCCESS;
        p_sys->f_sweepDepth = newval.f_float;
        if( !reallocate_buffer( p_filter, p_sys ) )
        {
            p_sys->f_sweepDepth = oldval.f_float;
            p_sys->i_bufferLength = p_sys->i_channels * ( (int)
                            ( ( p_sys->f_delayTime + p_sys->f_sweepDepth ) * 
                              p_filter->fmt_in.audio.i_rate/1000 ) + 1 );
        }
    }
    else if( !strncmp( psz_var, "sweep-rate", 10 ) )
    {
        if( newval.f_float > p_sys->f_sweepDepth )
            return VLC_SUCCESS;
        p_sys->f_sweepRate = newval.f_float;
        /* Calculate new f_sinMultiplier */
        if( p_sys->f_sweepDepth < small_value() ||
                p_filter->fmt_in.audio.i_rate < small_value() ) {
            p_sys->f_sinMultiplier = 0.0;
        }
        else {
            p_sys->f_sinMultiplier = 11 * p_sys->f_sweepRate /
                ( 7 * p_sys->f_sweepDepth * p_filter->fmt_in.audio.i_rate ) ;
        }
    }
    else if( !strncmp( psz_var, "feedback-gain", 13 ) )
        p_sys->f_feedbackGain = newval.f_float;
    else if( !strncmp( psz_var, "wet-mix", 7 ) )
        p_sys->f_wetLevel = newval.f_float;
    else if( !strncmp( psz_var, "dry-mix", 7 ) )
        p_sys->f_dryLevel = newval.f_float;

    return VLC_SUCCESS;
}

static int reallocate_buffer( filter_t *p_filter,  filter_sys_t *p_sys )
{
    p_sys->i_bufferLength = p_sys->i_channels * ( (int)( ( p_sys->f_delayTime
           + p_sys->f_sweepDepth ) * p_filter->fmt_in.audio.i_rate/1000 ) + 1 );

    float *temp = realloc( p_sys->p_delayLineStart, p_sys->i_bufferLength );
    if( unlikely( !temp ) )
    {
        msg_Err( p_filter, "Couldnt reallocate buffer for new delay." );
        return 0;
    }
    free( p_sys->p_delayLineStart );
    p_sys->p_delayLineStart = temp;
    p_sys->p_delayLineEnd = p_sys->p_delayLineStart + p_sys->i_bufferLength;
    free( temp );
    return 1;
}
