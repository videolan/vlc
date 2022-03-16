/*****************************************************************************
 * fps.c : fps conversion plugin for vlc
 *****************************************************************************
 * Copyright (C) 2014 VLC authors and VideoLAN
 *
 * Author: Ilkka Ollakka <ileoo at videolan dot org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_picture.h>

static int Open( filter_t * );
static picture_t *Filter( filter_t *p_filter, picture_t *p_picture);

#define CFG_PREFIX "fps-"

#define FPS_TEXT N_( "Frame rate" )

vlc_module_begin ()
    set_description( N_("FPS conversion video filter") )
    set_shortname( N_("FPS Converter" ))
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )

    add_shortcut( "fps" )
    add_string( CFG_PREFIX "fps", NULL, FPS_TEXT, NULL )
    set_callback_video_filter( Open )
vlc_module_end ()

static const char *const ppsz_filter_options[] = {
    "fps",
    NULL
};

/* We'll store pointer for previous picture we have received
   and copy that if needed on framerate increase (not preferred)*/
typedef struct
{
    date_t          next_output_pts; /**< output calculated PTS */
    picture_t       *p_previous_pic; /**< kept source picture used to produce filter output */
    vlc_tick_t      i_output_frame_interval;
} filter_sys_t;

static void SetOutputDate(filter_sys_t *p_sys, picture_t *pic)
{
    pic->date = date_Get( &p_sys->next_output_pts );
    date_Increment( &p_sys->next_output_pts, 1 );
}

static picture_t *Filter( filter_t *p_filter, picture_t *p_picture)
{
    filter_sys_t *p_sys = p_filter->p_sys;
    const vlc_tick_t src_date = p_picture->date;
    /* If input picture doesn't have actual valid timestamp,
        we don't really have currently a way to know what else
        to do with it other than drop it for now*/
    if( unlikely( src_date == VLC_TICK_INVALID) )
    {
        msg_Dbg( p_filter, "skipping non-dated picture");
        picture_Release( p_picture );
        return NULL;
    }

    p_picture->format.i_frame_rate = p_filter->fmt_out.video.i_frame_rate;
    p_picture->format.i_frame_rate_base = p_filter->fmt_out.video.i_frame_rate_base;

    /* First time we get some valid timestamp, we'll take it as base for output
        later on we retake new timestamp if it has jumped too much */
    if( unlikely( ( date_Get( &p_sys->next_output_pts ) == VLC_TICK_INVALID ) ||
                   ( src_date > ( date_Get( &p_sys->next_output_pts ) + p_sys->i_output_frame_interval ) )
                ) )
    {
        msg_Dbg( p_filter, "Resetting timestamps" );
        date_Set( &p_sys->next_output_pts, src_date );
        if( p_sys->p_previous_pic )
            picture_Release( p_sys->p_previous_pic );
        p_sys->p_previous_pic = picture_Hold( p_picture );
        SetOutputDate( p_sys, p_picture );
        return p_picture;
    }

    /* Check if we can skip input as better should follow */
    if( src_date <
        ( date_Get( &p_sys->next_output_pts ) - p_sys->i_output_frame_interval ) )
    {
        if( p_sys->p_previous_pic )
            picture_Release( p_sys->p_previous_pic );
        p_sys->p_previous_pic = p_picture;
        return NULL;
    }

    SetOutputDate( p_sys, p_sys->p_previous_pic );

    picture_t *last_pic = p_sys->p_previous_pic;
    /* Duplicating pictures are not that effective and framerate increase
        should be avoided, it's only here as filter should work in that direction too*/
    while( unlikely( (date_Get( &p_sys->next_output_pts ) + p_sys->i_output_frame_interval ) < src_date ) )
    {
        picture_t *p_tmp = NULL;
        p_tmp = picture_NewFromFormat( &p_filter->fmt_out.video );

        picture_Copy( p_tmp, p_sys->p_previous_pic);
        SetOutputDate( p_sys, p_tmp );

        vlc_picture_chain_AppendChain( last_pic, p_tmp );
        last_pic = p_tmp;
    }

    last_pic = p_sys->p_previous_pic;
    p_sys->p_previous_pic = p_picture;
    return last_pic;
}

static void Flush( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    date_Init( &p_sys->next_output_pts,
               p_filter->fmt_out.video.i_frame_rate, p_filter->fmt_out.video.i_frame_rate_base );
    if( p_sys->p_previous_pic )
    {
        picture_Release( p_sys->p_previous_pic );
        p_sys->p_previous_pic = NULL;
    }
}

static void Close( filter_t *p_filter )
{
    Flush( p_filter );
    if( p_filter->vctx_out )
        vlc_video_context_Release( p_filter->vctx_out );
}

static const struct vlc_filter_operations filter_ops =
{
    .filter_video = Filter, .close = Close,
    .flush = Flush,
};

static int Open( filter_t *p_filter )
{
    filter_sys_t *p_sys;

    /* This filter cannot change the format. */
    if( p_filter->fmt_out.video.i_chroma != p_filter->fmt_in.video.i_chroma )
        return VLC_EGENERIC;

    p_sys = p_filter->p_sys = vlc_obj_malloc( VLC_OBJECT(p_filter), sizeof( *p_sys ) );

    if( unlikely( !p_sys ) )
        return VLC_ENOMEM;

    config_ChainParse( p_filter, CFG_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );

    const unsigned int i_out_frame_rate = p_filter->fmt_out.video.i_frame_rate;
    const unsigned int i_out_frame_rate_base = p_filter->fmt_out.video.i_frame_rate_base;

    video_format_Clean( &p_filter->fmt_out.video );
    video_format_Copy( &p_filter->fmt_out.video, &p_filter->fmt_in.video );

    /* If we don't have fps option, use filter output values */
    if( var_InheritURational( p_filter, &p_filter->fmt_out.video.i_frame_rate,
                                        &p_filter->fmt_out.video.i_frame_rate_base, CFG_PREFIX "fps" ) )
    {
        p_filter->fmt_out.video.i_frame_rate = i_out_frame_rate;
        p_filter->fmt_out.video.i_frame_rate_base = i_out_frame_rate_base;
    }

    if( p_filter->fmt_out.video.i_frame_rate == 0 ) {
        msg_Err( p_filter, "Invalid output frame rate" );
        vlc_obj_free( VLC_OBJECT(p_filter), p_sys );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_filter, "Converting fps from %d/%d -> %d/%d",
            p_filter->fmt_in.video.i_frame_rate, p_filter->fmt_in.video.i_frame_rate_base,
            p_filter->fmt_out.video.i_frame_rate, p_filter->fmt_out.video.i_frame_rate_base );

    p_sys->i_output_frame_interval = vlc_tick_from_samples(p_filter->fmt_out.video.i_frame_rate_base,
                                                           p_filter->fmt_out.video.i_frame_rate);

    date_Init( &p_sys->next_output_pts,
               p_filter->fmt_out.video.i_frame_rate, p_filter->fmt_out.video.i_frame_rate_base );

    p_sys->p_previous_pic = NULL;

    p_filter->ops = &filter_ops;

    /* We don't change neither the format nor the picture */
    if ( p_filter->vctx_in )
        p_filter->vctx_out = vlc_video_context_Hold( p_filter->vctx_in );

    return VLC_SUCCESS;
}
