/*****************************************************************************
 * decoder_fifo.h: interface for decoders PES fifo
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
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
 * Required headers:
 * - "config.h"
 * - "common.h"
 * - "threads.h"
 * - "input.h"
 *****************************************************************************/

/*****************************************************************************
 * Macros
 *****************************************************************************/

/* FIXME: move to inline functions ??*/
#define DECODER_FIFO_ISEMPTY( fifo )    ( (fifo).i_start == (fifo).i_end )
#define DECODER_FIFO_ISFULL( fifo )     ( ( ( (fifo).i_end + 1 - (fifo).i_start ) \
                                          & FIFO_SIZE ) == 0 )
#define DECODER_FIFO_START( fifo )      ( (fifo).buffer[ (fifo).i_start ] )
#define DECODER_FIFO_INCSTART( fifo )   ( (fifo).i_start = ((fifo).i_start + 1)\
                                                           & FIFO_SIZE )
#define DECODER_FIFO_END( fifo )        ( (fifo).buffer[ (fifo).i_end ] )
#define DECODER_FIFO_INCEND( fifo )     ( (fifo).i_end = ((fifo).i_end + 1) \
                                                         & FIFO_SIZE )

/*****************************************************************************
 * decoder_fifo_t
 *****************************************************************************
 * This rotative FIFO contains PES packets that are to be decoded...
 *****************************************************************************/
typedef struct
{
    vlc_mutex_t         data_lock;                         /* fifo data lock */
    vlc_cond_t          data_wait;         /* fifo data conditional variable */

    /* buffer is an array of PES packets pointers */
    pes_packet_t *      buffer[FIFO_SIZE + 1];
    int                 i_start;
    int                 i_end;

} decoder_fifo_t;

/*****************************************************************************
 * bit_fifo_t : bit fifo descriptor
 *****************************************************************************
 * This type describes a bit fifo used to store bits while working with the
 * input stream at the bit level.
 *****************************************************************************/
typedef struct bit_fifo_s
{
    /* This unsigned integer allows us to work at the bit level. This buffer
     * can contain 32 bits, and the used space can be found on the MSb's side
     * and the available space on the LSb's side. */
    u32                 buffer;

    /* Number of bits available in the bit buffer */
    int                 i_available;

} bit_fifo_t;

/*****************************************************************************
 * bit_stream_t : bit stream descriptor
 *****************************************************************************
 * This type, based on a PES stream, includes all the structures needed to
 * handle the input stream like a bit stream.
 *****************************************************************************/
typedef struct bit_stream_s
{
    /*
     * Input structures
     */
    /* The input thread feeds the stream with fresh PES packets */
    input_thread_t *    p_input;
    /* The decoder fifo contains the data of the PES stream */
    decoder_fifo_t *    p_decoder_fifo;

    /*
     * Byte structures
     */
    /* Current TS packet (in the current PES packet of the PES stream) */
    ts_packet_t *       p_ts;
   /* Pointer to the next byte that is to be read (in the current TS packet) */
    byte_t *            p_byte;
    /* Pointer to the last byte that is to be read (in the current TS packet */
    byte_t *            p_end;

    /*
     * Bit structures
     */
    bit_fifo_t          fifo;

} bit_stream_t;


void decoder_fifo_next( bit_stream_t * p_bit_stream );
/*****************************************************************************
 * GetByte : reads the next byte in the input stream
 *****************************************************************************/
static __inline__ byte_t GetByte( bit_stream_t * p_bit_stream )
{
    /* Are there some bytes left in the current TS packet ? */
    /* could change this test to have a if (! (bytes--)) instead */
    if ( p_bit_stream->p_byte >= p_bit_stream->p_end )
    {
        /* no, switch to next TS packet */
        decoder_fifo_next( p_bit_stream );
    }

    return( *(p_bit_stream->p_byte++));
}

/*****************************************************************************
 * NeedBits : reads i_bits new bits in the bit stream and stores them in the
 *            bit buffer
 *****************************************************************************
 * - i_bits must be less or equal 32 !
 * - There is something important to notice with that function : if the number
 * of bits available in the bit buffer when calling NeedBits() is greater than
 * 24 (i_available > 24) but less than the number of needed bits
 * (i_available < i_bits), the byte returned by GetByte() will be shifted with
 * a negative value and the number of bits available in the bit buffer will be
 * set to more than 32 !
 *****************************************************************************/
static __inline__ void NeedBits( bit_stream_t * p_bit_stream, int i_bits )
{
    while ( p_bit_stream->fifo.i_available < i_bits )
    {
        p_bit_stream->fifo.buffer |= ((u32)GetByte( p_bit_stream )) << (24 - p_bit_stream->fifo.i_available);
        p_bit_stream->fifo.i_available += 8;
    }
}

/*****************************************************************************
 * DumpBits : removes i_bits bits from the bit buffer
 *****************************************************************************
 * - i_bits <= i_available
 * - i_bits < 32 (because (u32 << 32) <=> (u32 = u32))
 *****************************************************************************/
static __inline__ void DumpBits( bit_stream_t * p_bit_stream, int i_bits )
{
    p_bit_stream->fifo.buffer <<= i_bits;
    p_bit_stream->fifo.i_available -= i_bits;
}

/*****************************************************************************
 * DumpBits32 : removes 32 bits from the bit buffer
 *****************************************************************************
 * This function actually believes that you have already put 32 bits in the
 * bit buffer, so you can't you use it anytime.
 *****************************************************************************/
static __inline__ void DumpBits32( bit_stream_t * p_bit_stream )
{
    p_bit_stream->fifo.buffer = 0;
    p_bit_stream->fifo.i_available = 0;
}

/*
 * For the following functions, please read VERY CAREFULLY the warning in
 * NeedBits(). If i_bits > 24, the stream parser must be already aligned
 * on an 8-bit boundary, or you will get curious results (that is, you
 * need to call RealignBits() before).
 */

/*****************************************************************************
 * RemoveBits : removes i_bits bits from the bit buffer
 *****************************************************************************/
static __inline__ void RemoveBits( bit_stream_t * p_bit_stream, int i_bits )
{
    NeedBits( p_bit_stream, i_bits );
    DumpBits( p_bit_stream, i_bits );
}

/*****************************************************************************
 * RemoveBits32 : removes 32 bits from the bit buffer
 *****************************************************************************/
static __inline__ void RemoveBits32( bit_stream_t * p_bit_stream )
{
    NeedBits( p_bit_stream, 32 );
    DumpBits32( p_bit_stream );
}

/*****************************************************************************
 * ShowBits : return i_bits bits from the bit stream
 *****************************************************************************/
static __inline__ u32 ShowBits( bit_stream_t * p_bit_stream, int i_bits )
{
    NeedBits( p_bit_stream, i_bits );
    return( p_bit_stream->fifo.buffer >> (32 - i_bits) );
}

/*****************************************************************************
 * GetBits : returns i_bits bits from the bit stream and removes them
 *****************************************************************************/
static __inline__ u32 GetBits( bit_stream_t * p_bit_stream, int i_bits )
{
    u32 i_buffer;

    NeedBits( p_bit_stream, i_bits );
    i_buffer = p_bit_stream->fifo.buffer >> (32 - i_bits);
    DumpBits( p_bit_stream, i_bits );
    return( i_buffer );
}

/*****************************************************************************
 * GetBits32 : returns 32 bits from the bit stream and removes them
 *****************************************************************************/
static __inline__ u32 GetBits32( bit_stream_t * p_bit_stream )
{
    u32 i_buffer;

    NeedBits( p_bit_stream, 32 );
    i_buffer = p_bit_stream->fifo.buffer;
    DumpBits32( p_bit_stream );
    return( i_buffer );
}

/*****************************************************************************
 * RealignBits : realigns the bit buffer on an 8-bit boundary
 *****************************************************************************/
static __inline__ void RealignBits( bit_stream_t * p_bit_stream )
{
    DumpBits( p_bit_stream, p_bit_stream->fifo.i_available & 7 );
}
