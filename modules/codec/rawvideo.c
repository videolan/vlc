/*****************************************************************************
 * rawvideo.c: Pseudo video decoder/packetizer for raw video data
 *****************************************************************************
 * Copyright (C) 2001, 2002 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <vlc_codec.h>

/*****************************************************************************
 * decoder_sys_t : raw video decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /* Module mode */
    bool b_packetizer;

    /*
     * Input properties
     */
    size_t i_raw_size;
    bool b_invert;
    plane_t planes[PICTURE_PLANE_MAX];

    /*
     * Common properties
     */
    date_t pts;
};

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static int  OpenPacketizer( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );

static void *DecodeBlock  ( decoder_t *, block_t ** );

static picture_t *DecodeFrame( decoder_t *, block_t * );
static block_t   *SendFrame  ( decoder_t *, block_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_description( N_("Pseudo raw video decoder") )
    set_capability( "decoder", 50 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    set_callbacks( OpenDecoder, CloseDecoder )

    add_submodule ()
    set_description( N_("Pseudo raw video packetizer") )
    set_capability( "packetizer", 100 )
    set_callbacks( OpenPacketizer, CloseDecoder )
vlc_module_end ()

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    switch( p_dec->fmt_in.i_codec )
    {
        /* Planar YUV */
        case VLC_CODEC_I444:
        case VLC_CODEC_J444:
        case VLC_CODEC_I440:
        case VLC_CODEC_J440:
        case VLC_CODEC_I422:
        case VLC_CODEC_J422:
        case VLC_CODEC_I420:
        case VLC_CODEC_J420:
        case VLC_CODEC_YV12:
        case VLC_CODEC_YV9:
        case VLC_CODEC_I411:
        case VLC_CODEC_I410:
        case VLC_CODEC_GREY:
        case VLC_CODEC_YUVP:
        case VLC_CODEC_NV12:
        case VLC_CODEC_NV21:
        case VLC_CODEC_I422_10L:
        case VLC_CODEC_I422_10B:

        /* Packed YUV */
        case VLC_CODEC_YUYV:
        case VLC_CODEC_YVYU:
        case VLC_CODEC_UYVY:
        case VLC_CODEC_VYUY:

        /* RGB */
        case VLC_CODEC_RGB32:
        case VLC_CODEC_RGB24:
        case VLC_CODEC_RGB16:
        case VLC_CODEC_RGB15:
        case VLC_CODEC_RGB8:
        case VLC_CODEC_RGBP:
        case VLC_CODEC_RGBA:
            break;

        default:
            return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;
    /* Misc init */
    p_dec->p_sys->b_packetizer = false;
    p_sys->b_invert = false;

    if( (int)p_dec->fmt_in.video.i_height < 0 )
    {
        /* Frames are coded from bottom to top */
        p_dec->fmt_in.video.i_height =
            (unsigned int)(-(int)p_dec->fmt_in.video.i_height);
        p_sys->b_invert = true;
    }
    if( !p_dec->fmt_in.video.i_visible_width )
        p_dec->fmt_in.video.i_visible_width = p_dec->fmt_in.video.i_width;
    if( !p_dec->fmt_in.video.i_visible_height )
        p_dec->fmt_in.video.i_visible_height = p_dec->fmt_in.video.i_height;

    if( p_dec->fmt_in.video.i_visible_width <= 0
     || p_dec->fmt_in.video.i_visible_height <= 0 )
    {
        msg_Err( p_dec, "invalid display size %dx%d",
                 p_dec->fmt_in.video.i_width, p_dec->fmt_in.video.i_height );
        return VLC_EGENERIC;
    }

    es_format_Copy( &p_dec->fmt_out, &p_dec->fmt_in );

    date_Init( &p_sys->pts, p_dec->fmt_out.video.i_frame_rate,
               p_dec->fmt_out.video.i_frame_rate_base );
    if( p_dec->fmt_out.video.i_frame_rate == 0 ||
        p_dec->fmt_out.video.i_frame_rate_base == 0)
    {
        msg_Warn( p_dec, "invalid frame rate %d/%d, using 25 fps instead",
                  p_dec->fmt_out.video.i_frame_rate,
                  p_dec->fmt_out.video.i_frame_rate_base);
        date_Init( &p_sys->pts, 25, 1 );
    }

    /* Find out p_vdec->i_raw_size */
    video_format_Setup( &p_dec->fmt_out.video, p_dec->fmt_in.i_codec,
                        p_dec->fmt_in.video.i_visible_width,
                        p_dec->fmt_in.video.i_visible_height,
                        p_dec->fmt_in.video.i_sar_num,
                        p_dec->fmt_in.video.i_sar_den );
    picture_t picture;
    picture_Setup( &picture, p_dec->fmt_out.i_codec,
                   p_dec->fmt_in.video.i_width,
                   p_dec->fmt_in.video.i_height, 0, 1 );
    p_sys->i_raw_size = 0;
    for( int i = 0; i < picture.i_planes; i++ )
    {
        p_sys->i_raw_size += picture.p[i].i_visible_pitch *
                             picture.p[i].i_visible_lines;
        p_sys->planes[i] = picture.p[i];
    }

    if( !p_dec->fmt_in.video.i_sar_num || !p_dec->fmt_in.video.i_sar_den )
    {
        p_dec->fmt_out.video.i_sar_num = 1;
        p_dec->fmt_out.video.i_sar_den = 1;
    }

    /* Set callbacks */
    p_dec->pf_decode_video = (picture_t *(*)(decoder_t *, block_t **))
        DecodeBlock;
    p_dec->pf_packetize    = (block_t *(*)(decoder_t *, block_t **))
        DecodeBlock;

    return VLC_SUCCESS;
}

static int OpenPacketizer( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    int i_ret = OpenDecoder( p_this );

    if( i_ret == VLC_SUCCESS ) p_dec->p_sys->b_packetizer = true;

    return i_ret;
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function must be fed with complete frames.
 ****************************************************************************/
static void *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block;
    void *p_buf;

    if( !pp_block || !*pp_block ) return NULL;

    p_block = *pp_block;


    if( p_block->i_pts <= VLC_TS_INVALID && p_block->i_dts <= VLC_TS_INVALID &&
        !date_Get( &p_sys->pts ) )
    {
        /* We've just started the stream, wait for the first PTS. */
        block_Release( p_block );
        return NULL;
    }

    /* Date management: If there is a pts avaliable, use that. */
    if( p_block->i_pts > VLC_TS_INVALID )
    {
        date_Set( &p_sys->pts, p_block->i_pts );
    }
    else if( p_block->i_dts > VLC_TS_INVALID )
    {
        /* NB, davidf doesn't quite agree with this in general, it is ok
         * for rawvideo since it is in order (ie pts=dts), however, it
         * may not be ok for an out-of-order codec, so don't copy this
         * without thinking */
        date_Set( &p_sys->pts, p_block->i_dts );
    }

    if( p_block->i_buffer < p_sys->i_raw_size )
    {
        msg_Warn( p_dec, "invalid frame size (%zu < %zu)",
                  p_block->i_buffer, p_sys->i_raw_size );

        block_Release( p_block );
        return NULL;
    }

    if( p_sys->b_packetizer )
    {
        p_buf = SendFrame( p_dec, p_block );
    }
    else
    {
        p_buf = DecodeFrame( p_dec, p_block );
    }

    /* Date management: 1 frame per packet */
    date_Increment( &p_sys->pts, 1 );
    *pp_block = NULL;

    return p_buf;
}

/*****************************************************************************
 * FillPicture:
 *****************************************************************************/
static void FillPicture( decoder_t *p_dec, block_t *p_block, picture_t *p_pic )
{
    int i_plane;
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t *p_src = p_block->p_buffer;

    for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
    {
        int i_pitch = p_pic->p[i_plane].i_pitch;
        int i_visible_pitch = p_sys->planes[i_plane].i_visible_pitch;
        int i_visible_lines = p_sys->planes[i_plane].i_visible_lines;
        uint8_t *p_dst = p_pic->p[i_plane].p_pixels;
        uint8_t *p_dst_end = p_dst+i_pitch*i_visible_lines;

        if( p_sys->b_invert )
            for( p_dst_end -= i_pitch; p_dst <= p_dst_end;
                 p_dst_end -= i_pitch, p_src += i_visible_pitch )
                memcpy( p_dst_end, p_src, i_visible_pitch );
        else
            for( ; p_dst < p_dst_end;
                 p_dst += i_pitch, p_src += i_visible_pitch )
                memcpy( p_dst, p_src, i_visible_pitch );
    }
}

/*****************************************************************************
 * DecodeFrame: decodes a video frame.
 *****************************************************************************/
static picture_t *DecodeFrame( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    picture_t *p_pic;

    /* Get a new picture */
    p_pic = decoder_NewPicture( p_dec );
    if( !p_pic )
    {
        block_Release( p_block );
        return NULL;
    }

    FillPicture( p_dec, p_block, p_pic );

    p_pic->date = date_Get( &p_sys->pts );
    if( p_block->i_flags & BLOCK_FLAG_INTERLACED_MASK )
    {
        p_pic->b_progressive = false;
        p_pic->i_nb_fields = 2;
        if( p_block->i_flags & BLOCK_FLAG_TOP_FIELD_FIRST )
            p_pic->b_top_field_first = true;
        else
            p_pic->b_top_field_first = false;
    }
    else
        p_pic->b_progressive = true;

    block_Release( p_block );
    return p_pic;
}

/*****************************************************************************
 * SendFrame: send a video frame to the stream output.
 *****************************************************************************/
static block_t *SendFrame( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    p_block->i_dts = p_block->i_pts = date_Get( &p_sys->pts );

    if( p_sys->b_invert )
    {
        picture_t pic;
        uint8_t *p_tmp, *p_pixels;
        int i, j;

        /* Fill in picture_t fields */
        picture_Setup( &pic, p_dec->fmt_out.i_codec,
                       p_dec->fmt_out.video.i_width,
                       p_dec->fmt_out.video.i_height, 0, 1 );

        if( !pic.i_planes )
        {
            msg_Err( p_dec, "unsupported chroma" );
            return p_block;
        }

        p_tmp = malloc( pic.p[0].i_pitch );
        if( !p_tmp )
            return p_block;
        p_pixels = p_block->p_buffer;
        for( i = 0; i < pic.i_planes; i++ )
        {
            int i_pitch = pic.p[i].i_pitch;
            uint8_t *p_top = p_pixels;
            uint8_t *p_bottom = p_pixels + i_pitch *
                (pic.p[i].i_visible_lines - 1);

            for( j = 0; j < pic.p[i].i_visible_lines / 2; j++ )
            {
                memcpy( p_tmp, p_bottom, pic.p[i].i_visible_pitch  );
                memcpy( p_bottom, p_top, pic.p[i].i_visible_pitch  );
                memcpy( p_top, p_tmp, pic.p[i].i_visible_pitch  );
                p_top += i_pitch;
                p_bottom -= i_pitch;
            }

            p_pixels += i_pitch * pic.p[i].i_lines;
        }
        free( p_tmp );
    }

    return p_block;
}

/*****************************************************************************
 * CloseDecoder: decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    free( p_dec->p_sys );
}
