/*****************************************************************************
 * input_dec.c: Functions for the management of decoders
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: input_dec.c,v 1.62 2003/09/02 20:19:26 gbazin Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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
#include <stdlib.h>
#include <string.h>                                    /* memcpy(), memset() */

#include <vlc/vlc.h>

#include "vlc_block.h"
#include "stream_control.h"
#include "input_ext-dec.h"
#include "input_ext-intf.h"
#include "input_ext-plugins.h"

static decoder_t * CreateDecoder( input_thread_t *, es_descriptor_t * );
static int         DecoderThread( decoder_t * );
static void        DeleteDecoder( decoder_t * );

/*****************************************************************************
 * input_RunDecoder: spawns a new decoder thread
 *****************************************************************************/
decoder_fifo_t * input_RunDecoder( input_thread_t * p_input,
                                   es_descriptor_t * p_es )
{
    vlc_value_t    val;
    decoder_t      *p_dec;
    int            i_priority;

    /* Create the decoder configuration structure */
    p_dec = CreateDecoder( p_input, p_es );
    if( p_dec == NULL )
    {
        msg_Err( p_input, "could not create decoder" );
        return NULL;
    }

    p_dec->p_module = NULL;

    /* If we are in sout mode, search for packetizer module */
    var_Get( p_input, "sout", &val );
    if( !p_es->b_force_decoder && val.psz_string && *val.psz_string )
    {
        free( val.psz_string );
        val.b_bool = VLC_TRUE;

        if( p_es->i_cat == AUDIO_ES )
        {
            var_Get( p_input, "sout-audio", &val );
        }
        else if( p_es->i_cat == VIDEO_ES )
        {
            var_Get( p_input, "sout-video", &val );
        }

        if( val.b_bool )
        {
            p_dec->p_module =
                module_Need( p_dec, "packetizer", "$packetizer" );
        }
    }
    else
    {
        /* default Get a suitable decoder module */
        p_dec->p_module = module_Need( p_dec, "decoder", "$codec" );

        if( val.psz_string ) free( val.psz_string );
    }

    if( p_dec->p_module == NULL )
    {
        msg_Err( p_dec, "no suitable decoder module for fourcc `%4.4s'.\n"
                 "VLC probably does not support this sound or video format.",
                 (char*)&p_dec->p_fifo->i_fourcc );

        DeleteDecoder( p_dec );
        vlc_object_destroy( p_dec );
        return NULL;
    }

    if ( p_es->i_cat == AUDIO_ES )
    {
        i_priority = VLC_THREAD_PRIORITY_AUDIO;
    }
    else
    {
        i_priority = VLC_THREAD_PRIORITY_VIDEO;
    }

    /* Spawn the decoder thread */
    if( vlc_thread_create( p_dec, "decoder", DecoderThread,
                           i_priority, VLC_FALSE ) )
    {
        msg_Err( p_dec, "cannot spawn decoder thread \"%s\"",
                         p_dec->p_module->psz_object_name );
        module_Unneed( p_dec, p_dec->p_module );
        DeleteDecoder( p_dec );
        vlc_object_destroy( p_dec );
        return NULL;
    }

    p_input->stream.b_changed = 1;

    return p_dec->p_fifo;
}

/*****************************************************************************
 * input_EndDecoder: kills a decoder thread and waits until it's finished
 *****************************************************************************/
void input_EndDecoder( input_thread_t * p_input, es_descriptor_t * p_es )
{
    int i_dummy;
    decoder_t *p_dec = p_es->p_decoder_fifo->p_dec;

    p_es->p_decoder_fifo->b_die = 1;

    /* Make sure the thread leaves the NextDataPacket() function by
     * sending it a few null packets. */
    for( i_dummy = 0; i_dummy < PADDING_PACKET_NUMBER; i_dummy++ )
    {
        input_NullPacket( p_input, p_es );
    }

    if( p_es->p_pes != NULL )
    {
        input_DecodePES( p_es->p_decoder_fifo, p_es->p_pes );
    }

    /* Waiting for the thread to exit */
    /* I thought that unlocking was better since thread join can be long
     * but it actually creates late pictures and freezes --stef */
    /* vlc_mutex_unlock( &p_input->stream.stream_lock ); */
    vlc_thread_join( p_dec );
    /* vlc_mutex_lock( &p_input->stream.stream_lock ); */

    /* Unneed module */
    module_Unneed( p_dec, p_dec->p_module );

    /* Delete decoder configuration */
    DeleteDecoder( p_dec );

    /* Delete the decoder */
    vlc_object_destroy( p_dec );

    /* Tell the input there is no more decoder */
    p_es->p_decoder_fifo = NULL;

    p_input->stream.b_changed = 1;
}

/*****************************************************************************
 * input_DecodePES
 *****************************************************************************
 * Put a PES in the decoder's fifo.
 *****************************************************************************/
void input_DecodePES( decoder_fifo_t * p_decoder_fifo, pes_packet_t * p_pes )
{
    vlc_mutex_lock( &p_decoder_fifo->data_lock );

    p_pes->p_next = NULL;
    *p_decoder_fifo->pp_last = p_pes;
    p_decoder_fifo->pp_last = &p_pes->p_next;
    p_decoder_fifo->i_depth++;

    /* Warn the decoder that it's got work to do. */
    vlc_cond_signal( &p_decoder_fifo->data_wait );
    vlc_mutex_unlock( &p_decoder_fifo->data_lock );
}

/*****************************************************************************
 * Create a NULL packet for padding in case of a data loss
 *****************************************************************************/
void input_NullPacket( input_thread_t * p_input,
                       es_descriptor_t * p_es )
{
    data_packet_t *             p_pad_data;
    pes_packet_t *              p_pes;

    if( (p_pad_data = input_NewPacketForce( p_input->p_method_data,
                    PADDING_PACKET_SIZE)) == NULL )
    {
        msg_Err( p_input, "no new packet" );
        p_input->b_error = 1;
        return;
    }

    memset( p_pad_data->p_payload_start, 0, PADDING_PACKET_SIZE );
    p_pad_data->b_discard_payload = 1;
    p_pes = p_es->p_pes;

    if( p_pes != NULL )
    {
        p_pes->b_discontinuity = 1;
        p_pes->p_last->p_next = p_pad_data;
        p_pes->p_last = p_pad_data;
        p_pes->i_nb_data++;
    }
    else
    {
        if( (p_pes = input_NewPES( p_input->p_method_data )) == NULL )
        {
            msg_Err( p_input, "no PES packet" );
            p_input->b_error = 1;
            return;
        }

        p_pes->i_rate = p_input->stream.control.i_rate;
        p_pes->p_first = p_pes->p_last = p_pad_data;
        p_pes->i_nb_data = 1;
        p_pes->b_discontinuity = 1;
        input_DecodePES( p_es->p_decoder_fifo, p_pes );
    }
}

/*****************************************************************************
 * input_EscapeDiscontinuity: send a NULL packet to the decoders
 *****************************************************************************/
void input_EscapeDiscontinuity( input_thread_t * p_input )
{
    unsigned int i_es, i;

    for( i_es = 0; i_es < p_input->stream.i_selected_es_number; i_es++ )
    {
        es_descriptor_t * p_es = p_input->stream.pp_selected_es[i_es];

        if( p_es->p_decoder_fifo != NULL )
        {
            for( i = 0; i < PADDING_PACKET_NUMBER; i++ )
            {
                input_NullPacket( p_input, p_es );
            }
        }
    }
}

/*****************************************************************************
 * input_EscapeAudioDiscontinuity: send a NULL packet to the audio decoders
 *****************************************************************************/
void input_EscapeAudioDiscontinuity( input_thread_t * p_input )
{
    unsigned int i_es, i;

    for( i_es = 0; i_es < p_input->stream.i_selected_es_number; i_es++ )
    {
        es_descriptor_t * p_es = p_input->stream.pp_selected_es[i_es];

        if( p_es->p_decoder_fifo != NULL && p_es->i_cat == AUDIO_ES )
        {
            for( i = 0; i < PADDING_PACKET_NUMBER; i++ )
            {
                input_NullPacket( p_input, p_es );
            }
        }
    }
}

/*****************************************************************************
 * CreateDecoderFifo: create a decoder_fifo_t
 *****************************************************************************/
static decoder_t * CreateDecoder( input_thread_t * p_input,
                                  es_descriptor_t * p_es )
{
    decoder_t * p_dec;

    p_dec = vlc_object_create( p_input, VLC_OBJECT_DECODER );
    if( p_dec == NULL )
    {
        msg_Err( p_input, "out of memory" );
        return NULL;
    }

    p_dec->pf_init = 0;
    p_dec->pf_decode = 0;
    p_dec->pf_end = 0;
    p_dec->pf_run = 0;

    /* Select a new ES */
    INSERT_ELEM( p_input->stream.pp_selected_es,
                 p_input->stream.i_selected_es_number,
                 p_input->stream.i_selected_es_number,
                 p_es );

    /* Allocate the memory needed to store the decoder's fifo */
    //p_dec->p_fifo = (decoder_fifo_t *)malloc(sizeof(decoder_fifo_t));
    p_dec->p_fifo = vlc_object_create( p_input, VLC_OBJECT_DECODER_FIFO );
    if( p_dec->p_fifo == NULL )
    {
        msg_Err( p_input, "out of memory" );
        return NULL;
    }

    /* Initialize the decoder fifo */
    //memset( p_dec->p_fifo, 0, sizeof(decoder_fifo_t) );
    p_dec->p_module = NULL;

    /* Initialize the p_fifo structure */
    vlc_mutex_init( p_input, &p_dec->p_fifo->data_lock );
    vlc_cond_init( p_input, &p_dec->p_fifo->data_wait );
    p_es->p_decoder_fifo = p_dec->p_fifo;

    p_dec->p_fifo->i_id = p_es->i_id;
    p_dec->p_fifo->i_fourcc = p_es->i_fourcc;
    p_dec->p_fifo->p_demux_data   = p_es->p_demux_data;
    p_dec->p_fifo->p_waveformatex = p_es->p_waveformatex;
    p_dec->p_fifo->p_bitmapinfoheader = p_es->p_bitmapinfoheader;
    p_dec->p_fifo->p_stream_ctrl = &p_input->stream.control;
    p_dec->p_fifo->p_sout = p_input->stream.p_sout;

    p_dec->p_fifo->p_first = NULL;
    p_dec->p_fifo->pp_last = &p_dec->p_fifo->p_first;
    p_dec->p_fifo->i_depth = 0;
    p_dec->p_fifo->b_die = p_dec->p_fifo->b_error = 0;
    p_dec->p_fifo->p_packets_mgt = p_input->p_method_data;

    p_dec->p_fifo->p_dec = p_dec;

    vlc_object_attach( p_dec->p_fifo, p_input );
    vlc_object_attach( p_dec, p_input );

    return p_dec;
}

/*****************************************************************************
 * DecoderThread: the decoding main loop
 *****************************************************************************/
static int DecoderThread( decoder_t * p_dec )
{
    pes_packet_t  *p_pes;
    data_packet_t *p_data;
    block_t       *p_block;

    /* Temporary wrapper to keep old decoder api functional */
    if( p_dec->pf_run )
    {
        p_dec->pf_run( p_dec->p_fifo );
        return 0;
    }


    /* Initialize the decoder */
    p_dec->p_fifo->b_error = p_dec->pf_init( p_dec );

    /* The decoder's main loop */
    while( !p_dec->p_fifo->b_die && !p_dec->p_fifo->b_error )
    {
        int i_size;

        input_ExtractPES( p_dec->p_fifo, &p_pes );
        if( !p_pes )
        {
            p_dec->p_fifo->b_error = 1;
            break;
        }

	for( i_size = 0, p_data = p_pes->p_first;
	     p_data != NULL; p_data = p_data->p_next )
	{
 	    i_size += p_data->p_payload_end - p_data->p_payload_start;
	}
	p_block = block_New( p_dec, i_size );
	for( i_size = 0, p_data = p_pes->p_first;
	     p_data != NULL; p_data = p_data->p_next )
	{
            memcpy( p_block->p_buffer + i_size, p_data->p_payload_start,
		    p_data->p_payload_end - p_data->p_payload_start );
            i_size += p_data->p_payload_end - p_data->p_payload_start;
	}

        p_block->i_pts = p_pes->i_pts;
        p_block->i_dts = p_pes->i_dts;
        p_dec->p_fifo->b_error = p_dec->pf_decode( p_dec, p_block );

        input_DeletePES( p_dec->p_fifo->p_packets_mgt, p_pes );
    }

    /* If b_error is set, the decoder thread enters the error loop */
    if( p_dec->p_fifo->b_error )
    {
        /* Wait until a `die' order is sent */
        while( !p_dec->p_fifo->b_die )
        {
            /* Trash all received PES packets */
            input_ExtractPES( p_dec->p_fifo, NULL );
        }
    }

    /* End of the decoder */
    p_dec->pf_end( p_dec );

    return 0;
}

/*****************************************************************************
 * DeleteDecoderFifo: destroy a decoder_fifo_t
 *****************************************************************************/
static void DeleteDecoder( decoder_t * p_dec )
{
    vlc_object_detach( p_dec );
    vlc_object_detach( p_dec->p_fifo );

    msg_Dbg( p_dec,
             "killing decoder for 0x%x, fourcc `%4.4s', %d PES in FIFO",
             p_dec->p_fifo->i_id, (char*)&p_dec->p_fifo->i_fourcc,
             p_dec->p_fifo->i_depth );

    /* Free all packets still in the decoder fifo. */
    input_DeletePES( p_dec->p_fifo->p_packets_mgt,
                     p_dec->p_fifo->p_first );

    /* Destroy the lock and cond */
    vlc_cond_destroy( &p_dec->p_fifo->data_wait );
    vlc_mutex_destroy( &p_dec->p_fifo->data_lock );

    /* Free fifo */
    vlc_object_destroy( p_dec->p_fifo );
    //free( p_dec->p_fifo );
}
