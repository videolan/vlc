static __inline__ u8 GetByte (ac3_byte_stream_t * p_byte_stream)
{
    /* Are there some bytes left in the current buffer ? */
    if (p_byte_stream->p_byte >= p_byte_stream->p_end) {
	/* no, switch to next buffer */
	ac3_byte_stream_next (p_byte_stream);
    }

    return *(p_byte_stream->p_byte++);
}

static __inline__ void NeedBits (ac3_bit_stream_t * p_bit_stream, int i_bits)
{
    while (p_bit_stream->i_available < i_bits) {
        p_bit_stream->buffer |=
	    ((u32)GetByte (&p_bit_stream->byte_stream)) << (24 - p_bit_stream->i_available);
        p_bit_stream->i_available += 8;
    }
}

static __inline__ void DumpBits (ac3_bit_stream_t * p_bit_stream, int i_bits)
{
    p_bit_stream->buffer <<= i_bits;
    p_bit_stream->i_available -= i_bits;
    p_bit_stream->total_bits_read += i_bits;
}
