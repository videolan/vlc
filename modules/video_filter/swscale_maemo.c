/*****************************************************************************
 * swscale_maemo.c: scaling and chroma conversion using libswscale_nokia770
 *****************************************************************************
 * Copyright (C) 1999-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Lejeune <phytos@videolan.org>
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

#include "libswscale_nokia770/arm_jit_swscale.h"
#include "libswscale_nokia770/arm_colorconv.h"

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int  OpenScaler( vlc_object_t * );
static void CloseScaler( vlc_object_t * );

static picture_t *Filter( filter_t *, picture_t * );
static int Init( filter_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( N_("Video scaling filter") );
    set_capability( "video filter2", 1000 );
    set_category( CAT_VIDEO );
    set_subcategory( SUBCAT_VIDEO_VFILTER );
    set_callbacks( OpenScaler, CloseScaler );
vlc_module_end();

/*****************************************************************************
 * filter_sys_t : filter descriptor
 *****************************************************************************/
struct filter_sys_t
{
    struct SwsContextArmJit *ctx;

    es_format_t fmt_in;
    es_format_t fmt_out;
};

/*****************************************************************************
 * OpenScaler: probe the filter and return score
 *****************************************************************************/
static int OpenScaler( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys;

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_filter->p_sys = p_sys =
          (filter_sys_t *)malloc(sizeof(filter_sys_t)) ) == NULL )
    {
        return VLC_ENOMEM;
    }

    /* Misc init */
    p_sys->ctx = NULL;
    p_filter->pf_video_filter = Filter;
    es_format_Init( &p_sys->fmt_in, 0, 0 );
    es_format_Init( &p_sys->fmt_out, 0, 0 );

    if( Init( p_filter ) )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_filter, "%ix%i chroma: %4.4s -> %ix%i chroma: %4.4s",
             p_filter->fmt_in.video.i_width, p_filter->fmt_in.video.i_height,
             (char *)&p_filter->fmt_in.video.i_chroma,
             p_filter->fmt_out.video.i_width, p_filter->fmt_out.video.i_height,
             (char *)&p_filter->fmt_out.video.i_chroma );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseFilter: clean up the filter
 *****************************************************************************/
static void CloseScaler( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    if( p_sys->ctx )
        sws_arm_jit_free( p_sys->ctx );
    free( p_sys );
}

/*****************************************************************************
 * Helpers
 *****************************************************************************/

static bool IsFmtSimilar( const video_format_t *p_fmt1, const video_format_t *p_fmt2 )
{
    return p_fmt1->i_chroma == p_fmt2->i_chroma &&
           p_fmt1->i_width  == p_fmt2->i_width &&
           p_fmt1->i_height == p_fmt2->i_height;
}

static int Init( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    if( IsFmtSimilar( &p_filter->fmt_in.video, &p_sys->fmt_in ) &&
        IsFmtSimilar( &p_filter->fmt_out.video, &p_sys->fmt_out ) &&
        p_sys->ctx )
    {
        return VLC_SUCCESS;
    }

    if( ( p_filter->fmt_in.video.i_chroma != VLC_FOURCC('I','4','2','0') &&
          p_filter->fmt_in.video.i_chroma != VLC_FOURCC('I','Y','U','V') &&
          p_filter->fmt_in.video.i_chroma != VLC_FOURCC('Y','V','1','2') ) ||
          p_filter->fmt_out.video.i_chroma != VLC_FOURCC('Y','4','2','0') )
    {
        msg_Err( p_filter, "format not supported" );
        return VLC_EGENERIC;
    }

    if( p_sys->ctx )
        sws_arm_jit_free( p_sys->ctx );

    p_sys->ctx =
        sws_arm_jit_create_omapfb_yuv420_scaler_armv6(
            p_filter->fmt_in.video.i_width,
            p_filter->fmt_in.video.i_height,
            p_filter->fmt_out.video.i_width,
            p_filter->fmt_out.video.i_height, 2 );

    if( !p_sys->ctx )
    {
        msg_Err( p_filter, "could not init SwScaler" );
        return VLC_EGENERIC;
    }

    p_sys->fmt_in = p_filter->fmt_in;
    p_sys->fmt_out = p_filter->fmt_out;

    return VLC_SUCCESS;
}

/****************************************************************************
 * Filter: the whole thing
 ****************************************************************************
 * This function is called just after the thread is launched.
 ****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    uint8_t *src[3]; int src_stride[3];
    uint8_t *dst[3]; int dst_stride[3];
    picture_t *p_pic_dst;
    int i_plane;
    int i_nb_planes = p_pic->i_planes;

    /* Check if format properties changed */
    if( Init( p_filter ) != VLC_SUCCESS )
        return NULL;

    /* Request output picture */
    p_pic_dst = p_filter->pf_vout_buffer_new( p_filter );
    if( !p_pic_dst )
    {
        msg_Warn( p_filter, "can't get output picture" );
        return NULL;
    }

    for( i_plane = 0; i_plane < __MIN(3, p_pic->i_planes); i_plane++ )
    {
        src[i_plane] = p_pic->p[i_plane].p_pixels;
        src_stride[i_plane] = p_pic->p[i_plane].i_pitch;
    }
    for( i_plane = 0; i_plane < __MIN(3, i_nb_planes); i_plane++ )
    {
        dst[i_plane] = p_pic_dst->p[i_plane].p_pixels;
        dst_stride[i_plane] = p_pic_dst->p[i_plane].i_pitch;
    }

    sws_arm_jit_scale( p_sys->ctx, src, src_stride, 0,
                       p_filter->fmt_in.video.i_height, dst, dst_stride);

    picture_CopyProperties( p_pic_dst, p_pic );
    picture_Release( p_pic );

    return p_pic_dst;
}
