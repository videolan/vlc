/*****************************************************************************
 * scale.c: video scaling module for YUVP/A, I420 and RGBA pictures
 *  Uses the low quality "nearest neighbour" algorithm.
 *****************************************************************************
 * Copyright (C) 2003-2007 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Antoine Cellerier <dionoea @t videolan dot org>
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

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int  OpenFilter ( vlc_object_t * );
static picture_t *Filter( filter_t *, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Video scaling filter") )
    set_capability( "video converter", 10 )
    set_callback( OpenFilter )
vlc_module_end ()

/*****************************************************************************
 * OpenFilter: probe the filter and return score
 *****************************************************************************/
static int OpenFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;

    if( ( p_filter->fmt_in.video.i_chroma != VLC_CODEC_YUVP &&
          p_filter->fmt_in.video.i_chroma != VLC_CODEC_YUVA &&
          p_filter->fmt_in.video.i_chroma != VLC_CODEC_I420 &&
          p_filter->fmt_in.video.i_chroma != VLC_CODEC_YV12 &&
          p_filter->fmt_in.video.i_chroma != VLC_CODEC_RGB32 &&
          p_filter->fmt_in.video.i_chroma != VLC_CODEC_RGBA &&
          p_filter->fmt_in.video.i_chroma != VLC_CODEC_ARGB &&
          p_filter->fmt_in.video.i_chroma != VLC_CODEC_BGRA ) ||
        p_filter->fmt_in.video.i_chroma != p_filter->fmt_out.video.i_chroma )
    {
        return VLC_EGENERIC;
    }

    if( p_filter->fmt_in.video.orientation != p_filter->fmt_out.video.orientation )
        return VLC_EGENERIC;

#warning Converter cannot (really) change output format.
    video_format_ScaleCropAr( &p_filter->fmt_out.video, &p_filter->fmt_in.video );
    p_filter->pf_video_filter = Filter;

    msg_Dbg( p_filter, "%ix%i -> %ix%i", p_filter->fmt_in.video.i_width,
             p_filter->fmt_in.video.i_height, p_filter->fmt_out.video.i_width,
             p_filter->fmt_out.video.i_height );

    return VLC_SUCCESS;
}

/****************************************************************************
 * Filter: the whole thing
 ****************************************************************************/
static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_pic_dst;

    if( !p_pic ) return NULL;

#warning Converter cannot (really) change output format.
    video_format_ScaleCropAr( &p_filter->fmt_out.video, &p_filter->fmt_in.video );

    /* Request output picture */
    p_pic_dst = filter_NewPicture( p_filter );
    if( !p_pic_dst )
    {
        picture_Release( p_pic );
        return NULL;
    }

    if( p_filter->fmt_in.video.i_chroma != VLC_CODEC_RGBA &&
        p_filter->fmt_in.video.i_chroma != VLC_CODEC_ARGB &&
        p_filter->fmt_in.video.i_chroma != VLC_CODEC_BGRA &&
        p_filter->fmt_in.video.i_chroma != VLC_CODEC_RGB32 )
    {
        for( int i_plane = 0; i_plane < p_pic_dst->i_planes; i_plane++ )
        {
            const int i_src_pitch    = p_pic->p[i_plane].i_pitch;
            const int i_dst_pitch    = p_pic_dst->p[i_plane].i_pitch;
            const int i_src_height   = p_filter->fmt_in.video.i_height;
            const int i_src_width    = p_filter->fmt_in.video.i_width;
            const int i_dst_height   = p_filter->fmt_out.video.i_height;
            const int i_dst_width    = p_filter->fmt_out.video.i_width;
            const int i_dst_visible_lines =
                                       p_pic_dst->p[i_plane].i_visible_lines;
            const int i_dst_visible_pitch =
                                       p_pic_dst->p[i_plane].i_visible_pitch;
            const int i_dst_hidden_pitch  = i_dst_pitch - i_dst_visible_pitch;
#define SHIFT_SIZE 16
            const int i_height_coef  = ( i_src_height << SHIFT_SIZE )
                                       / i_dst_height;
            const int i_width_coef   = ( i_src_width << SHIFT_SIZE )
                                       / i_dst_width;
            const int i_src_height_1 = i_src_height - 1;
            const int i_src_width_1  = i_src_width - 1;

            uint8_t *p_src = p_pic->p[i_plane].p_pixels;
            uint8_t *p_dst = p_pic_dst->p[i_plane].p_pixels;
            uint8_t *p_dstendline = p_dst + i_dst_visible_pitch;
            const uint8_t *p_dstend = p_dst + i_dst_visible_lines*i_dst_pitch;

            const int i_shift_height = i_dst_height / i_src_height;
            const int i_shift_width = i_dst_width / i_src_width;

            int l = 1<<(SHIFT_SIZE-i_shift_height);
            for( ; p_dst < p_dstend;
                 p_dst += i_dst_hidden_pitch,
                 p_dstendline += i_dst_pitch, l += i_height_coef )
            {
                int k = 1<<(SHIFT_SIZE-i_shift_width);
                uint8_t *p_srcl = p_src
                       + (__MIN( i_src_height_1, l >> SHIFT_SIZE )*i_src_pitch);

                for( ; p_dst < p_dstendline; p_dst++, k += i_width_coef )
                {
                    *p_dst = p_srcl[__MIN( i_src_width_1, k >> SHIFT_SIZE )];
                }
            }
        }
    }
    else /* RGBA */
    {
        const int i_src_pitch = p_pic->p->i_pitch;
        const int i_dst_pitch = p_pic_dst->p->i_pitch;
        const int i_src_height   = p_filter->fmt_in.video.i_height;
        const int i_src_width    = p_filter->fmt_in.video.i_width;
        const int i_dst_height   = p_filter->fmt_out.video.i_height;
        const int i_dst_width    = p_filter->fmt_out.video.i_width;
        const int i_dst_visible_lines =
                                   p_pic_dst->p->i_visible_lines;
        const int i_dst_visible_pitch =
                                   p_pic_dst->p->i_visible_pitch;
        const int i_dst_hidden_pitch  = i_dst_pitch - i_dst_visible_pitch;
        const int i_height_coef  = ( i_src_height << SHIFT_SIZE )
                                   / i_dst_height;
        const int i_width_coef   = ( i_src_width << SHIFT_SIZE )
                                   / i_dst_width;
        const int i_src_height_1 = i_src_height - 1;
        const int i_src_width_1  = i_src_width - 1;

        uint32_t *p_src = (uint32_t*)p_pic->p->p_pixels;
        uint32_t *p_dst = (uint32_t*)p_pic_dst->p->p_pixels;
        uint32_t *p_dstendline = p_dst + (i_dst_visible_pitch>>2);
        const uint32_t *p_dstend = p_dst + i_dst_visible_lines*(i_dst_pitch>>2);

        const int i_shift_height = i_dst_height / i_src_height;
        const int i_shift_width = i_dst_width / i_src_width;

        int l = 1<<(SHIFT_SIZE-i_shift_height);
        for( ; p_dst < p_dstend;
             p_dst += (i_dst_hidden_pitch>>2),
             p_dstendline += (i_dst_pitch>>2),
             l += i_height_coef )
        {
            int k = 1<<(SHIFT_SIZE-i_shift_width);
            uint32_t *p_srcl = p_src
                    + (__MIN( i_src_height_1, l >> SHIFT_SIZE )*(i_src_pitch>>2));
            for( ; p_dst < p_dstendline; p_dst++, k += i_width_coef )
            {
                *p_dst = p_srcl[__MIN( i_src_width_1, k >> SHIFT_SIZE )];
            }
        }
    }

    picture_CopyProperties( p_pic_dst, p_pic );
    picture_Release( p_pic );
    return p_pic_dst;
}
