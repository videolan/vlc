/*****************************************************************************
 * aout_s16.c: 16 bit signed audio output functions
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
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
#include <stdio.h>                                           /* "intf_msg.h" */
#include <stdlib.h>                            /* calloc(), malloc(), free() */
#include <string.h>

#include <videolan/vlc.h>

#include "audio_output.h"
#include "aout_common.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void S16Play( aout_thread_t * p_aout, aout_fifo_t * p_fifo );

/*****************************************************************************
 * Functions
 *****************************************************************************/
void aout_S16MonoThread( aout_thread_t * p_aout )
{
    int  i_fifo;
    long l_buffer, l_buffer_limit, l_bytes;

    /* As the s32_buffer was created with calloc(), we don't have to set this
     * memory to zero and we can immediately jump into the thread's loop */
    while ( ! p_aout->b_die )
    {
        vlc_mutex_lock( &p_aout->fifos_lock );

        for ( i_fifo = 0; i_fifo < AOUT_MAX_FIFOS; i_fifo++ )
        {
            if( p_aout->fifo[i_fifo].b_die )
            {
                aout_FreeFifo( &p_aout->fifo[i_fifo] );
            }
            else
            {
                S16Play( p_aout, &p_aout->fifo[i_fifo] );
            }
        }

        vlc_mutex_unlock( &p_aout->fifos_lock );

        l_buffer_limit = p_aout->l_units; /* p_aout->b_stereo == 0 */

        for ( l_buffer = 0; l_buffer < l_buffer_limit; l_buffer++ )
        {
            ((s16 *)p_aout->buffer)[l_buffer] =
                     (s16)( ( p_aout->s32_buffer[l_buffer] / AOUT_MAX_FIFOS )
                            * p_aout->i_volume / 256 ) ;
            p_aout->s32_buffer[l_buffer] = 0;
        }

        l_bytes = p_aout->pf_getbufinfo( p_aout, l_buffer_limit );

        /* sizeof(s16) << (p_aout->b_stereo) == 2 */
        p_aout->date = mdate() + ((((mtime_t)((l_bytes + 2 * p_aout->i_latency) / 2)) * 1000000)
                                   / ((mtime_t)p_aout->l_rate))
                        + p_main->i_desync;
        p_aout->pf_play( p_aout, (byte_t *)p_aout->buffer,
                         l_buffer_limit * sizeof(s16) );

        if ( l_bytes > (l_buffer_limit * sizeof(s16)) )
        {
            msleep( p_aout->l_msleep );
        }
    }

    vlc_mutex_lock( &p_aout->fifos_lock );

    for ( i_fifo = 0; i_fifo < AOUT_MAX_FIFOS; i_fifo++ )
    {
        aout_FreeFifo( &p_aout->fifo[i_fifo] );
    }

    vlc_mutex_unlock( &p_aout->fifos_lock );

}

void aout_S16StereoThread( aout_thread_t * p_aout )
{
    int  i_fifo;
    long l_buffer, l_buffer_limit, l_bytes;

    /* As the s32_buffer was created with calloc(), we don't have to set this
     * memory to zero and we can immediately jump into the thread's loop */
    while ( ! p_aout->b_die )
    {
        vlc_mutex_lock( &p_aout->fifos_lock );

        for ( i_fifo = 0; i_fifo < AOUT_MAX_FIFOS; i_fifo++ )
        {
            if( p_aout->fifo[i_fifo].b_die )
            {
                aout_FreeFifo( &p_aout->fifo[i_fifo] );
            }
            else
            {
                S16Play( p_aout, &p_aout->fifo[i_fifo] );
            }
        }

        vlc_mutex_unlock( &p_aout->fifos_lock );

        l_buffer_limit = p_aout->l_units << 1; /* p_aout->b_stereo == 1 */

        for ( l_buffer = 0; l_buffer < l_buffer_limit; l_buffer++ )
        {
            ((s16 *)p_aout->buffer)[l_buffer] =
                     (s16)( ( p_aout->s32_buffer[l_buffer] / AOUT_MAX_FIFOS )
                            * p_aout->i_volume / 256 ) ;
            p_aout->s32_buffer[l_buffer] = 0;
        }

        l_bytes = p_aout->pf_getbufinfo( p_aout, l_buffer_limit );

        /* sizeof(s16) << (p_aout->b_stereo) == 4 */
        p_aout->date = mdate() + ((((mtime_t)((l_bytes + 4 * p_aout->i_latency) / 4)) * 1000000)
                                   / ((mtime_t)p_aout->l_rate))
                        + p_main->i_desync;
        p_aout->pf_play( p_aout, (byte_t *)p_aout->buffer,
                         l_buffer_limit * sizeof(s16) );

        if ( l_bytes > (l_buffer_limit * sizeof(s16)) )
        {
            msleep( p_aout->l_msleep );
        }
    }

    vlc_mutex_lock( &p_aout->fifos_lock );

    for ( i_fifo = 0; i_fifo < AOUT_MAX_FIFOS; i_fifo++ )
    {
        aout_FreeFifo( &p_aout->fifo[i_fifo] );
    }

    vlc_mutex_unlock( &p_aout->fifos_lock );
}

/* Following functions are local */

static void S16Play( aout_thread_t * p_aout, aout_fifo_t * p_fifo )
{
    long l_buffer = 0;
    long l_buffer_limit, l_units;

    switch ( p_fifo->i_type )
    {
    case AOUT_EMPTY_FIFO:

        break;

    case AOUT_INTF_MONO_FIFO:

        if ( p_fifo->l_units > p_aout->l_units )
        {
            /* p_aout->b_stereo == 1 */
            while ( l_buffer < (p_aout->l_units << 1) )
            {
                p_aout->s32_buffer[l_buffer++] +=
                    (s32)( ((s16 *)p_fifo->buffer)[p_fifo->l_unit] );
                p_aout->s32_buffer[l_buffer++] +=
                    (s32)( ((s16 *)p_fifo->buffer)[p_fifo->l_unit] );
                UPDATE_INCREMENT( p_fifo->unit_increment, p_fifo->l_unit )
            }
            p_fifo->l_units -= p_aout->l_units;
        }
        else /* p_fifo->l_units <= p_aout->l_units */
        {
            /* p_aout->b_stereo == 1 */
            while ( l_buffer < (p_fifo->l_units << 1) )
            {
                p_aout->s32_buffer[l_buffer++] +=
                    (s32)( ((s16 *)p_fifo->buffer)[p_fifo->l_unit] );
                p_aout->s32_buffer[l_buffer++] +=
                    (s32)( ((s16 *)p_fifo->buffer)[p_fifo->l_unit] );
                UPDATE_INCREMENT( p_fifo->unit_increment, p_fifo->l_unit )
            }
            free( p_fifo->buffer ); /* !! */
            p_fifo->i_type = AOUT_EMPTY_FIFO; /* !! */
        }
        break;

    case AOUT_INTF_STEREO_FIFO:

        if ( p_fifo->l_units > p_aout->l_units )
        {
            /* p_aout->b_stereo == 1 */
            while ( l_buffer < (p_aout->l_units << 1) )
            {
                p_aout->s32_buffer[l_buffer++] +=
                    (s32)( ((s16 *)p_fifo->buffer)[2*p_fifo->l_unit] );
                p_aout->s32_buffer[l_buffer++] +=
                    (s32)( ((s16 *)p_fifo->buffer)[2*p_fifo->l_unit+1] );
                UPDATE_INCREMENT( p_fifo->unit_increment, p_fifo->l_unit )
            }
            p_fifo->l_units -= p_aout->l_units;
        }
        else /* p_fifo->l_units <= p_aout->l_units */
        {
            /* p_aout->b_stereo == 1 */
            while ( l_buffer < (p_fifo->l_units << 1) )
            {
                p_aout->s32_buffer[l_buffer++] +=
                    (s32)( ((s16 *)p_fifo->buffer)[2*p_fifo->l_unit] );
                p_aout->s32_buffer[l_buffer++] +=
                    (s32)( ((s16 *)p_fifo->buffer)[2*p_fifo->l_unit+1] );
                UPDATE_INCREMENT( p_fifo->unit_increment, p_fifo->l_unit )
            }
            free( p_fifo->buffer );
            p_fifo->i_type = AOUT_EMPTY_FIFO;
        }
        break;

    case AOUT_ADEC_MONO_FIFO:
    case AOUT_ADEC_STEREO_FIFO:

        l_units = p_aout->l_units;
        while ( l_units > 0 )
        {
            if( !p_fifo->b_next_frame )
            {
                if( NextFrame(p_aout, p_fifo, p_aout->date + ((((mtime_t)(l_buffer >> 1)) * 1000000) / ((mtime_t)p_aout->l_rate))) )
                {
                    break;
                }
            }
            l_buffer_limit = p_aout->l_units << p_aout->b_stereo;

            while ( l_buffer < l_buffer_limit )
            {
                if( p_aout->b_stereo )
                {
                    p_aout->s32_buffer[l_buffer++] +=
                        (s32)( ((s16 *)p_fifo->buffer)[2*p_fifo->l_unit] );
                    p_aout->s32_buffer[l_buffer++] +=
                        (s32)( ((s16 *)p_fifo->buffer)[2*p_fifo->l_unit+1] );
                }
                else
                {
                    p_aout->s32_buffer[l_buffer++] +=
                        (s32)( ((s16 *)p_fifo->buffer)[p_fifo->l_unit] );
                }

                UPDATE_INCREMENT( p_fifo->unit_increment, p_fifo->l_unit )
                if ( p_fifo->l_unit >= 
                     ((AOUT_FIFO_SIZE + 1) * (p_fifo->l_frame_size >> p_fifo->b_stereo)) )
                {
                    p_fifo->l_unit -= 
                        ((AOUT_FIFO_SIZE + 1) * (p_fifo->l_frame_size >> p_fifo->b_stereo));
                }
            }
 
            if ( p_fifo->l_units > l_units )
            {
               p_fifo->l_units -= l_units;
                break;
            }
            else /* p_fifo->l_units <= l_units */
            {
                l_units -= p_fifo->l_units;

                vlc_mutex_lock( &p_fifo->data_lock );
                p_fifo->l_start_frame = p_fifo->l_next_frame;
                vlc_cond_signal( &p_fifo->data_wait );
                vlc_mutex_unlock( &p_fifo->data_lock );

                /* p_fifo->b_start_frame = 1; */
                p_fifo->l_next_frame += 1;
                p_fifo->l_next_frame &= AOUT_FIFO_SIZE;
                p_fifo->b_next_frame = 0;
            }
        }
        break;

    default:

        intf_DbgMsg("aout debug: unknown fifo type (%i)", p_fifo->i_type);
        break;
    }
}

