/*****************************************************************************
 * input_programs.c: es_descriptor_t, pgrm_descriptor_t management
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: input_programs.c,v 1.85 2002/05/13 21:55:30 fenrir Exp $
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
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input_ext-plugins.h"

/*
 * NOTICE : all of these functions expect you to have taken the lock on
 * p_input->stream.lock
 */

/*****************************************************************************
 * input_InitStream: init the stream descriptor of the given input
 *****************************************************************************/
int input_InitStream( input_thread_t * p_input, size_t i_data_len )
{

    p_input->stream.i_stream_id = 0;

    /* initialized to 0 since we don't give the signal to the interface
     * before the end of input initialization */
    p_input->stream.b_changed = 0;
    p_input->stream.pp_es = NULL;
    p_input->stream.pp_selected_es = NULL;
    p_input->stream.p_removed_es = NULL;
    p_input->stream.p_newly_selected_es = NULL;
    p_input->stream.pp_programs = NULL;
    p_input->stream.p_selected_program = NULL;
    p_input->stream.p_new_program = NULL;
    
    if( i_data_len )
    {
        if ( (p_input->stream.p_demux_data = malloc( i_data_len )) == NULL )
        {
            intf_ErrMsg( "Unable to allocate memory in input_InitStream");
            return 1;
        }
        memset( p_input->stream.p_demux_data, 0, i_data_len );
    }
    else
    {
        p_input->stream.p_demux_data = NULL;
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

    /* Free selected ES */
    if( p_input->stream.pp_selected_es != NULL )
    {
        free( p_input->stream.pp_selected_es );
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
    else
    {
        p_input->stream.pp_programs[i_pgrm_index]->p_demux_data = NULL;
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

    return p_input->stream.pp_areas[i_area_index];
}

/*****************************************************************************
 * input_SetProgram: changes the current program
 *****************************************************************************/
int input_SetProgram( input_thread_t * p_input, pgrm_descriptor_t * p_new_prg )
{
    int i_es_index;
    int i_required_audio_es;
    int i_required_spu_es;
    int i_audio_es = 0;
    int i_spu_es = 0;

    if ( p_input->stream.p_selected_program )
    {
        for ( i_es_index = 1 ; /* 0 should be the PMT */
                i_es_index < p_input->stream.p_selected_program->
                i_es_number ;
                i_es_index ++ )
        {
#define p_es p_input->stream.p_selected_program->pp_es[i_es_index]
            if ( p_es->p_decoder_fifo ) /* if the ES was selected */
            {
                input_UnselectES( p_input , p_es );
            }
#undef p_es
        }
    }
    /* Get the number of the required audio stream */
    if( p_main->b_audio )
    {
        /* Default is the first one */
        i_required_audio_es = config_GetIntVariable( "audio-channel" );
        if( i_required_audio_es < 0 )
        {
            i_required_audio_es = 1;
        }
    }
    else
    {
        i_required_audio_es = 0;
    }

    /* Same thing for subtitles */
    if( p_main->b_video )
    {
        /* for spu, default is none */
        i_required_spu_es = config_GetIntVariable( "spu-channel" );
        if( i_required_spu_es < 0 )
        {
            i_required_spu_es = 0;
        }
    }
    else
    {
        i_required_spu_es = 0;
    }

    for (i_es_index = 0 ; i_es_index < p_new_prg->i_es_number ; i_es_index ++ )
    {
        switch( p_new_prg->pp_es[i_es_index]->i_cat )
        {
            case VIDEO_ES:
                intf_WarnMsg( 4, "Selecting ES %x",
                            p_new_prg->pp_es[i_es_index]->i_id );
                input_SelectES( p_input, p_new_prg->pp_es[i_es_index] );
                break;
            case AUDIO_ES:
                i_audio_es += 1;
                if( i_audio_es <= i_required_audio_es )
                {
                    intf_WarnMsg( 4, "Selecting ES %x",
                                p_new_prg->pp_es[i_es_index]->i_id );
                    input_SelectES( p_input, p_new_prg->pp_es[i_es_index]);
                }
                break;
            /* Not sure this one is fully specification-compliant */
            case SPU_ES :
                i_spu_es += 1;
                if( i_spu_es <= i_required_spu_es )
                {
                    intf_WarnMsg( 4, "Selecting ES %x",
                                p_new_prg->pp_es[i_es_index]->i_id );
                    input_SelectES( p_input, p_new_prg->pp_es[i_es_index] );
                }
            break;
            default :
                intf_WarnMsg( 2, "ES %x has unknown type",
                            p_new_prg->pp_es[i_es_index]->i_id );
                break;
        }

    }


    p_input->stream.p_selected_program = p_new_prg;

    return( 0 );
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
    p_es->i_demux_fd = 0;

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
 * input_SelectES: selects an ES and spawns the associated decoder
 *****************************************************************************
 * Remember we are still supposed to have stream_lock when entering this
 * function ?
 *****************************************************************************/
int input_SelectES( input_thread_t * p_input, es_descriptor_t * p_es )
{
    if( p_es == NULL )
    {
        intf_ErrMsg( "input error: nothing to do in input_SelectES" );
        return -1;
    }

    intf_WarnMsg( 4, "input: selecting ES 0x%x", p_es->i_id );

    if( p_es->p_decoder_fifo != NULL )
    {
        intf_ErrMsg( "ES 0x%x is already selected", p_es->i_id );
        return( -1 );
    }

    p_es->thread_id = 0;

    switch( p_es->i_type )
    {
    case AC3_AUDIO_ES:
    case MPEG1_AUDIO_ES:
    case MPEG2_AUDIO_ES:
    case LPCM_AUDIO_ES:
        if( p_main->b_audio )
        {
            /* Release the lock, not to block the input thread during
             * the creation of the thread. */
            vlc_mutex_unlock( &p_input->stream.stream_lock );
            p_es->thread_id = input_RunDecoder( p_input, p_es );
            vlc_mutex_lock( &p_input->stream.stream_lock );
        }
        break;

    case MPEG1_VIDEO_ES:
    case MPEG2_VIDEO_ES:
    case MPEG4_VIDEO_ES:
    case MSMPEG4v1_VIDEO_ES:
    case MSMPEG4v2_VIDEO_ES:
    case MSMPEG4v3_VIDEO_ES:
    case DVD_SPU_ES:
        if( p_main->b_video )
        {
            /* Release the lock, not to block the input thread during
             * the creation of the thread. */
            vlc_mutex_unlock( &p_input->stream.stream_lock );
            p_es->thread_id = input_RunDecoder( p_input, p_es );
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

    intf_WarnMsg( 4, "input: unselecting ES 0x%x", p_es->i_id );

    if( p_es->p_decoder_fifo == NULL )
    {
        intf_ErrMsg( "ES 0x%x is not selected", p_es->i_id );
        return( -1 );
    }

    input_EndDecoder( p_input, p_es );
    p_es->p_pes = NULL;

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
            intf_WarnMsg( 4, "input: no more selected ES in input_UnselectES" );
            return( 1 );
        }
    }

    return( 0 );
}
