/*****************************************************************************
 * decoder_fifo.c: auxiliaries functions used in decoder_fifo.h
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
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
#include "defs.h"

#include <sys/types.h>                        /* on BSD, uio.h needs types.h */
#include <sys/uio.h>                                          /* for input.h */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "debug.h"                 /* XXX?? temporaire, requis par netlist.h */

#include "input.h"
#include "input_netlist.h"
#include "decoder_fifo.h"

void decoder_fifo_next( bit_stream_t * p_bit_stream )
{
    /* We are looking for the next TS packet that contains real data,
     * and not just a PES header */
    do
    {
        /* We were reading the last TS packet of this PES packet... It's
         * time to jump to the next PES packet */
        if ( p_bit_stream->p_ts->p_next_ts == NULL )
        {
            /* We are going to read/write the start and end indexes of the
             * decoder fifo and to use the fifo's conditional variable,
             * that's why we need to take the lock before */
            vlc_mutex_lock( &p_bit_stream->p_decoder_fifo->data_lock );

            /* Is the input thread dying ? */
            if ( p_bit_stream->p_input->b_die )
            {
                vlc_mutex_unlock( &(p_bit_stream->p_decoder_fifo->data_lock) );
                return;
            }

            /* We should increase the start index of the decoder fifo, but
             * if we do this now, the input thread could overwrite the
             * pointer to the current PES packet, and we weren't able to
             * give it back to the netlist. That's why we free the PES
             * packet first. */
            input_NetlistFreePES( p_bit_stream->p_input, DECODER_FIFO_START(*p_bit_stream->p_decoder_fifo) );
            DECODER_FIFO_INCSTART( *p_bit_stream->p_decoder_fifo );

            while ( DECODER_FIFO_ISEMPTY(*p_bit_stream->p_decoder_fifo) )
            {
                vlc_cond_wait( &p_bit_stream->p_decoder_fifo->data_wait, &p_bit_stream->p_decoder_fifo->data_lock );
                if ( p_bit_stream->p_input->b_die )
                {
                    vlc_mutex_unlock( &(p_bit_stream->p_decoder_fifo->data_lock) );
                    return;
                }
            }

            /* The next byte could be found in the next PES packet */
            p_bit_stream->p_ts = DECODER_FIFO_START( *p_bit_stream->p_decoder_fifo )->p_first_ts;

            /* We can release the fifo's data lock */
            vlc_mutex_unlock( &p_bit_stream->p_decoder_fifo->data_lock );
        }
        /* Perhaps the next TS packet of the current PES packet contains
         * real data (ie its payload's size is greater than 0) */
        else
        {
            p_bit_stream->p_ts = p_bit_stream->p_ts->p_next_ts;
        }
    } while ( p_bit_stream->p_ts->i_payload_start == p_bit_stream->p_ts->i_payload_end );

    /* We've found a TS packet which contains interesting data... */
    p_bit_stream->p_byte = p_bit_stream->p_ts->buffer + p_bit_stream->p_ts->i_payload_start;
    p_bit_stream->p_end = p_bit_stream->p_ts->buffer + p_bit_stream->p_ts->i_payload_end;
}

void PeekNextPacket( bit_stream_t * p_bit_stream )
{
    WORD_TYPE   buffer_left;
    int         i_bytes_left; /* FIXME : not portable in a 64bit environment */

    /* Put the remaining bytes (not aligned on a word boundary) in a
     * temporary buffer. */
    i_bytes_left = p_bit_stream->p_end - p_bit_stream->p_byte;
    buffer_left = *((WORD_TYPE *)p_bit_stream->p_end - 1);

    /* We are looking for the next TS packet that contains real data,
     * and not just a PES header */
    do
    {
        /* We were reading the last TS packet of this PES packet... It's
         * time to jump to the next PES packet */
        if ( p_bit_stream->p_ts->p_next_ts == NULL )
        {
            /* We are going to read/write the start and end indexes of the
             * decoder fifo and to use the fifo's conditional variable,
             * that's why we need to take the lock before */
            vlc_mutex_lock( &p_bit_stream->p_decoder_fifo->data_lock );

            /* Is the input thread dying ? */
            if ( p_bit_stream->p_input->b_die )
            {
                vlc_mutex_unlock( &(p_bit_stream->p_decoder_fifo->data_lock) );
                return;
            }

            /* We should increase the start index of the decoder fifo, but
             * if we do this now, the input thread could overwrite the
             * pointer to the current PES packet, and we weren't able to
             * give it back to the netlist. That's why we free the PES
             * packet first. */
            input_NetlistFreePES( p_bit_stream->p_input, DECODER_FIFO_START(*p_bit_stream->p_decoder_fifo) );
            DECODER_FIFO_INCSTART( *p_bit_stream->p_decoder_fifo );

            while ( DECODER_FIFO_ISEMPTY(*p_bit_stream->p_decoder_fifo) )
            {
                vlc_cond_wait( &p_bit_stream->p_decoder_fifo->data_wait, &p_bit_stream->p_decoder_fifo->data_lock );
                if ( p_bit_stream->p_input->b_die )
                {
                    vlc_mutex_unlock( &(p_bit_stream->p_decoder_fifo->data_lock) );
                    return;
                }
            }

            /* The next byte could be found in the next PES packet */
            p_bit_stream->p_ts = DECODER_FIFO_START( *p_bit_stream->p_decoder_fifo )->p_first_ts;

            /* We can release the fifo's data lock */
            vlc_mutex_unlock( &p_bit_stream->p_decoder_fifo->data_lock );
        }
        /* Perhaps the next TS packet of the current PES packet contains
         * real data (ie its payload's size is greater than 0) */
        else
        {
            p_bit_stream->p_ts = p_bit_stream->p_ts->p_next_ts;
        }
    } while ( p_bit_stream->p_ts->i_payload_start == p_bit_stream->p_ts->i_payload_end );

    /* We've found a TS packet which contains interesting data... */
    p_bit_stream->p_byte = p_bit_stream->p_ts->buffer + p_bit_stream->p_ts->i_payload_start;
    p_bit_stream->p_end = p_bit_stream->p_ts->buffer + p_bit_stream->p_ts->i_payload_end;

    /* Copy remaining bits of the previous packet */
    *((WORD_TYPE *)p_bit_stream->p_byte - 1) = buffer_left;
    p_bit_stream->p_byte -= i_bytes_left;
}
