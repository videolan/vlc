/*****************************************************************************
 * input_ext-dec.c: services to the decoders
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: input_ext-dec.c,v 1.24 2001/12/30 05:38:44 sam Exp $
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
#include "defs.h"

#include <string.h>                                    /* memcpy(), memset() */
#include <sys/types.h>                                              /* off_t */

#include "common.h"
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"

#include "stream_control.h"
#include "input_ext-dec.h"
#include "input_ext-intf.h"
#include "input_ext-plugins.h"

/*****************************************************************************
 * InitBitstream: initialize a bit_stream_t structure
 *****************************************************************************/
void InitBitstream( bit_stream_t * p_bit_stream, decoder_fifo_t * p_fifo,
                    void (* pf_bitstream_callback)( struct bit_stream_s *,
                                                    boolean_t ),
                    void * p_callback_arg )
{
    p_bit_stream->p_decoder_fifo = p_fifo;
    p_bit_stream->pf_next_data_packet = NextDataPacket;
    p_bit_stream->pf_bitstream_callback = pf_bitstream_callback;
    p_bit_stream->p_callback_arg = p_callback_arg;

    /* Get the first data packet. */
    vlc_mutex_lock( &p_fifo->data_lock );
    while ( p_fifo->p_first == NULL )
    {
        if ( p_fifo->b_die )
        {
            vlc_mutex_unlock( &p_fifo->data_lock );
            return;
        }
        vlc_cond_wait( &p_fifo->data_wait, &p_fifo->data_lock );
    }
    p_bit_stream->p_data = p_fifo->p_first->p_first;
    p_bit_stream->p_byte = p_bit_stream->p_data->p_payload_start;
    p_bit_stream->p_end  = p_bit_stream->p_data->p_payload_end;
    p_bit_stream->fifo.buffer = 0;
    p_bit_stream->fifo.i_available = 0;
    vlc_mutex_unlock( &p_fifo->data_lock );

    /* Call back the decoder. */
    if( p_bit_stream->pf_bitstream_callback != NULL )
    {
        p_bit_stream->pf_bitstream_callback( p_bit_stream, 1 );
    }

    if( p_bit_stream->p_byte <= p_bit_stream->p_end - sizeof(WORD_TYPE) )
    {
        /* Get aligned on a word boundary.
         * NB : we _will_ get aligned, because we have at most 
         * sizeof(WORD_TYPE) - 1 bytes to store, and at least
         * sizeof(WORD_TYPE) - 1 empty bytes in the bit buffer. */
        AlignWord( p_bit_stream );
    }
}

/*****************************************************************************
 * EmptyFifo : an error occured, use this function to empty the fifo
 *****************************************************************************/
void EmptyFifo( decoder_fifo_t * p_fifo )
{
    /* We take the lock, because we are going to read/write the start/end
     * indexes of the decoder fifo */
    vlc_mutex_lock (&p_fifo->data_lock);

    /* Wait until a `die' order is sent */
    while (!p_fifo->b_die)
    {
        /* Trash all received PES packets */
        p_fifo->pf_delete_pes( p_fifo->p_packets_mgt, p_fifo->p_first );

        /* Waiting for the input thread to put new PES packets in the fifo */
        vlc_cond_wait (&p_fifo->data_wait, &p_fifo->data_lock);
    }

    /* We can release the lock before leaving */
    vlc_mutex_unlock (&p_fifo->data_lock);
}

/*****************************************************************************
 * NextDataPacket: go to the next data packet
 *****************************************************************************/
void NextDataPacket( bit_stream_t * p_bit_stream )
{
    decoder_fifo_t *    p_fifo = p_bit_stream->p_decoder_fifo;
    boolean_t           b_new_pes;

    /* We are looking for the next data packet that contains real data,
     * and not just a PES header */
    do
    {
        /* We were reading the last data packet of this PES packet... It's
         * time to jump to the next PES packet */
        if( p_bit_stream->p_data->p_next == NULL )
        {
            pes_packet_t * p_next;

            vlc_mutex_lock( &p_fifo->data_lock );

            /* Free the previous PES packet. */
            p_next = p_fifo->p_first->p_next;
            p_fifo->p_first->p_next = NULL;
            p_fifo->pf_delete_pes( p_fifo->p_packets_mgt,
                                   p_fifo->p_first );
            p_fifo->p_first = p_next;
            p_fifo->i_depth--;

            if( p_fifo->p_first == NULL )
            {
                /* No PES in the FIFO. p_last is no longer valid. */
                p_fifo->pp_last = &p_fifo->p_first;

                /* Wait for the input to tell us when we receive a packet. */
                vlc_cond_wait( &p_fifo->data_wait, &p_fifo->data_lock );
            }

            /* The next byte could be found in the next PES packet */
            p_bit_stream->p_data = p_fifo->p_first->p_first;

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
}

/*****************************************************************************
 * UnalignedShowBits : places i_bits bits into the bit buffer, even when
 * not aligned on a word boundary
 *****************************************************************************/
u32 UnalignedShowBits( bit_stream_t * p_bit_stream, unsigned int i_bits )
{
    /* We just fill in the bit buffer. */
    while( p_bit_stream->fifo.i_available < i_bits )
    {
        if( p_bit_stream->p_byte < p_bit_stream->p_end )
        {
            p_bit_stream->fifo.buffer |= *(p_bit_stream->p_byte++)
                                            << (8 * sizeof(WORD_TYPE) - 8
                                            - p_bit_stream->fifo.i_available);
            p_bit_stream->fifo.i_available += 8;
        }
        else
        {
            p_bit_stream->pf_next_data_packet( p_bit_stream );

            if( (ptrdiff_t)p_bit_stream->p_byte & (sizeof(WORD_TYPE) - 1) )
            {
                /* We are not aligned anymore. */
                if( ((ptrdiff_t)p_bit_stream->p_byte
                                    & (sizeof(WORD_TYPE) - 1)) * 8
                        < p_bit_stream->fifo.i_available )
                {
                    /* We are not aligned, and won't be. Copy the first word
                     * of the packet in a temporary buffer, and we'll see
                     * later. */
                    int     i;

                    /* sizeof(WORD_TYPE) - number of bytes to trash
                     * from the last payload */
                    int     j;

                    p_bit_stream->i_showbits_buffer = 0;

                    for( j = i = 0 ; i < sizeof(WORD_TYPE) ; i++ )
                    {
                        if( p_bit_stream->p_byte >= p_bit_stream->p_end )
                        {
                            j = i;
                            p_bit_stream->pf_next_data_packet( p_bit_stream );
                        }
                        ((byte_t *)&p_bit_stream->i_showbits_buffer)[i] =
                            * p_bit_stream->p_byte;
                        p_bit_stream->p_byte++;
                    }

                    /* This is kind of kludgy. */
                    p_bit_stream->p_data->p_payload_start +=
                                                         sizeof(WORD_TYPE) - j;
                    p_bit_stream->p_byte =
                        (byte_t *)&p_bit_stream->i_showbits_buffer;
                    p_bit_stream->p_end =
                        (byte_t *)&p_bit_stream->i_showbits_buffer
                            + sizeof(WORD_TYPE);
                    p_bit_stream->showbits_data.p_next = p_bit_stream->p_data;
                    p_bit_stream->p_data = &p_bit_stream->showbits_data;
                }
                else
                {
                    /* We are not aligned, but we can be. */
                    AlignWord( p_bit_stream );
                }
            }

            /* We have 32 bits ready for reading, it will be enough. */
            break;
        }
    }

    /* It shouldn't loop :-)) */
    return( ShowBits( p_bit_stream, i_bits ) );
}

/*****************************************************************************
 * UnalignedGetBits : returns i_bits bits from the bit stream and removes
 * them from the buffer, even when the bit stream is not aligned on a word
 * boundary
 *****************************************************************************/
u32 UnalignedGetBits( bit_stream_t * p_bit_stream, unsigned int i_bits )
{
    u32         i_result;

    i_result = p_bit_stream->fifo.buffer
                    >> (8 * sizeof(WORD_TYPE) - i_bits);
    i_bits = -p_bit_stream->fifo.i_available;

    /* Gather missing bytes. */
    while( i_bits >= 8 )
    {
        if( p_bit_stream->p_byte < p_bit_stream->p_end )
        {
            i_result |= *(p_bit_stream->p_byte++) << (i_bits - 8);
            i_bits -= 8;
        }
        else
        {
            p_bit_stream->pf_next_data_packet( p_bit_stream );
            i_result |= *(p_bit_stream->p_byte++) << (i_bits - 8);
            i_bits -= 8;
        }
    }

    /* Gather missing bits. */
    if( i_bits > 0 )
    {
        unsigned int    i_tmp = 8 - i_bits;

        if( p_bit_stream->p_byte < p_bit_stream->p_end )
        {
            i_result |= *p_bit_stream->p_byte >> i_tmp;
            p_bit_stream->fifo.buffer = *(p_bit_stream->p_byte++)
                 << ( sizeof(WORD_TYPE) * 8 - i_tmp );
            p_bit_stream->fifo.i_available = i_tmp;
        }
        else
        {
            p_bit_stream->pf_next_data_packet( p_bit_stream );
            i_result |= *p_bit_stream->p_byte >> i_tmp;
            p_bit_stream->fifo.buffer = *(p_bit_stream->p_byte++)
                 << ( sizeof(WORD_TYPE) * 8 - i_tmp );
            p_bit_stream->fifo.i_available = i_tmp;
        }
    }
    else
    {
        p_bit_stream->fifo.i_available = 0;
        p_bit_stream->fifo.buffer = 0;
    }

    if( p_bit_stream->p_byte <= p_bit_stream->p_end - sizeof(WORD_TYPE) )
    {
        /* Get aligned on a word boundary. Otherwise it is safer
         * to do it the next time.
         * NB : we _will_ get aligned, because we have at most
         * sizeof(WORD_TYPE) - 1 bytes to store, and at least
         * sizeof(WORD_TYPE) - 1 empty bytes in the bit buffer. */
        AlignWord( p_bit_stream );
    }

    return( i_result );
}

/*****************************************************************************
 * UnalignedRemoveBits : removes i_bits (== -i_available) from the bit
 * buffer, even when the bit stream is not aligned on a word boundary
 *****************************************************************************/
void UnalignedRemoveBits( bit_stream_t * p_bit_stream )
{
    /* First remove all unnecessary bytes. */
    while( p_bit_stream->fifo.i_available <= -8 )
    {
        if( p_bit_stream->p_byte < p_bit_stream->p_end )
        {
            p_bit_stream->p_byte++;
            p_bit_stream->fifo.i_available += 8;
        }
        else
        {
            p_bit_stream->pf_next_data_packet( p_bit_stream );
            p_bit_stream->p_byte++;
            p_bit_stream->fifo.i_available += 8;
        }
    }

    /* Remove unnecessary bits. */
    if( p_bit_stream->fifo.i_available < 0 )
    {
        if( p_bit_stream->p_byte < p_bit_stream->p_end )
        {
            p_bit_stream->fifo.buffer = *(p_bit_stream->p_byte++)
                 << ( sizeof(WORD_TYPE) * 8 - 8
                         - p_bit_stream->fifo.i_available );
            p_bit_stream->fifo.i_available += 8;
        }
        else
        {
            p_bit_stream->pf_next_data_packet( p_bit_stream );
            p_bit_stream->fifo.buffer = *(p_bit_stream->p_byte++)
                 << ( sizeof(WORD_TYPE) * 8 - 8
                         - p_bit_stream->fifo.i_available );
            p_bit_stream->fifo.i_available += 8;
        }
    }
    else
    {
        p_bit_stream->fifo.buffer = 0;
    }

    if( p_bit_stream->p_byte <= p_bit_stream->p_end - sizeof(WORD_TYPE) )
    {
        /* Get aligned on a word boundary. Otherwise it is safer
         * to do it the next time.
         * NB : we _will_ get aligned, because we have at most 
         * sizeof(WORD_TYPE) - 1 bytes to store, and at least
         * sizeof(WORD_TYPE) - 1 empty bytes in the bit buffer. */
        AlignWord( p_bit_stream );
    }
}

