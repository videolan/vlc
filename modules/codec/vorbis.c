/*****************************************************************************
 * vorbis.c: vorbis decoder module making use of libvorbis.
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: vorbis.c,v 1.4 2002/11/03 13:22:44 gbazin Exp $
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                    /* memcpy(), memset() */

#include <vlc/vlc.h>
#include <vlc/aout.h>
#include <vlc/decoder.h>
#include <input_ext-dec.h>

#include <vlc/input.h>

#include <ogg/ogg.h>
#include <vorbis/codec.h>

/*****************************************************************************
 * dec_thread_t : vorbis decoder thread descriptor
 *****************************************************************************/
typedef struct dec_thread_t
{
    /*
     * Thread properties
     */
    vlc_thread_t        thread_id;                /* id for thread functions */

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

    /*
     * Input properties
     */
    decoder_fifo_t         *p_fifo;            /* stores the PES stream data */
    pes_packet_t           *p_pes;            /* current PES we are decoding */

    /*
     * Output properties
     */
    aout_instance_t        *p_aout;
    aout_input_t           *p_aout_input;
    audio_sample_format_t   output_format;
    audio_date_t            end_date;

} dec_thread_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder  ( vlc_object_t * );
static int  RunDecoder   ( decoder_fifo_t * );
static void CloseDecoder ( dec_thread_t * );

static void DecodePacket ( dec_thread_t * );
static int  GetOggPacket ( dec_thread_t *, ogg_packet *, mtime_t * );

static void Interleave   ( float *, const float **, int, int );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Vorbis decoder module") );
    set_capability( "decoder", 100 );
    set_callbacks( OpenDecoder, NULL );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_fifo_t *p_fifo = (decoder_fifo_t*) p_this;

    if( p_fifo->i_fourcc != VLC_FOURCC('v','o','r','b') )
    {
        return VLC_EGENERIC;
    }

    p_fifo->pf_run = RunDecoder;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * RunDecoder: the vorbis decoder
 *****************************************************************************/
static int RunDecoder( decoder_fifo_t * p_fifo )
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

    /* Take care of the initial Vorbis header */
    vorbis_info_init( &p_dec->vi );
    vorbis_comment_init( &p_dec->vc );

    if( GetOggPacket( p_dec, &oggpacket, &i_pts ) != VLC_SUCCESS )
        goto error;

    oggpacket.b_o_s = 1; /* yes this actually is a b_o_s packet :) */
    if( vorbis_synthesis_headerin( &p_dec->vi, &p_dec->vc, &oggpacket ) < 0 )
    {
        msg_Err( p_dec->p_fifo, "This bitstream does not contain Vorbis "
                 "audio data");
        goto error;
    }

    /* The next two packets in order are the comment and codebook headers.
       We need to watch out that these packets are not missing as a
       missing or corrupted header is fatal. */
    if( GetOggPacket( p_dec, &oggpacket, &i_pts ) != VLC_SUCCESS )
        goto error;

    if( vorbis_synthesis_headerin( &p_dec->vi, &p_dec->vc, &oggpacket ) < 0 )
    {
        msg_Err( p_dec->p_fifo, "2nd Vorbis header is corrupted" );
        goto error;
    }

    if( GetOggPacket( p_dec, &oggpacket, &i_pts ) != VLC_SUCCESS )
        goto error;

    if( vorbis_synthesis_headerin( &p_dec->vi, &p_dec->vc, &oggpacket ) < 0 )
    {
        msg_Err( p_dec->p_fifo, "3rd Vorbis header is corrupted" );
        goto error;
    }

    /* Initialize the Vorbis packet->PCM decoder */
    vorbis_synthesis_init( &p_dec->vd, &p_dec->vi );
    vorbis_block_init( &p_dec->vd, &p_dec->vb );

    p_dec->output_format.i_format = VLC_FOURCC('f','l','3','2');
    p_dec->output_format.i_channels = p_dec->vi.channels;
    p_dec->output_format.i_rate = p_dec->vi.rate;

    aout_DateInit( &p_dec->end_date, p_dec->vi.rate );
    p_dec->p_aout = NULL;
    p_dec->p_aout_input = aout_DecNew( p_dec->p_fifo,
                                       &p_dec->p_aout,
                                       &p_dec->output_format );

    if( p_dec->p_aout_input == NULL )
    {
        msg_Err( p_dec->p_fifo, "failed to create aout fifo" );
        goto error;
    }

    /* vorbis decoder thread's main loop */
    while( (!p_dec->p_fifo->b_die) && (!p_dec->p_fifo->b_error) )
    {
        DecodePacket( p_dec );
    }

    /* If b_error is set, the vorbis decoder thread enters the error loop */
    if( p_dec->p_fifo->b_error )
    {
        DecoderError( p_dec->p_fifo );
    }

    /* End of the vorbis decoder thread */
    CloseDecoder( p_dec );

    return 0;

 error:
    DecoderError( p_fifo );
    if( p_dec )
    {
        if( p_dec->p_fifo )
            p_dec->p_fifo->b_error = 1;

        /* End of the vorbis decoder thread */
        CloseDecoder( p_dec );
    }

    return -1;

}

/*****************************************************************************
 * DecodePacket: decodes a Vorbis packet.
 *****************************************************************************/
static void DecodePacket( dec_thread_t *p_dec )
{
    aout_buffer_t *p_aout_buffer;
    ogg_packet    oggpacket;
    float         **pp_pcm;
    int           i_samples;
    mtime_t       i_pts;

    if( GetOggPacket( p_dec, &oggpacket, &i_pts ) != VLC_SUCCESS )
    {
        /* This should mean an eos */
        return;
    }

    /* Date management */
    if( i_pts > 0 && i_pts != aout_DateGet( &p_dec->end_date ) )
    {
        aout_DateSet( &p_dec->end_date, i_pts );
    }

    if( vorbis_synthesis( &p_dec->vb, &oggpacket ) == 0 )
        vorbis_synthesis_blockin( &p_dec->vd, &p_dec->vb );

    /* **pp_pcm is a multichannel float vector. In stereo, for
     * example, pp_pcm[0] is left, and pp_pcm[1] is right. i_samples is
     * the size of each channel. Convert the float values
     * (-1.<=range<=1.) to whatever PCM format and write it out */

    while( ( i_samples = vorbis_synthesis_pcmout( &p_dec->vd, &pp_pcm ) ) > 0 )
    {

        p_aout_buffer = aout_DecNewBuffer( p_dec->p_aout, p_dec->p_aout_input,
                                           i_samples );
        if( !p_aout_buffer )
        {
            msg_Err( p_dec->p_fifo, "cannot get aout buffer" );
            p_dec->p_fifo->b_error = 1;
            return;
        }

        /* Interleave the samples */
        Interleave( (float *)p_aout_buffer->p_buffer, (const float **)pp_pcm,
                    p_dec->vi.channels, i_samples );

        /* Tell libvorbis how many samples we actually consumed */
        vorbis_synthesis_read( &p_dec->vd, i_samples );

        /* Date management */
        p_aout_buffer->start_date = aout_DateGet( &p_dec->end_date );
        p_aout_buffer->end_date = aout_DateIncrement( &p_dec->end_date,
                                                      i_samples );

        aout_DecPlay( p_dec->p_aout, p_dec->p_aout_input, p_aout_buffer );
    }

}

/*****************************************************************************
 * GetOggPacket: get the following vorbis packet from the stream and send back
 *               the result in an ogg packet (for easy decoding by libvorbis).
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
 * Interleave: helper function to interleave channels
 *****************************************************************************/
static void Interleave( float *p_out, const float **pp_in, int i_channels,
                        int i_samples )
{
    int i, j;

    for ( j = 0; j < i_samples; j++ )
    {
        for ( i = 0; i < i_channels; i++ )
        {
            p_out[j * i_channels + i] = pp_in[i][j];
        }
    }
}

/*****************************************************************************
 * CloseDecoder: vorbis decoder destruction
 *****************************************************************************/
static void CloseDecoder( dec_thread_t * p_dec )
{
    if( p_dec->p_aout_input != NULL )
    {
        aout_DecDelete( p_dec->p_aout, p_dec->p_aout_input );
    }

    if( p_dec )
    {
        if( p_dec->p_pes )
            input_DeletePES( p_dec->p_fifo->p_packets_mgt, p_dec->p_pes );
        vorbis_block_clear( &p_dec->vb );
        vorbis_dsp_clear( &p_dec->vd );
        vorbis_comment_clear( &p_dec->vc );
        vorbis_info_clear( &p_dec->vi );  /* must be called last */
        free( p_dec );
    }
}
