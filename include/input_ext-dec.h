/*****************************************************************************
 * input_ext-dec.h: structures exported to the VideoLAN decoders
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: input_ext-dec.h,v 1.58 2002/05/18 17:47:46 sam Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Michel Kaempf <maxx@via.ecp.fr>
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

/* ES streams types - see ISO/IEC 13818-1 table 2-29 numbers */
#define MPEG1_VIDEO_ES      0x01
#define MPEG2_VIDEO_ES      0x02
#define MPEG1_AUDIO_ES      0x03
#define MPEG2_AUDIO_ES      0x04
#define AC3_AUDIO_ES        0x81
/* These ones might violate the norm : */
#define DVD_SPU_ES          0x82
#define LPCM_AUDIO_ES       0x83
#define MSMPEG4v1_VIDEO_ES  0x40
#define MSMPEG4v2_VIDEO_ES  0x41
#define MSMPEG4v3_VIDEO_ES  0x42
#define MPEG4_VIDEO_ES      0x50

#define UNKNOWN_ES          0xFF

/* Structures exported to the decoders */

/*****************************************************************************
 * data_packet_t
 *****************************************************************************
 * Describe a data packet.
 *****************************************************************************/
typedef struct data_packet_s
{
    /* Used to chain the packets that carry data for a same PES or PSI */
    struct data_packet_s *  p_next;

    /* start of the PS or TS packet */
    byte_t *                p_demux_start;
    /* start of the PES payload in this packet */
    byte_t *                p_payload_start;
    byte_t *                p_payload_end; /* guess ? :-) */
    /* is the packet messed up ? */
    boolean_t               b_discard_payload;

    /* pointer to the real data */
    struct data_buffer_s *  p_buffer;
} data_packet_t;

/*****************************************************************************
 * pes_packet_t
 *****************************************************************************
 * Describes an PES packet, with its properties, and pointers to the TS packets
 * containing it.
 *****************************************************************************/
typedef struct pes_packet_s
{
    /* Chained list to the next PES packet (depending on the context) */
    struct pes_packet_s *   p_next;

    /* PES properties */
    boolean_t               b_data_alignment;  /* used to find the beginning of
                                                * a video or audio unit      */
    boolean_t               b_discontinuity; /* This packet doesn't follow the
                                              * previous one                 */

    mtime_t                 i_pts;    /* PTS for this packet (zero if unset) */
    mtime_t                 i_dts;    /* DTS for this packet (zero if unset) */
    int                     i_rate;                /* current pace of reading
                                                    * (see stream_control.h) */

    unsigned int            i_pes_size;    /* size of the current PES packet */

    /* Chained list to packets */
    data_packet_t *         p_first;      /* The first packet contained by this
                                           * PES (used by decoders). */
    data_packet_t *         p_last;    /* The last packet contained by this
                                          PES (used by the buffer allocator) */
    unsigned int            i_nb_data; /* Number of data packets in the chained
                                                                        list */
} pes_packet_t;

/*****************************************************************************
 * decoder_fifo_t
 *****************************************************************************
 * This rotative FIFO contains PES packets that are to be decoded.
 *****************************************************************************/
typedef struct decoder_fifo_s
{
    /* Thread structures */
    vlc_mutex_t             data_lock;                     /* fifo data lock */
    vlc_cond_t              data_wait;     /* fifo data conditional variable */

    /* Data */
    pes_packet_t *          p_first;
    pes_packet_t **         pp_last;
    int                     i_depth;   /* number of PES packets in the stack */

    /* Communication interface between input and decoders */
    boolean_t               b_die;          /* the decoder should return now */
    boolean_t               b_error;      /* the decoder is in an error loop */
    struct input_buffers_s *p_packets_mgt;   /* packets management services
                                              * data */
} decoder_fifo_t;

/*****************************************************************************
 * bit_fifo_t : bit fifo descriptor
 *****************************************************************************
 * This type describes a bit fifo used to store bits while working with the
 * input stream at the bit level.
 *****************************************************************************/
typedef u32         WORD_TYPE;

typedef struct bit_fifo_s
{
    /* This unsigned integer allows us to work at the bit level. This buffer
     * can contain 32 bits, and the used space can be found on the MSb's side
     * and the available space on the LSb's side. */
    WORD_TYPE           buffer;

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
     * Bit structures
     */
    bit_fifo_t              fifo;

    /*
     * Input structures
     */
    /* The decoder fifo contains the data of the PES stream */
    decoder_fifo_t *        p_decoder_fifo;

    /* Callback to the decoder used when changing data packets ; set
     * to NULL if your decoder doesn't need it. */
    void                 (* pf_bitstream_callback)( struct bit_stream_s *,
                                                    boolean_t b_new_pes );
    /* Optional argument to the callback */
    void *                  p_callback_arg;

    /*
     * PTS retrieval
     */
    mtime_t                 i_pts, i_dts;
    byte_t *                p_pts_validity;

    /*
     * Byte structures
     */
    /* Current data packet (in the current PES packet of the PES stream) */
    data_packet_t *         p_data;
    /* Pointer to the next byte that is to be read (in the current packet) */
    byte_t *                p_byte;
    /* Pointer to the last byte that is to be read (in the current packet */
    byte_t *                p_end;
    /* Temporary buffer in case we're not aligned when changing data packets */
    WORD_TYPE               i_showbits_buffer;
    data_packet_t           showbits_data;
} bit_stream_t;

/*****************************************************************************
 * Inline functions used by the decoders to read bit_stream_t
 *****************************************************************************/

/*
 * DISCUSSION : How to use the bit_stream structures
 *
 * sizeof(WORD_TYPE) (usually 32) bits are read at the same time, thus
 * minimizing the number of p_byte changes.
 * Bits are read via GetBits() or ShowBits.
 *
 * XXX : Be aware that if, in the forthcoming functions, i_bits > 24,
 * the data have to be already aligned on an 8-bit boundary, or wrong
 * results will be returned. Use RealignBits() if unsure.
 */

#if (WORD_TYPE == u32)
#   define WORD_AT      U32_AT
#   define WORD_SIGNED  s32
#elif (WORD_TYPE == u64)
#   define WORD_AT      U64_AT
#   define WORD_SIGNED  s64
#else
#   error Unsupported WORD_TYPE
#endif

/*****************************************************************************
 * Prototypes from input_ext-dec.c
 *****************************************************************************/
#ifndef __PLUGIN__
void InitBitstream  ( struct bit_stream_s *, struct decoder_fifo_s *,
                      void (* pf_bitstream_callback)( struct bit_stream_s *,
                                                      boolean_t ),
                      void * p_callback_arg );
boolean_t NextDataPacket( struct decoder_fifo_s *, struct data_packet_s ** );
void BitstreamNextDataPacket( struct bit_stream_s * );
u32  UnalignedShowBits( struct bit_stream_s *, unsigned int );
void UnalignedRemoveBits( struct bit_stream_s * );
u32  UnalignedGetBits( struct bit_stream_s *, unsigned int );
void CurrentPTS( struct bit_stream_s *, mtime_t *, mtime_t * );
#else
#   define InitBitstream p_symbols->InitBitstream
#   define NextDataPacket p_symbols->NextDataPacket
#   define BitstreamNextDataPacket p_symbols->BitstreamNextDataPacket
#   define UnalignedShowBits p_symbols->UnalignedShowBits
#   define UnalignedRemoveBits p_symbols->UnalignedRemoveBits
#   define UnalignedGetBits p_symbols->UnalignedGetBits
#   define CurrentPTS p_symbols->CurrentPTS
#endif

/*****************************************************************************
 * AlignWord : fill in the bit buffer so that the byte pointer be aligned
 * on a word boundary (XXX: there must be at least sizeof(WORD_TYPE) - 1
 * empty bytes in the bit buffer)
 *****************************************************************************/
static inline void AlignWord( bit_stream_t * p_bit_stream )
{
    while( (ptrdiff_t)p_bit_stream->p_byte
             & (sizeof(WORD_TYPE) - 1) )
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
            BitstreamNextDataPacket( p_bit_stream );
            p_bit_stream->fifo.buffer |= *(p_bit_stream->p_byte++)
                << (8 * sizeof(WORD_TYPE) - 8
                     - p_bit_stream->fifo.i_available);
            p_bit_stream->fifo.i_available += 8;
        }
    }
}

/*****************************************************************************
 * ShowBits : return i_bits bits from the bit stream
 *****************************************************************************/
static inline u32 ShowBits( bit_stream_t * p_bit_stream,
                            unsigned int i_bits )
{
    if( p_bit_stream->fifo.i_available >= i_bits )
    {
        return( p_bit_stream->fifo.buffer >> (8 * sizeof(WORD_TYPE) - i_bits) );
    }

    if( p_bit_stream->p_byte <= p_bit_stream->p_end - sizeof(WORD_TYPE) )
    {
        return( (p_bit_stream->fifo.buffer |
                    (WORD_AT( p_bit_stream->p_byte )
                        >> p_bit_stream->fifo.i_available))
                    >> (8 * sizeof(WORD_TYPE) - i_bits) );
    }

    return( UnalignedShowBits( p_bit_stream, i_bits ) );
}

/*****************************************************************************
 * ShowSignedBits : return i_bits bits from the bit stream, using signed
 *                  arithmetic
 *****************************************************************************/
static inline s32 ShowSignedBits( bit_stream_t * p_bit_stream,
                                  unsigned int i_bits )
{
    if( p_bit_stream->fifo.i_available >= i_bits )
    {
        return( (WORD_SIGNED)p_bit_stream->fifo.buffer
                    >> (8 * sizeof(WORD_TYPE) - i_bits) );
    }

    /* You can probably do something a little faster, but now I'm tired. */
    return( (WORD_SIGNED)(ShowBits( p_bit_stream, i_bits ) << (32 - i_bits))
             >> (32 - i_bits) );
}

/*****************************************************************************
 * RemoveBits : removes i_bits bits from the bit buffer
 *              XXX: do not use for 32 bits, see RemoveBits32
 *****************************************************************************/
static inline void RemoveBits( bit_stream_t * p_bit_stream,
                               unsigned int i_bits )
{
    p_bit_stream->fifo.i_available -= i_bits;

    if( p_bit_stream->fifo.i_available >= 0 )
    {
        p_bit_stream->fifo.buffer <<= i_bits;
        return;
    }

    if( p_bit_stream->p_byte <= p_bit_stream->p_end - sizeof(WORD_TYPE) )
    {
        p_bit_stream->fifo.buffer = WORD_AT( p_bit_stream->p_byte )
                                        << ( -p_bit_stream->fifo.i_available );
        ((WORD_TYPE *)p_bit_stream->p_byte)++;
        p_bit_stream->fifo.i_available += sizeof(WORD_TYPE) * 8;
        return;
    }

    UnalignedRemoveBits( p_bit_stream );
}

/*****************************************************************************
 * RemoveBits32 : removes 32 bits from the bit buffer (and as a side effect,
 *                refill it)
 *****************************************************************************/
#if (WORD_TYPE == u32)
static inline void RemoveBits32( bit_stream_t * p_bit_stream )
{
    if( p_bit_stream->p_byte <= p_bit_stream->p_end - sizeof(WORD_TYPE) )
    {
        if( p_bit_stream->fifo.i_available )
        {
            p_bit_stream->fifo.buffer = WORD_AT( p_bit_stream->p_byte )
                            << (32 - p_bit_stream->fifo.i_available);
            ((WORD_TYPE *)p_bit_stream->p_byte)++;
            return;
        }

        ((WORD_TYPE *)p_bit_stream->p_byte)++;
        return;
    }

    p_bit_stream->fifo.i_available -= 32;
    UnalignedRemoveBits( p_bit_stream );
}
#else
#   define RemoveBits32( p_bit_stream )     RemoveBits( p_bit_stream, 32 )
#endif

/*****************************************************************************
 * GetBits : returns i_bits bits from the bit stream and removes them
 *           XXX: do not use for 32 bits, see GetBits32
 *****************************************************************************/
static inline u32 GetBits( bit_stream_t * p_bit_stream,
                           unsigned int i_bits )
{
    u32             i_result;

    p_bit_stream->fifo.i_available -= i_bits;

    if( p_bit_stream->fifo.i_available >= 0 )
    {
        i_result = p_bit_stream->fifo.buffer
                        >> (8 * sizeof(WORD_TYPE) - i_bits);
        p_bit_stream->fifo.buffer <<= i_bits;
        return( i_result );
    }

    if( p_bit_stream->p_byte <= p_bit_stream->p_end - sizeof(WORD_TYPE) )
    {
        i_result = p_bit_stream->fifo.buffer
                        >> (8 * sizeof(WORD_TYPE) - i_bits);
        p_bit_stream->fifo.buffer = WORD_AT( p_bit_stream->p_byte );
        ((WORD_TYPE *)p_bit_stream->p_byte)++;
        i_result |= p_bit_stream->fifo.buffer
                        >> (8 * sizeof(WORD_TYPE)
                                     + p_bit_stream->fifo.i_available);
        p_bit_stream->fifo.buffer <<= ( -p_bit_stream->fifo.i_available );
        p_bit_stream->fifo.i_available += sizeof(WORD_TYPE) * 8;
        return( i_result );
    }

    return UnalignedGetBits( p_bit_stream, i_bits );
}

/*****************************************************************************
 * GetSignedBits : returns i_bits bits from the bit stream and removes them,
 *                 using signed arithmetic
 *                 XXX: do not use for 32 bits
 *****************************************************************************/
static inline s32 GetSignedBits( bit_stream_t * p_bit_stream,
                                 unsigned int i_bits )
{
    if( p_bit_stream->fifo.i_available >= i_bits )
    {
        s32             i_result;

        p_bit_stream->fifo.i_available -= i_bits;
        i_result = (WORD_SIGNED)p_bit_stream->fifo.buffer
                        >> (8 * sizeof(WORD_TYPE) - i_bits);
        p_bit_stream->fifo.buffer <<= i_bits;
        return( i_result );
    }

    /* You can probably do something a little faster, but now I'm tired. */
    return( (WORD_SIGNED)(GetBits( p_bit_stream, i_bits ) << (32 - i_bits))
             >> (32 - i_bits) );
}

/*****************************************************************************
 * GetBits32 : returns 32 bits from the bit stream and removes them
 *****************************************************************************/
#if (WORD_TYPE == u32)
static inline u32 GetBits32( bit_stream_t * p_bit_stream )
{
    u32             i_result;

    if( p_bit_stream->fifo.i_available == 32 )
    {
        p_bit_stream->fifo.i_available = 0;
        i_result = p_bit_stream->fifo.buffer;
        p_bit_stream->fifo.buffer = 0;
        return( i_result );
    }

    if( p_bit_stream->p_byte <= p_bit_stream->p_end - sizeof(WORD_TYPE) )
    {
        if( p_bit_stream->fifo.i_available )
        {
            i_result = p_bit_stream->fifo.buffer;
            p_bit_stream->fifo.buffer = WORD_AT( p_bit_stream->p_byte );
            ((WORD_TYPE *)p_bit_stream->p_byte)++;
            i_result |= p_bit_stream->fifo.buffer
                             >> (p_bit_stream->fifo.i_available);
            p_bit_stream->fifo.buffer <<= (32 - p_bit_stream->fifo.i_available);
            return( i_result );
        }

        i_result = WORD_AT( p_bit_stream->p_byte );
        ((WORD_TYPE *)p_bit_stream->p_byte)++;
        return( i_result );
    }

    p_bit_stream->fifo.i_available -= 32;
    return UnalignedGetBits( p_bit_stream, 32 );
}
#else
#   define GetBits32( p_bit_stream )    GetBits( p_bit_stream, 32 )
#endif

/*****************************************************************************
 * RealignBits : realigns the bit buffer on an 8-bit boundary
 *****************************************************************************/
static inline void RealignBits( bit_stream_t * p_bit_stream )
{
    p_bit_stream->fifo.buffer <<= (p_bit_stream->fifo.i_available & 0x7);
    p_bit_stream->fifo.i_available &= ~0x7;
}


/*****************************************************************************
 * GetChunk : reads a large chunk of data
 *****************************************************************************
 * The position in the stream must be byte-aligned, if unsure call
 * RealignBits(). p_buffer must point to a buffer at least as big as i_buf_len
 * otherwise your code will crash.
 *****************************************************************************/
static inline void GetChunk( bit_stream_t * p_bit_stream,
                             byte_t * p_buffer, size_t i_buf_len )
{
    ptrdiff_t           i_available;

    /* We need to take care because i_buf_len may be < 4. */
    while( p_bit_stream->fifo.i_available > 0 && i_buf_len )
    {
        *p_buffer = p_bit_stream->fifo.buffer >> (8 * sizeof(WORD_TYPE) - 8);
        p_buffer++;
        i_buf_len--;
        p_bit_stream->fifo.buffer <<= 8;
        p_bit_stream->fifo.i_available -= 8;
    }

    if( (i_available = p_bit_stream->p_end - p_bit_stream->p_byte)
            >= i_buf_len )
    {
        FAST_MEMCPY( p_buffer, p_bit_stream->p_byte, i_buf_len );
        p_bit_stream->p_byte += i_buf_len;
    }
    else
    {
        do
        {
            FAST_MEMCPY( p_buffer, p_bit_stream->p_byte, i_available );
            p_bit_stream->p_byte = p_bit_stream->p_end;
            p_buffer += i_available;
            i_buf_len -= i_available;
            BitstreamNextDataPacket( p_bit_stream );
        }
        while( (i_available = p_bit_stream->p_end - p_bit_stream->p_byte)
                <= i_buf_len && !p_bit_stream->p_decoder_fifo->b_die );

        if( i_buf_len )
        {
            FAST_MEMCPY( p_buffer, p_bit_stream->p_byte, i_buf_len );
            p_bit_stream->p_byte += i_buf_len;
        }
    }

    if( p_bit_stream->p_byte <= p_bit_stream->p_end - sizeof(WORD_TYPE) )
    {
        AlignWord( p_bit_stream );
    }
}


/*
 * Communication interface between input and decoders
 */

/*****************************************************************************
 * decoder_config_t
 *****************************************************************************
 * Standard pointers given to the decoders as a toolbox.
 *****************************************************************************/
typedef struct decoder_config_s
{
    u16                     i_id;
    u8                      i_type;         /* type of the elementary stream */

    void *                  p_demux_data;
    struct stream_ctrl_s *  p_stream_ctrl;
    struct decoder_fifo_s * p_decoder_fifo;
} decoder_config_t;

/*****************************************************************************
 * Prototypes from input_dec.c
 *****************************************************************************/
#ifndef __PLUGIN__
void DecoderError      ( struct decoder_fifo_s * p_fifo );
#else
#   define DecoderError p_symbols->DecoderError
#endif

