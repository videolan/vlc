/*****************************************************************************
 * i420_10_P010.c : Planar YUV 4:2:0 to SemiPlanar P010 4:2:0
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
static void I420_10_P010( filter_t *, picture_t *, picture_t * );
static picture_t *I420_10_P010_Filter( filter_t *, picture_t * );

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

    if ( p_filter->fmt_out.video.i_chroma != VLC_CODEC_P010 )
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

    if ( p_filter->fmt_in.video.i_chroma != VLC_CODEC_I420_10L)
        return -1;

    filter_sys_t *p_sys = vlc_obj_malloc( VLC_OBJECT( p_filter ),
                                          sizeof(*p_sys) );
    if (!p_sys)
         return VLC_ENOMEM;

    p_filter->pf_video_filter = I420_10_P010_Filter;
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
VIDEO_FILTER_WRAPPER( I420_10_P010 )

/*****************************************************************************
 * planar I420 4:2:0 10-bit Y:U:V to semiplanar P010 10/16-bit 4:2:0 Y:UV
 *****************************************************************************/
static void I420_10_P010( filter_t *p_filter, picture_t *p_src,
                                           picture_t *p_dst )
{
    p_dst->format.i_x_offset = p_src->format.i_x_offset;
    p_dst->format.i_y_offset = p_src->format.i_y_offset;

    const size_t pitch[3] = {
        p_src->p[Y_PLANE].i_pitch,
        p_src->p[U_PLANE].i_pitch,
        p_src->p[V_PLANE].i_pitch,
    };

    const uint8_t *plane[3] = {
        (uint8_t*)p_src->p[Y_PLANE].p_pixels,
        (uint8_t*)p_src->p[U_PLANE].p_pixels,
        (uint8_t*)p_src->p[V_PLANE].p_pixels,
    };

    CopyFromI420_10ToP010( p_dst, plane, pitch,
                        p_src->format.i_y_offset + p_src->format.i_visible_height,
                        &p_filter->p_sys->cache );
}

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("YUV 10-bits planar to semiplanar 10-bits conversions") )
    set_capability( "video converter", 160 )
    set_callbacks( Create, Delete )
vlc_module_end ()
