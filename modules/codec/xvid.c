/*****************************************************************************
 * xvid.c: a decoder for libxvidcore, the Xvid video codec
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: xvid.c,v 1.4 2003/01/07 21:49:01 fenrir Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

#include <xvid.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int RunDecoder  ( decoder_fifo_t * );
static int OpenDecoder ( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Xvid video decoder") );
    set_capability( "decoder", 50 );
    set_callbacks( OpenDecoder, NULL );
    add_bool( "xvid-direct-render", 0, NULL, "direct rendering",
              "Use libxvidcore's direct rendering feature." );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************
 * FIXME: find fourcc formats supported by xvid
 *****************************************************************************/
static int OpenDecoder ( vlc_object_t *p_this )
{
    decoder_fifo_t *p_fifo = (decoder_fifo_t*) p_this;

    if( p_fifo->i_fourcc != VLC_FOURCC('x','v','i','d')
         && p_fifo->i_fourcc != VLC_FOURCC('X','V','I','D')
         && p_fifo->i_fourcc != VLC_FOURCC('D','I','V','X') )
    {
        return VLC_EGENERIC;
    }

    p_fifo->pf_run = RunDecoder;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * RunDecoder: this function is called just after the thread is created
 *****************************************************************************/
static int RunDecoder ( decoder_fifo_t *p_fifo )
{
    XVID_INIT_PARAM    xinit;
    XVID_DEC_PARAM     xparam;
    BITMAPINFOHEADER * p_format;
    void *             p_xvid;

    pes_packet_t *     p_pes = NULL;
    bit_stream_t       bit_stream;
    vout_thread_t *    p_vout;

    uint8_t *  p_buffer, * p_image;
    int        i_ret;
    int        i_width, i_height, i_chroma, i_aspect;
    int        i_size, i_offset, i_image_size;

    vlc_bool_t b_direct = config_GetInt( p_fifo, "xvid-direct-render" );

    if( InitBitstream( &bit_stream, p_fifo, NULL, NULL ) != VLC_SUCCESS )
    {
        msg_Err( p_fifo, "cannot initialise bitstream" );
        p_fifo->b_error = VLC_TRUE;
        DecoderError( p_fifo );
        return VLC_EGENERIC;
    }
    if( ( p_format = (BITMAPINFOHEADER *)p_fifo->p_bitmapinfoheader ) == NULL )
    {
        i_width  = 1;
        i_height = 1;   // avoid segfault anyway it's wrong
    }
    else
    {
        /* Guess picture properties from the BIH */
        i_width = p_format->biWidth;
        i_height = p_format->biHeight;
    }

    i_chroma = VLC_FOURCC('Y','V','1','2');
    i_aspect = VOUT_ASPECT_FACTOR * i_width / i_height;

    /* XXX: Completely arbitrary buffer size */
    i_size = i_width * i_height / 4;
    i_image_size = b_direct ? 0 : i_width * i_height * 4;
    i_offset = 0;

    p_buffer = malloc( i_size + i_image_size );
    p_image = p_buffer + i_size;

    if( !p_buffer )
    {
        msg_Err( p_fifo, "out of memory" );
        p_fifo->b_error = VLC_TRUE;
        CloseBitstream( &bit_stream );
        DecoderError( p_fifo );
        return VLC_EGENERIC;
    }

    xinit.cpu_flags = 0;
    xvid_init( NULL, 0, &xinit, NULL );

    xparam.width = i_width;
    xparam.height = i_height;
    i_ret = xvid_decore( NULL, XVID_DEC_CREATE, &xparam, NULL );

    if( i_ret )
    {
        msg_Err( p_fifo, "cannot create xvid decoder" );
        p_fifo->b_error = VLC_TRUE;
        free( p_buffer );
        CloseBitstream( &bit_stream );
        DecoderError( p_fifo );
        return VLC_EGENERIC;
    }

    p_xvid = xparam.handle;

    /* Spawn a video output if there is none. First we look amongst our
     * children, then we look for any other vout that might be available */
    p_vout = vout_Request( p_fifo, NULL,
                           i_width, i_height, i_chroma, i_aspect );

    if( !p_vout )
    {
        msg_Err( p_fifo, "could not spawn vout" );
        p_fifo->b_error = VLC_TRUE;
        xvid_decore( p_xvid, XVID_DEC_DESTROY, NULL, NULL );
        free( p_buffer );
        CloseBitstream( &bit_stream );
        DecoderError( p_fifo );
        return VLC_EGENERIC;
    }

    /* Main loop */
    while( !p_fifo->b_die && !p_fifo->b_error )
    {
        XVID_DEC_FRAME xframe;
        XVID_DEC_PICTURE xpic;
        mtime_t i_pts = 0;
        picture_t *p_pic;

        GetChunk( &bit_stream, p_buffer + i_offset, i_size - i_offset );

        if( p_pes )
        {
            input_DeletePES( p_fifo->p_packets_mgt, p_pes );
        }

        input_ExtractPES( p_fifo, &p_pes );
        if( p_pes )
        {
            /* Don't trust the sucker */
            //i_pts = p_pes->i_pts + DEFAULT_PTS_DELAY;
            i_pts = mdate() + DEFAULT_PTS_DELAY;
        }

        if( p_fifo->b_die || p_fifo->b_error )
        {
            break;
        }

        while( !(p_pic = vout_CreatePicture( p_vout, 0, 0, 0 ) ) )
        {
            if( p_fifo->b_die || p_fifo->b_error )
            {
                break;
            } 
            msleep( VOUT_OUTMEM_SLEEP );
        }

        if( !p_pic )
        {
            break;
        }

        if( b_direct )
        {
            xpic.y = p_pic->p[0].p_pixels;
            xpic.u = p_pic->p[1].p_pixels;
            xpic.v = p_pic->p[2].p_pixels;
            xpic.stride_y = p_pic->p[0].i_pitch;
            xpic.stride_u = p_pic->p[1].i_pitch;
            xpic.stride_v = p_pic->p[2].i_pitch;
        }

        /* Decode the stuff */
        xframe.bitstream = p_buffer;
        xframe.length = i_size;
        xframe.image = b_direct ? (void*)&xpic : p_image;
        xframe.stride = i_width;
        xframe.colorspace = b_direct ? XVID_CSP_EXTERN : XVID_CSP_YV12;
        i_ret = xvid_decore( p_xvid, XVID_DEC_DECODE, &xframe, NULL );
        /* FIXME: check i_ret */

        if( !b_direct )
        {
            /* TODO: use pf_memcpy when this is stable. */
            memcpy( p_pic->p[0].p_pixels,
                    p_image,
                    i_width * i_height );
            memcpy( p_pic->p[2].p_pixels,
                    p_image + i_width * i_height,
                    i_width * i_height / 4 );
            memcpy( p_pic->p[1].p_pixels,
                    p_image + i_width * i_height + i_width * i_height / 4,
                    i_width * i_height / 4 );
        }

        vout_DatePicture( p_vout, p_pic, i_pts );
        vout_DisplayPicture( p_vout, p_pic );

        /* Move the remaining data. TODO: only do this when necessary. */
        memmove( p_buffer, p_buffer + xframe.length, i_size - xframe.length );
        i_offset = i_size - xframe.length;
    }

    /* Clean up everything */
    vout_Request( p_fifo, p_vout, 0, 0, 0, 0 );

    xvid_decore( p_xvid, XVID_DEC_DESTROY, NULL, NULL );

    if( p_pes )
    {
        input_DeletePES( p_fifo->p_packets_mgt, p_pes );
    }

    free( p_buffer );
    CloseBitstream( &bit_stream );

    if( p_fifo->b_error )
    {
        DecoderError( p_fifo );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

