/*****************************************************************************
 * input.c : internal management of input streams for the audio output
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: input.c,v 1.4 2002/08/14 00:23:59 massiot Exp $
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
#include <stdlib.h>                            /* calloc(), malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>

#include "audio_output.h"
#include "aout_internal.h"

/*****************************************************************************
 * aout_InputNew : allocate a new input and rework the filter pipeline
 *****************************************************************************/
static aout_input_t * InputNew( aout_instance_t * p_aout,
                                audio_sample_format_t * p_format )
{
    aout_input_t *  p_input = malloc(sizeof(aout_input_t));

    if ( p_input == NULL ) return NULL;

    vlc_mutex_lock( &p_aout->mixer_lock );
    while ( p_aout->b_mixer_active )
    {
        vlc_cond_wait( &p_aout->mixer_signal, &p_aout->mixer_lock );
    }

    if ( p_aout->i_nb_inputs == 0 )
    {
        /* Recreate the output using the new format. */
        if ( aout_OutputNew( p_aout, p_format ) < 0 )
        {
            free( p_input );
            return NULL;
        }
    }
    else
    {
        aout_MixerDelete( p_aout );
    }

    memcpy( &p_input->input, p_format,
            sizeof(audio_sample_format_t) );
    p_input->input.i_bytes_per_sec =
                            aout_FormatToByterate( &p_input->input );

    /* Prepare FIFO. */
    aout_FifoInit( p_aout, &p_input->fifo );
    p_input->p_first_byte_to_mix = NULL;
    p_input->next_packet_date = 0;

    /* Create filters. */
    if ( aout_FiltersCreatePipeline( p_aout, p_input->pp_filters,
                                     &p_input->i_nb_filters, &p_input->input,
                                     &p_aout->mixer.input ) < 0 )
    {
        msg_Err( p_aout, "couldn't set an input pipeline" );

        aout_FifoDestroy( p_aout, &p_input->fifo );

        free( p_input );

        if ( !p_aout->i_nb_inputs )
        {
            aout_OutputDelete( p_aout );
        }
        return NULL;
    }

    p_aout->pp_inputs[p_aout->i_nb_inputs] = p_input;
    p_aout->i_nb_inputs++;

    if ( aout_MixerNew( p_aout ) < 0 )
    {
        p_aout->i_nb_inputs--;
        aout_FiltersDestroyPipeline( p_aout, p_input->pp_filters,
                                     p_input->i_nb_filters );
        aout_FifoDestroy( p_aout, &p_input->fifo );

        free( p_input );

        if ( !p_aout->i_nb_inputs )
        {
            aout_OutputDelete( p_aout );
        }   
    }

    /* Prepare hints for the buffer allocator. */
    p_input->input_alloc.i_alloc_type = AOUT_ALLOC_HEAP;
    p_input->input_alloc.i_bytes_per_sec = -1;

    aout_FiltersHintBuffers( p_aout, p_input->pp_filters,
                             p_input->i_nb_filters,
                             &p_input->input_alloc );

    /* i_bytes_per_sec is still == -1 if no filters */
    p_input->input_alloc.i_bytes_per_sec = __MAX(
                                    p_input->input_alloc.i_bytes_per_sec,
                                    p_input->input.i_bytes_per_sec );
    /* Allocate in the heap, it is more convenient for the decoder. */
    p_input->input_alloc.i_alloc_type = AOUT_ALLOC_HEAP;

    vlc_mutex_unlock( &p_aout->mixer_lock );

    msg_Dbg( p_aout, "input 0x%x created", p_input );
    return p_input;
}

aout_input_t * __aout_InputNew( vlc_object_t * p_this,
                                aout_instance_t ** pp_aout,
                                audio_sample_format_t * p_format )
{
    /* Create an audio output if there is none. */
    *pp_aout = vlc_object_find( p_this, VLC_OBJECT_AOUT, FIND_ANYWHERE );

    if( *pp_aout == NULL )
    {
        msg_Dbg( p_this, "no aout present, spawning one" );

        *pp_aout = aout_NewInstance( p_this );
        /* Everything failed, I'm a loser, I just wanna die */
        if( *pp_aout == NULL )
        {
            return NULL;
        }
    }
    else
    {
        vlc_object_release( *pp_aout );
    }

    return InputNew( *pp_aout, p_format );
}

/*****************************************************************************
 * aout_InputDelete : delete an input
 *****************************************************************************/
void aout_InputDelete( aout_instance_t * p_aout, aout_input_t * p_input )
{
    int i_input;

    msg_Dbg( p_aout, "input 0x%x destroyed", p_input );

    vlc_mutex_lock( &p_aout->mixer_lock );
    while ( p_aout->b_mixer_active )
    {
        vlc_cond_wait( &p_aout->mixer_signal, &p_aout->mixer_lock );
    }

    for ( i_input = 0; i_input < p_aout->i_nb_inputs; i_input++ )
    {
        if ( p_aout->pp_inputs[i_input] == p_input )
        {
            break;
        }
    }

    if ( i_input == p_aout->i_nb_inputs )
    {
        msg_Err( p_aout, "cannot find an input to delete" );
        return;
    }

    /* Remove the input from the list. */
    memmove( &p_aout->pp_inputs[i_input], &p_aout->pp_inputs[i_input + 1],
             (AOUT_MAX_INPUTS - i_input - 1) * sizeof(aout_input_t *) );
    p_aout->i_nb_inputs--;

    vlc_mutex_unlock( &p_aout->mixer_lock );

    aout_FiltersDestroyPipeline( p_aout, p_input->pp_filters,
                                 p_input->i_nb_filters );
    aout_FifoDestroy( p_aout, &p_input->fifo );

    free( p_input );

    if ( !p_aout->i_nb_inputs )
    {
        aout_OutputDelete( p_aout );
        aout_MixerDelete( p_aout );
    }
}

/*****************************************************************************
 * aout_InputPlay : play a buffer
 *****************************************************************************/
void aout_InputPlay( aout_instance_t * p_aout, aout_input_t * p_input,
                     aout_buffer_t * p_buffer )
{
    vlc_mutex_lock( &p_aout->input_lock );
    while( p_aout->b_change_requested )
    {
        vlc_cond_wait( &p_aout->input_signal, &p_aout->input_lock );
    }
    p_aout->i_inputs_active++;
    vlc_mutex_unlock( &p_aout->input_lock );

    aout_FiltersPlay( p_aout, p_input->pp_filters, p_input->i_nb_filters,
                      &p_buffer );

    aout_FifoPush( p_aout, &p_input->fifo, p_buffer );

    vlc_mutex_lock( &p_aout->input_lock );
    p_aout->i_inputs_active--;
    vlc_cond_broadcast( &p_aout->input_signal );
    vlc_mutex_unlock( &p_aout->input_lock );
}
