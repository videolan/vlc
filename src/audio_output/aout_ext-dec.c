/*****************************************************************************
 * aout_ext-dec.c : exported fifo management functions
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: aout_ext-dec.c,v 1.1 2001/05/01 04:18:18 sam Exp $
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
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

#include <stdio.h>                                           /* "intf_msg.h" */
#include <stdlib.h>                            /* calloc(), malloc(), free() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"                             /* mtime_t, mdate(), msleep() */

#include "intf_msg.h"                        /* intf_DbgMsg(), intf_ErrMsg() */

#include "audio_output.h"
#include "aout_common.h"

#include "main.h"

/*****************************************************************************
 * aout_CreateFifo
 *****************************************************************************/
aout_fifo_t * aout_CreateFifo( int i_type, int i_channels, long l_rate,
                               long l_units, long l_frame_size,
                               void *p_buffer )
{
#define P_AOUT p_main->p_aout
    int i_fifo;

    /* Spawn an audio output if there is none */
    if( P_AOUT == NULL )
    {
        P_AOUT = aout_CreateThread( NULL );

        if( P_AOUT == NULL )
        {
            return NULL;
        }
    }

    /* Take the fifos lock */
    vlc_mutex_lock( &P_AOUT->fifos_lock );

    /* Looking for a free fifo structure */
    for( i_fifo = 0; i_fifo < AOUT_MAX_FIFOS; i_fifo++ )
    {
        if( P_AOUT->fifo[i_fifo].i_type == AOUT_EMPTY_FIFO )
        {
            /* Not very clever, but at least we know which fifo it is */
            P_AOUT->fifo[i_fifo].i_fifo = i_fifo;
            break;
        }
    }

    if( i_fifo == AOUT_MAX_FIFOS )
    {
        intf_ErrMsg( "aout error: no fifo available" );
        vlc_mutex_unlock( &P_AOUT->fifos_lock );
        return( NULL );
    }

    /* Initialize the new fifo structure */
    switch ( P_AOUT->fifo[i_fifo].i_type = i_type )
    {
        case AOUT_INTF_MONO_FIFO:
        case AOUT_INTF_STEREO_FIFO:
            P_AOUT->fifo[i_fifo].b_die = 0;

            P_AOUT->fifo[i_fifo].i_channels = i_channels;
            P_AOUT->fifo[i_fifo].b_stereo = ( i_channels == 2 );
            P_AOUT->fifo[i_fifo].l_rate = l_rate;

            P_AOUT->fifo[i_fifo].buffer = p_buffer;

            P_AOUT->fifo[i_fifo].l_unit = 0;
            InitializeIncrement( &P_AOUT->fifo[i_fifo].unit_increment,
                                 l_rate, P_AOUT->l_rate );
            P_AOUT->fifo[i_fifo].l_units = l_units;
            break;

        case AOUT_ADEC_MONO_FIFO:
        case AOUT_ADEC_STEREO_FIFO:
            P_AOUT->fifo[i_fifo].b_die = 0;

            P_AOUT->fifo[i_fifo].i_channels = i_channels;
            P_AOUT->fifo[i_fifo].b_stereo = ( i_channels == 2 );
            P_AOUT->fifo[i_fifo].l_rate = l_rate;

            P_AOUT->fifo[i_fifo].l_frame_size = l_frame_size;
            /* Allocate the memory needed to store the audio frames. As the
             * fifo is a rotative fifo, we must be able to find out whether
             * the fifo is full or empty, that's why we must in fact allocate
             * memory for (AOUT_FIFO_SIZE+1) audio frames. */
            P_AOUT->fifo[i_fifo].buffer = malloc( sizeof(s16) *
                                   ( AOUT_FIFO_SIZE + 1 ) * l_frame_size );
            if ( P_AOUT->fifo[i_fifo].buffer == NULL )
            {
                intf_ErrMsg( "aout error: cannot create frame buffer" );
                P_AOUT->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO;
                vlc_mutex_unlock( &P_AOUT->fifos_lock );
                return( NULL );
            }

            /* Allocate the memory needed to store the dates of the frames */
            P_AOUT->fifo[i_fifo].date =
                           malloc( sizeof(mtime_t) * ( AOUT_FIFO_SIZE +  1) );

            if ( P_AOUT->fifo[i_fifo].date == NULL )
            {
                intf_ErrMsg( "aout error: cannot create date buffer");
                free( P_AOUT->fifo[i_fifo].buffer );
                P_AOUT->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO;
                vlc_mutex_unlock( &P_AOUT->fifos_lock );
                return( NULL );
            }

            /* Set the fifo's buffer as empty (the first frame that is to be
             * played is also the first frame that is not to be played) */
            P_AOUT->fifo[i_fifo].l_start_frame = 0;
            /* P_AOUT->fifo[i_fifo].l_next_frame = 0; */
            P_AOUT->fifo[i_fifo].l_end_frame = 0;

            /* Waiting for the audio decoder to compute enough frames to work
             * out the fifo's current rate (as soon as the decoder has decoded
             * enough frames, the members of the fifo structure that are not
             * initialized now will be calculated) */
            P_AOUT->fifo[i_fifo].b_start_frame = 0;
            P_AOUT->fifo[i_fifo].b_next_frame = 0;
            break;

        default:
            intf_ErrMsg( "aout error: unknown fifo type 0x%x",
                         P_AOUT->fifo[i_fifo].i_type );
            P_AOUT->fifo[i_fifo].i_type = AOUT_EMPTY_FIFO;
            vlc_mutex_unlock( &P_AOUT->fifos_lock );
            return( NULL );
    }

    /* Release the fifos lock */
    vlc_mutex_unlock( &P_AOUT->fifos_lock );

    intf_WarnMsg( 2, "aout info: fifo #%i allocated, %i channels, rate %li",
                  P_AOUT->fifo[i_fifo].i_fifo, P_AOUT->fifo[i_fifo].i_channels,
                  P_AOUT->fifo[i_fifo].l_rate );

    /* Return the pointer to the fifo structure */
    return( &P_AOUT->fifo[i_fifo] );
#undef P_AOUT
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

        case AOUT_INTF_MONO_FIFO:
        case AOUT_INTF_STEREO_FIFO:

            free( p_fifo->buffer );
            p_fifo->i_type = AOUT_EMPTY_FIFO;

            break;

        case AOUT_ADEC_MONO_FIFO:
        case AOUT_ADEC_STEREO_FIFO:

            free( p_fifo->buffer );
            free( p_fifo->date );
            p_fifo->i_type = AOUT_EMPTY_FIFO;

            break;

        default:

            break;
    }
}

