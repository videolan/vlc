/*****************************************************************************
 * input_dec.c: Functions for the management of decoders
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: input_dec.c,v 1.48 2002/10/27 16:58:12 gbazin Exp $
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
#include <sys/types.h>                                              /* off_t */

#include <vlc/vlc.h>

#include "stream_control.h"
#include "input_ext-dec.h"
#include "input_ext-intf.h"
#include "input_ext-plugins.h"

static decoder_fifo_t * CreateDecoderFifo( input_thread_t *,
                                           es_descriptor_t * );
static void             DeleteDecoderFifo( decoder_fifo_t * );

/*****************************************************************************
 * input_RunDecoder: spawns a new decoder thread
 *****************************************************************************/
decoder_fifo_t * input_RunDecoder( input_thread_t * p_input,
                                   es_descriptor_t * p_es )
{
    decoder_fifo_t *p_fifo;
    int i_priority;

    /* Create the decoder configuration structure */
    p_fifo = CreateDecoderFifo( p_input, p_es );

    if( p_fifo == NULL )
    {
        msg_Err( p_input, "could not create decoder fifo" );
        return NULL;
    }

    /* Get a suitable module */
    p_fifo->p_module = module_Need( p_fifo, "decoder", "$codec" );
    if( p_fifo->p_module == NULL )
    {
        msg_Err( p_fifo, "no suitable decoder module for fourcc `%4.4s'",
                       (char*)&p_fifo->i_fourcc );
        DeleteDecoderFifo( p_fifo );
        vlc_object_destroy( p_fifo );
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
    if( vlc_thread_create( p_fifo, "decoder", p_fifo->pf_run,
                           i_priority, VLC_FALSE ) )
    {
        msg_Err( p_fifo, "cannot spawn decoder thread \"%s\"",
                         p_fifo->p_module->psz_object_name );
        module_Unneed( p_fifo, p_fifo->p_module );
        return NULL;
    }

    p_input->stream.b_changed = 1;

    return p_fifo;
}


/*****************************************************************************
 * input_EndDecoder: kills a decoder thread and waits until it's finished
 *****************************************************************************/
void input_EndDecoder( input_thread_t * p_input, es_descriptor_t * p_es )
{
    int i_dummy;

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
    vlc_thread_join( p_es->p_decoder_fifo );
    /* vlc_mutex_lock( &p_input->stream.stream_lock ); */

    /* Delete decoder configuration */
    DeleteDecoderFifo( p_es->p_decoder_fifo );

    /* Unneed module */
    module_Unneed( p_es->p_decoder_fifo, p_es->p_decoder_fifo->p_module );

    /* Delete the fifo */
    vlc_object_destroy( p_es->p_decoder_fifo );

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

/****************************************************************************
 * input_ExtractPES
 *****************************************************************************
 * Extract a PES from the fifo. If pp_pes is NULL then the PES is just
 * deleted, otherwise *pp_pes will point to this PES.
 ****************************************************************************/
void input_ExtractPES( decoder_fifo_t *p_fifo, pes_packet_t **pp_pes )
{
    pes_packet_t *p_pes;

    vlc_mutex_lock( &p_fifo->data_lock );

    if( p_fifo->p_first == NULL )
    {
        if( p_fifo->b_die )
        {
            vlc_mutex_unlock( &p_fifo->data_lock );
            if( pp_pes ) *pp_pes = NULL;
            return;
        }

        /* Signal the input thread we're waiting. This is only
         * needed in case of slave clock (ES plug-in) but it won't
         * harm. */
        vlc_cond_signal( &p_fifo->data_wait );

        /* Wait for the input to tell us when we received a packet. */
        vlc_cond_wait( &p_fifo->data_wait, &p_fifo->data_lock );
    }

    p_pes = p_fifo->p_first;
    p_fifo->p_first = p_pes->p_next;
    p_pes->p_next = NULL;
    p_fifo->i_depth--;

    if( !p_fifo->p_first )
    {
        /* No PES in the FIFO. p_last is no longer valid. */
        p_fifo->pp_last = &p_fifo->p_first;
    }

    vlc_mutex_unlock( &p_fifo->data_lock );

    if( pp_pes )
        *pp_pes = p_pes;
    else
        input_DeletePES( p_fifo->p_packets_mgt, p_pes );
}

/****************************************************************************
 * input_FlushPESFifo
 *****************************************************************************
 * Empties the PES fifo of the decoder.
 ****************************************************************************/
void input_FlushPESFifo( decoder_fifo_t *p_fifo )
{
    pes_packet_t * p_pes;

    vlc_mutex_lock( &p_fifo->data_lock );
    while( p_fifo->p_first )
    {
        p_pes = p_fifo->p_first;
        p_fifo->p_first = p_fifo->p_first->p_next;
        input_DeletePES( p_fifo->p_packets_mgt, p_pes );
    }
    /* No PES in the FIFO. p_last is no longer valid. */
    p_fifo->pp_last = &p_fifo->p_first;
    vlc_mutex_unlock( &p_fifo->data_lock );
}

/*****************************************************************************
 * input_EscapeDiscontinuity: send a NULL packet to the decoders
 *****************************************************************************/
void input_EscapeDiscontinuity( input_thread_t * p_input )
{
    int     i_es, i;

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
    int     i_es, i;

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
static decoder_fifo_t * CreateDecoderFifo( input_thread_t * p_input,
                                           es_descriptor_t * p_es )
{
    decoder_fifo_t * p_fifo;

    /* Decoder FIFO */
    p_fifo = vlc_object_create( p_input, VLC_OBJECT_DECODER );
    if( p_fifo == NULL )
    {
        msg_Err( p_input, "out of memory" );
        return NULL;
    }

    /* Select a new ES */
    p_input->stream.i_selected_es_number++;
    p_input->stream.pp_selected_es = realloc(
                                          p_input->stream.pp_selected_es,
                                          p_input->stream.i_selected_es_number
                                           * sizeof(es_descriptor_t *) );
    if( p_input->stream.pp_selected_es == NULL )
    {
        msg_Err( p_input, "out of memory" );
        vlc_object_destroy( p_fifo );
        return NULL;
    }

    p_input->stream.pp_selected_es[p_input->stream.i_selected_es_number - 1]
            = p_es;

    /* Initialize the p_fifo structure */
    vlc_mutex_init( p_input, &p_fifo->data_lock );
    vlc_cond_init( p_input, &p_fifo->data_wait );
    p_es->p_decoder_fifo = p_fifo;

    p_fifo->i_id = p_es->i_id;
    p_fifo->i_fourcc = p_es->i_fourcc;
    p_fifo->p_demux_data = p_es->p_demux_data;
    
    p_fifo->p_stream_ctrl = &p_input->stream.control;
    p_fifo->p_sout = p_input->stream.p_sout;

    p_fifo->p_first = NULL;
    p_fifo->pp_last = &p_fifo->p_first;
    p_fifo->i_depth = 0;
    p_fifo->b_die = p_fifo->b_error = 0;
    p_fifo->p_packets_mgt = p_input->p_method_data;

    vlc_object_attach( p_fifo, p_input );

    return p_fifo;
}

/*****************************************************************************
 * DeleteDecoderFifo: destroy a decoder_fifo_t
 *****************************************************************************/
static void DeleteDecoderFifo( decoder_fifo_t * p_fifo )
{
    vlc_object_detach( p_fifo );

    msg_Dbg( p_fifo, "killing decoder for 0x%x, fourcc `%4.4s', %d PES in FIFO",
                     p_fifo->i_id, (char*)&p_fifo->i_fourcc, p_fifo->i_depth );

    /* Free all packets still in the decoder fifo. */
    input_DeletePES( p_fifo->p_packets_mgt,
                     p_fifo->p_first );

    /* Destroy the lock and cond */
    vlc_cond_destroy( &p_fifo->data_wait );
    vlc_mutex_destroy( &p_fifo->data_lock );
}

