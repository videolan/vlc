/*****************************************************************************
 * ac3_decoder.c: ac3 decoder thread
 * (c)1999 VideoLAN
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>                                              /* getpid() */

#include <stdio.h>                                           /* "intf_msg.h" */
#include <stdlib.h>                                      /* malloc(), free() */
#include <sys/soundcard.h>                               /* "audio_output.h" */
#include <sys/uio.h>                                            /* "input.h" */

#include "common.h"
#include "config.h"
#include "mtime.h"
#include "vlc_thread.h"
#include "debug.h"                                      /* "input_netlist.h" */

#include "intf_msg.h"                        /* intf_DbgMsg(), intf_ErrMsg() */

#include "input.h"                                           /* pes_packet_t */
#include "input_netlist.h"                         /* input_NetlistFreePES() */
#include "decoder_fifo.h"         /* DECODER_FIFO_(ISEMPTY|START|INCSTART)() */

#include "audio_output.h"

#include "ac3_decoder.h"
#include "ac3_parse.h"
#include "ac3_exponent.h"
#include "ac3_bit_allocate.h"
#include "ac3_mantissa.h"
#include "ac3_rematrix.h"
#include "ac3_imdct.h"
#include "ac3_downmix.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int      InitThread              ( ac3dec_thread_t * p_adec );
static void     RunThread               ( ac3dec_thread_t * p_adec );
static void     ErrorThread             ( ac3dec_thread_t * p_adec );
static void     EndThread               ( ac3dec_thread_t * p_adec );

/*****************************************************************************
 * ac3dec_CreateThread: creates an ac3 decoder thread
 *****************************************************************************/
ac3dec_thread_t * ac3dec_CreateThread( input_thread_t * p_input )
{
    ac3dec_thread_t *   p_ac3dec;

    intf_DbgMsg("ac3dec debug: creating ac3 decoder thread\n");

    /* Allocate the memory needed to store the thread's structure */
    if ( (p_ac3dec = (ac3dec_thread_t *)malloc( sizeof(ac3dec_thread_t) )) == NULL )
    {
        intf_ErrMsg("ac3dec error: not enough memory for ac3dec_CreateThread() to create the new thread\n");
        return( NULL );
    }

    /*
     * Initialize the thread properties
     */
    p_ac3dec->b_die = 0;
    p_ac3dec->b_error = 0;

    /*
     * Initialize the input properties
     */
    /* Initialize the decoder fifo's data lock and conditional variable and set
     * its buffer as empty */
    vlc_mutex_init( &p_ac3dec->fifo.data_lock );
    vlc_cond_init( &p_ac3dec->fifo.data_wait );
    p_ac3dec->fifo.i_start = 0;
    p_ac3dec->fifo.i_end = 0;
    /* Initialize the bit stream structure */
    p_ac3dec->bit_stream.p_input = p_input;
    p_ac3dec->bit_stream.p_decoder_fifo = &p_ac3dec->fifo;
    p_ac3dec->bit_stream.fifo.buffer = 0;
    p_ac3dec->bit_stream.fifo.i_available = 0;

    /*
     * Initialize the output properties
     */
    p_ac3dec->p_aout = p_input->p_aout;
    p_ac3dec->p_aout_fifo = NULL;

    imdct_init();

    /* Spawn the ac3 decoder thread */
    if ( vlc_thread_create(&p_ac3dec->thread_id, "ac3 decoder", (vlc_thread_func_t)RunThread, (void *)p_ac3dec) )
    {
        intf_ErrMsg( "ac3dec error: can't spawn ac3 decoder thread\n" );
        free( p_ac3dec );
        return( NULL );
    }

    intf_DbgMsg( "ac3dec debug: ac3 decoder thread (%p) created\n", p_ac3dec );
    return( p_ac3dec );
}

/*****************************************************************************
 * ac3dec_DestroyThread: destroys an ac3 decoder thread
 *****************************************************************************/
void ac3dec_DestroyThread( ac3dec_thread_t * p_ac3dec )
{
    intf_DbgMsg( "ac3dec debug: requesting termination of ac3 decoder thread %p\n", p_ac3dec );

    /* Ask thread to kill itself */
    p_ac3dec->b_die = 1;
    /* Make sure the decoder thread leaves the GetByte() function */
    vlc_mutex_lock( &(p_ac3dec->fifo.data_lock) );
    vlc_cond_signal( &(p_ac3dec->fifo.data_wait) );
    vlc_mutex_unlock( &(p_ac3dec->fifo.data_lock) );

    /* Waiting for the decoder thread to exit */
    /* Remove this as soon as the "status" flag is implemented */
    vlc_thread_join( p_ac3dec->thread_id );
}

/* Following functions are local */

/*****************************************************************************
 * decode_find_sync()
 *****************************************************************************/
static __inline__ int decode_find_sync( ac3dec_thread_t * p_ac3dec )
{
#ifdef AC3_SIGSEGV
    int i = 0;
#endif

    while ( (!p_ac3dec->b_die) && (!p_ac3dec->b_error) )
    {
        NeedBits( &(p_ac3dec->bit_stream), 16 );
        if ( (p_ac3dec->bit_stream.fifo.buffer >> (32 - 16)) == 0x0b77 )
        {
            DumpBits( &(p_ac3dec->bit_stream), 16 );
            p_ac3dec->total_bits_read = 16;
#ifdef AC3_SIGSEGV
	    if ( i )
	    {
		fprintf( stderr, "ac3dec debug: %i bit(s) skipped to synkronize\n", i );
	    }
#endif
            return( 0 );
        }
#ifdef AC3_SIGSEGV
        DumpBits( &(p_ac3dec->bit_stream), 1 );
	i += 1;
#else
        DumpBits( &(p_ac3dec->bit_stream), 8 );
#endif
    }
    return( -1 );
}

/*****************************************************************************
 * InitThread : initialize an ac3 decoder thread
 *****************************************************************************/
static int InitThread( ac3dec_thread_t * p_ac3dec )
{
    aout_fifo_t         aout_fifo;

    intf_DbgMsg( "ac3dec debug: initializing ac3 decoder thread %p\n", p_ac3dec );

    /* Our first job is to initialize the bit stream structure with the
     * beginning of the input stream */
    vlc_mutex_lock( &p_ac3dec->fifo.data_lock );
    while ( DECODER_FIFO_ISEMPTY(p_ac3dec->fifo) )
    {
        vlc_cond_wait( &p_ac3dec->fifo.data_wait, &p_ac3dec->fifo.data_lock );
    }
    p_ac3dec->bit_stream.p_ts = DECODER_FIFO_START( p_ac3dec->fifo )->p_first_ts;
    p_ac3dec->bit_stream.i_byte = p_ac3dec->bit_stream.p_ts->i_payload_start;
    vlc_mutex_unlock( &p_ac3dec->fifo.data_lock );

    aout_fifo.i_type = AOUT_ADEC_STEREO_FIFO;
    aout_fifo.b_stereo = 1;

    aout_fifo.l_frame_size = AC3DEC_FRAME_SIZE;

    /* Creating the audio output fifo */
    if ( (p_ac3dec->p_aout_fifo = aout_CreateFifo(p_ac3dec->p_aout, &aout_fifo)) == NULL )
    {
        return( -1 );
    }

    intf_DbgMsg( "ac3dec debug: ac3 decoder thread %p initialized\n", p_ac3dec );
    return( 0 );
}

/*****************************************************************************
 * RunThread : ac3 decoder thread
 *****************************************************************************/
static void RunThread( ac3dec_thread_t * p_ac3dec )
{
    /*
    mtime_t mdate = 0;
    */

    intf_DbgMsg( "ac3dec debug: running ac3 decoder thread (%p) (pid == %i)\n", p_ac3dec, getpid() );

    /* Initializing the ac3 decoder thread */
    if ( InitThread(p_ac3dec) )
    {
        p_ac3dec->b_error = 1;
    }

    /* ac3 decoder thread's main loop */
    while ( (!p_ac3dec->b_die) && (!p_ac3dec->b_error) )
    {
        p_ac3dec->b_invalid = 0;

        decode_find_sync( p_ac3dec );
	switch ( p_ac3dec->syncinfo.fscod )
	{
		case 0:
			p_ac3dec->p_aout_fifo->l_rate = 48000;
			break;

		case 1:
			p_ac3dec->p_aout_fifo->l_rate = 44100;
			break;

		case 2:
			p_ac3dec->p_aout_fifo->l_rate = 32000;
			break;

		default:
			fprintf( stderr, "ac3dec debug: invalid fscod\n" );
			break;
	}

	/*
	p_ac3dec->p_aout_fifo->date[p_ac3dec->p_aout_fifo->l_end_frame] = mdate;
	mdate += 32000;
	*/
	if ( DECODER_FIFO_START(p_ac3dec->fifo)->b_has_pts )
	{
		p_ac3dec->p_aout_fifo->date[p_ac3dec->p_aout_fifo->l_end_frame] = DECODER_FIFO_START(p_ac3dec->fifo)->i_pts;
		DECODER_FIFO_START(p_ac3dec->fifo)->b_has_pts = 0;
	}
	else
	{
		p_ac3dec->p_aout_fifo->date[p_ac3dec->p_aout_fifo->l_end_frame] = LAST_MDATE;
	}

	parse_syncinfo( p_ac3dec );

	parse_bsi( p_ac3dec );

	/* frame 1 */
	parse_audblk( p_ac3dec );
	exponent_unpack( p_ac3dec );
	bit_allocate( p_ac3dec );
	mantissa_unpack( p_ac3dec );
	if ( p_ac3dec->b_invalid )
	{
		continue;
	}
	if ( p_ac3dec->bsi.acmod == 0x2 )
	{
		rematrix( p_ac3dec );
	}
	imdct( p_ac3dec );
	downmix( p_ac3dec, ((ac3dec_frame_t *)p_ac3dec->p_aout_fifo->buffer)[ p_ac3dec->p_aout_fifo->l_end_frame ] );
	vlc_mutex_lock( &p_ac3dec->p_aout_fifo->data_lock );
	p_ac3dec->p_aout_fifo->l_end_frame = (p_ac3dec->p_aout_fifo->l_end_frame + 1) & AOUT_FIFO_SIZE;
	vlc_mutex_unlock( &p_ac3dec->p_aout_fifo->data_lock );

	/* frame 2 */
	parse_audblk( p_ac3dec );
	exponent_unpack( p_ac3dec );
	bit_allocate( p_ac3dec );
	mantissa_unpack( p_ac3dec );
	if ( p_ac3dec->b_invalid )
	{
		continue;
	}
	if ( p_ac3dec->bsi.acmod == 0x2 )
	{
		rematrix( p_ac3dec );
	}
	imdct( p_ac3dec );
	downmix( p_ac3dec, ((ac3dec_frame_t *)p_ac3dec->p_aout_fifo->buffer)[ p_ac3dec->p_aout_fifo->l_end_frame ] );
	p_ac3dec->p_aout_fifo->date[p_ac3dec->p_aout_fifo->l_end_frame] = LAST_MDATE;
	vlc_mutex_lock( &p_ac3dec->p_aout_fifo->data_lock );
	p_ac3dec->p_aout_fifo->l_end_frame = (p_ac3dec->p_aout_fifo->l_end_frame + 1) & AOUT_FIFO_SIZE;
	vlc_mutex_unlock( &p_ac3dec->p_aout_fifo->data_lock );

	/* frame 3 */
	parse_audblk( p_ac3dec );
	exponent_unpack( p_ac3dec );
	bit_allocate( p_ac3dec );
	mantissa_unpack( p_ac3dec );
	if ( p_ac3dec->b_invalid )
	{
		continue;
	}
	if ( p_ac3dec->bsi.acmod == 0x2 )
	{
		rematrix( p_ac3dec );
	}
	imdct( p_ac3dec );
	downmix( p_ac3dec, ((ac3dec_frame_t *)p_ac3dec->p_aout_fifo->buffer)[ p_ac3dec->p_aout_fifo->l_end_frame ] );
	p_ac3dec->p_aout_fifo->date[p_ac3dec->p_aout_fifo->l_end_frame] = LAST_MDATE;
	vlc_mutex_lock( &p_ac3dec->p_aout_fifo->data_lock );
	p_ac3dec->p_aout_fifo->l_end_frame = (p_ac3dec->p_aout_fifo->l_end_frame + 1) & AOUT_FIFO_SIZE;
	vlc_mutex_unlock( &p_ac3dec->p_aout_fifo->data_lock );

	/* frame 4 */
	parse_audblk( p_ac3dec );
	exponent_unpack( p_ac3dec );
	bit_allocate( p_ac3dec );
	mantissa_unpack( p_ac3dec );
	if ( p_ac3dec->b_invalid )
	{
		continue;
	}
	if ( p_ac3dec->bsi.acmod == 0x2 )
	{
		rematrix( p_ac3dec );
	}
	imdct( p_ac3dec );
	downmix( p_ac3dec, ((ac3dec_frame_t *)p_ac3dec->p_aout_fifo->buffer)[ p_ac3dec->p_aout_fifo->l_end_frame ] );
	p_ac3dec->p_aout_fifo->date[p_ac3dec->p_aout_fifo->l_end_frame] = LAST_MDATE;
	vlc_mutex_lock( &p_ac3dec->p_aout_fifo->data_lock );
	p_ac3dec->p_aout_fifo->l_end_frame = (p_ac3dec->p_aout_fifo->l_end_frame + 1) & AOUT_FIFO_SIZE;
	vlc_mutex_unlock( &p_ac3dec->p_aout_fifo->data_lock );

	/* frame 5 */
	parse_audblk( p_ac3dec );
	exponent_unpack( p_ac3dec );
	bit_allocate( p_ac3dec );
	mantissa_unpack( p_ac3dec );
	if ( p_ac3dec->b_invalid )
	{
		continue;
	}
	if ( p_ac3dec->bsi.acmod == 0x2 )
	{
		rematrix( p_ac3dec );
	}
	imdct( p_ac3dec );
	downmix( p_ac3dec, ((ac3dec_frame_t *)p_ac3dec->p_aout_fifo->buffer)[ p_ac3dec->p_aout_fifo->l_end_frame ] );
	p_ac3dec->p_aout_fifo->date[p_ac3dec->p_aout_fifo->l_end_frame] = LAST_MDATE;
	vlc_mutex_lock( &p_ac3dec->p_aout_fifo->data_lock );
	p_ac3dec->p_aout_fifo->l_end_frame = (p_ac3dec->p_aout_fifo->l_end_frame + 1) & AOUT_FIFO_SIZE;
	vlc_mutex_unlock( &p_ac3dec->p_aout_fifo->data_lock );

	/* frame 6 */
	parse_audblk( p_ac3dec );
	exponent_unpack( p_ac3dec );
	bit_allocate( p_ac3dec );
	mantissa_unpack( p_ac3dec );
	if ( p_ac3dec->b_invalid )
	{
		continue;
	}
	if ( p_ac3dec->bsi.acmod == 0x2 )
	{
		rematrix( p_ac3dec );
	}
	imdct( p_ac3dec );
	downmix( p_ac3dec, ((ac3dec_frame_t *)p_ac3dec->p_aout_fifo->buffer)[ p_ac3dec->p_aout_fifo->l_end_frame ] );
	p_ac3dec->p_aout_fifo->date[p_ac3dec->p_aout_fifo->l_end_frame] = LAST_MDATE;
	vlc_mutex_lock( &p_ac3dec->p_aout_fifo->data_lock );
	p_ac3dec->p_aout_fifo->l_end_frame = (p_ac3dec->p_aout_fifo->l_end_frame + 1) & AOUT_FIFO_SIZE;
	vlc_mutex_unlock( &p_ac3dec->p_aout_fifo->data_lock );

	parse_auxdata( p_ac3dec );
    }

    /* If b_error is set, the ac3 decoder thread enters the error loop */
    if ( p_ac3dec->b_error )
    {
        ErrorThread( p_ac3dec );
    }

    /* End of the ac3 decoder thread */
    EndThread( p_ac3dec );
}

/*****************************************************************************
 * ErrorThread : ac3 decoder's RunThread() error loop
 *****************************************************************************/
static void ErrorThread( ac3dec_thread_t * p_ac3dec )
{
    /* We take the lock, because we are going to read/write the start/end
     * indexes of the decoder fifo */
    vlc_mutex_lock( &p_ac3dec->fifo.data_lock );

    /* Wait until a `die' order is sent */
    while( !p_ac3dec->b_die )
    {
        /* Trash all received PES packets */
        while( !DECODER_FIFO_ISEMPTY(p_ac3dec->fifo) )
        {
            input_NetlistFreePES( p_ac3dec->bit_stream.p_input, DECODER_FIFO_START(p_ac3dec->fifo) );
	    DECODER_FIFO_INCSTART( p_ac3dec->fifo );
        }

        /* Waiting for the input thread to put new PES packets in the fifo */
        vlc_cond_wait( &p_ac3dec->fifo.data_wait, &p_ac3dec->fifo.data_lock );
    }

    /* We can release the lock before leaving */
    vlc_mutex_unlock( &p_ac3dec->fifo.data_lock );
}

/*****************************************************************************
 * EndThread : ac3 decoder thread destruction
 *****************************************************************************/
static void EndThread( ac3dec_thread_t * p_ac3dec )
{
    intf_DbgMsg( "ac3dec debug: destroying ac3 decoder thread %p\n", p_ac3dec );

    /* If the audio output fifo was created, we destroy it */
    if ( p_ac3dec->p_aout_fifo != NULL )
    {
        aout_DestroyFifo( p_ac3dec->p_aout_fifo );
    }

    /* Destroy descriptor */
    free( p_ac3dec );

    intf_DbgMsg( "ac3dec debug: ac3 decoder thread %p destroyed\n", p_ac3dec );
}
