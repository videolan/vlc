/*****************************************************************************
 * input_dec.c: Functions for the management of decoders
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: input_dec.c,v 1.33 2002/05/12 01:39:36 massiot Exp $
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

#include <videolan/vlc.h>

#include "stream_control.h"
#include "input_ext-dec.h"
#include "input_ext-intf.h"
#include "input_ext-plugins.h"

static decoder_config_t * CreateDecoderConfig( input_thread_t * p_input,
                                               es_descriptor_t * p_es );
static void DeleteDecoderConfig( decoder_config_t * p_config );

/*****************************************************************************
 * input_RunDecoder: spawns a new decoder thread
 *****************************************************************************/
vlc_thread_t input_RunDecoder( input_thread_t * p_input,
                               es_descriptor_t * p_es )
{
    vlc_thread_t thread_id;
    char * psz_plugin = NULL;

    if( p_es->i_type == MPEG1_AUDIO_ES || p_es->i_type == MPEG2_AUDIO_ES )
    {
        psz_plugin = config_GetPszVariable( "mpeg-adec" );
    }
    if( p_es->i_type == AC3_AUDIO_ES )
    {
        psz_plugin = config_GetPszVariable( "ac3-adec" );
    }

    /* Get a suitable module */
    p_es->p_module = module_Need( MODULE_CAPABILITY_DECODER, psz_plugin,
                                  (void *)&p_es->i_type );
    if( psz_plugin ) free( psz_plugin );
    if( p_es->p_module == NULL )
    {
        intf_ErrMsg( "input error: no suitable decoder module for type 0x%x",
                      p_es->i_type );
        return( 0 );
    }

    /* Create the decoder configuration structure */
    p_es->p_config = CreateDecoderConfig( p_input, p_es );

    if( p_es->p_config == NULL )
    {
        intf_ErrMsg( "input error: could not create decoder config" );
        module_Unneed( p_es->p_module );
        return( 0 );
    }

    /* Spawn the decoder thread */
    if ( vlc_thread_create( &thread_id, "decoder",
         (vlc_thread_func_t)p_es->p_module->
             p_functions->dec.functions.dec.pf_run,
         (void *)p_es->p_config) ) 
    {
        intf_ErrMsg( "input error: can't spawn decoder thread \"%s\"",
                     p_es->p_module->psz_name );
        free( p_es->p_config );
        module_Unneed( p_es->p_module );
        return( 0 );
    }

    p_input->stream.b_changed = 1;

    return thread_id;
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
//    vlc_mutex_unlock( &p_input->stream.stream_lock );
    vlc_thread_join( p_es->thread_id );
//    vlc_mutex_lock( &p_input->stream.stream_lock );

    /* Delete decoder configuration */
    DeleteDecoderConfig( p_es->p_config );

    /* Unneed module */
    module_Unneed( p_es->p_module );

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
 * input_EscapeDiscontinuity: send a NULL packet to the decoders
 *****************************************************************************/
void input_EscapeDiscontinuity( input_thread_t * p_input,
                                pgrm_descriptor_t * p_pgrm )
{
    int     i_es, i;

    for( i_es = 0; i_es < p_pgrm->i_es_number; i_es++ )
    {
        es_descriptor_t * p_es = p_pgrm->pp_es[i_es];

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
    int     i_pgrm, i_es, i;

    for( i_pgrm = 0; i_pgrm < p_input->stream.i_pgrm_number; i_pgrm++ )
    {
        pgrm_descriptor_t * p_pgrm = p_input->stream.pp_programs[i_pgrm];

        for( i_es = 0; i_es < p_pgrm->i_es_number; i_es++ )
        {
            es_descriptor_t * p_es = p_pgrm->pp_es[i_es];

            if( p_es->p_decoder_fifo != NULL && p_es->b_audio )
            {
                for( i = 0; i < PADDING_PACKET_NUMBER; i++ )
                {
                    input_NullPacket( p_input, p_es );
                }
            }
        }
    }
}

/*****************************************************************************
 * CreateDecoderConfig: create a decoder_config_t
 *****************************************************************************/
static decoder_config_t * CreateDecoderConfig( input_thread_t * p_input,
                                               es_descriptor_t * p_es )
{
    decoder_config_t * p_config;

    p_config = (decoder_config_t *)malloc( sizeof(decoder_config_t) );
    if( p_config == NULL )
    {
        intf_ErrMsg( "Unable to allocate memory in CreateDecoderConfig" );
        return NULL;
    }

    /* Decoder FIFO */
    if( (p_config->p_decoder_fifo =
            (decoder_fifo_t *)malloc( sizeof(decoder_fifo_t) )) == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        free( p_config );
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
        intf_ErrMsg( "Unable to realloc memory" );
        free( p_config->p_decoder_fifo );
        free( p_config );
        return NULL;
    }
    p_input->stream.pp_selected_es[p_input->stream.i_selected_es_number - 1]
            = p_es;

    /* Initialize the p_config structure */
    vlc_mutex_init(&p_config->p_decoder_fifo->data_lock);
    vlc_cond_init(&p_config->p_decoder_fifo->data_wait);
    p_es->p_decoder_fifo = p_config->p_decoder_fifo;

    p_config->i_id = p_es->i_id;
    p_config->i_type = p_es->i_type;
    p_config->p_demux_data = p_es->p_demux_data;
    
    p_config->p_stream_ctrl = &p_input->stream.control;

    p_config->p_decoder_fifo->p_first = NULL;
    p_config->p_decoder_fifo->pp_last = &p_config->p_decoder_fifo->p_first;
    p_config->p_decoder_fifo->i_depth = 0;
    p_config->p_decoder_fifo->b_die = p_config->p_decoder_fifo->b_error = 0;
    p_config->p_decoder_fifo->p_packets_mgt = p_input->p_method_data;

    return p_config;
}

/*****************************************************************************
 * DeleteDecoderConfig: create a decoder_config_t
 *****************************************************************************/
static void DeleteDecoderConfig( decoder_config_t * p_config )
{
    intf_StatMsg( "input stats: killing decoder for 0x%x, type 0x%x, %d PES in FIFO",
                  p_config->i_id, p_config->i_type,
                  p_config->p_decoder_fifo->i_depth );
    /* Free all packets still in the decoder fifo. */
    input_DeletePES( p_config->p_decoder_fifo->p_packets_mgt,
                     p_config->p_decoder_fifo->p_first );

    /* Destroy the lock and cond */
    vlc_cond_destroy( &p_config->p_decoder_fifo->data_wait );
    vlc_mutex_destroy( &p_config->p_decoder_fifo->data_lock );

    free( p_config->p_decoder_fifo );

    free( p_config );
}

