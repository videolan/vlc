static __inline__ u8 GetByte (ac3_byte_stream_t * p_byte_stream)
{
    /* Are there some bytes left in the current buffer ? */
    if (p_byte_stream->p_byte >= p_byte_stream->p_end) {
	/* no, switch to next buffer */
	ac3_byte_stream_next (p_byte_stream);
    }

    return *(p_byte_stream->p_byte++);
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
static __inline__ void NeedBits (ac3_bit_stream_t * p_bit_stream, int i_bits)
{
    while (p_bit_stream->i_available < i_bits) {
        p_bit_stream->buffer |=
	    ((u32)GetByte (&p_bit_stream->byte_stream)) << (24 - p_bit_stream->i_available);
        p_bit_stream->i_available += 8;
    }
}

/*****************************************************************************
 * DumpBits : removes i_bits bits from the bit buffer
 *****************************************************************************
 * - i_bits <= i_available
 * - i_bits < 32 (because (u32 << 32) <=> (u32 = u32))
 *****************************************************************************/
static __inline__ void DumpBits (ac3_bit_stream_t * p_bit_stream, int i_bits)
{
    p_bit_stream->buffer <<= i_bits;
    p_bit_stream->i_available -= i_bits;
    p_bit_stream->total_bits_read += i_bits;
}
