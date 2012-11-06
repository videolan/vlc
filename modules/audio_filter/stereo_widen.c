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
    float *pf_begin;    /* circular buffer to store samples */
    float *pf_write;    /* where to write current sample    */
    int   i_len;        /* delay in number of samples       */
    float f_delay;      /* delay in milliseconds            */
    float f_feedback;
    float f_crossfeed;
    float f_dry_mix;
    bool  b_free_buf;   /* used if callback to delay fails to       *
                         * allocate buffer, then dont free it twice */
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

    add_float( "delay", 20, DELAY_TEXT, DELAY_LONGTEXT, true )
    add_float_with_range( "feedback", 0.3, 0.0, 0.9,
        FEEDBACK_TEXT, FEEDBACK_LONGTEXT, true )
    add_float_with_range( "crossfeed", 0.3, 0.0, 0.8,
        CROSSFEED_TEXT, CROSSFEED_LONGTEXT, true )
    add_float_with_range( "dry-mix", 0.8, 0.0, 1.0,
        DRYMIX_TEXT, DRYMIX_LONGTEXT, true )
vlc_module_end ()

/*****************************************************************************
 * Open: Allocate buffer
 *****************************************************************************/
static int Open( vlc_object_t *obj )
{
    filter_t *p_filter  = (filter_t *)obj;
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
    p_sys->stor = var_CreateGetFloat( obj, var ); \
    var_AddCallback( p_filter, var, paramCallback, p_sys );

    CREATE_VAR( f_delay, "delay" )
    CREATE_VAR( f_feedback, "feedback" )
    CREATE_VAR( f_crossfeed, "crossfeed" )
    CREATE_VAR( f_dry_mix, "dry-mix" )

    /* Compute buffer length and allocate space */
    p_sys->i_len = 2 * p_sys->f_delay * p_filter->fmt_in.audio.i_rate / 1000;
    p_sys->pf_begin = calloc( p_sys->i_len + 2, sizeof(float) );
    if( unlikely(!p_sys->pf_begin) )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }
    p_sys->b_free_buf = true;
    p_sys->pf_write = p_sys->pf_begin;
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
        if( pf_read > p_sys->pf_begin + p_sys->i_len )
            pf_read = p_sys->pf_begin;

        float left  = p_out[0];
        float right = p_out[1];

        *(p_out++) = p_sys->f_dry_mix * left  - p_sys->f_crossfeed * right
                        - p_sys->f_feedback * pf_read[1];
        *(p_out++) = p_sys->f_dry_mix * right - p_sys->f_crossfeed * left
                        - p_sys->f_feedback * pf_read[0];
        p_sys->pf_write[0] = left ;
        p_sys->pf_write[1] = right;

        /* if at end of buffer place pf_write at begin */
        if( p_sys->pf_write  == p_sys->pf_begin + p_sys->i_len )
            p_sys->pf_write  =  p_sys->pf_begin;
        else
            p_sys->pf_write += 2;
    }

    return p_block;
}

/*****************************************************************************
 * Close: close the plugin
 *****************************************************************************/
static void Close( vlc_object_t *obj )
{
    filter_t *p_filter  = (filter_t *)obj;
    filter_sys_t *p_sys = p_filter->p_sys;

#define DEL_VAR(var) \
    var_DelCallback( p_filter, var, paramCallback, p_sys ); \
    var_Destroy( p_filter, var );

    DEL_VAR( "feedback" );
    DEL_VAR( "crossfeed" );
    DEL_VAR( "dry-mix" );
    var_Destroy( p_filter, "delay" );
    if( p_sys->b_free_buf )
        free( p_sys->pf_begin );
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

    if( !strcmp( psz_var, "delay" ) )
    {
        p_sys->f_delay = newval.f_float;
        /* Free previous buffer and allocate new circular buffer */
        free( p_sys->pf_begin );
        p_sys->i_len = 2 * p_sys->f_delay * p_filter->fmt_in.audio.i_rate /1000;
        p_sys->pf_begin = calloc( p_sys->i_len + 2, sizeof(float) );
        if( unlikely(!p_sys->pf_begin) )
        {
            p_sys->b_free_buf = false;
            msg_Dbg( p_filter, "Couldnt allocate buffer for delay" );
            Close( p_this );
        }
    }
    else if( !strcmp( psz_var, "feedback" ) )
        p_sys->f_feedback = newval.f_float;
    else if( !strcmp( psz_var, "crossfeed" ) )
        p_sys->f_feedback = newval.f_float;
    else if( !strcmp( psz_var, "dry-mix" ) )
        p_sys->f_dry_mix = newval.f_float;

    return VLC_SUCCESS;
}
