/*****************************************************************************
 * stereo_widen.c : simple stereo widening effect
 *****************************************************************************
 * Copyright (C) 2012 VLC authors and VideoLAN
 *
 * Author : Sukrit Sangwan < sukritsangwan at gmail dot com >
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

#include <vlc_common.h>
#include <vlc_aout.h>
#include <vlc_filter.h>
#include <vlc_plugin.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

static block_t *Filter ( filter_t *, block_t * );
static int paramCallback( vlc_object_t *, char const *, vlc_value_t ,
                            vlc_value_t , void * );

struct filter_sys_t
{
    float *pf_ringbuf;  /* circular buffer to store samples */
    float *pf_write;    /* where to write current sample    */
    size_t i_len;       /* delay in number of samples       */
    float f_delay;      /* delay in milliseconds            */
    float f_feedback;
    float f_crossfeed;
    float f_dry_mix;
};

#define HELP_TEXT N_("This filter enhances the stereo effect by "\
            "suppressing mono (signal common to both channels) "\
            "and by delaying the signal of left into right and vice versa, "\
            "thereby widening the stereo effect.")
#define DELAY_TEXT N_("Delay time")
#define DELAY_LONGTEXT N_("Time in ms of the delay of left signal into right "\
            "and vice versa.")
#define FEEDBACK_TEXT N_("Feedback gain")
#define FEEDBACK_LONGTEXT N_("Amount of gain in delayed left signal into "\
            "right and vice versa. Gives a delay effect of left signal in "\
            "right output and vice versa which gives widening effect.")
#define CROSSFEED_TEXT N_("Crossfeed")
#define CROSSFEED_LONGTEXT N_("Cross feed of left into right with inverted "\
            "phase. This helps in suppressing the mono. If the value is 1 it "\
            "will cancel all the signal common to both channels.")
#define DRYMIX_TEXT N_("Dry mix")
#define DRYMIX_LONGTEXT N_("Level of input signal of original channel.")

#define CONFIG_PREFIX "stereowiden-"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_shortname( N_("Stereo Enhancer") )
    set_description( N_("Simple stereo widening effect") )
    set_help( HELP_TEXT )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AFILTER )
    set_capability( "audio filter", 0 )
    set_callbacks( Open, Close )

    add_float_with_range( CONFIG_PREFIX "delay", 20, 1, 100,
        DELAY_TEXT, DELAY_LONGTEXT, true )
    add_float_with_range( CONFIG_PREFIX "feedback", 0.3, 0.0, 0.9,
        FEEDBACK_TEXT, FEEDBACK_LONGTEXT, true )
    add_float_with_range( CONFIG_PREFIX "crossfeed", 0.3, 0.0, 0.8,
        CROSSFEED_TEXT, CROSSFEED_LONGTEXT, true )
    add_float_with_range( CONFIG_PREFIX "dry-mix", 0.8, 0.0, 1.0,
        DRYMIX_TEXT, DRYMIX_LONGTEXT, true )
vlc_module_end ()

/*****************************************************************************
 * Open: Allocate buffer
 *****************************************************************************/
static int MakeRingBuffer( float **pp_buffer, size_t *pi_buffer,
                           float **pp_write, float f_delay, unsigned i_rate )
{
    const size_t i_size = (2 * (size_t)(1 + f_delay * i_rate / 1000));

    if( unlikely(SIZE_MAX / sizeof(float) < i_size) )
        return VLC_EGENERIC;

    float *p_realloc = realloc( *pp_buffer, i_size * sizeof(float) );
    if( !p_realloc )
        return VLC_ENOMEM;

    memset( p_realloc, 0, i_size * sizeof(float) );
    *pp_write = *pp_buffer = p_realloc;
    *pi_buffer = i_size;

    return VLC_SUCCESS;
}

static int Open( vlc_object_t *obj )
{
    filter_t *p_filter  = (filter_t *)obj;
    vlc_object_t *p_aout = p_filter->obj.parent;
    filter_sys_t *p_sys;

    if( p_filter->fmt_in.audio.i_format != VLC_CODEC_FL32 ||
     !AOUT_FMTS_IDENTICAL( &p_filter->fmt_in.audio, &p_filter->fmt_out.audio) )
        return VLC_EGENERIC;

    if( p_filter->fmt_in.audio.i_channels != 2 )
    {
        msg_Err ( p_filter, "stereo enhance requires stereo" );
        return VLC_EGENERIC;
    }

    p_sys = p_filter->p_sys = malloc( sizeof(filter_sys_t) );
    if( unlikely(!p_sys) )
        return VLC_ENOMEM;

#define CREATE_VAR( stor, var ) \
    p_sys->stor = var_CreateGetFloat( p_aout, var ); \
    var_AddCallback( p_aout, var, paramCallback, p_sys );

    CREATE_VAR( f_delay,     CONFIG_PREFIX "delay" )
    CREATE_VAR( f_feedback,  CONFIG_PREFIX "feedback" )
    CREATE_VAR( f_crossfeed, CONFIG_PREFIX "crossfeed" )
    CREATE_VAR( f_dry_mix,   CONFIG_PREFIX "dry-mix" )

    /* Compute buffer length and allocate space */
    p_sys->pf_ringbuf = NULL;
    p_sys->i_len = 0;
    if( MakeRingBuffer( &p_sys->pf_ringbuf, &p_sys->i_len, &p_sys->pf_write,
                        p_sys->f_delay, p_filter->fmt_in.audio.i_rate ) != VLC_SUCCESS )
    {
        Close( obj );
        return VLC_ENOMEM;
    }

    p_filter->pf_audio_filter = Filter;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Filter: process each sample
 *****************************************************************************/
static block_t *Filter( filter_t *p_filter, block_t *p_block )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    float *p_out = (float *)p_block->p_buffer;
    float *pf_read;

    for (unsigned i = p_block->i_nb_samples; i > 0; i--)
    {
        pf_read = p_sys->pf_write + 2;
        /* if at end of buffer put read ptr at begin */
        if( pf_read >= p_sys->pf_ringbuf + p_sys->i_len )
            pf_read = p_sys->pf_ringbuf;

        float left  = p_out[0];
        float right = p_out[1];

        *(p_out++) = p_sys->f_dry_mix * left  - p_sys->f_crossfeed * right
                        - p_sys->f_feedback * pf_read[1];
        *(p_out++) = p_sys->f_dry_mix * right - p_sys->f_crossfeed * left
                        - p_sys->f_feedback * pf_read[0];
        *(p_sys->pf_write++) = left ;
        *(p_sys->pf_write++) = right;

        /* if at end of buffer place pf_write at begin */
        if( p_sys->pf_write  == p_sys->pf_ringbuf + p_sys->i_len )
            p_sys->pf_write  =  p_sys->pf_ringbuf;
    }

    return p_block;
}

/*****************************************************************************
 * Close: close the plugin
 *****************************************************************************/
static void Close( vlc_object_t *obj )
{
    filter_t *p_filter  = (filter_t *)obj;
    vlc_object_t *p_aout = p_filter->obj.parent;
    filter_sys_t *p_sys = p_filter->p_sys;

#define DEL_VAR(var) \
    var_DelCallback( p_aout, var, paramCallback, p_sys ); \
    var_Destroy( p_aout, var );

    DEL_VAR( CONFIG_PREFIX "feedback" );
    DEL_VAR( CONFIG_PREFIX "crossfeed" );
    DEL_VAR( CONFIG_PREFIX "dry-mix" );
    DEL_VAR( CONFIG_PREFIX "delay" );

    free( p_sys->pf_ringbuf );
    free( p_sys );
}


/**********************************************************************
 * Callback to update params on the fly
 **********************************************************************/
static int paramCallback( vlc_object_t *p_this, char const *psz_var,
                            vlc_value_t oldval, vlc_value_t newval,
                            void *p_data )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = (filter_sys_t *) p_data;

    VLC_UNUSED(oldval);
    VLC_UNUSED(p_this);

    if( !strcmp( psz_var, CONFIG_PREFIX "delay" ) )
    {
        if( MakeRingBuffer( &p_sys->pf_ringbuf, &p_sys->i_len, &p_sys->pf_write,
                            newval.f_float, p_filter->fmt_in.audio.i_rate ) != VLC_SUCCESS )
        {
            msg_Dbg( p_filter, "Couldnt allocate buffer for delay" );
        }
        else
        {
            p_sys->f_delay = newval.f_float;
        }
    }
    else if( !strcmp( psz_var, CONFIG_PREFIX "feedback" ) )
        p_sys->f_feedback = newval.f_float;
    else if( !strcmp( psz_var, CONFIG_PREFIX "crossfeed" ) )
        p_sys->f_feedback = newval.f_float;
    else if( !strcmp( psz_var, CONFIG_PREFIX "dry-mix" ) )
        p_sys->f_dry_mix = newval.f_float;

    return VLC_SUCCESS;
}
