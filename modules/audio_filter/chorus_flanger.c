/*****************************************************************************
 * chorus_flanger.c
 *****************************************************************************
 * Copyright (C) 2009 the VideoLAN team
 * $Id$
 *
 * Author: Srikanth Raju < srikiraju at gmail dot com >
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/**
 * Basic chorus/flanger/delay audio filter
 * This implements a variable delay filter for VLC. It has some issues with
 * interpolation and sounding 'correct'.
 */


#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <math.h>

#include <vlc_common.h>
#include <vlc_plugin.h>

#include <vlc_aout.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static int  Open     ( vlc_object_t * );
static void Close    ( vlc_object_t * );
static void DoWork   ( aout_instance_t * , aout_filter_t *,
                       aout_buffer_t * , aout_buffer_t * );

struct aout_filter_sys_t
{
    /* TODO: Cleanup and optimise */
    int i_cumulative;
    int i_channels, i_sampleRate;
    float f_delayTime, f_feedbackGain;  /* delayTime is in milliseconds */
    float f_wetLevel, f_dryLevel;
    float f_sweepDepth, f_sweepRate;

    float f_step,f_offset;
    int i_step,i_offset;
    float f_temp;
    float f_sinMultiplier;

    /* This data is for the the circular queue which stores the samples. */
    int i_bufferLength;
    float * pf_delayLineStart, * pf_delayLineEnd;
    float * pf_write;
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/


vlc_module_begin ()
    set_description( N_("Sound Delay") )
    set_shortname( N_("delay") )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    add_shortcut( "delay" )
    add_float( "delay-time", 40, NULL, N_("Delay time"),
        N_("Time in milliseconds of the average delay. Note average"), true )
    add_float( "sweep-depth", 6, NULL, N_("Sweep Depth"),
        N_("Time in milliseconds of the maximum sweep depth. Thus, the sweep "
            "range will be delay-time +/- sweep-depth."), true )
    add_float( "sweep-rate", 6, NULL, N_("Sweep Rate"),
        N_("Rate of change of sweep depth in milliseconds shift per second "
           "of play"), true )
    add_float_with_range( "feedback-gain", 0.5, -0.9, 0.9, NULL,
        N_("Feedback Gain"), N_("Gain on Feedback loop"), true )
    add_float_with_range( "wet-mix", 0.4, -0.999, 0.999, NULL,
        N_("Wet mix"), N_("Level of delayed signal"), true )
    add_float_with_range( "dry-mix", 0.4, -0.999, 0.999, NULL,
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
    aout_filter_t *p_filter = (aout_filter_t*)p_this;
    aout_filter_sys_t *p_sys;

    if ( !AOUT_FMTS_SIMILAR( &p_filter->input, &p_filter->output ) )
    {
        msg_Err( p_filter, "input and output formats are not similar" );
        return VLC_EGENERIC;
    }

    if( p_filter->input.i_format != VLC_CODEC_FL32 ||
        p_filter->output.i_format != VLC_CODEC_FL32 )
    {
        p_filter->input.i_format = VLC_CODEC_FL32;
        p_filter->output.i_format = VLC_CODEC_FL32;
        msg_Warn( p_filter, "bad input or output format" );
    }

    p_filter->pf_do_work = DoWork;
    p_filter->b_in_place = true;

    p_sys = p_filter->p_sys = malloc( sizeof( aout_filter_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->i_channels       = aout_FormatNbChannels( &p_filter->input );
    p_sys->f_delayTime      = var_CreateGetFloat( p_this, "delay-time" );
    p_sys->f_sweepDepth     = var_CreateGetFloat( p_this, "sweep-depth" );
    p_sys->f_sweepRate      = var_CreateGetFloat( p_this, "sweep-rate" );
    p_sys->f_feedbackGain   = var_CreateGetFloat( p_this, "feedback-gain" );
    p_sys->f_dryLevel       = var_CreateGetFloat( p_this, "dry-mix" );
    p_sys->f_wetLevel       = var_CreateGetFloat( p_this, "wet-mix" );

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
                + p_sys->f_sweepDepth ) * p_filter->input.i_rate/1000 ) + 1 );

    msg_Dbg( p_filter , "Buffer length:%d, Channels:%d, Sweep Depth:%f, Delay "
            "time:%f, Sweep Rate:%f, Sample Rate: %d", p_sys->i_bufferLength,
            p_sys->i_channels, p_sys->f_sweepDepth, p_sys->f_delayTime,
            p_sys->f_sweepRate, p_filter->input.i_rate );
    if( p_sys->i_bufferLength <= 0 )
    {
        msg_Err( p_filter, "Delay-time, Sampl rate or Channels was incorrect" );
        free(p_sys);
        return VLC_EGENERIC;
    }

    p_sys->pf_delayLineStart = calloc( p_sys->i_bufferLength, sizeof( float ) );
    if( !p_sys->pf_delayLineStart )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }

    p_sys->i_cumulative = 0;
    p_sys->f_step = p_sys->f_sweepRate / 1000.0;
    p_sys->i_step = p_sys->f_sweepRate > 0 ? 1 : 0;
    p_sys->f_offset = 0;
    p_sys->i_offset = 0;
    p_sys->f_temp = 0;

    p_sys->pf_delayLineEnd = p_sys->pf_delayLineStart + p_sys->i_bufferLength;
    p_sys->pf_write = p_sys->pf_delayLineStart;

    if( p_sys->f_sweepDepth < small_value() ||
            p_filter->input.i_rate < small_value() ) {
        p_sys->f_sinMultiplier = 0.0;
    }
    else {
        p_sys->f_sinMultiplier = 11 * p_sys->f_sweepRate /
            ( 7 * p_sys->f_sweepDepth * p_filter->input.i_rate ) ;
    }
    p_sys->i_sampleRate = p_filter->input.i_rate;

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
 * @param p_aout Audio output object
 * @param p_filter This filter object
 * @param p_in_buf Input buffer
 * @param p_out_buf Output buffer
 */
static void DoWork( aout_instance_t *p_aout, aout_filter_t *p_filter,
                    aout_buffer_t *p_in_buf, aout_buffer_t *p_out_buf )
{
    VLC_UNUSED( p_aout );

    struct aout_filter_sys_t *p_sys = p_filter->p_sys;
    int i_chan;
    int i_samples = p_in_buf->i_nb_samples; /* Gives the number of samples */
    /* maximum number of samples to offset in buffer */
    int i_maxOffset = floor( p_sys->f_sweepDepth * p_sys->i_sampleRate / 1000 );
    float *p_out = (float*)p_out_buf->p_buffer;
    float *p_in =  (float*)p_in_buf->p_buffer;

    float *pf_ptr, f_diff = 0, f_frac = 0, f_temp = 0 ;

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
    p_out_buf->i_nb_bytes = p_in_buf->i_nb_bytes;

    /* Process each sample */
    for( int i = 0; i < i_samples ; i++ )
    {
        /* Use a sine function as a oscillator wave. TODO */
        /* f_offset = sinf( ( p_sys->i_cumulative ) * p_sys->f_sinMultiplier ) *
         * (int)floor(p_sys->f_sweepDepth * p_sys->i_sampleRate / 1000);
         */

        /* Triangle oscillator. Step using ints, because floats give rounding */
        p_sys->i_offset+=p_sys->i_step;
        p_sys->f_offset = p_sys->i_offset * p_sys->f_step;
        if( abs( p_sys->i_step ) > 0 )
        {
            if( p_sys->i_offset >=  floor( p_sys->f_sweepDepth *
                        p_sys->i_sampleRate / p_sys->f_sweepRate ))
            {
                p_sys->f_offset = i_maxOffset;
                p_sys->i_step = -1 * ( p_sys->i_step );
            }
            if( p_sys->i_offset <= floor( -1 * p_sys->f_sweepDepth *
                        p_sys->i_sampleRate / p_sys->f_sweepRate ) )
            {
                p_sys->f_offset = -i_maxOffset;
                p_sys->i_step = -1 * ( p_sys->i_step );
            }
        }
        /* Calculate position in delay */
        pf_ptr = p_sys->pf_write + i_maxOffset * p_sys->i_channels +
            (int)( floor( p_sys->f_offset ) ) * p_sys->i_channels;

        /* Handle Overflow */
        if( pf_ptr < p_sys->pf_delayLineStart )
        {
            pf_ptr += p_sys->i_bufferLength - p_sys->i_channels;
        }
        if( pf_ptr > p_sys->pf_delayLineEnd - 2*p_sys->i_channels )
        {
            pf_ptr -= p_sys->i_bufferLength - p_sys->i_channels;
        }
        /* For interpolation */
        f_frac = ( p_sys->f_offset - (int)p_sys->f_offset );
        for( i_chan = 0; i_chan < p_sys->i_channels; i_chan++ )
        {
            f_diff =  *( pf_ptr + p_sys->i_channels + i_chan )
                        - *( pf_ptr + i_chan );
            f_temp = ( *( pf_ptr + i_chan ) );//+ f_diff * f_frac);
            /*Linear Interpolation. FIXME. This creates LOTS of noise */
            sanitize(&f_temp);
            p_out[i_chan] = p_sys->f_dryLevel * p_in[i_chan] +
                p_sys->f_wetLevel * f_temp;
            *( p_sys->pf_write + i_chan ) = p_in[i_chan] +
                p_sys->f_feedbackGain * f_temp;
        }
        if( p_sys->pf_write == p_sys->pf_delayLineStart )
            for( i_chan = 0; i_chan < p_sys->i_channels; i_chan++ )
                *( p_sys->pf_delayLineEnd - p_sys->i_channels + i_chan )
                    = *( p_sys->pf_delayLineStart + i_chan );

        p_in += p_sys->i_channels;
        p_out += p_sys->i_channels;
        p_sys->pf_write += p_sys->i_channels;
        if( p_sys->pf_write == p_sys->pf_delayLineEnd - p_sys->i_channels )
        {
            p_sys->pf_write = p_sys->pf_delayLineStart;
        }

    }
    return;
}

/**
 * Close: Destructor
 * @param p_this pointer to this filter object
 */
static void Close( vlc_object_t *p_this )
{
    aout_filter_t *p_filter = ( aout_filter_t* )p_this;
    aout_filter_sys_t *p_sys = p_filter->p_sys;

    free( p_sys->pf_delayLineStart );
    free( p_sys );
}
