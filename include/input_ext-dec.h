/*****************************************************************************
 * input_ext-dec.h: structures exported to the VideoLAN decoders
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: input_ext-dec.h,v 1.5 2000/12/22 17:53:30 massiot Exp $
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

/* Structures exported to the decoders */

/*****************************************************************************
 * data_packet_t
 *****************************************************************************
 * Describe a data packet.
 *****************************************************************************/
typedef struct data_packet_s
{
    /* Nothing before this line, the code relies on that */
    byte_t *                p_buffer;                     /* raw data packet */

    /* Decoders information */
    byte_t *                p_payload_start;
                                  /* start of the PES payload in this packet */
    byte_t *                p_payload_end;                    /* guess ? :-) */
    boolean_t               b_discard_payload;  /* is the packet messed up ? */

    /* Used to chain the TS packets that carry data for a same PES or PSI */
    struct data_packet_s *  p_next;
} data_packet_t;

/*****************************************************************************
 * pes_packet_t
 *****************************************************************************
 * Describes an PES packet, with its properties, and pointers to the TS packets
 * containing it.
 *****************************************************************************/
typedef struct pes_packet_s
{
    /* PES properties */
    boolean_t               b_messed_up;  /* At least one of the data packets
                                           * has a questionable content      */
    boolean_t               b_data_alignment;  /* used to find the beginning of
                                                * a video or audio unit      */
    boolean_t               b_discontinuity; /* This packet doesn't follow the
                                              * previous one                 */

    boolean_t               b_has_pts;       /* is the following field set ? */
    mtime_t                 i_pts; /* the PTS for this packet (if set above) */

    int                     i_pes_size;    /* size of the current PES packet */

    /* Pointers to packets (packets are then linked by the p_prev and
       p_next fields of the data_packet_t struct) */
    data_packet_t *         p_first;      /* The first packet containing this
                                           * PES (used by decoders). */
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
    pes_packet_t *          buffer[FIFO_SIZE + 1];
    int                     i_start;
    int                     i_end;

    /* Communication interface between input and decoders */
    boolean_t               b_die;          /* the decoder should return now */
    boolean_t               b_error;      /* the decoder is in an error loop */
    void *                  p_packets_mgt;   /* packets management services
                                              * data (netlist...)            */
    void                 (* pf_delete_pes)( void *, pes_packet_t * );
                                     /* function to use when releasing a PES */
} decoder_fifo_t;

/* Macros to manage a decoder_fifo_t structure. Please remember to take
 * data_lock before using them. */
#define DECODER_FIFO_ISEMPTY( fifo )  ( (fifo).i_start == (fifo).i_end )
#define DECODER_FIFO_ISFULL( fifo )   ( ( ((fifo).i_end + 1 - (fifo).i_start)\
                                          & FIFO_SIZE ) == 0 )
#define DECODER_FIFO_START( fifo )    ( (fifo).buffer[ (fifo).i_start ] )
#define DECODER_FIFO_INCSTART( fifo ) ( (fifo).i_start = ((fifo).i_start + 1)\
                                                         & FIFO_SIZE )
#define DECODER_FIFO_END( fifo )      ( (fifo).buffer[ (fifo).i_end ] )
#define DECODER_FIFO_INCEND( fifo )   ( (fifo).i_end = ((fifo).i_end + 1) \
                                                       & FIFO_SIZE )

/*****************************************************************************
 * bit_fifo_t : bit fifo descriptor
 *****************************************************************************
 * This type describes a bit fifo used to store bits while working with the
 * input stream at the bit level.
 *****************************************************************************/
typedef u32         WORD_TYPE;        /* only u32 is supported at the moment */

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
     * Input structures
     */
    /* The decoder fifo contains the data of the PES stream */
    decoder_fifo_t *        p_decoder_fifo;

    /* Function to jump to the next data packet */
    void                 (* pf_next_data_packet)( struct bit_stream_s * );

    /*
     * Byte structures
     */
    /* Current data packet (in the current PES packet of the PES stream) */
    data_packet_t *         p_data;
    /* Pointer to the next byte that is to be read (in the current TS packet) */
    byte_t *                p_byte;
    /* Pointer to the last byte that is to be read (in the current TS packet */
    byte_t *                p_end;

    /*
     * Bit structures
     */
    bit_fifo_t              fifo;
} bit_stream_t;

/*****************************************************************************
 * Inline functions used by the decoders to read bit_stream_t
 *****************************************************************************/

/*
 * Philosophy of the first implementation : the bit buffer is first filled by
 * NeedBits, then the buffer can be read via p_bit_stream->fifo.buffer, and
 * unnecessary bits are dumped with a DumpBits() call.
 */

/*****************************************************************************
 * GetByte : reads the next byte in the input stream
 *****************************************************************************/
static __inline__ byte_t GetByte( bit_stream_t * p_bit_stream )
{
    /* Are there some bytes left in the current data packet ? */
    /* could change this test to have a if (! (bytes--)) instead */
    if ( p_bit_stream->p_byte >= p_bit_stream->p_end )
    {
        /* no, switch to next data packet */
        p_bit_stream->pf_next_data_packet( p_bit_stream );
    }

    return( *(p_bit_stream->p_byte++) );
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
        p_bit_stream->fifo.buffer |= ((WORD_TYPE)GetByte( p_bit_stream ))
                                     << (sizeof(WORD_TYPE) - 8
                                            - p_bit_stream->fifo.i_available);
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


/*
 * Philosophy of the second implementation : WORD_LENGTH (usually 32) bits
 * are read at the same time, thus minimizing the number of p_byte changes.
 * Bits are read via GetBits() or ShowBits. This is slightly faster. Be
 * aware that if, in the forthcoming functions, i_bits > 24, the data have to
 * be already aligned on an 8-bit boundary, or wrong results will be
 * returned.
 */

#if (WORD_TYPE != u32)
#   error Not supported word
#endif

/*
 * This is stolen from the livid source who stole it from the kernel
 * FIXME: The macro swab32 for little endian machines does
 *        not seem to work correctly
 */

#if defined(SYS_BEOS)
#   define swab32(x) B_BENDIAN_TO_HOST_INT32(x)
#else
#   if __BYTE_ORDER == __BIG_ENDIAN
#       define swab32(x) (x)
#   else
#       if defined (__i386__)
static __inline__ const u32 __i386_swab32( u32 x )
{
    __asm__("bswap %0" : "=r" (x) : "0" (x));
    return x;
}
#           define swab32(x) __i386_swab32(x)
#       else
#           define swab32(x)                                                 \
            ( ( (u32)(((u8*)&x)[0]) << 24 ) | ( (u32)(((u8*)&x)[1]) << 16 ) |\
              ( (u32)(((u8*)&x)[2]) << 8 )  | ( (u32)(((u8*)&x)[3])) )
#       endif
#   endif
#endif

/*****************************************************************************
 * ShowBits : return i_bits bits from the bit stream
 *****************************************************************************/
static __inline__ WORD_TYPE ShowWord( bit_stream_t * p_bit_stream )
{
    if( p_bit_stream->p_byte <= p_bit_stream->p_end - sizeof(WORD_TYPE) )
    {
        return( swab32( *((WORD_TYPE *)p_bit_stream->p_byte) ) );
    }

    p_bit_stream->pf_next_data_packet( p_bit_stream );
    return( swab32( *((WORD_TYPE *)p_bit_stream->p_byte) ) );
}

static __inline__ WORD_TYPE ShowBits( bit_stream_t * p_bit_stream, int i_bits )
{
    if( p_bit_stream->fifo.i_available >= i_bits )
    {
        return( p_bit_stream->fifo.buffer >> (8 * sizeof(WORD_TYPE) - i_bits) );
    }

    return( (p_bit_stream->fifo.buffer |
            (ShowWord( p_bit_stream ) >> p_bit_stream->fifo.i_available))
                    >> (8 * sizeof(WORD_TYPE) - i_bits) );
}

/*****************************************************************************
 * GetWord : returns the next word to be read
 *****************************************************************************/
static __inline__ WORD_TYPE GetWord( bit_stream_t * p_bit_stream )
{
    if( p_bit_stream->p_byte <= p_bit_stream->p_end - sizeof(WORD_TYPE) )
    {
        return( swab32( *(((WORD_TYPE *)p_bit_stream->p_byte)++) ) );
    }
    else
    {
        p_bit_stream->pf_next_data_packet( p_bit_stream );
        return( swab32( *(((WORD_TYPE *)p_bit_stream->p_byte)++) ) );
    }
}

/*****************************************************************************
 * RemoveBits : removes i_bits bits from the bit buffer
 *****************************************************************************/
static __inline__ void RemoveBits( bit_stream_t * p_bit_stream, int i_bits )
{
    p_bit_stream->fifo.i_available -= i_bits;

    if( p_bit_stream->fifo.i_available >= 0 )
    {
        p_bit_stream->fifo.buffer <<= i_bits;
        return;
    }
    p_bit_stream->fifo.buffer = GetWord( p_bit_stream )
                            << ( -p_bit_stream->fifo.i_available );
    p_bit_stream->fifo.i_available += sizeof(WORD_TYPE) * 8;
}

/*****************************************************************************
 * RemoveBits32 : removes 32 bits from the bit buffer (and as a side effect,
 *                refill it). This should be faster than RemoveBits, though
 *                RemoveBits will work, too.
 *****************************************************************************/
static __inline__ void RemoveBits32( bit_stream_t * p_bit_stream )
{
    p_bit_stream->fifo.buffer = GetWord( p_bit_stream )
                        << (32 - p_bit_stream->fifo.i_available);
}

/*****************************************************************************
 * GetBits : returns i_bits bits from the bit stream and removes them
 *****************************************************************************/
static __inline__ WORD_TYPE GetBits( bit_stream_t * p_bit_stream, int i_bits )
{
    u32             i_result;

    p_bit_stream->fifo.i_available -= i_bits;
    if( p_bit_stream->fifo.i_available >= 0 )
    {
        i_result = p_bit_stream->fifo.buffer >> (8 * sizeof(WORD_TYPE) - i_bits);
        p_bit_stream->fifo.buffer <<= i_bits;
        return( i_result );
    }

    i_result = p_bit_stream->fifo.buffer >> (8 * sizeof(WORD_TYPE) - i_bits);
    p_bit_stream->fifo.buffer = GetWord( p_bit_stream );
    i_result |= p_bit_stream->fifo.buffer
                             >> (8 * sizeof(WORD_TYPE)
                                     + p_bit_stream->fifo.i_available);
    p_bit_stream->fifo.buffer <<= ( -p_bit_stream->fifo.i_available );
    p_bit_stream->fifo.i_available += sizeof(WORD_TYPE) * 8;

    return( i_result );
}

/*****************************************************************************
 * GetBits32 : returns 32 bits from the bit stream and removes them
 *****************************************************************************/
static __inline__ WORD_TYPE GetBits32( bit_stream_t * p_bit_stream )
{
    WORD_TYPE               i_result;

    i_result = p_bit_stream->fifo.buffer;
    p_bit_stream->fifo.buffer = GetWord( p_bit_stream );
    i_result |= p_bit_stream->fifo.buffer
                             >> (p_bit_stream->fifo.i_available);
    p_bit_stream->fifo.buffer <<= (8 * sizeof(WORD_TYPE)
                                    - p_bit_stream->fifo.i_available);
    
    return( i_result );
}

/*****************************************************************************
 * RealignBits : realigns the bit buffer on an 8-bit boundary
 *****************************************************************************/
static __inline__ void RealignBits( bit_stream_t * p_bit_stream )
{
    p_bit_stream->fifo.buffer <<= (p_bit_stream->fifo.i_available & 0x7);
    p_bit_stream->fifo.i_available &= ~0x7;
}


/*
 * Philosophy of the third implementation : the decoder asks for n bytes,
 * and we will copy them in its buffer.
 */

/*****************************************************************************
 * GetChunk : reads a large chunk of data
 *****************************************************************************
 * The position in the stream must be byte-aligned, if unsure call
 * RealignBits(). p_buffer must to a buffer at least as big as i_buf_len
 * otherwise your code will crash.
 *****************************************************************************/
static __inline__ void GetChunk( bit_stream_t * p_bit_stream,
                                 byte_t * p_buffer, size_t i_buf_len )
{
    int     i_available;

    if( (i_available = p_bit_stream->p_end - p_bit_stream->p_byte)
            >= i_buf_len )
    {
        memcpy( p_buffer, p_bit_stream->p_byte, i_buf_len );
        p_bit_stream->p_byte += i_buf_len;
    }
    else
    {
        do
        {
            memcpy( p_buffer, p_bit_stream->p_byte, i_available );
            p_bit_stream->p_byte = p_bit_stream->p_end;
            p_buffer += i_available;
            i_buf_len -= i_available;
            p_bit_stream->pf_next_data_packet( p_bit_stream );
        }
        while( (i_available = p_bit_stream->p_end - p_bit_stream->p_byte)
                <= i_buf_len );

        if( i_buf_len )
        {
            memcpy( p_buffer, p_bit_stream->p_byte, i_buf_len );
            p_bit_stream->p_byte += i_buf_len;
        }
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

    struct stream_ctrl_s *  p_stream_ctrl;
    struct decoder_fifo_s * p_decoder_fifo;
    void                 (* pf_init_bit_stream)( struct bit_stream_s *,
                                                 struct decoder_fifo_s * );
} decoder_config_t;

/*****************************************************************************
 * vdec_config_t
 *****************************************************************************
 * Pointers given to video decoders threads.
 *****************************************************************************/
struct vout_thread_s;

typedef struct vdec_config_s
{
    struct vout_thread_s *  p_vout;

    struct picture_s *   (* pf_create_picture)( struct vout_thread_s *,
                                                int i_type, int i_width,
                                                int i_height );
    void                 (* pf_destroy_picture)( struct vout_thread_s *,
                                                 struct picture_s * );
    void                 (* pf_display_picture)( struct vout_thread_s *,
                                                 struct picture_s * );
    void                 (* pf_date_picture)( struct vout_thread_s *,
                                              struct picture_s *, mtime_t date );
    void                 (* pf_link_picture)( struct vout_thread_s *,
                                              struct picture_s *, mtime_t date );
    void                 (* pf_unlink_picture)( struct vout_thread_s *,
                                                struct picture_s *, mtime_t date );
    struct subpicture_s *(* pf_create_subpicture)( struct vout_thread_s *,
                                                   int i_type, int i_size );
    void                 (* pf_destroy_subpicture)( struct vout_thread_s *,
                                                    struct subpicture_s * );
    void                 (* pf_display_subpicture)( struct vout_thread_s *,
                                                    struct subpicture_s * );

    decoder_config_t        decoder_config;
} vdec_config_t;

/*****************************************************************************
 * adec_config_t
 *****************************************************************************
 * Pointers given to audio decoders threads.
 *****************************************************************************/
struct aout_thread_s;

typedef struct adec_config_s
{
    struct aout_thread_s *  p_aout;

    struct aout_fifo_s * (* pf_create_fifo)( struct aout_thread_s *,
                                            struct aout_fifo_s * );
    void                 (* pf_destroy_fifo)( struct aout_thread_s *);

    decoder_config_t        decoder_config;
} adec_config_t;


/*
 * Communication interface between decoders and input
 */

/*****************************************************************************
 * decoder_capabilities_t
 *****************************************************************************
 * Structure returned by a call to GetCapabilities() of the decoder.
 *****************************************************************************/
typedef struct decoder_capabilities_s
{
    int                     i_dec_type;
    u8                      i_stream_type;   /* == i_type in es_descriptor_t */
    int                     i_weight; /* for a given stream type, the decoder
                                       * with higher weight will be spawned  */

    vlc_thread_t         (* pf_create_thread)( void * );
} decoder_capabilities_t;

/* Decoder types */
#define NONE_D              0
#define VIDEO_D             1
#define AUDIO_D             2
