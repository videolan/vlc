/*****************************************************************************
 * input_programs.c: es_descriptor_t, pgrm_descriptor_t management
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: input_programs.c,v 1.59 2001/06/27 09:53:57 massiot Exp $
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
#include "defs.h"

#include <stdlib.h>
#include <string.h>                                    /* memcpy(), memset() */
#include <sys/types.h>                                              /* off_t */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "debug.h"

#include "intf_msg.h"

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input.h"

#include "main.h"                                     /* --noaudio --novideo */

/*
 * NOTICE : all of these functions expect you to have taken the lock on
 * p_input->stream.lock
 */

/*****************************************************************************
 * input_InitStream: init the stream descriptor of the given input
 *****************************************************************************/
int input_InitStream( input_thread_t * p_input, size_t i_data_len )
{

    p_input->stream.i_method = INPUT_METHOD_NONE;
    p_input->stream.i_stream_id = 0;

    /* initialized to 0 since we don't give the signal to the interface
     * before the end of input initialization */
    p_input->stream.b_changed = 0;
    p_input->stream.pp_es = NULL;
    p_input->stream.pp_selected_es = NULL;
    p_input->stream.p_removed_es = NULL;
    p_input->stream.p_newly_selected_es = NULL;
    p_input->stream.pp_programs = NULL;

    if( i_data_len )
    {
        if ( (p_input->stream.p_demux_data = malloc( i_data_len )) == NULL )
        {
            intf_ErrMsg( "Unable to allocate memory in input_InitStream");
            return 1;
        }
        memset( p_input->stream.p_demux_data, 0, i_data_len );
    }

    return 0;
}

/*****************************************************************************
 * input_EndStream: free all stream descriptors
 *****************************************************************************/
void input_EndStream( input_thread_t * p_input )
{
    /* Free all programs and associated ES, and associated decoders. */
    while( p_input->stream.i_pgrm_number )
    {
        input_DelProgram( p_input, p_input->stream.pp_programs[0] );
    }

    /* Free standalone ES */
    while( p_input->stream.i_es_number )
    {
        input_DelES( p_input, p_input->stream.pp_es[0] );
    }

    /* Free all areas */
    while( p_input->stream.i_area_nb )
    {
        input_DelArea( p_input, p_input->stream.pp_areas[0] );
    }

    if( p_input->stream.p_demux_data != NULL )
    {
        free( p_input->stream.p_demux_data );
    }
}

/*****************************************************************************
 * input_FindProgram: returns a pointer to a program described by its ID
 *****************************************************************************/
pgrm_descriptor_t * input_FindProgram( input_thread_t * p_input, u16 i_pgrm_id )
{
    int     i;

    for( i = 0; i < p_input->stream.i_pgrm_number; i++ )
    {
        if( p_input->stream.pp_programs[i]->i_number == i_pgrm_id )
        {
            return p_input->stream.pp_programs[i];
        }
    }

    return( NULL );
}

/*****************************************************************************
 * input_AddProgram: add and init a program descriptor
 *****************************************************************************
 * This program descriptor will be referenced in the given stream descriptor
 *****************************************************************************/
pgrm_descriptor_t * input_AddProgram( input_thread_t * p_input,
                                      u16 i_pgrm_id, size_t i_data_len )
{
    /* Where to add the pgrm */
    int i_pgrm_index = p_input->stream.i_pgrm_number;

    intf_DbgMsg("Adding description for pgrm %d", i_pgrm_id);

    /* Add an entry to the list of program associated with the stream */
    p_input->stream.i_pgrm_number++;
    p_input->stream.pp_programs = realloc( p_input->stream.pp_programs,
                                           p_input->stream.i_pgrm_number
                                            * sizeof(pgrm_descriptor_t *) );
    if( p_input->stream.pp_programs == NULL )
    {
        intf_ErrMsg( "Unable to realloc memory in input_AddProgram" );
        return( NULL );
    }
    
    /* Allocate the structure to store this description */
    p_input->stream.pp_programs[i_pgrm_index] =
                                        malloc( sizeof(pgrm_descriptor_t) );
    if( p_input->stream.pp_programs[i_pgrm_index] == NULL )
    {
        intf_ErrMsg( "Unable to allocate memory in input_AddProgram" );
        return( NULL );
    }
    
    /* Init this entry */
    p_input->stream.pp_programs[i_pgrm_index]->i_number = i_pgrm_id;
    p_input->stream.pp_programs[i_pgrm_index]->b_is_ok = 0;
    p_input->stream.pp_programs[i_pgrm_index]->i_version = 0;

    p_input->stream.pp_programs[i_pgrm_index]->i_es_number = 0;
    p_input->stream.pp_programs[i_pgrm_index]->pp_es = NULL;

    input_ClockInit( p_input->stream.pp_programs[i_pgrm_index] );

    p_input->stream.pp_programs[i_pgrm_index]->i_synchro_state
                                                = SYNCHRO_START;

    if( i_data_len )
    {
        p_input->stream.pp_programs[i_pgrm_index]->p_demux_data =
            malloc( i_data_len );
        if( p_input->stream.pp_programs[i_pgrm_index]->p_demux_data == NULL )
        {
            intf_ErrMsg( "Unable to allocate memory in input_AddProgram" );
            return( NULL );
        }
        memset( p_input->stream.pp_programs[i_pgrm_index]->p_demux_data, 0,
                i_data_len );
    }

    return p_input->stream.pp_programs[i_pgrm_index];
}

/*****************************************************************************
 * input_DelProgram: destroy a program descriptor
 *****************************************************************************
 * All ES descriptions referenced in the descriptor will be deleted.
 *****************************************************************************/
void input_DelProgram( input_thread_t * p_input, pgrm_descriptor_t * p_pgrm )
{
    int i_pgrm_index;

    ASSERT( p_pgrm );

    intf_DbgMsg("Deleting description for pgrm %d", p_pgrm->i_number);

    /* Free the structures that describe the es that belongs to that program */
    while( p_pgrm->i_es_number )
    {
        input_DelES( p_input, p_pgrm->pp_es[0] );
    }

    /* Free the demux data */
    if( p_pgrm->p_demux_data != NULL )
    {
        free( p_pgrm->p_demux_data );
    }

    /* Find the program in the programs table */
    for( i_pgrm_index = 0; i_pgrm_index < p_input->stream.i_pgrm_number;
         i_pgrm_index++ )
    {
        if( p_input->stream.pp_programs[i_pgrm_index] == p_pgrm )
            break;
    }

    /* Remove this program from the stream's list of programs */
    p_input->stream.i_pgrm_number--;

    p_input->stream.pp_programs[i_pgrm_index] =
        p_input->stream.pp_programs[p_input->stream.i_pgrm_number];
    p_input->stream.pp_programs = realloc( p_input->stream.pp_programs,
                                           p_input->stream.i_pgrm_number
                                            * sizeof(pgrm_descriptor_t *) );

    if( p_input->stream.i_pgrm_number && p_input->stream.pp_programs == NULL)
    {
        intf_ErrMsg( "input error: unable to realloc program list"
                     " in input_DelProgram" );
    }

    /* Free the description of this program */
    free( p_pgrm );
}

/*****************************************************************************
 * input_AddArea: add and init an area descriptor
 *****************************************************************************
 * This area descriptor will be referenced in the given stream descriptor
 *****************************************************************************/
input_area_t * input_AddArea( input_thread_t * p_input )
{
    /* Where to add the pgrm */
    int i_area_index = p_input->stream.i_area_nb;

    intf_DbgMsg("Adding description for area %d", i_area_index );

    /* Add an entry to the list of program associated with the stream */
    p_input->stream.i_area_nb++;
    p_input->stream.pp_areas = realloc( p_input->stream.pp_areas,
                                        p_input->stream.i_area_nb
                                            * sizeof(input_area_t *) );
    if( p_input->stream.pp_areas == NULL )
    {
        intf_ErrMsg( "Unable to realloc memory in input_AddArea" );
        return( NULL );
    }
    
    /* Allocate the structure to store this description */
    p_input->stream.pp_areas[i_area_index] =
                                        malloc( sizeof(input_area_t) );
    if( p_input->stream.pp_areas[i_area_index] == NULL )
    {
        intf_ErrMsg( "Unable to allocate memory in input_AddArea" );
        return( NULL );
    }
    
    /* Init this entry */
    p_input->stream.pp_areas[i_area_index]->i_id = 0;
    p_input->stream.pp_areas[i_area_index]->i_start = 0;
    p_input->stream.pp_areas[i_area_index]->i_size = 0;
    p_input->stream.pp_areas[i_area_index]->i_tell = 0;
    p_input->stream.pp_areas[i_area_index]->i_seek = NO_SEEK;
    p_input->stream.pp_areas[i_area_index]->i_part_nb = 1;
    p_input->stream.pp_areas[i_area_index]->i_part= 0;
    p_input->stream.pp_areas[i_area_index]->i_angle_nb = 1;
    p_input->stream.pp_areas[i_area_index]->i_angle = 0;

    return p_input->stream.pp_areas[i_area_index];
}

/*****************************************************************************
 * input_DelArea: destroy a area descriptor
 *****************************************************************************
 * All ES descriptions referenced in the descriptor will be deleted.
 *****************************************************************************/
void input_DelArea( input_thread_t * p_input, input_area_t * p_area )
{
    int i_area_index;

    ASSERT( p_area );

    intf_DbgMsg("Deleting description for area %d", p_area->i_id );

    /* Find the area in the areas table */
    for( i_area_index = 0; i_area_index < p_input->stream.i_area_nb;
         i_area_index++ )
    {
        if( p_input->stream.pp_areas[i_area_index] == p_area )
            break;
    }

    /* Remove this area from the stream's list of areas */
    p_input->stream.i_area_nb--;

    p_input->stream.pp_areas[i_area_index] =
        p_input->stream.pp_areas[p_input->stream.i_area_nb];
    p_input->stream.pp_areas = realloc( p_input->stream.pp_areas,
                                           p_input->stream.i_area_nb
                                            * sizeof(input_area_t *) );

    if( p_input->stream.i_area_nb && p_input->stream.pp_areas == NULL)
    {
        intf_ErrMsg( "input error: unable to realloc area list"
                     " in input_DelArea" );
    }

    /* Free the description of this area */
    free( p_area );
}


/*****************************************************************************
 * input_FindES: returns a pointer to an ES described by its ID
 *****************************************************************************/
es_descriptor_t * input_FindES( input_thread_t * p_input, u16 i_es_id )
{
    int     i;

    for( i = 0; i < p_input->stream.i_es_number; i++ )
    {
        if( p_input->stream.pp_es[i]->i_id == i_es_id )
        {
            return p_input->stream.pp_es[i];
        }
    }

    return( NULL );
}

/*****************************************************************************
 * input_AddES:
 *****************************************************************************
 * Reserve a slot in the table of ES descriptors for the ES and add it to the
 * list of ES of p_pgrm. If p_pgrm if NULL, then the ES is considered as stand
 * alone (PSI ?)
 *****************************************************************************/
es_descriptor_t * input_AddES( input_thread_t * p_input,
                               pgrm_descriptor_t * p_pgrm, u16 i_es_id,
                               size_t i_data_len )
{
    es_descriptor_t * p_es;

    intf_DbgMsg("Adding description for ES 0x%x", i_es_id);

    p_es = (es_descriptor_t *)malloc( sizeof(es_descriptor_t) );
    if( p_es == NULL )
    {
        intf_ErrMsg( "Unable to allocate memory in input_AddES" );
        return( NULL);
    }
    p_input->stream.i_es_number++;
    p_input->stream.pp_es = realloc( p_input->stream.pp_es,
                                     p_input->stream.i_es_number
                                      * sizeof(es_descriptor_t *) );
    if( p_input->stream.pp_es == NULL )
    {
        intf_ErrMsg( "Unable to realloc memory in input_AddES" );
        return( NULL );
    }
    p_input->stream.pp_es[p_input->stream.i_es_number - 1] = p_es;

    /* Init its values */
    p_es->i_id = i_es_id;
    p_es->psz_desc[0] = '\0';
    p_es->p_pes = NULL;
    p_es->p_decoder_fifo = NULL;
    p_es->b_audio = 0;
    p_es->i_cat = UNKNOWN_ES;

    if( i_data_len )
    {
        p_es->p_demux_data = malloc( i_data_len );
        if( p_es->p_demux_data == NULL )
        {
            intf_ErrMsg( "Unable to allocate memory in input_AddES" );
            return( NULL );
        }
        memset( p_es->p_demux_data, 0, i_data_len );
    }
    else
    {
        p_es->p_demux_data = NULL;
    }

    /* Add this ES to the program definition if one is given */
    if( p_pgrm )
    {
        p_pgrm->i_es_number++;
        p_pgrm->pp_es = realloc( p_pgrm->pp_es,
                                 p_pgrm->i_es_number
                                  * sizeof(es_descriptor_t *) );
        if( p_pgrm->pp_es == NULL )
        {
            intf_ErrMsg( "Unable to realloc memory in input_AddES" );
            return( NULL );
        }
        p_pgrm->pp_es[p_pgrm->i_es_number - 1] = p_es;
        p_es->p_pgrm = p_pgrm;
    }
    else
    {
        p_es->p_pgrm = NULL;
    }

    return p_es;
}

/*****************************************************************************
 * input_DelES:
 *****************************************************************************/
void input_DelES( input_thread_t * p_input, es_descriptor_t * p_es )
{
    int                     i_index, i_es_index;
    pgrm_descriptor_t *     p_pgrm;

    ASSERT( p_es );
    p_pgrm = p_es->p_pgrm;

    /* Kill associated decoder, if any. */
    if( p_es->p_decoder_fifo != NULL )
    {
        input_EndDecoder( p_input, p_es );
    }

    /* Remove this ES from the description of the program if it is associated to
     * one */
    if( p_pgrm )
    {
        for( i_index = 0; i_index < p_pgrm->i_es_number; i_index++ )
        {
            if( p_pgrm->pp_es[i_index] == p_es )
            {
                p_pgrm->i_es_number--;
                p_pgrm->pp_es[i_index] = p_pgrm->pp_es[p_pgrm->i_es_number];
                p_pgrm->pp_es = realloc( p_pgrm->pp_es,
                                         p_pgrm->i_es_number
                                          * sizeof(es_descriptor_t *));
                if( p_pgrm->i_es_number && p_pgrm->pp_es == NULL )
                {
                    intf_ErrMsg( "Unable to realloc memory in input_DelES" );
                }
                break;
            }
        }
    }

    /* Free the demux data */
    if( p_es->p_demux_data != NULL )
    {
        free( p_es->p_demux_data );
    }

    /* Find the ES in the ES table */
    for( i_es_index = 0; i_es_index < p_input->stream.i_es_number;
         i_es_index++ )
    {
        if( p_input->stream.pp_es[i_es_index] == p_es )
            break;
    }

    /* Free the ES */
    free( p_es );
    p_input->stream.i_es_number--;
    p_input->stream.pp_es[i_es_index] =
                    p_input->stream.pp_es[p_input->stream.i_es_number];
    p_input->stream.pp_es = realloc( p_input->stream.pp_es,
                                     p_input->stream.i_es_number
                                      * sizeof(es_descriptor_t *));
    if( p_input->stream.i_es_number && p_input->stream.pp_es == NULL )
    {
        intf_ErrMsg( "Unable to realloc memory in input_DelES" );
    }
    
}

/*****************************************************************************
 * InitDecConfig: initializes a decoder_config_t
 *****************************************************************************/
static int InitDecConfig( input_thread_t * p_input, es_descriptor_t * p_es,
                          decoder_config_t * p_config )
{
    p_config->i_id = p_es->i_id;
    p_config->i_type = p_es->i_type;
    p_config->p_stream_ctrl =
        &p_input->stream.control;

    /* Decoder FIFO */
    if( (p_config->p_decoder_fifo =
            (decoder_fifo_t *)malloc( sizeof(decoder_fifo_t) )) == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        return( -1 );
    }

    vlc_mutex_init(&p_config->p_decoder_fifo->data_lock);
    vlc_cond_init(&p_config->p_decoder_fifo->data_wait);
    p_config->p_decoder_fifo->i_start = p_config->p_decoder_fifo->i_end = 0;
    p_config->p_decoder_fifo->b_die = p_config->p_decoder_fifo->b_error = 0;
    p_config->p_decoder_fifo->p_packets_mgt = p_input->p_method_data;
    p_config->p_decoder_fifo->pf_delete_pes = p_input->pf_delete_pes;
    p_es->p_decoder_fifo = p_config->p_decoder_fifo;

    p_config->pf_init_bit_stream = p_input->pf_init_bit_stream;

    p_input->stream.i_selected_es_number++;

    p_input->stream.pp_selected_es = realloc(
                                       p_input->stream.pp_selected_es,
                                       p_input->stream.i_selected_es_number
                                        * sizeof(es_descriptor_t *) );
    if( p_input->stream.pp_selected_es == NULL )
    {
        intf_ErrMsg( "Unable to realloc memory in input_SelectES" );
        return(-1);
    }
    p_input->stream.pp_selected_es[p_input->stream.i_selected_es_number - 1]
            = p_es;

    return( 0 );
}

/*****************************************************************************
 * GetVdecConfig: returns a valid vdec_config_t
 *****************************************************************************/
static vdec_config_t * GetVdecConfig( input_thread_t * p_input,
                                      es_descriptor_t * p_es )
{
    vdec_config_t *     p_config;

    p_config = (vdec_config_t *)malloc( sizeof(vdec_config_t) );
    if( p_config == NULL )
    {
        intf_ErrMsg( "Unable to allocate memory in GetVdecConfig" );
        return( NULL );
    }
    if( InitDecConfig( p_input, p_es, &p_config->decoder_config ) == -1 )
    {
        free( p_config );
        return( NULL );
    }

    return( p_config );
}

/*****************************************************************************
 * GetAdecConfig: returns a valid adec_config_t
 *****************************************************************************/
static adec_config_t * GetAdecConfig( input_thread_t * p_input,
                                      es_descriptor_t * p_es )
{
    adec_config_t *     p_config;

    p_config = (adec_config_t *)malloc( sizeof(adec_config_t));
    if( p_config == NULL )
    {
        intf_ErrMsg( "Unable to allocate memory in GetAdecConfig" );
        return( NULL );
    }

    if( InitDecConfig( p_input, p_es, &p_config->decoder_config ) == -1 )
    {
        free( p_config );
        return( NULL );
    }

    return( p_config );
}

/*****************************************************************************
 * input_SelectES: selects an ES and spawns the associated decoder
 *****************************************************************************
 * Remember we are still supposed to have stream_lock when entering this
 * function ?
 *****************************************************************************/
/* FIXME */
vlc_thread_t adec_CreateThread( void * );
vlc_thread_t ac3dec_CreateThread( void * );
vlc_thread_t ac3spdif_CreateThread( void * );
vlc_thread_t spdif_CreateThread( void * );
vlc_thread_t vpar_CreateThread( void * );
vlc_thread_t spudec_CreateThread( void * );
vlc_thread_t lpcmdec_CreateThread( void * );

int input_SelectES( input_thread_t * p_input, es_descriptor_t * p_es )
{
    /* FIXME ! */
    decoder_capabilities_t  decoder;
    void *                  p_config;

    if( p_es == NULL )
    {
        intf_ErrMsg( "Nothing to do in input_SelectES" );
        return -1;
    }

#ifdef TRACE_INPUT
    intf_DbgMsg( "Selecting ES 0x%x", p_es->i_id );
#endif

    if( p_es->p_decoder_fifo != NULL )
    {
        intf_ErrMsg( "ES 0x%x is already selected", p_es->i_id );
        return( -1 );
    }

    switch( p_es->i_type )
    {
    case MPEG1_AUDIO_ES:
    case MPEG2_AUDIO_ES:
        if( p_main->b_audio )
        {
            decoder.pf_create_thread = adec_CreateThread;
            p_config = (void *)GetAdecConfig( p_input, p_es );
            p_main->b_ac3 = 0;

            /* Release the lock, not to block the input thread during
             * the creation of the thread. */
            vlc_mutex_unlock( &p_input->stream.stream_lock );
            p_es->thread_id = input_RunDecoder( &decoder, p_config );
            vlc_mutex_lock( &p_input->stream.stream_lock );
        }
        break;

    case MPEG1_VIDEO_ES:
    case MPEG2_VIDEO_ES:
        if( p_main->b_video )
        {
            decoder.pf_create_thread = vpar_CreateThread;
            p_config = (void *)GetVdecConfig( p_input, p_es );

            /* Release the lock, not to block the input thread during
             * the creation of the thread. */
            vlc_mutex_unlock( &p_input->stream.stream_lock );
            p_es->thread_id = input_RunDecoder( &decoder, p_config );
            vlc_mutex_lock( &p_input->stream.stream_lock );
        }
        break;

    case AC3_AUDIO_ES:
        if( p_main->b_audio )
        {
            if( main_GetIntVariable( AOUT_SPDIF_VAR, 0 ) )
            {
                decoder.pf_create_thread = spdif_CreateThread;
            }
            else
            {
                decoder.pf_create_thread = ac3dec_CreateThread;
            }

            p_config = (void *)GetAdecConfig( p_input, p_es );
            p_main->b_ac3 = 1;

            /* Release the lock, not to block the input thread during
             * the creation of the thread. */
            vlc_mutex_unlock( &p_input->stream.stream_lock );
            p_es->thread_id = input_RunDecoder( &decoder, p_config );
            vlc_mutex_lock( &p_input->stream.stream_lock );
        }
        break;
    case LPCM_AUDIO_ES:
        if( p_main->b_audio )
        {
            decoder.pf_create_thread = lpcmdec_CreateThread;
            p_config = (void *)GetAdecConfig( p_input, p_es );
            p_main->b_ac3 = 0;

            /* Release the lock, not to block the input thread during
             * the creation of the thread. */
            vlc_mutex_unlock( &p_input->stream.stream_lock );
            p_es->thread_id = input_RunDecoder( &decoder, p_config );
            vlc_mutex_lock( &p_input->stream.stream_lock );
        }
        break;
    case DVD_SPU_ES:
        if( p_main->b_video )
        {
            decoder.pf_create_thread = spudec_CreateThread;
            p_config = (void *)GetVdecConfig( p_input, p_es );

            /* Release the lock, not to block the input thread during
             * the creation of the thread. */
            vlc_mutex_unlock( &p_input->stream.stream_lock );
            p_es->thread_id = input_RunDecoder( &decoder, p_config );
            vlc_mutex_lock( &p_input->stream.stream_lock );
        }
        break;

    default:
        intf_ErrMsg( "Unknown stream type 0x%x", p_es->i_type );
        return( -1 );
        break;
    }

    if( p_es->thread_id == 0 )
    {
        return( -1 );
    }

    return( 0 );
}

/*****************************************************************************
 * input_UnselectES: removes an ES from the list of selected ES
 *****************************************************************************/
int input_UnselectES( input_thread_t * p_input, es_descriptor_t * p_es )
{

    int     i_index = 0;

    if( p_es == NULL )
    {
        intf_ErrMsg( "Nothing to do in input_UnselectES" );
        return -1;
    }

#ifdef TRACE_INPUT
    intf_DbgMsg( "Unselecting ES 0x%x", p_es->i_id );
#endif

    if( p_es->p_decoder_fifo == NULL )
    {
        intf_ErrMsg( "ES 0x%x is not selected", p_es->i_id );
        return( -1 );
    }

    input_EndDecoder( p_input, p_es );

    if( ( p_es->p_decoder_fifo == NULL ) &&
        ( p_input->stream.i_selected_es_number > 0 ) )
    {
        p_input->stream.i_selected_es_number--;

        while( ( i_index < p_input->stream.i_selected_es_number ) &&
               ( p_input->stream.pp_selected_es[i_index] != p_es ) )
        {
            i_index++;
        }

        p_input->stream.pp_selected_es[i_index] = 
          p_input->stream.pp_selected_es[p_input->stream.i_selected_es_number];

        p_input->stream.pp_selected_es = realloc(
                                           p_input->stream.pp_selected_es,
                                           p_input->stream.i_selected_es_number
                                            * sizeof(es_descriptor_t *) );

        if( p_input->stream.pp_selected_es == NULL )
        {
#ifdef TRACE_INPUT
            intf_DbgMsg( "No more selected ES in input_UnselectES" );
#endif
            return( 1 );
        }
    }

    return( 0 );
}
