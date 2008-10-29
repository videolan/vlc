/*****************************************************************************
 * imageresample.c: scaling and chroma conversion using the old libavcodec API
 *****************************************************************************
 * Copyright (C) 1999-2001 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout.h>
#include <vlc_filter.h>

/* ffmpeg header */
#ifdef HAVE_LIBAVCODEC_AVCODEC_H
#   include <libavcodec/avcodec.h>
#elif defined(HAVE_FFMPEG_AVCODEC_H)
#   include <ffmpeg/avcodec.h>
#else
#   include <avcodec.h>
#endif

#include "../codec/avcodec/avcodec.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenFilter( vlc_object_t * );
static void CloseFilter( vlc_object_t * );

static void Conversion( filter_t *, picture_t *, picture_t * );
static picture_t *Conversion_Filter( filter_t *, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_capability( "video filter2", 50 )
    set_callbacks( OpenFilter, CloseFilter )
    set_description( N_("FFmpeg video filter") )
vlc_module_end ()

/*****************************************************************************
 * chroma_sys_t: chroma method descriptor
 *****************************************************************************
 * This structure is part of the chroma transformation descriptor, it
 * describes the chroma plugin specific properties.
 *****************************************************************************/
struct filter_sys_t
{
    int i_src_vlc_chroma;
    int i_src_ffmpeg_chroma;
    int i_dst_vlc_chroma;
    int i_dst_ffmpeg_chroma;
    AVPicture tmp_pic;
    ImgReSampleContext *p_rsc;
};

/*****************************************************************************
 * OpenFilter: allocate a chroma function
 *****************************************************************************
 * This function allocates and initializes a chroma function
 *****************************************************************************/
int OpenFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    int i_ffmpeg_chroma[2];

    /*
     * Check the source chroma first, then the destination chroma
     */
    if( GetFfmpegChroma( &i_ffmpeg_chroma[0], p_filter->fmt_in.video ) == VLC_EGENERIC )
        return VLC_EGENERIC;
    if( GetFfmpegChroma( &i_ffmpeg_chroma[1], p_filter->fmt_out.video ) == VLC_EGENERIC )
        return VLC_EGENERIC;

    p_filter->pf_video_filter = Conversion_Filter;

    p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
    {
        return VLC_ENOMEM;
    }

    p_filter->p_sys->i_src_vlc_chroma = p_filter->fmt_in.video.i_chroma;
    p_filter->p_sys->i_dst_vlc_chroma = p_filter->fmt_out.video.i_chroma;
    p_filter->p_sys->i_src_ffmpeg_chroma = i_ffmpeg_chroma[0];
    p_filter->p_sys->i_dst_ffmpeg_chroma = i_ffmpeg_chroma[1];

    if( ( p_filter->fmt_in.video.i_height != p_filter->fmt_out.video.i_height ||
          p_filter->fmt_in.video.i_width != p_filter->fmt_out.video.i_width ) &&
        ( p_filter->p_sys->i_dst_vlc_chroma == VLC_FOURCC('I','4','2','0') ||
          p_filter->p_sys->i_dst_vlc_chroma == VLC_FOURCC('Y','V','1','2') ))
    {
        msg_Dbg( p_filter, "preparing to resample picture" );
        p_filter->p_sys->p_rsc =
            img_resample_init( p_filter->fmt_out.video.i_width,
                               p_filter->fmt_out.video.i_height,
                               p_filter->fmt_in.video.i_width,
                               p_filter->fmt_in.video.i_height );
        avpicture_alloc( &p_filter->p_sys->tmp_pic,
                         p_filter->p_sys->i_dst_ffmpeg_chroma,
                         p_filter->fmt_in.video.i_width,
                         p_filter->fmt_in.video.i_height );
    }
    else
    {
        msg_Dbg( p_filter, "no resampling" );
        p_filter->p_sys->p_rsc = NULL;
    }

    return VLC_SUCCESS;
}

VIDEO_FILTER_WRAPPER( Conversion )

/*****************************************************************************
 * ChromaConversion: actual chroma conversion function
 *****************************************************************************/
static void Conversion( filter_t *p_filter,
                        picture_t *p_src, picture_t *p_dest )
{
    AVPicture src_pic;
    AVPicture dest_pic;
    int i;

    /* Prepare the AVPictures for converion */
    for( i = 0; i < p_src->i_planes; i++ )
    {
        src_pic.data[i] = p_src->p[i].p_pixels;
        src_pic.linesize[i] = p_src->p[i].i_pitch;
    }
    for( i = 0; i < p_dest->i_planes; i++ )
    {
        dest_pic.data[i] = p_dest->p[i].p_pixels;
        dest_pic.linesize[i] = p_dest->p[i].i_pitch;
    }

    /* Special cases */
    if( p_filter->p_sys->i_src_vlc_chroma == VLC_FOURCC('Y','V','1','2') ||
        p_filter->p_sys->i_src_vlc_chroma == VLC_FOURCC('Y','V','U','9') )
    {
        /* Invert U and V */
        src_pic.data[1] = p_src->p[2].p_pixels;
        src_pic.data[2] = p_src->p[1].p_pixels;
    }
    if( p_filter->p_sys->i_dst_vlc_chroma == VLC_FOURCC('Y','V','1','2') ||
        p_filter->p_sys->i_dst_vlc_chroma == VLC_FOURCC('Y','V','U','9') )
    {
        /* Invert U and V */
        dest_pic.data[1] = p_dest->p[2].p_pixels;
        dest_pic.data[2] = p_dest->p[1].p_pixels;
    }
    if( p_filter->p_sys->i_src_ffmpeg_chroma == PIX_FMT_RGB24 )
        if( p_filter->fmt_in.video.i_bmask == 0x00ff0000 )
            p_filter->p_sys->i_src_ffmpeg_chroma = PIX_FMT_BGR24;

    if( p_filter->p_sys->p_rsc )
    {
        img_convert( &p_filter->p_sys->tmp_pic,
                     p_filter->p_sys->i_dst_ffmpeg_chroma,
                     &src_pic, p_filter->p_sys->i_src_ffmpeg_chroma,
                     p_filter->fmt_in.video.i_width,
                     p_filter->fmt_in.video.i_height );
        img_resample( p_filter->p_sys->p_rsc, &dest_pic,
                      &p_filter->p_sys->tmp_pic );
    }
    else
    {
        img_convert( &dest_pic, p_filter->p_sys->i_dst_ffmpeg_chroma,
                     &src_pic, p_filter->p_sys->i_src_ffmpeg_chroma,
                     p_filter->fmt_in.video.i_width,
                     p_filter->fmt_in.video.i_height );
    }
}

/*****************************************************************************
 * CloseFilter: free the chroma function
 *****************************************************************************
 * This function frees the previously allocated chroma function
 *****************************************************************************/
void CloseFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    if( p_filter->p_sys->p_rsc )
    {
        img_resample_close( p_filter->p_sys->p_rsc );
        avpicture_free( &p_filter->p_sys->tmp_pic );
    }
    free( p_filter->p_sys );
}
