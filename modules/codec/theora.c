/*****************************************************************************
 * theora.c: theora decoder module making use of libtheora.
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: theora.c,v 1.4 2003/03/30 18:14:36 gbazin Exp $
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

#include <theora/theora.h>

/*****************************************************************************
 * dec_thread_t : theora decoder thread descriptor
 *****************************************************************************/
typedef struct dec_thread_t
{
    /*
     * Thread properties
     */
    vlc_thread_t        thread_id;                /* id for thread functions */

    /*
     * Theora properties
     */
    theora_info      ti;                        /* theora bitstream settings */
    theora_state     td;                   /* theora bitstream user comments */

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

static void theora_CopyPicture( dec_thread_t *, picture_t *, yuv_buffer * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Theora video decoder") );
    set_capability( "decoder", 100 );
    set_callbacks( OpenDecoder, NULL );
    add_shortcut( "theora" );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_fifo_t *p_fifo = (decoder_fifo_t*) p_this;

    if( p_fifo->i_fourcc != VLC_FOURCC('t','h','e','o') )
    {
        return VLC_EGENERIC;
    }

    p_fifo->pf_run = RunDecoder;
    return VLC_SUCCESS;
}
/*****************************************************************************
 * RunDecoder: the theora decoder
 *****************************************************************************/
static int RunDecoder( decoder_fifo_t *p_fifo )
{
    dec_thread_t *p_dec;
    ogg_packet oggpacket;
    int i_chroma, i_aspect;
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

    /* Take care of the initial Theora header */
    if( GetOggPacket( p_dec, &oggpacket, &i_pts ) != VLC_SUCCESS )
        goto error;

    oggpacket.b_o_s = 1; /* yes this actually is a b_o_s packet :) */
    if( theora_decode_header( &p_dec->ti, &oggpacket ) < 0 )
    {
        msg_Err( p_dec->p_fifo, "This bitstream does not contain Theora "
                 "video data");
        goto error;
    }

    /* Initialize decoder */
    theora_decode_init( &p_dec->td, &p_dec->ti );
    msg_Dbg( p_dec->p_fifo, "%dx%d %.02f fps video",
             p_dec->ti.width, p_dec->ti.height,
             (double)p_dec->ti.fps_numerator/p_dec->ti.fps_denominator);

    /* Initialize video output */
    if( p_dec->ti.aspect_denominator )
        i_aspect = VOUT_ASPECT_FACTOR * p_dec->ti.aspect_numerator /
                    p_dec->ti.aspect_denominator;
    else
        i_aspect = VOUT_ASPECT_FACTOR * p_dec->ti.width / p_dec->ti.height;

    i_chroma = VLC_FOURCC('Y','V','1','2');

    p_dec->p_vout = vout_Request( p_dec->p_fifo, p_dec->p_vout,
                                  p_dec->ti.width, p_dec->ti.height,
                                  i_chroma, i_aspect );

    /* theora decoder thread's main loop */
    while( (!p_dec->p_fifo->b_die) && (!p_dec->p_fifo->b_error) )
    {
        DecodePacket( p_dec );
    }

    /* If b_error is set, the theora decoder thread enters the error loop */
    if( p_dec->p_fifo->b_error )
    {
        DecoderError( p_dec->p_fifo );
    }

    /* End of the theora decoder thread */
    CloseDecoder( p_dec );

    return 0;

 error:
    DecoderError( p_fifo );
    if( p_dec )
    {
        if( p_dec->p_fifo )
            p_dec->p_fifo->b_error = 1;

        /* End of the theora decoder thread */
        CloseDecoder( p_dec );
    }

    return -1;
}

/*****************************************************************************
 * DecodePacket: decodes a Theora packet.
 *****************************************************************************/
static void DecodePacket( dec_thread_t *p_dec )
{
    ogg_packet oggpacket;
    picture_t *p_pic;
    mtime_t i_pts;
    yuv_buffer yuv;

    if( GetOggPacket( p_dec, &oggpacket, &i_pts ) != VLC_SUCCESS )
    {
        /* This should mean an eos */
        return;
    }

    theora_decode_packetin( &p_dec->td, &oggpacket );

    /* Decode */
    theora_decode_YUVout( &p_dec->td, &yuv );

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
        return;

    theora_CopyPicture( p_dec, p_pic, &yuv );

    vout_DatePicture( p_dec->p_vout, p_pic, i_pts );
    vout_DisplayPicture( p_dec->p_vout, p_pic );
}

/*****************************************************************************
 * GetOggPacket: get the following theora packet from the stream and send back
 *               the result in an ogg packet (for easy decoding by libtheora).
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
 * CloseDecoder: theora decoder destruction
 *****************************************************************************/
static void CloseDecoder( dec_thread_t * p_dec )
{

    if( p_dec )
    {
        if( p_dec->p_pes )
            input_DeletePES( p_dec->p_fifo->p_packets_mgt, p_dec->p_pes );

        vout_Request( p_dec->p_fifo, p_dec->p_vout, 0, 0, 0, 0 );

        free( p_dec );
    }
}

/*****************************************************************************
 * theora_CopyPicture: copy a picture from theora internal buffers to a
 *                     picture_t structure.
 *****************************************************************************/
static void theora_CopyPicture( dec_thread_t *p_dec, picture_t *p_pic,
                                yuv_buffer *yuv )
{
    int i_plane, i_line, i_width, i_dst_stride, i_src_stride;
    u8  *p_dst, *p_src;

    for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
    {
        p_dst = p_pic->p[i_plane].p_pixels;
        p_src = i_plane ? (i_plane - 1 ? yuv->v : yuv->u ) : yuv->y;
        i_width = p_pic->p[i_plane].i_visible_pitch;
        i_dst_stride = p_pic->p[i_plane].i_pitch;
        i_src_stride = i_plane ? yuv->uv_stride : yuv->y_stride;

        for( i_line = 0; i_line < p_pic->p[i_plane].i_lines; i_line++ )
        {
            p_dec->p_fifo->p_vlc->pf_memcpy( p_dst, p_src, i_width );
            p_src += i_src_stride;
            p_dst += i_dst_stride;
        }
    }
}
