/*****************************************************************************
 * video filter: video filter doing chroma conversion and resizing
 *               using the ffmpeg library
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/decoder.h>
#include "vlc_filter.h"

/* ffmpeg header */
#ifdef HAVE_FFMPEG_AVCODEC_H
#   include <ffmpeg/avcodec.h>
#else
#   include <avcodec.h>
#endif

#include "ffmpeg.h"

void E_(InitLibavcodec) ( vlc_object_t *p_object );
static int CheckInit( filter_t *p_filter );
static picture_t *Process( filter_t *p_filter, picture_t *p_pic );
static picture_t *Deinterlace( filter_t *p_filter, picture_t *p_pic );

/*****************************************************************************
 * filter_sys_t : filter descriptor
 *****************************************************************************/
struct filter_sys_t
{
    vlc_bool_t b_resize;
    vlc_bool_t b_convert;

    es_format_t fmt_in;
    int i_src_ffmpeg_chroma;
    es_format_t fmt_out;
    int i_dst_ffmpeg_chroma;

    AVPicture tmp_pic;
    ImgReSampleContext *p_rsc;
};

/*****************************************************************************
 * OpenFilter: probe the filter and return score
 *****************************************************************************/
int E_(OpenFilter)( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys;
    vlc_bool_t b_convert, b_resize;

    /* Check if we can handle that formats */
    if( E_(GetFfmpegChroma)( p_filter->fmt_in.video.i_chroma ) < 0 ||
        E_(GetFfmpegChroma)( p_filter->fmt_out.video.i_chroma ) < 0 )
    {
        return VLC_EGENERIC;
    }

    b_resize =
        p_filter->fmt_in.video.i_width != p_filter->fmt_out.video.i_width ||
        p_filter->fmt_in.video.i_height != p_filter->fmt_out.video.i_height;
    b_convert =
        p_filter->fmt_in.video.i_chroma != p_filter->fmt_out.video.i_chroma;

    if( !b_resize && !b_convert )
    {
        /* Nothing to do */
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
    p_sys->p_rsc = NULL;
    p_sys->b_convert = b_convert;
    p_sys->i_src_ffmpeg_chroma =
        E_(GetFfmpegChroma)( p_filter->fmt_in.video.i_chroma );
    p_sys->i_dst_ffmpeg_chroma =
        E_(GetFfmpegChroma)( p_filter->fmt_out.video.i_chroma );
    p_filter->pf_video_filter = Process;
    es_format_Init( &p_sys->fmt_in, 0, 0 );
    es_format_Init( &p_sys->fmt_out, 0, 0 );

    if( CheckInit( p_filter ) != VLC_SUCCESS )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    if( p_sys->b_resize && p_sys->b_convert )
    {
        if ( p_filter->fmt_in.video.i_width * p_filter->fmt_in.video.i_height >
             p_filter->fmt_out.video.i_width * p_filter->fmt_out.video.i_height )
        {
            /* Resizing then conversion */
            avpicture_alloc( &p_sys->tmp_pic, p_sys->i_src_ffmpeg_chroma,
                             p_filter->fmt_out.video.i_width,
                             p_filter->fmt_out.video.i_height );
        }
        else
        {
            /* Conversion then resizing */
            avpicture_alloc( &p_sys->tmp_pic, p_sys->i_dst_ffmpeg_chroma,
                             p_filter->fmt_in.video.i_width,
                             p_filter->fmt_in.video.i_height );
        }
    }

    msg_Dbg( p_filter, "input: %ix%i %4.4s -> %ix%i %4.4s",
             p_filter->fmt_in.video.i_width, p_filter->fmt_in.video.i_height,
             (char *)&p_filter->fmt_in.video.i_chroma,
             p_filter->fmt_out.video.i_width, p_filter->fmt_out.video.i_height,
             (char *)&p_filter->fmt_out.video.i_chroma );

    /* libavcodec needs to be initialized for some chroma conversions */
    E_(InitLibavcodec)(p_this);

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseFilter: clean up the filter
 *****************************************************************************/
void E_(CloseFilter)( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t*)p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    if( p_sys->p_rsc ) img_resample_close( p_sys->p_rsc );

    avpicture_free( &p_sys->tmp_pic );

    free( p_sys );
}

/*****************************************************************************
 * CheckInit: Initialise filter when necessary
 *****************************************************************************/
static int CheckInit( filter_t *p_filter )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    if( p_filter->fmt_in.video.i_width != p_sys->fmt_in.video.i_width ||
        p_filter->fmt_in.video.i_height != p_sys->fmt_in.video.i_height ||
        p_filter->fmt_out.video.i_width != p_sys->fmt_out.video.i_width ||
        p_filter->fmt_out.video.i_height != p_sys->fmt_out.video.i_height )
    {
        if( p_sys->p_rsc ) img_resample_close( p_sys->p_rsc );
        p_sys->p_rsc = 0;

        p_sys->b_resize =
          p_filter->fmt_in.video.i_width != p_filter->fmt_out.video.i_width ||
          p_filter->fmt_in.video.i_height != p_filter->fmt_out.video.i_height;

        if( p_sys->b_resize )
        {
            p_sys->p_rsc = img_resample_init( p_filter->fmt_out.video.i_width,
                               p_filter->fmt_out.video.i_height,
                               p_filter->fmt_in.video.i_width,
                               p_filter->fmt_in.video.i_height );

            if( !p_sys->p_rsc )
            {
                msg_Err( p_filter, "img_resample_init failed" );
                return VLC_EGENERIC;
            }
        }

        p_sys->fmt_in = p_filter->fmt_in;
        p_sys->fmt_out = p_filter->fmt_out;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Do the processing here
 *****************************************************************************/
static picture_t *Process( filter_t *p_filter, picture_t *p_pic )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    AVPicture src_pic, dest_pic, inter_pic;
    AVPicture *p_src, *p_dst;
    picture_t *p_pic_dst;
    vlc_bool_t b_resize = p_sys->b_resize;
    int i;

    /* Check if format properties changed */
    if( CheckInit( p_filter ) != VLC_SUCCESS ) return 0;

    /* Request output picture */
    p_pic_dst = p_filter->pf_vout_buffer_new( p_filter );
    if( !p_pic_dst )
    {
        msg_Warn( p_filter, "can't get output picture" );
        p_pic->pf_release( p_pic );
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

    /* Special cases */
    if( p_filter->fmt_in.video.i_chroma == VLC_FOURCC('Y','V','1','2') ||
        p_filter->fmt_in.video.i_chroma == VLC_FOURCC('Y','V','U','9') )
    {
        /* Invert U and V */
        src_pic.data[1] = p_pic->p[2].p_pixels;
        src_pic.data[2] = p_pic->p[1].p_pixels;
    }
    if( p_filter->fmt_out.video.i_chroma == VLC_FOURCC('Y','V','1','2') ||
        p_filter->fmt_out.video.i_chroma == VLC_FOURCC('Y','V','U','9') )
    {
        /* Invert U and V */
        dest_pic.data[1] = p_pic_dst->p[2].p_pixels;
        dest_pic.data[2] = p_pic_dst->p[1].p_pixels;
    }
    if( p_sys->i_src_ffmpeg_chroma == PIX_FMT_RGB24 )
        if( p_filter->fmt_in.video.i_bmask == 0x00ff0000 )
            p_sys->i_src_ffmpeg_chroma = PIX_FMT_BGR24;

    p_src = &src_pic;

    if( b_resize && p_sys->p_rsc )
    {
        p_dst = &dest_pic;
        if ( p_filter->fmt_in.video.i_width * p_filter->fmt_in.video.i_height >
             p_filter->fmt_out.video.i_width * p_filter->fmt_out.video.i_height )
        {
            if ( p_sys->b_convert ) p_dst = &p_sys->tmp_pic;
            img_resample( p_sys->p_rsc, p_dst, p_src );
            b_resize = VLC_FALSE;
            p_src = p_dst;
        }
    }

    if( p_sys->b_convert )
    {
        video_format_t *p_fmt = &p_filter->fmt_out.video;
        p_dst = &dest_pic;
        if( b_resize )
        {
            p_dst = &p_sys->tmp_pic;
            p_fmt = &p_filter->fmt_in.video;
        }

        img_convert( p_dst, p_sys->i_dst_ffmpeg_chroma,
                     p_src, p_sys->i_src_ffmpeg_chroma,
                     p_fmt->i_width, p_fmt->i_height );

        p_src = p_dst;
    }

    if( b_resize && p_sys->p_rsc )
    {
        p_dst = &dest_pic;
        img_resample( p_sys->p_rsc, p_dst, p_src );
    }

    /* Special case for RV32 -> YUVA */
    if( !p_sys->b_resize &&
        p_filter->fmt_in.video.i_chroma == VLC_FOURCC('R','V','3','2') &&
        p_filter->fmt_out.video.i_chroma == VLC_FOURCC('Y','U','V','A') )
    {
        uint8_t *p_src = p_pic->p[0].p_pixels;
        int i_src_pitch = p_pic->p[0].i_pitch;
        uint8_t *p_dst = p_pic_dst->p[3].p_pixels;
        int i_dst_pitch = p_pic_dst->p[3].i_pitch;
        int j;

        for( i = 0; i < p_filter->fmt_out.video.i_height; i++ )
        {
            for( j = 0; j < p_filter->fmt_out.video.i_width; j++ )
            {
              p_dst[j] = p_src[j*4+3];
            }
            p_src += i_src_pitch;
            p_dst += i_dst_pitch;
        }
    }
    else if( p_filter->fmt_out.video.i_chroma == VLC_FOURCC('Y','U','V','A') )
    {
        /* Special case for YUVA */
        memset( p_pic_dst->p[3].p_pixels, 0xFF,
                p_pic_dst->p[3].i_pitch * p_pic_dst->p[3].i_lines );
    }

    p_pic_dst->date = p_pic->date;
    p_pic_dst->b_force = p_pic->b_force;
    p_pic_dst->i_nb_fields = p_pic->i_nb_fields;
    p_pic_dst->b_progressive = p_pic->b_progressive;
    p_pic_dst->b_top_field_first = p_pic->b_top_field_first;

    p_pic->pf_release( p_pic );
    return p_pic_dst;
}

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
