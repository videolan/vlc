/*****************************************************************************
 * aout_ext-dec.c : exported fifo management functions
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: aout_ext-dec.c,v 1.17 2002/06/01 18:04:49 sam Exp $
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
#include <stdlib.h>                            /* calloc(), malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>

#include "audio_output.h"

/*****************************************************************************
 * aout_CreateFifo
 *****************************************************************************/
aout_fifo_t * __aout_CreateFifo( vlc_object_t *p_this, int i_format,
                                 int i_channels, int i_rate, int i_frame_size,
                                 void *p_buffer )
{
    aout_thread_t *p_aout;
    aout_fifo_t   *p_fifo = NULL;
    int i_index;

    /* Spawn an audio output if there is none */
    p_aout = vlc_object_find( p_this->p_vlc, VLC_OBJECT_AOUT, FIND_CHILD );

    if( p_aout )
    {
        if( p_aout->fifo[0].i_format != i_format )
        {
            msg_Dbg( p_this, "changing aout type" );
            vlc_object_unlink_all( p_aout );
            vlc_object_release( p_aout );
            aout_DestroyThread( p_aout );
            p_aout = NULL;
        }
    }

    if( p_aout == NULL )
    {
        msg_Dbg( p_this, "no aout present, spawning one" );

        p_aout = aout_CreateThread( p_this, i_channels, i_rate );
        /* Everything failed */
        if( p_aout == NULL )
        {
            return NULL;
        }
    }

    /* temporary hack to switch output type (mainly for spdif)
     * FIXME: to be adapted when several output are available */
    /* Take the fifos lock */
    vlc_mutex_lock( &p_aout->fifos_lock );

    /* Look for a free fifo structure */
    for( i_index = 0; i_index < AOUT_MAX_FIFOS; i_index++ )
    {
        if( p_aout->fifo[i_index].i_format == AOUT_FIFO_NONE )
        {
            p_fifo = &p_aout->fifo[i_index];
            p_fifo->i_fifo = i_index;
            break;
        }
    }

    if( p_fifo == NULL )
    {
        msg_Err( p_aout, "no fifo available" );
        vlc_mutex_unlock( &p_aout->fifos_lock );
        return NULL;
    }

    /* Initialize the new fifo structure */
    switch ( p_fifo->i_format = i_format )
    {
        case AOUT_FIFO_PCM:
        case AOUT_FIFO_SPDIF:
            p_fifo->b_die = 0;

            p_fifo->i_channels = i_channels;
            p_fifo->i_rate = i_rate;
            p_fifo->i_frame_size = i_frame_size;

            p_fifo->i_unit_limit = (AOUT_FIFO_SIZE + 1)
                                     * (i_frame_size / i_channels);

            /* Allocate the memory needed to store the audio frames and their
             * dates. As the fifo is a rotative fifo, we must be able to find
             * out whether the fifo is full or empty, that's why we must in
             * fact allocate memory for (AOUT_FIFO_SIZE+1) audio frames. */
            p_fifo->date = malloc( ( sizeof(s16) * i_frame_size 
                                      + sizeof(mtime_t) )
                                   * ( AOUT_FIFO_SIZE + 1 ) );
            if ( p_fifo->date == NULL )
            {
                msg_Err( p_aout, "out of memory" );
                p_fifo->i_format = AOUT_FIFO_NONE;
                vlc_mutex_unlock( &p_aout->fifos_lock );
                return NULL;
            }

            p_fifo->buffer = (u8 *)p_fifo->date + sizeof(mtime_t)
                                                     * ( AOUT_FIFO_SIZE + 1 );

            /* Set the fifo's buffer as empty (the first frame that is to be
             * played is also the first frame that is not to be played) */
            p_fifo->i_start_frame = 0;
            /* p_fifo->i_next_frame = 0; */
            p_fifo->i_end_frame = 0;

            /* Waiting for the audio decoder to compute enough frames to work
             * out the fifo's current rate (as soon as the decoder has decoded
             * enough frames, the members of the fifo structure that are not
             * initialized now will be calculated) */
            p_fifo->b_start_frame = 0;
            p_fifo->b_next_frame = 0;
            break;

        default:
            msg_Err( p_aout, "unknown fifo type 0x%x", p_fifo->i_format );
            p_fifo->i_format = AOUT_FIFO_NONE;
            vlc_mutex_unlock( &p_aout->fifos_lock );
            return NULL;
    }

    /* Release the fifos lock */
    vlc_mutex_unlock( &p_aout->fifos_lock );

    msg_Dbg( p_aout, "fifo #%i allocated, %i channels, rate %li, "
                     "frame size %i", p_fifo->i_fifo, p_fifo->i_channels,
                     p_fifo->i_rate, p_fifo->i_frame_size );

    /* Return the pointer to the fifo structure */
    return p_fifo;
}

/*****************************************************************************
 * aout_DestroyFifo
 *****************************************************************************/
void aout_DestroyFifo( aout_fifo_t * p_fifo )
{
//X    intf_Warn( p_fifo, 2, "fifo #%i destroyed", p_fifo->i_fifo );

    vlc_mutex_lock( &p_fifo->data_lock );
    p_fifo->b_die = 1;
    vlc_mutex_unlock( &p_fifo->data_lock );
}

/*****************************************************************************
 * aout_FreeFifo
 *****************************************************************************/
void aout_FreeFifo( aout_fifo_t * p_fifo )
{
    switch ( p_fifo->i_format )
    {
        case AOUT_FIFO_NONE:

            break;

        case AOUT_FIFO_PCM:
        case AOUT_FIFO_SPDIF:

            free( p_fifo->date );
            p_fifo->i_format = AOUT_FIFO_NONE;

            break;

        default:

            break;
    }
}

