/*****************************************************************************
 * aout_pcm.c: PCM audio output functions
 *****************************************************************************
 * Copyright (C) 1999-2002 VideoLAN
 * $Id: aout_pcm.c,v 1.8 2002/06/01 12:32:01 sam Exp $
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
#include "aout_pcm.h"

/* Biggest difference allowed between scheduled playing date and actual date 
   (in microseconds) */
#define MAX_DELTA 50000

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void FillBuffer( aout_thread_t * p_aout, aout_fifo_t * p_fifo );
static int  NextFrame ( aout_thread_t * p_aout, aout_fifo_t * p_fifo,
                        mtime_t aout_date );

 /*****************************************************************************
 * Functions
 *****************************************************************************/
void aout_PCMThread( aout_thread_t * p_aout )
{
    int i_fifo;
    int i_buffer, i_buffer_limit, i_units = 0;

#if defined(WIN32)
    if( !SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL) )
        msg_Warn( p_aout, "could not change priority of aout_PCMThread()" );
#endif

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
                FillBuffer( p_aout, &p_aout->fifo[i_fifo] );
            }
        }
        vlc_mutex_unlock( &p_aout->fifos_lock );

        i_buffer_limit = p_aout->i_units * p_aout->i_channels;

        switch ( p_aout->i_format )
        {
        case AOUT_FMT_U8:
            for ( i_buffer = 0; i_buffer < i_buffer_limit; i_buffer++ )
            {
                ((u8*)p_aout->buffer)[i_buffer] = (u8)(
                  (p_aout->s32_buffer[i_buffer] / AOUT_MAX_FIFOS / 256 + 128)
                     * p_aout->i_volume / 256);
                 p_aout->s32_buffer[i_buffer] = 0;
            }
            break;

        case AOUT_FMT_S8:
            for ( i_buffer = 0; i_buffer < i_buffer_limit; i_buffer++ )
            {
                ((s8*)p_aout->buffer)[i_buffer] = (s8)(
                  p_aout->s32_buffer[i_buffer] / AOUT_MAX_FIFOS / 256
                     * p_aout->i_volume / 256);
                 p_aout->s32_buffer[i_buffer] = 0;
            }
            break;

        case AOUT_FMT_U16_LE:
        case AOUT_FMT_U16_BE:
            for ( i_buffer = 0; i_buffer < i_buffer_limit; i_buffer++ )
            {
                ((u16*)p_aout->buffer)[i_buffer] = (u16)(
                  (p_aout->s32_buffer[i_buffer] / AOUT_MAX_FIFOS + 128)
                     * p_aout->i_volume / 256);
                 p_aout->s32_buffer[i_buffer] = 0;
            }
            break;

        case AOUT_FMT_S16_LE:
        case AOUT_FMT_S16_BE:
            for ( i_buffer = 0; i_buffer < i_buffer_limit; i_buffer++ )
            {
                ((s16*)p_aout->buffer)[i_buffer] = (s16)(
                  p_aout->s32_buffer[i_buffer] / AOUT_MAX_FIFOS
                     * p_aout->i_volume / 256);
                 p_aout->s32_buffer[i_buffer] = 0;
            }
            break;
        }

        switch ( p_aout->i_format )
        {
        case AOUT_FMT_U8:
        case AOUT_FMT_S8:
            i_units = p_aout->pf_getbufinfo( p_aout, i_buffer_limit );

            p_aout->date = mdate() + ((((mtime_t)((i_units +
                p_aout->i_latency) / p_aout->i_channels)) * 1000000) /
                ((mtime_t)p_aout->i_rate)) + p_aout->p_vlc->i_desync;

            p_aout->pf_play( p_aout, (byte_t *)p_aout->buffer,
                             i_buffer_limit );
            break;

        case AOUT_FMT_U16_LE:
        case AOUT_FMT_U16_BE:
        case AOUT_FMT_S16_LE:
        case AOUT_FMT_S16_BE:
            i_units = p_aout->pf_getbufinfo( p_aout, i_buffer_limit * 2 ) / 2;

            p_aout->date = mdate() + ((((mtime_t)((i_units +
                p_aout->i_latency / 2) / p_aout->i_channels)) * 1000000) /
                ((mtime_t)p_aout->i_rate)) + p_aout->p_vlc->i_desync;

            p_aout->pf_play( p_aout, (byte_t *)p_aout->buffer,
                             i_buffer_limit * 2 );
            break;
        }

        /* Sleep until there is only AOUT_BUFFER_DURATION/2 worth of audio
         * left to play in the aout plugin, then we can start refill the
         * plugin's buffer */
        if( i_units > (i_buffer_limit/2) )
            msleep( (i_units - i_buffer_limit/2) * AOUT_BUFFER_DURATION
                    / i_buffer_limit );
    }

    vlc_mutex_lock( &p_aout->fifos_lock );
    for ( i_fifo = 0; i_fifo < AOUT_MAX_FIFOS; i_fifo++ )
    {
        aout_FreeFifo( &p_aout->fifo[i_fifo] );
    }
    vlc_mutex_unlock( &p_aout->fifos_lock );
}

/* Following functions are local */

/*****************************************************************************
 * InitializeIncrement: change i_x/i_y to i_a+i_b/i_c
 *****************************************************************************/
static inline void InitializeIncrement( aout_increment_t *p_inc,
                                        int i_x, int i_y )
{
    p_inc->i_r = -i_y;
    p_inc->i_a = 0;

    while ( i_x >= i_y )
    {
        p_inc->i_a++;
        i_x -= i_y;
    }

    p_inc->i_b = i_x;
    p_inc->i_c = i_y;
}

/*****************************************************************************
 * UpdateIncrement
 *****************************************************************************/
static inline void UpdateIncrement( aout_increment_t *p_inc, int *pi_integer )
{
    if( (p_inc->i_r += p_inc->i_b) >= 0 )
    {
        *pi_integer += p_inc->i_a + 1;
        p_inc->i_r -= p_inc->i_c;
    }
    else
    {
        *pi_integer += p_inc->i_a;
    }
}

/*****************************************************************************
 * FillBuffer: Read data from decoder fifo, and put it in S32_buffer
 *****************************************************************************/
static void FillBuffer( aout_thread_t * p_aout, aout_fifo_t * p_fifo )
{
    int i_buffer = 0;
    int i_buffer_limit, i_units;

    switch ( p_fifo->i_format )
    {
    case AOUT_FIFO_PCM:

        i_units = p_aout->i_units;

        /* While there are empty frames in the buffer, fill them */
        while ( i_units > 0 )
        {
            /* If there is no next frame, wait for one */
            if( !p_fifo->b_next_frame )
            {
                if( NextFrame(p_aout, p_fifo, p_aout->date + 
                        ((((mtime_t)(i_buffer >> 1)) * 1000000) / 
                        ((mtime_t)p_aout->i_rate))) )
                {
                    break;
                }
            }

            i_buffer_limit = p_aout->i_units * p_aout->i_channels;

            while ( i_buffer < i_buffer_limit )
            {
                /* FIXME: make this more generic */
                if( p_aout->i_channels == 2 )
                {
                    p_aout->s32_buffer[i_buffer++] +=
                        (s32)( ((s16 *)p_fifo->buffer)[2*p_fifo->i_unit] );
                    p_aout->s32_buffer[i_buffer++] +=
                        (s32)( ((s16 *)p_fifo->buffer)[2*p_fifo->i_unit+1] );
                }
                else if( p_aout->i_channels == 1 )
                {
                    p_aout->s32_buffer[i_buffer++] +=
                        (s32)( ((s16 *)p_fifo->buffer)[p_fifo->i_unit] );
                }
                else
                {
                    msg_Err( p_aout, "unsupported number of channels" );
                }

                UpdateIncrement(&p_fifo->unit_increment, &p_fifo->i_unit);

                if( p_fifo->i_unit >= p_fifo->i_unit_limit )
                {
                    p_fifo->i_unit -= p_fifo->i_unit_limit;
                }
            }
 
            if( p_fifo->i_units > i_units )
            {
                p_fifo->i_units -= i_units;
                break;
            }
            else
            {
                i_units -= p_fifo->i_units;

                vlc_mutex_lock( &p_fifo->data_lock );
                p_fifo->i_start_frame = p_fifo->i_next_frame;
             /* p_fifo->b_start_frame = 1; */
                p_fifo->i_next_frame = AOUT_FIFO_INC( p_fifo->i_next_frame );
                p_fifo->b_next_frame = 0;
                vlc_cond_signal( &p_fifo->data_wait );
                vlc_mutex_unlock( &p_fifo->data_lock );

            }
        }
        break;

    default:
        break;
    }
}

static int NextFrame( aout_thread_t * p_aout, aout_fifo_t * p_fifo,
                      mtime_t aout_date )
{
    int i_units, i_units_dist, i_rate;
    mtime_t i_delta;    

    /* We take the lock */
    vlc_mutex_lock( &p_fifo->data_lock );
    if ( p_fifo->b_die )
    {
        vlc_mutex_unlock( &p_fifo->data_lock );
        return( -1 );
    }

    /* Are we looking for a dated start frame ? */
    if ( !p_fifo->b_start_frame )
    {
        while ( p_fifo->i_start_frame != p_fifo->i_end_frame )
        {
            if ( p_fifo->date[p_fifo->i_start_frame] != LAST_MDATE )
            {
                p_fifo->b_start_frame = 1;
                p_fifo->i_next_frame = AOUT_FIFO_INC( p_fifo->i_start_frame );
                p_fifo->i_unit = p_fifo->i_start_frame * 
                        (p_fifo->i_frame_size / p_fifo->i_channels);
                break;
            }
            p_fifo->i_start_frame = AOUT_FIFO_INC( p_fifo->i_start_frame );
        }

        if ( p_fifo->i_start_frame == p_fifo->i_end_frame )
        {
            vlc_mutex_unlock( &p_fifo->data_lock );
            return( -1 );
        }
    }

    /* We are looking for the next dated frame */
    /* FIXME : is the output fifo full ?? -- pretty unlikely given its size */
    while ( !p_fifo->b_next_frame )
    {
        while ( p_fifo->i_next_frame != p_fifo->i_end_frame )
        {
            if ( p_fifo->date[p_fifo->i_next_frame] != LAST_MDATE )
            {
                p_fifo->b_next_frame = 1;
                break;
            }
            p_fifo->i_next_frame = AOUT_FIFO_INC( p_fifo->i_next_frame );
        }

        while ( p_fifo->i_next_frame == p_fifo->i_end_frame )
        {
            vlc_cond_wait( &p_fifo->data_wait, &p_fifo->data_lock );
            if ( p_fifo->b_die )
            {
                vlc_mutex_unlock( &p_fifo->data_lock );
                return( -1 );
            }
        }
    }

    i_units = ((p_fifo->i_next_frame - p_fifo->i_start_frame) & AOUT_FIFO_SIZE)
               * (p_fifo->i_frame_size / p_fifo->i_channels);

    i_delta = aout_date - p_fifo->date[p_fifo->i_start_frame];

    /* Resample if delta is too long */
    if( abs(i_delta) > MAX_DELTA )
    {
        i_rate = p_fifo->i_rate + (i_delta / 256);
    }
    else
    {
        i_rate = p_fifo->i_rate;
    }

    InitializeIncrement( &p_fifo->unit_increment, i_rate, p_aout->i_rate );

    /* Calculate how many units we're going to write */
    i_units_dist = p_fifo->i_unit - p_fifo->i_start_frame
                                * (p_fifo->i_frame_size / p_fifo->i_channels);

    /* Manage the fifo wrapping */
    if( i_units_dist > p_fifo->i_unit_limit / 2 )
    {
        i_units -= (i_units_dist - p_fifo->i_unit_limit);
    }
    else if( i_units_dist < - p_fifo->i_unit_limit / 2 )
    {
        i_units -= (i_units_dist + p_fifo->i_unit_limit);
    }
    else
    {
        i_units -= i_units_dist;
    }

    p_fifo->i_units = 1 + ( i_units * p_aout->i_rate / i_rate );

    /* We release the lock before leaving */
    vlc_mutex_unlock( &p_fifo->data_lock );
    return( 0 );
}

