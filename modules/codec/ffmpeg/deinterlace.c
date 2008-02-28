/*****************************************************************************
 * video filter: video filter doing chroma conversion and resizing
 *               using the ffmpeg library
 *****************************************************************************
 * Copyright (C) 1999-2001 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>
#include <vlc_codec.h>
#include <vlc_vout.h>
#include <vlc_filter.h>

/* ffmpeg header */
#ifdef HAVE_LIBAVCODEC_AVCODEC_H
#   include <libavcodec/avcodec.h>
#else
#   include <avcodec.h>
#endif

#include "ffmpeg.h"

static picture_t *Deinterlace( filter_t *p_filter, picture_t *p_pic );

/*****************************************************************************
 * filter_sys_t : filter descriptor
 *****************************************************************************/
struct filter_sys_t
{
    vlc_bool_t b_resize;
    vlc_bool_t b_convert;
    vlc_bool_t b_resize_first;
    vlc_bool_t b_enable_croppadd;

    es_format_t fmt_in;
    int i_src_ffmpeg_chroma;
    es_format_t fmt_out;
    int i_dst_ffmpeg_chroma;

    AVPicture tmp_pic;
};

/*****************************************************************************
 * OpenDeinterlace: probe the filter and return score
 *****************************************************************************/
int E_(OpenDeinterlace)( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys;

    /* Check if we can handle that formats */
    if( E_(GetFfmpegChroma)( p_filter->fmt_in.video.i_chroma ) < 0 )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_filter->p_sys = p_sys =
          (filter_sys_t *)malloc(sizeof(filter_sys_t)) ) == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        return VLC_EGENERIC;
    }

    /* Misc init */
    p_sys->i_src_ffmpeg_chroma =
        E_(GetFfmpegChroma)( p_filter->fmt_in.video.i_chroma );
    p_filter->pf_video_filter = Deinterlace;

    msg_Dbg( p_filter, "deinterlacing" );

    /* libavcodec needs to be initialized for some chroma conversions */
    E_(InitLibavcodec)(p_this);

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseDeinterlace: clean up the filter
 *****************************************************************************/
void E_(CloseDeinterlace)( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    free( p_sys );
}

/*****************************************************************************
 * Do the processing here
 *****************************************************************************/
static picture_t *Deinterlace( filter_t *p_filter, picture_t *p_pic )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    AVPicture src_pic, dest_pic;
    picture_t *p_pic_dst;
    int i;

    /* Request output picture */
    p_pic_dst = p_filter->pf_vout_buffer_new( p_filter );
    if( !p_pic_dst )
    {
        msg_Warn( p_filter, "can't get output picture" );
        return NULL;
    }

    /* Prepare the AVPictures for the conversion */
    for( i = 0; i < p_pic->i_planes; i++ )
    {
        src_pic.data[i] = p_pic->p[i].p_pixels;
        src_pic.linesize[i] = p_pic->p[i].i_pitch;
    }
    for( i = 0; i < p_pic_dst->i_planes; i++ )
    {
        dest_pic.data[i] = p_pic_dst->p[i].p_pixels;
        dest_pic.linesize[i] = p_pic_dst->p[i].i_pitch;
    }

    avpicture_deinterlace( &dest_pic, &src_pic, p_sys->i_src_ffmpeg_chroma,
                           p_filter->fmt_in.video.i_width,
                           p_filter->fmt_in.video.i_height );

    p_pic_dst->date = p_pic->date;
    p_pic_dst->b_force = p_pic->b_force;
    p_pic_dst->i_nb_fields = p_pic->i_nb_fields;
    p_pic_dst->b_progressive = VLC_TRUE;
    p_pic_dst->b_top_field_first = p_pic->b_top_field_first;

    p_pic->pf_release( p_pic );
    return p_pic_dst;
}
