/*****************************************************************************
 * rawvideo.c: Pseudo video decoder/packetizer for raw video data
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: rawvideo.c,v 1.7 2003/10/24 21:27:06 gbazin Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/decoder.h>
#include <vlc/input.h>
#include <vlc/sout.h>

#include "codecs.h"

/*****************************************************************************
 * decoder_sys_t : raw video decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /* Module mode */
    vlc_bool_t b_packetizer;

    /*
     * Input properties
     */
    int i_raw_size;

    /*
     * Output properties
     */
    vout_thread_t *p_vout;

    /*
     * Packetizer output properties
     */
    sout_packetizer_input_t *p_sout_input;
    sout_format_t           sout_format;

    /*
     * Common properties
     */
    mtime_t i_pts;

};

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int OpenDecoder   ( vlc_object_t * );
static int OpenPacketizer( vlc_object_t * );

static int InitDecoder   ( decoder_t * );
static int RunDecoder    ( decoder_t *, block_t * );
static int EndDecoder    ( decoder_t * );

static int DecodeFrame   ( decoder_t *, block_t * );
static int SendFrame     ( decoder_t *, block_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Pseudo Raw Video decoder") );
    set_capability( "decoder", 50 );
    set_callbacks( OpenDecoder, NULL );

    add_submodule();
    set_description( _("Pseudo Raw Video packetizer") );
    set_capability( "packetizer", 100 );
    set_callbacks( OpenPacketizer, NULL );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to choose.
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    switch( p_dec->p_fifo->i_fourcc )
    {
        /* Planar YUV */
        case VLC_FOURCC('I','4','4','4'):
        case VLC_FOURCC('I','4','2','2'):
        case VLC_FOURCC('I','4','2','0'):
        case VLC_FOURCC('Y','V','1','2'):
        case VLC_FOURCC('I','Y','U','V'):
        case VLC_FOURCC('I','4','1','1'):
        case VLC_FOURCC('I','4','1','0'):

        /* Packed YUV */
        case VLC_FOURCC('Y','U','Y','2'):
        case VLC_FOURCC('U','Y','V','Y'):

        /* RGB */
        case VLC_FOURCC('R','V','3','2'):
        case VLC_FOURCC('R','V','2','4'):
        case VLC_FOURCC('R','V','1','6'):
        case VLC_FOURCC('R','V','1','5'):
            break;

        default:
            return VLC_EGENERIC;
    }

    p_dec->pf_init = InitDecoder;
    p_dec->pf_decode = RunDecoder;
    p_dec->pf_end = EndDecoder;

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
    {
        msg_Err( p_dec, "out of memory" );
        return VLC_EGENERIC;
    }
    p_dec->p_sys->b_packetizer = VLC_FALSE;

    return VLC_SUCCESS;
}

static int OpenPacketizer( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    int i_ret = OpenDecoder( p_this );

    if( i_ret == VLC_SUCCESS ) p_dec->p_sys->b_packetizer = VLC_TRUE;

    return i_ret;
}

/*****************************************************************************
 * InitDecoder: Initalize the decoder
 *****************************************************************************/
static int InitDecoder( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    video_frame_format_t format;

    p_sys->i_pts = 0;

    p_sys->p_sout_input = NULL;
    p_sys->sout_format.i_cat = VIDEO_ES;
    p_sys->sout_format.i_block_align = 0;
    p_sys->sout_format.i_bitrate     = 0;
    p_sys->sout_format.i_extra_data  = 0;
    p_sys->sout_format.p_extra_data  = NULL;

#define bih ((BITMAPINFOHEADER*)p_dec->p_fifo->p_bitmapinfoheader)
    if( bih == NULL )
    {
        msg_Err( p_dec, "info missing, fatal" );
        return VLC_EGENERIC;
    }
    if( bih->biWidth <= 0 || bih->biHeight <= 0 )
    {
        msg_Err( p_dec, "invalid display size %dx%d",
                 bih->biWidth, bih->biHeight );
        return VLC_EGENERIC;
    }

    if( p_sys->b_packetizer )
    {
        /* add an input for the stream ouput */
        p_sys->sout_format.i_width  = bih->biWidth;
        p_sys->sout_format.i_height = bih->biHeight;
        p_sys->sout_format.i_fourcc = p_dec->p_fifo->i_fourcc;

        p_sys->p_sout_input =
            sout_InputNew( p_dec, &p_sys->sout_format );

        if( !p_sys->p_sout_input )
        {
            msg_Err( p_dec, "cannot add a new stream" );
            return VLC_EGENERIC;
        }
    }
    else
    {
        /* Initialize video output */
        p_sys->p_vout = vout_Request( p_dec, NULL,
                                      bih->biWidth, bih->biHeight,
                                      p_dec->p_fifo->i_fourcc,
                                      VOUT_ASPECT_FACTOR * bih->biWidth /
                                      bih->biHeight );
        if( p_sys->p_vout == NULL )
        {
            msg_Err( p_dec, "failed to create vout" );
            return VLC_EGENERIC;
        }
    }

    /* Find out p_vdec->i_raw_size */
    vout_InitFormat( &format, p_dec->p_fifo->i_fourcc,
                     bih->biWidth, bih->biHeight,
                     bih->biWidth * VOUT_ASPECT_FACTOR / bih->biHeight );
    p_sys->i_raw_size = format.i_bits_per_pixel *
        format.i_width * format.i_height / 8;
#undef bih

    return VLC_SUCCESS;
}

/****************************************************************************
 * RunDecoder: the whole thing
 ****************************************************************************
 * This function must be fed with ogg packets.
 ****************************************************************************/
static int RunDecoder( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int i_ret;

#if 0
    if( !aout_DateGet( &p_sys->end_date ) && !p_block->i_pts )
    {
        /* We've just started the stream, wait for the first PTS. */
        block_Release( p_block );
        return VLC_SUCCESS;
    }
#endif

    /* Date management */
    if( p_block->i_pts > 0 && p_block->i_pts != p_sys->i_pts )
    {
        p_sys->i_pts = p_block->i_pts;
    }

    if( p_block->i_buffer < p_sys->i_raw_size )
    {
        msg_Warn( p_dec, "invalid frame size (%d < %d)",
                  p_block->i_buffer, p_sys->i_raw_size );

        block_Release( p_block );
        return VLC_EGENERIC;
    }

    if( p_sys->b_packetizer )
    {
        i_ret = SendFrame( p_dec, p_block );
    }
    else
    {
        i_ret = DecodeFrame( p_dec, p_block );
    }

    /* Date management: 1 frame per packet */
    p_sys->i_pts += ( I64C(1000000) * 1.0 / 25 /*FIXME*/ );

    block_Release( p_block );
    return i_ret;
}

/*****************************************************************************
 * FillPicture:
 *****************************************************************************/
static void FillPicture( decoder_t *p_dec, block_t *p_block, picture_t *p_pic )
{
    uint8_t *p_src, *p_dst;
    int i_src, i_plane, i_line, i_width;

    p_src  = p_block->p_buffer;
    i_src = p_block->i_buffer;

    for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
    {
        p_dst = p_pic->p[i_plane].p_pixels;
        i_width = p_pic->p[i_plane].i_visible_pitch;

        for( i_line = 0; i_line < p_pic->p[i_plane].i_lines; i_line++ )
        {
            p_dec->p_vlc->pf_memcpy( p_dst, p_src, i_width );
            p_src += i_width;
            p_dst += p_pic->p[i_plane].i_pitch;
        }
    }
}

/*****************************************************************************
 * DecodeFrame: decodes a video frame.
 *****************************************************************************/
static int DecodeFrame( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    picture_t *p_pic;

    /* Get a new picture */
    while( !(p_pic = vout_CreatePicture( p_sys->p_vout, 0, 0, 0 ) ) )
    {
        if( p_dec->p_fifo->b_die || p_dec->p_fifo->b_error )
        {
            return VLC_EGENERIC;
        }
        msleep( VOUT_OUTMEM_SLEEP );
    }
    if( !p_pic ) return VLC_EGENERIC;

    FillPicture( p_dec, p_block, p_pic );

    vout_DatePicture( p_sys->p_vout, p_pic, p_sys->i_pts );
    vout_DisplayPicture( p_sys->p_vout, p_pic );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SendFrame: send a video frame to the stream output.
 *****************************************************************************/
static int SendFrame( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    sout_buffer_t *p_sout_buffer =
        sout_BufferNew( p_sys->p_sout_input->p_sout, p_block->i_buffer );

    if( !p_sout_buffer ) return VLC_EGENERIC;

    p_dec->p_vlc->pf_memcpy( p_sout_buffer->p_buffer,
                             p_block->p_buffer, p_block->i_buffer );

    p_sout_buffer->i_dts = p_sout_buffer->i_pts = p_sys->i_pts;

    sout_InputSendBuffer( p_sys->p_sout_input, p_sout_buffer );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * EndDecoder: decoder destruction
 *****************************************************************************/
static int EndDecoder( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( !p_sys->b_packetizer )
    {
        vout_Request( p_dec, p_sys->p_vout, 0, 0, 0, 0 );
    }

    if( p_sys->p_sout_input != NULL )
    {
        sout_InputDelete( p_sys->p_sout_input );
    }

    free( p_sys );

    return VLC_SUCCESS;
}
