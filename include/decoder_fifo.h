/******************************************************************************
 * decoder_fifo.h: interface for decoders PES fifo
 * (c)1999 VideoLAN
 ******************************************************************************
 * Required headers:
 * - <pthread.h>
 * - "config.h"
 * - "common.h"
 * - "input.h"
 ******************************************************************************/

/******************************************************************************
 * Macros
 ******************************************************************************/

/* ?? move to inline functions */
#define DECODER_FIFO_ISEMPTY( fifo )    ( (fifo).i_start == (fifo).i_end )
#define DECODER_FIFO_ISFULL( fifo )     ( ( ( (fifo).i_end + 1 - (fifo).i_start ) \
                                          & FIFO_SIZE ) == 0 )
#define DECODER_FIFO_START( fifo )      ( (fifo).buffer[ (fifo).i_start ] )
#define DECODER_FIFO_INCSTART( fifo )   ( (fifo).i_start = ((fifo).i_start + 1) \
                                                           & FIFO_SIZE ) 
#define DECODER_FIFO_END( fifo )        ( (fifo).buffer[ (fifo).i_end ] )
#define DECODER_FIFO_INCEND( fifo )     ( (fifo).i_end = ((fifo).i_end + 1) \
                                                         & FIFO_SIZE )

/******************************************************************************
 * decoder_fifo_t
 ******************************************************************************
 * This rotative FIFO contains PES packets that are to be decoded...
 ******************************************************************************/
typedef struct
{
    pthread_mutex_t     data_lock;                          /* fifo data lock */
    pthread_cond_t      data_wait;          /* fifo data conditional variable */

    /* buffer is an array of PES packets pointers */
    pes_packet_t *      buffer[FIFO_SIZE + 1];
    int                 i_start;
    int                 i_end;

} decoder_fifo_t;

/****************************************************************************** * bit_fifo_t : bit fifo descriptor
 ****************************************************************************** * This type describes a bit fifo used to store bits while working with the
 * input stream at the bit level.
 ******************************************************************************/
typedef struct bit_fifo_s
{
    /* This unsigned integer allows us to work at the bit level. This buffer
     * can contain 32 bits, and the used space can be found on the MSb's side
     * and the available space on the LSb's side. */
    u32                 buffer;

    /* Number of bits available in the bit buffer */
    int                 i_available;

} bit_fifo_t;

/****************************************************************************** * bit_stream_t : bit stream descriptor
 ****************************************************************************** * This type, based on a PES stream, includes all the structures needed to
 * handle the input stream like a bit stream.
 ******************************************************************************/
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
    /* Index of the next byte that is to be read (in the current TS packet) */
    unsigned int        i_byte;

    /*
     * Bit structures
     */
    bit_fifo_t          fifo;

} bit_stream_t;


/*****************************************************************************
 * GetByte : reads the next byte in the input stream
 *****************************************************************************/
static __inline__ byte_t GetByte( bit_stream_t * p_bit_stream )
{
    /* Are there some bytes left in the current TS packet ? */
    if ( p_bit_stream->i_byte < p_bit_stream->p_ts->i_payload_end )
    {
        return( p_bit_stream->p_ts->buffer[ p_bit_stream->i_byte++ ] );
    }
    else
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
                pthread_mutex_lock( &p_bit_stream->p_decoder_fifo->data_lock );
                /* We should increase the start index of the decoder fifo, but
                 * if we do this now, the input thread could overwrite the
                 * pointer to the current PES packet, and we weren't able to
                 * give it back to the netlist. That's why we free the PES
                 * packet first. */
                input_NetlistFreePES( p_bit_stream->p_input, DECODER_FIFO_START(*p_bit_stream->p_decoder_fifo) );
                DECODER_FIFO_INCSTART( *p_bit_stream->p_decoder_fifo );

                /* !! b_die !! */
                while ( DECODER_FIFO_ISEMPTY(*p_bit_stream->p_decoder_fifo) )
                {
                    pthread_cond_wait( &p_bit_stream->p_decoder_fifo->data_wait,
                                       &p_bit_stream->p_decoder_fifo->data_lock );
                }

                /* The next byte could be found in the next PES packet */
                p_bit_stream->p_ts = DECODER_FIFO_START( *p_bit_stream->p_decoder_fifo )->p_first_ts;

                /* We can release the fifo's data lock */
                pthread_mutex_unlock( &p_bit_stream->p_decoder_fifo->data_lock );
            }
            /* Perhaps the next TS packet of the current PES packet contains
             * real data (ie its payload's size is greater than 0) */
            else
            {
                p_bit_stream->p_ts = p_bit_stream->p_ts->p_next_ts;
            }
        } while ( p_bit_stream->p_ts->i_payload_start == p_bit_stream->p_ts->i_payload_end );

        /* We've found a TS packet which contains interesting data... As we
         * return the payload's first byte, we set i_byte to the following
         * one */
        p_bit_stream->i_byte = p_bit_stream->p_ts->i_payload_start;
        return( p_bit_stream->p_ts->buffer[ p_bit_stream->i_byte++ ] );
    }
}

/****************************************************************************** * NeedBits : reads i_bits new bits in the bit stream and stores them in the
 *            bit buffer
 ****************************************************************************** * - i_bits must be less or equal 32 !
 * - There is something important to notice with that function : if the number
 * of bits available in the bit buffer when calling NeedBits() is greater than
 * 24 (i_available > 24) but less than the number of needed bits
 * (i_available < i_bits), the byte returned by GetByte() will be shifted with
 * a negative value and the number of bits available in the bit buffer will be
 * set to more than 32 !
 ******************************************************************************/
static __inline__ void NeedBits( bit_stream_t * p_bit_stream, int i_bits )
{
    while ( p_bit_stream->fifo.i_available < i_bits )
    {
        p_bit_stream->fifo.buffer |= ((u32)GetByte( p_bit_stream )) << (24 - p_bit_stream->fifo.i_available);
        p_bit_stream->fifo.i_available += 8;
    }
}

/****************************************************************************** * DumpBits : removes i_bits bits from the bit buffer
 ****************************************************************************** * - i_bits <= i_available
 * - i_bits < 32 (because (u32 << 32) <=> (u32 = u32))
 ******************************************************************************/
static __inline__ void DumpBits( bit_stream_t * p_bit_stream, int i_bits )
{
    p_bit_stream->fifo.buffer <<= i_bits;
    p_bit_stream->fifo.i_available -= i_bits;
}

