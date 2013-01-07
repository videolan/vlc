/*****************************************************************************
 * i422_i420.c : Planar YUV 4:2:2 to Planar YUV 4:2:0 conversion module for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 - 2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Damien Fouilleul <damienf@videolan.org>
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

#define SRC_FOURCC  "I422,J422"
#define DEST_FOURCC "I420,IYUV,J420,YV12,YUVA"

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
static int  Activate ( vlc_object_t * );

static void I422_I420( filter_t *, picture_t *, picture_t * );
static void I422_YV12( filter_t *, picture_t *, picture_t * );
static void I422_YUVA( filter_t *, picture_t *, picture_t * );
static picture_t *I422_I420_Filter( filter_t *, picture_t * );
static picture_t *I422_YV12_Filter( filter_t *, picture_t * );
static picture_t *I422_YUVA_Filter( filter_t *, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Conversions from " SRC_FOURCC " to " DEST_FOURCC) )
    set_capability( "video filter2", 60 )
    set_callbacks( Activate, NULL )
vlc_module_end ()

/*****************************************************************************
 * Activate: allocate a chroma function
 *****************************************************************************
 * This function allocates and initializes a chroma function
 *****************************************************************************/
static int Activate( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    if( p_filter->fmt_in.video.i_width & 1
     || p_filter->fmt_in.video.i_height & 1 )
    {
        return -1;
    }

    if( p_filter->fmt_in.video.i_width != p_filter->fmt_out.video.i_width
     || p_filter->fmt_in.video.i_height != p_filter->fmt_out.video.i_height )
        return -1;

    switch( p_filter->fmt_in.video.i_chroma )
    {
        case VLC_CODEC_I422:
        case VLC_CODEC_J422:
            switch( p_filter->fmt_out.video.i_chroma )
            {
                case VLC_CODEC_I420:
                case VLC_CODEC_J420:
                    p_filter->pf_video_filter = I422_I420_Filter;
                    break;

                case VLC_CODEC_YV12:
                    p_filter->pf_video_filter = I422_YV12_Filter;
                    break;

                case VLC_CODEC_YUV420A:
                    p_filter->pf_video_filter = I422_YUVA_Filter;
                    break;

                default:
                    return -1;
            }
            break;

        default:
            return -1;
    }
    return 0;
}

/* Following functions are local */
VIDEO_FILTER_WRAPPER( I422_I420 )
VIDEO_FILTER_WRAPPER( I422_YV12 )
VIDEO_FILTER_WRAPPER( I422_YUVA )

/*****************************************************************************
 * I422_I420: planar YUV 4:2:2 to planar I420 4:2:0 Y:U:V
 *****************************************************************************/
static void I422_I420( filter_t *p_filter, picture_t *p_source,
                                           picture_t *p_dest )
{
    uint16_t i_dpy = p_dest->p[Y_PLANE].i_pitch;
    uint16_t i_spy = p_source->p[Y_PLANE].i_pitch;
    uint16_t i_dpuv = p_dest->p[U_PLANE].i_pitch;
    uint16_t i_spuv = p_source->p[U_PLANE].i_pitch;
    uint16_t i_width = p_filter->fmt_in.video.i_width;
    uint16_t i_y = p_filter->fmt_in.video.i_height;
    uint8_t *p_dy = p_dest->Y_PIXELS + (i_y-1)*i_dpy;
    uint8_t *p_y = p_source->Y_PIXELS + (i_y-1)*i_spy;
    uint8_t *p_du = p_dest->U_PIXELS + (i_y/2-1)*i_dpuv;
    uint8_t *p_u = p_source->U_PIXELS + (i_y-1)*i_spuv;
    uint8_t *p_dv = p_dest->V_PIXELS + (i_y/2-1)*i_dpuv;
    uint8_t *p_v = p_source->V_PIXELS + (i_y-1)*i_spuv;
    i_y /= 2;

    for ( ; i_y--; )
    {
        memcpy(p_dy, p_y, i_width); p_dy -= i_dpy; p_y -= i_spy;
        memcpy(p_dy, p_y, i_width); p_dy -= i_dpy; p_y -= i_spy;
        memcpy(p_du, p_u, i_width/2); p_du -= i_dpuv; p_u -= 2*i_spuv;
        memcpy(p_dv, p_v, i_width/2); p_dv -= i_dpuv; p_v -= 2*i_spuv;
    }
}

/*****************************************************************************
 * I422_YV12: planar YUV 4:2:2 to planar YV12 4:2:0 Y:V:U
 *****************************************************************************/
static void I422_YV12( filter_t *p_filter, picture_t *p_source,
                                           picture_t *p_dest )
{
    uint16_t i_dpy = p_dest->p[Y_PLANE].i_pitch;
    uint16_t i_spy = p_source->p[Y_PLANE].i_pitch;
    uint16_t i_dpuv = p_dest->p[U_PLANE].i_pitch;
    uint16_t i_spuv = p_source->p[U_PLANE].i_pitch;
    uint16_t i_width = p_filter->fmt_in.video.i_width;
    uint16_t i_y = p_filter->fmt_in.video.i_height;
    uint8_t *p_dy = p_dest->Y_PIXELS + (i_y-1)*i_dpy;
    uint8_t *p_y = p_source->Y_PIXELS + (i_y-1)*i_spy;
    uint8_t *p_du = p_dest->V_PIXELS + (i_y/2-1)*i_dpuv; /* U and V are swapped */
    uint8_t *p_u = p_source->U_PIXELS + (i_y-1)*i_spuv;
    uint8_t *p_dv = p_dest->U_PIXELS + (i_y/2-1)*i_dpuv; /* U and V are swapped */
    uint8_t *p_v = p_source->V_PIXELS + (i_y-1)*i_spuv;
    i_y /= 2;

    for ( ; i_y--; )
    {
        memcpy(p_dy, p_y, i_width); p_dy -= i_dpy; p_y -= i_spy;
        memcpy(p_dy, p_y, i_width); p_dy -= i_dpy; p_y -= i_spy;
        memcpy(p_du, p_u, i_width/2); p_du -= i_dpuv; p_u -= 2*i_spuv;
        memcpy(p_dv, p_v, i_width/2); p_dv -= i_dpuv; p_v -= 2*i_spuv;
    }
}

/*****************************************************************************
 * I422_YUVA: planar YUV 4:2:2 to planar YUVA 4:2:0:4 Y:U:V:A
 *****************************************************************************/
static void I422_YUVA( filter_t *p_filter, picture_t *p_source,
                                           picture_t *p_dest )
{
    I422_I420( p_filter, p_source, p_dest );
    memset( p_dest->p[A_PLANE].p_pixels, 0xff,
                p_dest->p[A_PLANE].i_lines * p_dest->p[A_PLANE].i_pitch );
}
