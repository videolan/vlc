/*****************************************************************************
 * vorbis.c Vorbis audio packetizer
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: vorbis.c,v 1.1 2003/06/23 23:51:31 gbazin Exp $
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
#include <vlc/aout.h>
#include <vlc/decoder.h>
#include <vlc/input.h>
#include <vlc/sout.h>

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */
#include "codecs.h"                         /* WAVEFORMATEX BITMAPINFOHEADER */

#include <ogg/ogg.h>
#include <vorbis/codec.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
typedef struct packetizer_s
{
    /*
     * Input properties
     */
    decoder_fifo_t         *p_fifo;            /* stores the PES stream data */
    pes_packet_t           *p_pes;            /* current PES we are decoding */

    /* Output properties */
    sout_packetizer_input_t *p_sout_input;
    sout_format_t           output_format;

    /*
     * Vorbis properties
     */
    vorbis_info      vi; /* struct that stores all the static vorbis bitstream
                            settings */
    vorbis_comment   vc; /* struct that stores all the bitstream user
                          * comments */
    vorbis_dsp_state vd; /* central working state for the packet->PCM
                          * decoder */
    vorbis_block     vb; /* local working space for packet->PCM decode */

    uint64_t                i_samplescount;

    mtime_t                 i_interpolated_pts;
    int                     i_last_block_size;

} packetizer_t;

static int  Open    ( vlc_object_t * );
static int  Run     ( decoder_fifo_t * );

static int  InitThread     ( packetizer_t * );
static void PacketizeThread   ( packetizer_t * );
static void EndThread      ( packetizer_t * );

static int GetOggPacket( packetizer_t *, ogg_packet *, mtime_t * );
static int SendOggPacket( packetizer_t *, ogg_packet *, mtime_t, int );

#define FREE( p ) if( p ) free( p ); p = NULL

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Vorbis audio packetizer") );
    set_capability( "packetizer", 50 );
    set_callbacks( Open, NULL );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the packetizer and return score
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    decoder_fifo_t *p_fifo = (decoder_fifo_t*) p_this;

    if( p_fifo->i_fourcc != VLC_FOURCC( 'v', 'o', 'r', 'b') )
    {
        return VLC_EGENERIC;
    }

    p_fifo->pf_run = Run;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * RunDecoder: this function is called just after the thread is created
 *****************************************************************************/
static int Run( decoder_fifo_t *p_fifo )
{
    packetizer_t *p_pack;
    int b_error;

    msg_Info( p_fifo, "Running vorbis packetizer" );
    if( !( p_pack = malloc( sizeof( packetizer_t ) ) ) )
    {
        msg_Err( p_fifo, "out of memory" );
        DecoderError( p_fifo );
        return -1;
    }
    memset( p_pack, 0, sizeof( packetizer_t ) );

    p_pack->p_fifo = p_fifo;

    if( InitThread( p_pack ) != 0 )
    {
        DecoderError( p_fifo );
        return -1;
    }

    while( ( !p_pack->p_fifo->b_die )&&( !p_pack->p_fifo->b_error ) )
    {
        PacketizeThread( p_pack );
    }


    if( ( b_error = p_pack->p_fifo->b_error ) )
    {
        DecoderError( p_pack->p_fifo );
    }

    EndThread( p_pack );

    FREE( p_pack );

    if( b_error )
    {
        return -1;
    }

    return 0;
}

/*****************************************************************************
 * InitThread: initialize data before entering main loop
 *****************************************************************************/
static int InitThread( packetizer_t *p_pack )
{ 
    mtime_t    i_pts;
    ogg_packet oggpacket;

    p_pack->output_format.i_cat = AUDIO_ES;
    p_pack->output_format.i_fourcc = p_pack->p_fifo->i_fourcc;
    p_pack->output_format.i_sample_rate = 0;
    p_pack->output_format.i_channels    = 0;
    p_pack->output_format.i_block_align = 0;
    p_pack->output_format.i_bitrate     = 0;
    p_pack->output_format.i_extra_data  = 0;
    p_pack->output_format.p_extra_data  = NULL;


    p_pack->p_sout_input = NULL;

    p_pack->i_samplescount = 0;
    p_pack->i_interpolated_pts = 0;

    p_pack->p_pes  = NULL;

    /* Take care of vorbis init */
    vorbis_info_init( &p_pack->vi );
    vorbis_comment_init( &p_pack->vc );

    /* Take care of the initial Vorbis headers */
    if( GetOggPacket( p_pack, &oggpacket, &i_pts ) != VLC_SUCCESS )
        goto error;

    oggpacket.b_o_s = 1; /* yes this actually is a b_o_s packet :) */
    if( vorbis_synthesis_headerin( &p_pack->vi, &p_pack->vc, &oggpacket ) < 0 )
    {
        msg_Err( p_pack->p_fifo, "This bitstream does not contain Vorbis "
                 "audio data");
        goto error;
    }

    /* add a input for the stream ouput */
    p_pack->output_format.i_sample_rate = p_pack->vi.rate;
    p_pack->output_format.i_channels    = p_pack->vi.channels;
    p_pack->output_format.i_block_align = 1;
    p_pack->output_format.i_bitrate     = p_pack->vi.bitrate_nominal;

    p_pack->p_sout_input =
        sout_InputNew( p_pack->p_fifo, &p_pack->output_format );

    if( !p_pack->p_sout_input )
    {
        msg_Err( p_pack->p_fifo, "cannot add a new stream" );
        p_pack->p_fifo->b_error = 1;
        goto error;
    }
    msg_Dbg( p_pack->p_fifo, "channels:%d samplerate:%d bitrate:%d",
             p_pack->vi.channels, p_pack->vi.rate, p_pack->vi.bitrate_nominal);

    if( SendOggPacket( p_pack, &oggpacket, 0, 0 ) != VLC_SUCCESS )
        goto error;

    /* The next two packets in order are the comment and codebook headers.
       We need to watch out that these packets are not missing as a
       missing or corrupted header is fatal. */
    if( GetOggPacket( p_pack, &oggpacket, &i_pts ) != VLC_SUCCESS )
        goto error;

    if( vorbis_synthesis_headerin( &p_pack->vi, &p_pack->vc, &oggpacket ) < 0 )
    {
        msg_Err( p_pack->p_fifo, "2nd Vorbis header is corrupted" );
        goto error;
    }
    
    if( SendOggPacket( p_pack, &oggpacket, 0, 0 ) != VLC_SUCCESS )
        goto error;

    if( GetOggPacket( p_pack, &oggpacket, &i_pts ) != VLC_SUCCESS )
        goto error;

    if( vorbis_synthesis_headerin( &p_pack->vi, &p_pack->vc, &oggpacket ) < 0 )
    {
        msg_Err( p_pack->p_fifo, "3rd Vorbis header is corrupted" );
        goto error;
    }

    if( SendOggPacket( p_pack, &oggpacket, 0, 0 ) != VLC_SUCCESS )
        goto error;

    /* Initialize the Vorbis packet->PCM decoder */
    vorbis_synthesis_init( &p_pack->vd, &p_pack->vi );
    vorbis_block_init( &p_pack->vd, &p_pack->vb );

    return 0;

 error:
    EndThread( p_pack );
    return -1;
}

/*****************************************************************************
 * PacketizeThread: packetize an unit (here copy a complete ogg packet)
 *****************************************************************************/
static void PacketizeThread( packetizer_t *p_pack )
{
    mtime_t    i_pts;
    int        i_samples, i_block_size;
    ogg_packet oggpacket;

    /* Timestamp all the packets */
    if( GetOggPacket( p_pack, &oggpacket, &i_pts ) != VLC_SUCCESS )
        goto error;

    if( i_pts <= 0 && p_pack->i_interpolated_pts <= 0 )
    {
        msg_Dbg( p_pack->p_fifo, "need a starting pts" );
        return;
    }

    if( i_pts > 0 ) p_pack->i_interpolated_pts = i_pts;

    i_block_size = vorbis_packet_blocksize( &p_pack->vi, &oggpacket );
    if( i_block_size < 0 ) i_block_size = 0; /* non audio packet */
    i_samples = ( p_pack->i_last_block_size + i_block_size ) >> 2;
    p_pack->i_last_block_size = i_block_size;

    if( SendOggPacket( p_pack, &oggpacket, p_pack->i_interpolated_pts,
                       i_samples ) != VLC_SUCCESS )
        goto error;

    p_pack->i_interpolated_pts += 1000000 * (uint64_t)i_samples
        / p_pack->vi.rate;

    return;

 error:
    p_pack->p_fifo->b_error = 1;
}

/*****************************************************************************
 * EndThread : packetizer thread destruction
 *****************************************************************************/
static void EndThread ( packetizer_t *p_pack)
{
    if( p_pack->p_pes )
        input_DeletePES( p_pack->p_fifo->p_packets_mgt, p_pack->p_pes );

    vorbis_block_clear( &p_pack->vb );
    vorbis_dsp_clear( &p_pack->vd );
    vorbis_comment_clear( &p_pack->vc );
    vorbis_info_clear( &p_pack->vi );  /* must be called last */

    if( p_pack->p_sout_input )
    {
        sout_InputDelete( p_pack->p_sout_input );
    }
}

/*****************************************************************************
 * GetOggPacket: get the following vorbis packet from the stream and send back
 *               the result in an ogg packet (for easy decoding by libvorbis).
 *****************************************************************************
 * Returns VLC_EGENERIC in case of eof.
 *****************************************************************************/
static int GetOggPacket( packetizer_t *p_pack, ogg_packet *p_oggpacket,
                         mtime_t *p_pts )
{
    if( p_pack->p_pes ) input_DeletePES( p_pack->p_fifo->p_packets_mgt,
                                         p_pack->p_pes );

    input_ExtractPES( p_pack->p_fifo, &p_pack->p_pes );
    if( !p_pack->p_pes ) return VLC_EGENERIC;

    p_oggpacket->packet = p_pack->p_pes->p_first->p_payload_start;
    p_oggpacket->bytes = p_pack->p_pes->i_pes_size;
    p_oggpacket->granulepos = p_pack->p_pes->i_dts;
    p_oggpacket->b_o_s = 0;
    p_oggpacket->e_o_s = 0;
    p_oggpacket->packetno = 0;

    *p_pts = p_pack->p_pes->i_pts;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * SendOggPacket: send an ogg packet to the stream output.
 *****************************************************************************
 * Returns VLC_EGENERIC in case of error.
 *****************************************************************************/
static int SendOggPacket( packetizer_t *p_pack, ogg_packet *p_oggpacket,
                          mtime_t i_pts, int i_samples )
{
    sout_buffer_t *p_sout_buffer =
        sout_BufferNew( p_pack->p_sout_input->p_sout, p_oggpacket->bytes );

    if( !p_sout_buffer )
    {
        p_pack->p_fifo->b_error = 1;
        return VLC_EGENERIC;
    }

    p_pack->p_fifo->p_vlc->pf_memcpy( p_sout_buffer->p_buffer,
                                      p_oggpacket->packet,
                                      p_oggpacket->bytes );

    p_sout_buffer->i_bitrate = p_pack->vi.bitrate_nominal;

    p_sout_buffer->i_dts = i_pts;
    p_sout_buffer->i_pts = i_pts;

    p_sout_buffer->i_length = 1000000 * (uint64_t)i_samples / p_pack->vi.rate;

    p_pack->i_samplescount += i_samples;

    sout_InputSendBuffer( p_pack->p_sout_input, p_sout_buffer );

    return VLC_SUCCESS;
}
