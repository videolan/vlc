/*****************************************************************************
 * aout_ext-dec.c : exported fifo management functions
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: aout_ext-dec.c,v 1.12 2002/02/15 13:32:54 sam Exp $
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
 *          Cyril Deguet <asmax@via.ecp.fr>
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
#include <stdio.h>                                           /* "intf_msg.h" */
#include <stdlib.h>                            /* calloc(), malloc(), free() */
#include <string.h>

#include <videolan/vlc.h>

#include "audio_output.h"
#include "aout_common.h"

/*****************************************************************************
 * aout_CreateFifo
 *****************************************************************************/
aout_fifo_t * aout_CreateFifo( int i_type, int i_channels, long l_rate,
                               long l_units, long l_frame_size,
                               void *p_buffer )
{
    aout_thread_t *p_aout;
    int i_fifo;

    intf_WarnMsg( 3, "aout info: fifo type %d, %d channels, rate %d, "
                     "%d units, frame size %d",
                     i_type, i_channels, l_rate, l_units, l_frame_size );

    /* Spawn an audio output if there is none */
    vlc_mutex_lock( &p_aout_bank->lock );

    if( p_aout_bank->i_count == 0 )
    {
        intf_WarnMsg( 1, "aout: no aout present, spawning one" );

        p_aout = aout_CreateThread( NULL, i_channels, l_rate );

        /* Everything failed */
        if( p_aout == NULL )
        {
            vlc_mutex_unlock( &p_aout_bank->lock );
            return NULL;
        }

        p_aout_bank->pp_aout[ p_aout_bank->i_count ] = p_aout;
        p_aout_bank->i_count++;
    }
    /* temporary hack to switch output type (mainly for spdif)
     * FIXME: to be adapted when several output are available */
    else if( p_aout_bank->pp_aout[0]->fifo[0].i_type != i_type )
    {
        intf_WarnMsg( 1, "aout: changing aout type" );

        aout_DestroyThread( p_aout_bank->pp_aout[0], NULL );

        p_aout = aout_CreateThread( NULL, i_channels, l_rate );

        /* Everything failed */
        if( p_aout == NULL )
        {
            vlc_mutex_unlock( &p_aout_bank->lock );
            return NULL;
        }

        p_aout_bank->pp_aout[0] = p_aout;

    }
    else
    {
        /* Take the first audio output FIXME: take the best one */
        p_aout = p_aout_bank->pp_aout[ 0 ];
    }

    /* Take the fifos lock */
    vlc_mutex_lock( &p_aout->fifos_lock );

    /* Looking for a free fifo structure */
    for( i_fifo = 0; i_fifo < AOUT_MAX_FIFOS; i_fifo++ )
    {
        if( p_aout->fifo[i_fifo].i_type == AOUT_EMPTY_FIFO )
        {
            /* Not very clever, but at least we know which fifo it is */
            p_aout->fifo[i_fifo].i_fifo = i_fifo;
            break;
        }
    }

    if( i_fifo == AOUT_MAX_FIFOS )
    {
        intf_ErrMsg( "aout error: no fifo available" );
        vlc_mutex_unlock( &p_aout->fifos_lock );
        vlc_mutex_unlock( &p_aout_bank->lock );
        return( NULL );
    }

    /* Initialize the new fifo structure */
    switch ( p_aout->fifo[i_fifo].i_type = i_type )
    {
        case AOUT_ADEC_MONO_FIFO:
        case AOUT_ADEC_STEREO_FIFO:
        case AOUT_ADEC_SPDIF_FIFO:
            p_aout->fifo[i_fifo].b_die = 0;

            p_aout->fifo[i_fifo].i_channels = i_channels;
            p_aout->fifo[i_fifo].b_stereo = ( i_channels == 2 );
            p_aout->fifo[i_fifo].l_rate = l_rate;

            p_aout->fifo[i_fifo].l_frame_size = l_frame_size;
            /* Allocate the memory needed to store the audio frames. As the
             * fifo is a rotative fifo, we must be able to find out whether
             * the fifo is full or empty, that's why we must in fact allocate
             * memory for (AOUT_FIFO_SIZE+1) audio frames. */
            p_aout->fifo[i_fifo].buffer = malloc( sizeof(s16) *
                                   ( AOUT_FIFO_SIZE + 1 ) * l_frame_size );
            if ( p_aout->fifo[i_fifo].buffer == NULL )
            {
                intf_ErrMsg( "aout error: cannot create frame buffer" );
                p_aout->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO;
                vlc_mutex_unlock( &p_aout->fifos_lock );
                vlc_mutex_unlock( &p_aout_bank->lock );
                return( NULL );
            }

            /* Allocate the memory needed to store the dates of the frames */
            p_aout->fifo[i_fifo].date =
                           malloc( sizeof(mtime_t) * ( AOUT_FIFO_SIZE +  1) );

            if ( p_aout->fifo[i_fifo].date == NULL )
            {
                intf_ErrMsg( "aout error: cannot create date buffer");
                free( p_aout->fifo[i_fifo].buffer );
                p_aout->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO;
                vlc_mutex_unlock( &p_aout->fifos_lock );
                vlc_mutex_unlock( &p_aout_bank->lock );
                return( NULL );
            }

            /* Set the fifo's buffer as empty (the first frame that is to be
             * played is also the first frame that is not to be played) */
            p_aout->fifo[i_fifo].l_start_frame = 0;
            /* p_aout->fifo[i_fifo].l_next_frame = 0; */
            p_aout->fifo[i_fifo].l_end_frame = 0;

            /* Waiting for the audio decoder to compute enough frames to work
             * out the fifo's current rate (as soon as the decoder has decoded
             * enough frames, the members of the fifo structure that are not
             * initialized now will be calculated) */
            p_aout->fifo[i_fifo].b_start_frame = 0;
            p_aout->fifo[i_fifo].b_next_frame = 0;
            break;

        default:
            intf_ErrMsg( "aout error: unknown fifo type 0x%x",
                         p_aout->fifo[i_fifo].i_type );
            p_aout->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO;
            vlc_mutex_unlock( &p_aout->fifos_lock );
            vlc_mutex_unlock( &p_aout_bank->lock );
            return( NULL );
    }

    /* Release the fifos lock */
    vlc_mutex_unlock( &p_aout->fifos_lock );
    vlc_mutex_unlock( &p_aout_bank->lock );

    intf_WarnMsg( 2, "aout info: fifo #%i allocated, %i channels, rate %li",
                  p_aout->fifo[i_fifo].i_fifo, p_aout->fifo[i_fifo].i_channels,
                  p_aout->fifo[i_fifo].l_rate );

    /* Return the pointer to the fifo structure */
    return( &p_aout->fifo[i_fifo] );
}

/*****************************************************************************
 * aout_DestroyFifo
 *****************************************************************************/
void aout_DestroyFifo( aout_fifo_t * p_fifo )
{
    intf_WarnMsg( 2, "aout info: fifo #%i destroyed", p_fifo->i_fifo );

    p_fifo->b_die = 1;
}

/*****************************************************************************
 * aout_FreeFifo
 *****************************************************************************/
void aout_FreeFifo( aout_fifo_t * p_fifo )
{
    switch ( p_fifo->i_type )
    {
        case AOUT_EMPTY_FIFO:

            break;

        case AOUT_ADEC_MONO_FIFO:
        case AOUT_ADEC_STEREO_FIFO:
        case AOUT_ADEC_SPDIF_FIFO:

            free( p_fifo->buffer );
            free( p_fifo->date );
            p_fifo->i_type = AOUT_EMPTY_FIFO;

            break;

        default:

            break;
    }
}

