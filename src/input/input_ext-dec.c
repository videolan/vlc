/*****************************************************************************
 * input_ext-dec.c: services to the decoders
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 *
 * Authors: 
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

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "intf_msg.h"

#include "stream_control.h"
#include "input_ext-dec.h"
#include "input_ext-intf.h"

#include "input.h"

/*****************************************************************************
 * InitBitstream: initialize a bit_stream_t structure
 *****************************************************************************/
void InitBitstream( bit_stream_t * p_bit_stream, decoder_fifo_t * p_fifo )
{
    p_bit_stream->p_decoder_fifo = p_fifo;
    p_bit_stream->pf_next_data_packet = NextDataPacket;
    p_bit_stream->pf_bitstream_callback = NULL;
    p_bit_stream->p_callback_arg = NULL;

    /* Get the first data packet. */
    vlc_mutex_lock( &p_fifo->data_lock );
    while ( DECODER_FIFO_ISEMPTY( *p_fifo ) )
    {
        if ( p_fifo->b_die )
        {
            vlc_mutex_unlock( &p_fifo->data_lock );
            return;
        }
        vlc_cond_wait( &p_fifo->data_wait, &p_fifo->data_lock );
    }
    p_bit_stream->p_data = DECODER_FIFO_START( *p_fifo )->p_first;
    p_bit_stream->p_byte = p_bit_stream->p_data->p_payload_start;
    p_bit_stream->p_end  = p_bit_stream->p_data->p_payload_end;
    p_bit_stream->fifo.buffer = 0;
    p_bit_stream->fifo.i_available = 0;
    vlc_mutex_unlock( &p_fifo->data_lock );
}

/*****************************************************************************
 * NextDataPacket: go to the next data packet
 *****************************************************************************/
void NextDataPacket( bit_stream_t * p_bit_stream )
{
    WORD_TYPE           buffer_left;
    ptrdiff_t           i_bytes_left;
    decoder_fifo_t *    p_fifo = p_bit_stream->p_decoder_fifo;
    boolean_t           b_new_pes;

    /* Put the remaining bytes (not aligned on a word boundary) in a
     * temporary buffer. */
    i_bytes_left = p_bit_stream->p_end - p_bit_stream->p_byte;
    buffer_left = *((WORD_TYPE *)p_bit_stream->p_end - 1);

    /* We are looking for the next data packet that contains real data,
     * and not just a PES header */
    do
    {
        /* We were reading the last data packet of this PES packet... It's
         * time to jump to the next PES packet */
        if( p_bit_stream->p_data->p_next == NULL )
        {
            /* We are going to read/write the start and end indexes of the
             * decoder fifo and to use the fifo's conditional variable,
             * that's why we need to take the lock before. */
            vlc_mutex_lock( &p_fifo->data_lock );

            /* Free the previous PES packet. */
            p_fifo->pf_delete_pes( p_fifo->p_packets_mgt,
                                   DECODER_FIFO_START( *p_fifo ) );
            DECODER_FIFO_INCSTART( *p_fifo );

            if( DECODER_FIFO_ISEMPTY( *p_fifo ) )
            {
                /* Wait for the input to tell us when we receive a packet. */
                vlc_cond_wait( &p_fifo->data_wait, &p_fifo->data_lock );
            }

            /* The next byte could be found in the next PES packet */
            p_bit_stream->p_data = DECODER_FIFO_START( *p_fifo )->p_first;

            vlc_mutex_unlock( &p_fifo->data_lock );

            b_new_pes = 1;
        }
        else
        {
            /* Perhaps the next data packet of the current PES packet contains
             * real data (ie its payload's size is greater than 0). */
            p_bit_stream->p_data = p_bit_stream->p_data->p_next;

            b_new_pes = 0;
        }
    } while ( p_bit_stream->p_data->p_payload_start
               == p_bit_stream->p_data->p_payload_end );

    /* We've found a data packet which contains interesting data... */
    p_bit_stream->p_byte = p_bit_stream->p_data->p_payload_start;
    p_bit_stream->p_end  = p_bit_stream->p_data->p_payload_end;

    /* Call back the decoder. */
    if( p_bit_stream->pf_bitstream_callback != NULL )
    {
        p_bit_stream->pf_bitstream_callback( p_bit_stream, b_new_pes );
    }

    /* Copy remaining bits of the previous packet */
    *((WORD_TYPE *)p_bit_stream->p_byte - 1) = buffer_left;
    p_bit_stream->p_byte -= i_bytes_left;
}
