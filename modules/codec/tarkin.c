/*****************************************************************************
 * tarkin.c: tarkin decoder module making use of libtarkin.
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: tarkin.c,v 1.4 2002/11/28 21:00:48 gbazin Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                    /* memcpy(), memset() */

#include <ogg/ogg.h>

/* FIXME */
// use 16 bit signed integers as wavelet coefficients
#define TYPE int16_t
// we'll actually use TYPE_BITS bits of type (e.g. 9 magnitude + 1 sign)
#define TYPE_BITS 10
// use the rle entropy coder
#define RLECODER 1

#include <tarkin.h>

/*****************************************************************************
 * dec_thread_t : tarkin decoder thread descriptor
 *****************************************************************************/
typedef struct dec_thread_t
{
    /*
     * Thread properties
     */
    vlc_thread_t        thread_id;                /* id for thread functions */

    /*
     * Tarkin properties
     */
    TarkinStream *tarkin_stream;

    TarkinInfo       ti;                        /* tarkin bitstream settings */
    TarkinComment    tc;                   /* tarkin bitstream user comments */
    TarkinTime           tarkdate;

    /*
     * Input properties
     */
    decoder_fifo_t         *p_fifo;            /* stores the PES stream data */
    pes_packet_t           *p_pes;            /* current PES we are decoding */

    /*
     * Output properties
     */
    vout_thread_t *p_vout;

} dec_thread_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder  ( vlc_object_t * );
static int  RunDecoder   ( decoder_fifo_t * );
static void CloseDecoder ( dec_thread_t * );

static void DecodePacket ( dec_thread_t * );
static int  GetOggPacket ( dec_thread_t *, ogg_packet *, mtime_t * );

static void tarkin_CopyPicture( dec_thread_t *, picture_t *, uint8_t *, int );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Tarkin decoder module") );
    set_capability( "decoder", 100 );
    set_callbacks( OpenDecoder, NULL );
    add_shortcut( "tarkin" );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_fifo_t *p_fifo = (decoder_fifo_t*) p_this;

    if( p_fifo->i_fourcc != VLC_FOURCC('t','a','r','k') )
    {
        return VLC_EGENERIC;
    }

    p_fifo->pf_run = RunDecoder;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * RunDecoder: the tarkin decoder
 *****************************************************************************/
static int RunDecoder( decoder_fifo_t *p_fifo )
{
    dec_thread_t *p_dec;
    ogg_packet oggpacket;
    mtime_t i_pts;

    /* Allocate the memory needed to store the thread's structure */
    if( (p_dec = (dec_thread_t *)malloc (sizeof(dec_thread_t)) )
            == NULL)
    {
        msg_Err( p_fifo, "out of memory" );
        goto error;
    }

    /* Initialize the thread properties */
    memset( p_dec, 0, sizeof(dec_thread_t) );
    p_dec->p_fifo = p_fifo;
    p_dec->p_pes  = NULL;
    p_dec->p_vout = NULL;

    /* Take care of the initial Tarkin header */
    p_dec->tarkin_stream = tarkin_stream_new();
    tarkin_info_init(&p_dec->ti);
    tarkin_comment_init(&p_dec->tc);

    if( GetOggPacket( p_dec, &oggpacket, &i_pts ) != VLC_SUCCESS )
        goto error;

    oggpacket.b_o_s = 1; /* yes this actually is a b_o_s packet :) */
    if( tarkin_synthesis_headerin( &p_dec->ti, &p_dec->tc, &oggpacket ) < 0 )
    {
        msg_Err( p_dec->p_fifo, "This bitstream does not contain Tarkin "
                 "video data");
        goto error;
    }

    /* The next two packets in order are the comment and codebook headers.
       We need to watch out that these packets are not missing as a
       missing or corrupted header is fatal. */
    if( GetOggPacket( p_dec, &oggpacket, &i_pts ) != VLC_SUCCESS )
        goto error;

    if( tarkin_synthesis_headerin( &p_dec->ti, &p_dec->tc, &oggpacket ) < 0 )
    {
        msg_Err( p_dec->p_fifo, "2nd Tarkin header is corrupted" );
        goto error;
    }

    if( GetOggPacket( p_dec, &oggpacket, &i_pts ) != VLC_SUCCESS )
        goto error;

    if( tarkin_synthesis_headerin( &p_dec->ti, &p_dec->tc, &oggpacket ) < 0 )
    {
        msg_Err( p_dec->p_fifo, "3rd Tarkin header is corrupted" );
        goto error;
    }

    /* Initialize the tarkin decoder */
    tarkin_synthesis_init( p_dec->tarkin_stream, &p_dec->ti );

    /* tarkin decoder thread's main loop */
    while( (!p_dec->p_fifo->b_die) && (!p_dec->p_fifo->b_error) )
    {
        DecodePacket( p_dec );
    }

    /* If b_error is set, the tarkin decoder thread enters the error loop */
    if( p_dec->p_fifo->b_error )
    {
        DecoderError( p_dec->p_fifo );
    }

    /* End of the tarkin decoder thread */
    CloseDecoder( p_dec );

    return 0;

 error:
    DecoderError( p_fifo );
    if( p_dec )
    {
        if( p_dec->p_fifo )
            p_dec->p_fifo->b_error = 1;

        /* End of the tarkin decoder thread */
        CloseDecoder( p_dec );
    }

    return -1;
}

/*****************************************************************************
 * DecodePacket: decodes a Tarkin packet.
 *****************************************************************************/
static void DecodePacket( dec_thread_t *p_dec )
{
    ogg_packet oggpacket;
    picture_t *p_pic;
    mtime_t i_pts;
    int i_width, i_height, i_chroma, i_stride, i_aspect;
    uint8_t *rgb;

    if( GetOggPacket( p_dec, &oggpacket, &i_pts ) != VLC_SUCCESS )
    {
        /* This should mean an eos */
        return;
    }

    tarkin_synthesis_packetin( p_dec->tarkin_stream, &oggpacket );

    while( tarkin_synthesis_frameout( p_dec->tarkin_stream,
                                      &rgb, 0, &p_dec->tarkdate ) == 0 )
    {

        i_width = p_dec->tarkin_stream->layer->desc.width;
        i_height = p_dec->tarkin_stream->layer->desc.height;
        switch( p_dec->tarkin_stream->layer->desc.format )
        {
        case TARKIN_RGB24:
            /*i_chroma = VLC_FOURCC('R','G','B','A');*/
            i_chroma = VLC_FOURCC('R','V','2','4');
            i_stride = i_width * 3;
            break;
        case TARKIN_RGB32:
            i_chroma = VLC_FOURCC('R','V','3','2');
            i_stride = i_width * 4;
            break;
        case TARKIN_RGBA:
            i_chroma = VLC_FOURCC('R','G','B','A');
            i_stride = i_width * 4;
            break;
        default:
            i_chroma = VLC_FOURCC('Y','V','1','2');
            i_stride = i_width;
            break;
        }
        i_aspect = VOUT_ASPECT_FACTOR * i_width / i_height;
        p_dec->p_vout = vout_Request( p_dec->p_fifo, p_dec->p_vout,
                                      i_width, i_height, i_chroma, i_aspect );

        /* Get a new picture */
        while( !(p_pic = vout_CreatePicture( p_dec->p_vout, 0, 0, 0 ) ) )
        {
            if( p_dec->p_fifo->b_die || p_dec->p_fifo->b_error )
            {
                return;
            }
            msleep( VOUT_OUTMEM_SLEEP );
        }
        if( !p_pic )
            break;

        tarkin_CopyPicture( p_dec, p_pic, rgb, i_stride );

        tarkin_synthesis_freeframe( p_dec->tarkin_stream, rgb );

        vout_DatePicture( p_dec->p_vout, p_pic, mdate()+DEFAULT_PTS_DELAY/*i_pts*/ );
        vout_DisplayPicture( p_dec->p_vout, p_pic );

    }
}

/*****************************************************************************
 * GetOggPacket: get the following tarkin packet from the stream and send back
 *               the result in an ogg packet (for easy decoding by libtarkin).
 *****************************************************************************
 * Returns VLC_EGENERIC in case of eof.
 *****************************************************************************/
static int GetOggPacket( dec_thread_t *p_dec, ogg_packet *p_oggpacket,
                         mtime_t *p_pts )
{
    if( p_dec->p_pes ) input_DeletePES( p_dec->p_fifo->p_packets_mgt,
                                        p_dec->p_pes );

    input_ExtractPES( p_dec->p_fifo, &p_dec->p_pes );
    if( !p_dec->p_pes ) return VLC_EGENERIC;

    p_oggpacket->packet = p_dec->p_pes->p_first->p_payload_start;
    p_oggpacket->bytes = p_dec->p_pes->i_pes_size;
    p_oggpacket->granulepos = p_dec->p_pes->i_dts;
    p_oggpacket->b_o_s = 0;
    p_oggpacket->e_o_s = 0;
    p_oggpacket->packetno = 0;

    *p_pts = p_dec->p_pes->i_pts;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseDecoder: tarkin decoder destruction
 *****************************************************************************/
static void CloseDecoder( dec_thread_t * p_dec )
{

    if( p_dec )
    {
        if( p_dec->p_pes )
            input_DeletePES( p_dec->p_fifo->p_packets_mgt, p_dec->p_pes );

        vout_Request( p_dec->p_fifo, p_dec->p_vout, 0, 0, 0, 0 );

        if( p_dec->tarkin_stream )
            tarkin_stream_destroy( p_dec->tarkin_stream );

        free( p_dec );
    }
}

/*****************************************************************************
 * tarkin_CopyPicture: copy a picture from tarkin internal buffers to a
 *                     picture_t structure.
 *****************************************************************************/
static void tarkin_CopyPicture( dec_thread_t *p_dec, picture_t *p_pic,
                                uint8_t *p_src, int i_pitch )
{
    int i_plane, i_line, i_src_stride, i_dst_stride;
    u8  *p_dst;

    for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
    {
        p_dst = p_pic->p[i_plane].p_pixels;
        i_dst_stride = p_pic->p[i_plane].i_pitch;
        i_src_stride = i_pitch;

        for( i_line = 0; i_line < p_pic->p[i_plane].i_lines; i_line++ )
        {
            p_dec->p_fifo->p_vlc->pf_memcpy( p_dst, p_src,
                                             i_src_stride );

            p_src += i_src_stride;
            p_dst += i_dst_stride;
        }
    }
}
