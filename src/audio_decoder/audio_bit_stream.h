/*****************************************************************************
 * audio_bit_stream.h: getbits functions for the audio decoder
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *****************************************************************************/

static __inline__ u8 GetByte (adec_bit_stream_t * p_bit_stream)
{
    /* Are there some bytes left in the current buffer ? */
    if (p_bit_stream->byte_stream.p_byte >= p_bit_stream->byte_stream.p_end) {
	/* no, switch to next buffer */
	adec_byte_stream_next (&p_bit_stream->byte_stream);
    }

    p_bit_stream->total_bytes_read++;

    return *(p_bit_stream->byte_stream.p_byte++);
}

static __inline__ void NeedBits (adec_bit_stream_t * p_bit_stream, int i_bits)
{
    while (p_bit_stream->i_available < i_bits) {
        p_bit_stream->buffer |=
	    ((u32)GetByte (p_bit_stream)) << (24 - p_bit_stream->i_available);
        p_bit_stream->i_available += 8;
    }
}

static __inline__ void DumpBits (adec_bit_stream_t * p_bit_stream, int i_bits)
{
    p_bit_stream->buffer <<= i_bits;
    p_bit_stream->i_available -= i_bits;
}
