/***************************************************************************** 
 * decoder_fifo.c: auxiliaries functions used in decoder_fifo.h
 * (c)1998 VideoLAN 
 *****************************************************************************/

#include <sys/uio.h>

#include "config.h"
#include "common.h"
#include "mtime.h"
#include "vlc_thread.h"

#include "debug.h"                    /* ?? temporaire, requis par netlist.h */

#include "input.h"
#include "input_netlist.h"
#include "decoder_fifo.h"

void decoder_fifo_next( bit_stream_t * p_bit_stream )
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
	    vlc_mutex_lock( &p_bit_stream->p_decoder_fifo->data_lock );

	    /* Is the input thread dying ? */
	    if ( p_bit_stream->p_input->b_die )
	    {
		vlc_mutex_unlock( &(p_bit_stream->p_decoder_fifo->data_lock) );
		return;
	    }

	    /* We should increase the start index of the decoder fifo, but
	     * if we do this now, the input thread could overwrite the
	     * pointer to the current PES packet, and we weren't able to
	     * give it back to the netlist. That's why we free the PES
	     * packet first. */
	    input_NetlistFreePES( p_bit_stream->p_input, DECODER_FIFO_START(*p_bit_stream->p_decoder_fifo) );
	    DECODER_FIFO_INCSTART( *p_bit_stream->p_decoder_fifo );

	    while ( DECODER_FIFO_ISEMPTY(*p_bit_stream->p_decoder_fifo) )
	    {
		vlc_cond_wait( &p_bit_stream->p_decoder_fifo->data_wait, &p_bit_stream->p_decoder_fifo->data_lock );
		if ( p_bit_stream->p_input->b_die )
		{
		    vlc_mutex_unlock( &(p_bit_stream->p_decoder_fifo->data_lock) );
		    return;
		}
	    }

	    /* The next byte could be found in the next PES packet */
	    p_bit_stream->p_ts = DECODER_FIFO_START( *p_bit_stream->p_decoder_fifo )->p_first_ts;

	    /* We can release the fifo's data lock */
	    vlc_mutex_unlock( &p_bit_stream->p_decoder_fifo->data_lock );
	}
	/* Perhaps the next TS packet of the current PES packet contains
	 * real data (ie its payload's size is greater than 0) */
	else
	{
	    p_bit_stream->p_ts = p_bit_stream->p_ts->p_next_ts;
	}
    } while ( p_bit_stream->p_ts->i_payload_start == p_bit_stream->p_ts->i_payload_end );

    /* We've found a TS packet which contains interesting data... */
    p_bit_stream->p_byte = p_bit_stream->p_ts->buffer + p_bit_stream->p_ts->i_payload_start;
    p_bit_stream->p_end = p_bit_stream->p_ts->buffer + p_bit_stream->p_ts->i_payload_end;
}
