/*****************************************************************************
 * ac3_bit_stream.h: getbits functions for the ac3 decoder
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 *
 * Authors: Michel Lespinasse <walken@zoy.org>
 *          Renaud Dartus <reno@videolan.org>
 *
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

static __inline__ u32 bitstream_get (ac3_bit_stream_t * p_bit_stream, 
                                     u32 num_bits)
{
    u32 result=0;
    while (p_bit_stream->i_available < num_bits)
    {
        if (p_bit_stream->byte_stream.p_byte >= p_bit_stream->byte_stream.p_end)
        {
            ac3dec_thread_t * p_ac3dec = p_bit_stream->byte_stream.info;
            
            /* no, switch to next buffer */
            if(!p_ac3dec->p_fifo->b_die)
                ac3_byte_stream_next (&p_bit_stream->byte_stream);
        }
        p_bit_stream->buffer |=((u32) *(p_bit_stream->byte_stream.p_byte++)) 
            << (24 - p_bit_stream->i_available);
        p_bit_stream->i_available += 8;
    }
    result = p_bit_stream->buffer >> (32 - num_bits);
    p_bit_stream->buffer <<= num_bits;
    p_bit_stream->i_available -= num_bits;
    p_bit_stream->total_bits_read += num_bits;
    
    return result;
}
