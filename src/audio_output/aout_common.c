/*****************************************************************************
 * aout_common.c: generic audio output functions
 *****************************************************************************
 * Copyright (C) 1999-2002 VideoLAN
 * $Id
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
 * Functions
 *****************************************************************************/

/* Read data from decoder fifo, and put it in S32_buffer */
void aout_FillBuffer( aout_thread_t * p_aout, aout_fifo_t * p_fifo )
{
    long l_buffer = 0;
    long l_buffer_limit, l_units;

    switch ( p_fifo->i_type )
    {
    case AOUT_EMPTY_FIFO:

        break;

    case AOUT_ADEC_MONO_FIFO:
    case AOUT_ADEC_STEREO_FIFO:

        l_units = p_aout->l_units;
        while ( l_units > 0 )
        {
            if( !p_fifo->b_next_frame )
            {
                if( NextFrame(p_aout, p_fifo, p_aout->date + 
                        ((((mtime_t)(l_buffer >> 1)) * 1000000) / 
                        ((mtime_t)p_aout->l_rate))) )
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
                if( p_fifo->l_unit >= ((AOUT_FIFO_SIZE + 1) * 
                        (p_fifo->l_frame_size >> p_fifo->b_stereo)) )
                {
                    p_fifo->l_unit -= ((AOUT_FIFO_SIZE + 1) * 
                            (p_fifo->l_frame_size >> p_fifo->b_stereo));
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

