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
