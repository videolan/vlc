/*****************************************************************************
 * i420_nv12.c : Planar YUV 4:2:0 to SemiPlanar NV12 4:2:0
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
 *
 * Authors: Steve Lhomme <robux4@videolabs.io>
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
#include "copy.h"

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
static void I420_NV12( filter_t *, picture_t *, picture_t * );
static void YV12_NV12( filter_t *, picture_t *, picture_t * );
static picture_t *I420_NV12_Filter( filter_t *, picture_t * );
static picture_t *YV12_NV12_Filter( filter_t *, picture_t * );

struct filter_sys_t
{
    copy_cache_t cache;
};

/*****************************************************************************
 * Create: allocate a chroma function
 *****************************************************************************
 * This function allocates and initializes a chroma function
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    if ( p_filter->fmt_out.video.i_chroma != VLC_CODEC_NV12 )
        return -1;

    /* video must be even, because 4:2:0 is subsampled by 2 in both ways */
    if( p_filter->fmt_in.video.i_width  & 1
     || p_filter->fmt_in.video.i_height & 1 )
    {
        return -1;
    }

    /* resizing not supported */
    if( p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width !=
            p_filter->fmt_out.video.i_x_offset + p_filter->fmt_out.video.i_visible_width
       || p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height !=
            p_filter->fmt_out.video.i_y_offset + p_filter->fmt_out.video.i_visible_height
       || p_filter->fmt_in.video.orientation != p_filter->fmt_out.video.orientation )
        return -1;

    switch( p_filter->fmt_in.video.i_chroma )
    {
        case VLC_CODEC_I420:
        case VLC_CODEC_J420:
            p_filter->pf_video_filter = I420_NV12_Filter;
            break;

        case VLC_CODEC_YV12:
            p_filter->pf_video_filter = YV12_NV12_Filter;
            break;

        default:
            return -1;
    }

    filter_sys_t *p_sys = vlc_obj_malloc( VLC_OBJECT( p_filter ),
                                          sizeof(*p_sys) );
    if (!p_sys)
         return VLC_ENOMEM;

    CopyInitCache( &p_sys->cache, p_filter->fmt_in.video.i_x_offset +
                                  p_filter->fmt_in.video.i_visible_width );
    p_filter->p_sys = p_sys;

    return 0;
}

static void Delete(vlc_object_t *p_this)
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;
    CopyCleanCache( &p_sys->cache );
}

/* Following functions are local */
VIDEO_FILTER_WRAPPER( I420_NV12 )
VIDEO_FILTER_WRAPPER( YV12_NV12 )

static void I420_YUV( filter_sys_t *p_sys, picture_t *p_src, picture_t *p_dst, bool invertUV )
{
    p_dst->format.i_x_offset = p_src->format.i_x_offset;
    p_dst->format.i_y_offset = p_src->format.i_y_offset;

    const size_t u_plane = invertUV ? V_PLANE : U_PLANE;
    const size_t v_plane = invertUV ? U_PLANE : V_PLANE;

    const size_t pitch[3] = {
        p_src->p[Y_PLANE].i_pitch,
        p_src->p[u_plane].i_pitch,
        p_src->p[v_plane].i_pitch,
    };

    const uint8_t *plane[3] = {
        (uint8_t*)p_src->p[Y_PLANE].p_pixels,
        (uint8_t*)p_src->p[u_plane].p_pixels,
        (uint8_t*)p_src->p[v_plane].p_pixels,
    };

    Copy420_P_to_SP( p_dst, plane, pitch,
                     p_src->format.i_y_offset + p_src->format.i_visible_height,
                     &p_sys->cache );
}

/*****************************************************************************
 * planar I420 4:2:0 Y:U:V to planar NV12 4:2:0 Y:UV
 *****************************************************************************/
static void I420_NV12( filter_t *p_filter, picture_t *p_src,
                                           picture_t *p_dst )
{
    I420_YUV( p_filter->p_sys, p_src, p_dst, false );
}

/*****************************************************************************
 * planar YV12 4:2:0 Y:V:U to planar NV12 4:2:0 Y:UV
 *****************************************************************************/
static void YV12_NV12( filter_t *p_filter, picture_t *p_src,
                                           picture_t *p_dst )
{
    I420_YUV( p_filter->p_sys, p_src, p_dst, true );
}


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("YUV planar to semiplanar conversions") )
    set_capability( "video converter", 160 )
    set_callbacks( Create, Delete )
vlc_module_end ()
