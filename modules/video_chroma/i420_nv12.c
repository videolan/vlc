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

typedef struct
{
    copy_cache_t cache;
} filter_sys_t;

#define GET_PITCHES( pic ) { \
    pic->p[Y_PLANE].i_pitch, \
    pic->p[U_PLANE].i_pitch, \
    pic->p[V_PLANE].i_pitch  \
}

#define GET_PLANES( pic ) { \
    pic->p[Y_PLANE].p_pixels, \
    pic->p[U_PLANE].p_pixels, \
    pic->p[V_PLANE].p_pixels \
}

/*****************************************************************************
 * planar I420 4:2:0 Y:U:V to planar NV12 4:2:0 Y:UV
 *****************************************************************************/
static void I420_NV12( filter_t *p_filter, picture_t *p_src,
                                           picture_t *p_dst )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    p_dst->format.i_x_offset = p_src->format.i_x_offset;
    p_dst->format.i_y_offset = p_src->format.i_y_offset;
    const size_t pitches[] = GET_PITCHES( p_src );
    const uint8_t *planes[] = GET_PLANES( p_src );

    Copy420_P_to_SP( p_dst, planes, pitches,
                     p_src->format.i_y_offset + p_src->format.i_visible_height,
                     &p_sys->cache );
}

/*****************************************************************************
 * planar YV12 4:2:0 Y:V:U to planar NV12 4:2:0 Y:UV
 *****************************************************************************/
static void YV12_NV12( filter_t *p_filter, picture_t *p_src,
                                           picture_t *p_dst )
{
    picture_SwapUV( p_src );
    I420_NV12( p_filter, p_src, p_dst );
}

static void NV12_I420( filter_t *p_filter, picture_t *p_src,
                                           picture_t *p_dst )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    p_dst->format.i_x_offset = p_src->format.i_x_offset;
    p_dst->format.i_y_offset = p_src->format.i_y_offset;
    const size_t pitches[] = GET_PITCHES( p_src );
    const uint8_t *planes[] = GET_PLANES( p_src );

    Copy420_SP_to_P( p_dst, planes, pitches,
                     p_src->format.i_y_offset + p_src->format.i_visible_height,
                     &p_sys->cache );
}

static void NV12_YV12( filter_t *p_filter, picture_t *p_src,
                                           picture_t *p_dst )
{
    NV12_I420( p_filter, p_src, p_dst );
    picture_SwapUV( p_dst );
}

static void I42010B_P010( filter_t *p_filter, picture_t *p_src,
                                              picture_t *p_dst )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    p_dst->format.i_x_offset = p_src->format.i_x_offset;
    p_dst->format.i_y_offset = p_src->format.i_y_offset;
    const size_t pitches[] = GET_PITCHES( p_src );
    const uint8_t *planes[] = GET_PLANES( p_src );

    Copy420_16_P_to_SP( p_dst, planes, pitches,
                        p_src->format.i_y_offset + p_src->format.i_visible_height,
                        -6, &p_sys->cache );
}

static void P010_I42010B( filter_t *p_filter, picture_t *p_src,
                                              picture_t *p_dst )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    p_dst->format.i_x_offset = p_src->format.i_x_offset;
    p_dst->format.i_y_offset = p_src->format.i_y_offset;
    const size_t pitches[] = GET_PITCHES( p_src );
    const uint8_t *planes[] = GET_PLANES( p_src );

    Copy420_16_SP_to_P( p_dst, planes, pitches,
                        p_src->format.i_y_offset + p_src->format.i_visible_height,
                        6, &p_sys->cache );
}

/* Following functions are local */
VIDEO_FILTER_WRAPPER( I420_NV12 )
VIDEO_FILTER_WRAPPER( YV12_NV12 )
VIDEO_FILTER_WRAPPER( NV12_I420 )
VIDEO_FILTER_WRAPPER( NV12_YV12 )
VIDEO_FILTER_WRAPPER( I42010B_P010 )
VIDEO_FILTER_WRAPPER( P010_I42010B )

/*****************************************************************************
 * Create: allocate a chroma function
 *****************************************************************************
 * This function allocates and initializes a chroma function
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;


    /* video must be even, because 4:2:0 is subsampled by 2 in both ways */
    if( p_filter->fmt_in.video.i_width  & 1
     || p_filter->fmt_in.video.i_height & 1 )
        return -1;

    /* resizing not supported */
    if( p_filter->fmt_in.video.i_x_offset + p_filter->fmt_in.video.i_visible_width !=
            p_filter->fmt_out.video.i_x_offset + p_filter->fmt_out.video.i_visible_width
       || p_filter->fmt_in.video.i_y_offset + p_filter->fmt_in.video.i_visible_height !=
            p_filter->fmt_out.video.i_y_offset + p_filter->fmt_out.video.i_visible_height
       || p_filter->fmt_in.video.orientation != p_filter->fmt_out.video.orientation )
        return -1;

    vlc_fourcc_t infcc = p_filter->fmt_in.video.i_chroma;
    vlc_fourcc_t outfcc = p_filter->fmt_out.video.i_chroma;
    uint8_t pixel_bytes = 1;

    switch( infcc )
    {
        case VLC_CODEC_I420:
        case VLC_CODEC_J420:
            if( outfcc != VLC_CODEC_NV12 )
                return -1;
            p_filter->pf_video_filter = I420_NV12_Filter;
            break;

        case VLC_CODEC_YV12:
            if( outfcc != VLC_CODEC_NV12 )
                return -1;
            p_filter->pf_video_filter = YV12_NV12_Filter;
            break;
        case VLC_CODEC_NV12:
            switch( outfcc )
            {
                case VLC_CODEC_I420:
                case VLC_CODEC_J420:
                    p_filter->pf_video_filter = NV12_I420_Filter;
                    break;
                case VLC_CODEC_YV12:
                    p_filter->pf_video_filter = NV12_YV12_Filter;
                    break;
                default:
                    return -1;
            }
            break;

        case VLC_CODEC_I420_10L:
            if( outfcc != VLC_CODEC_P010 )
                return -1;
            pixel_bytes = 2;
            p_filter->pf_video_filter = I42010B_P010_Filter;
            break;

        case VLC_CODEC_P010:
            if( outfcc != VLC_CODEC_I420_10L )
                return -1;
            pixel_bytes = 2;
            p_filter->pf_video_filter = P010_I42010B_Filter;
            break;

        default:
            return -1;
    }

    filter_sys_t *p_sys = vlc_obj_malloc( VLC_OBJECT( p_filter ),
                                          sizeof(*p_sys) );
    if (!p_sys)
         return VLC_ENOMEM;

    if( CopyInitCache( &p_sys->cache, ( p_filter->fmt_in.video.i_x_offset +
                       p_filter->fmt_in.video.i_visible_width ) * pixel_bytes ) )
        return VLC_ENOMEM;

    p_filter->p_sys = p_sys;

    return 0;
}

static void Delete(vlc_object_t *p_this)
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;
    CopyCleanCache( &p_sys->cache );
}

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("YUV planar to semiplanar conversions") )
    set_capability( "video converter", 160 )
    set_callbacks( Create, Delete )
vlc_module_end ()
