/*****************************************************************************
 * video.c : video encoder using ffmpeg library
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: video.c,v 1.3 2003/04/26 14:54:49 gbazin Exp $
 *
 * Authors: Laurent Aimar
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
#include <vlc/vout.h>
#include <vlc/input.h>
#include <vlc/decoder.h>

#include <stdlib.h>

#include "codecs.h"
#include "encoder.h"

/* ffmpeg header */
#ifdef HAVE_FFMPEG_AVCODEC_H
#   include <ffmpeg/avcodec.h>
#else
#   include <avcodec.h>
#endif

int  E_( OpenEncoderVideo ) ( vlc_object_t * );
void E_( CloseEncoderVideo )( vlc_object_t * );

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Init     ( video_encoder_t *p_encoder );
static int  Encode   ( video_encoder_t *p_encoder,
                               picture_t *p_pic, void *p_data, size_t *pi_data );
static void End      ( video_encoder_t *p_encoder );

/*****************************************************************************
 * Local definitions
 *****************************************************************************/
struct encoder_sys_t
{
    char *psz_codec;

    AVCodecContext  *p_context;
    AVCodec         *p_codec;

    AVFrame         *p_frame;
};

/*****************************************************************************
 * OpenEncoderVideo:
 *****************************************************************************
 *
 *****************************************************************************/
int  E_( OpenEncoderVideo ) ( vlc_object_t *p_this )
{
    video_encoder_t *p_encoder = (video_encoder_t*)p_this;


    /* *** check supported codec *** */
    switch( p_encoder->i_codec )
    {
        case VLC_FOURCC( 'm', 'p', '1', 'v' ):
        case VLC_FOURCC( 'm', 'p', 'g', 'v' ):
        case VLC_FOURCC( 'm', 'p', '4', 'v' ):
            break;
        default:
            return VLC_EGENERIC;
    }

    /* *** init library */
    avcodec_init();
    avcodec_register_all();

    /* *** fix parameters *** */
    /* FIXME be clever, some codec support additional chroma */
    if( p_encoder->i_chroma != VLC_FOURCC( 'I', '4', '2', '0' ) )
    switch( p_encoder->i_chroma )
    {
        case VLC_FOURCC( 'I', '4', '2', '0' ):
        case VLC_FOURCC( 'I', '4', '2', '2' ):
        case VLC_FOURCC( 'Y', 'U', 'Y', '2' ):
            break;
        default:
            p_encoder->i_chroma = VLC_FOURCC( 'I', '4', '2', '0' );
            return VLC_EGENERIC;
    }
#if 0
    p_encoder->i_width = ( p_encoder->i_width + 15 )&0xfffff8;
    p_encoder->i_height = ( p_encoder->i_height + 15 )&0xfffff8;
#endif

    /* *** set exported functions *** */
    p_encoder->pf_init = Init;
    p_encoder->pf_encode = Encode;
    p_encoder->pf_end = End;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseEncoderVideo:
 *****************************************************************************
 *
 *****************************************************************************/
void E_( CloseEncoderVideo )( vlc_object_t *p_this )
{
    ;
}

/*****************************************************************************
 * Init:
 *****************************************************************************
 *
 *****************************************************************************/
static int  Init     ( video_encoder_t *p_encoder )
{
    encoder_sys_t *p_sys;
    int           i_codec;

    /* *** allocate memory *** */
    if( !( p_encoder->p_sys = p_sys = malloc( sizeof( encoder_sys_t ) ) ) )
    {
        msg_Err( p_encoder, "out of memory" );
        return VLC_EGENERIC;
    }
    memset( p_sys, 0, sizeof( encoder_sys_t ) );

    /* *** ask for the codec *** */
    switch( p_encoder->i_codec )
    {
        case VLC_FOURCC( 'm', 'p', '1', 'v' ):
        case VLC_FOURCC( 'm', 'p', 'g', 'v' ):
            p_encoder->p_sys->psz_codec = "MPEG I";
            i_codec = CODEC_ID_MPEG1VIDEO;
            break;
        case VLC_FOURCC( 'm', 'p', '4', 'v' ):
            p_encoder->p_sys->psz_codec = "MPEG-4";
            i_codec = CODEC_ID_MPEG4;
            break;
        default:
            return VLC_EGENERIC;
    }
    if( ( p_sys->p_codec = avcodec_find_encoder( i_codec ) ) == NULL )
    {
        msg_Err( p_encoder, "cannot find encoder for %s", p_encoder->p_sys->psz_codec );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_encoder, "encoding with %s", p_encoder->p_sys->psz_codec );

#define p_frame   p_sys->p_frame
#define p_context p_sys->p_context
    /* *** set context properties  *** */
    p_context = avcodec_alloc_context();
    p_context->bit_rate = config_GetInt( p_encoder, "encoder-ffmpeg-video-bitrate" ) * 1000;
    p_context->width = p_encoder->i_width;
    p_context->height= p_encoder->i_height;
#if LIBAVCODEC_BUILD >= 4662
    p_context->frame_rate = 25 * DEFAULT_FRAME_RATE_BASE;
#else
    p_context->frame_rate = 25 * FRAME_RATE_BASE;
#endif
    p_context->gop_size = config_GetInt( p_encoder, "encoder-ffmpeg-video-max-key-interval" );
    p_context->qmin = __MAX( __MIN( config_GetInt( p_encoder, "encoder-ffmpeg-video-min-quant" ), 31 ), 1 );
    p_context->qmax = __MAX( __MIN( config_GetInt( p_encoder, "encoder-ffmpeg-video-max-quant" ), 31 ), 1 );

    if( avcodec_open( p_context, p_encoder->p_sys->p_codec ) < 0 )
    {
        msg_Err( p_encoder, "failed to open %s codec", p_encoder->p_sys->psz_codec );
        return VLC_EGENERIC;
    }

    p_frame = avcodec_alloc_frame();

    switch( p_encoder->i_chroma )
    {
        case VLC_FOURCC( 'I', '4', '2', '0' ):
            p_frame->pict_type = PIX_FMT_YUV420P;
            break;
        case VLC_FOURCC( 'I', '4', '2', '2' ):
            p_frame->pict_type = PIX_FMT_YUV422P;
            break;
        case VLC_FOURCC( 'Y', 'U', 'Y', '2' ):
            p_frame->pict_type = PIX_FMT_YUV422;
            break;
        default:
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;

#undef  p_context
#undef  p_frame
}

/*****************************************************************************
 * Encode:
 *****************************************************************************
 *
 *****************************************************************************/
static int  Encode   ( video_encoder_t *p_encoder,
                       picture_t *p_pic, void *p_data, size_t *pi_data )
{
#define p_frame   p_encoder->p_sys->p_frame
#define p_context p_encoder->p_sys->p_context
    int i;

    for( i = 0; i < 3; i++ )
    {
        p_frame->linesize[i] = p_pic->p[i].i_pitch;
        p_frame->data[i]     = p_pic->p[i].p_pixels;
    }
    *pi_data = avcodec_encode_video( p_context, p_data, *pi_data, p_frame );

    return VLC_SUCCESS;
#undef  p_context
#undef  p_frame
}

/*****************************************************************************
 * End:
 *****************************************************************************
 *
 *****************************************************************************/
static void End      ( video_encoder_t *p_encoder )
{
    avcodec_close( p_encoder->p_sys->p_context );
    free( p_encoder->p_sys->p_context );

    p_encoder->p_sys->p_context = NULL;
    p_encoder->p_sys->p_codec = NULL;
}

