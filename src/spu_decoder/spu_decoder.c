/*****************************************************************************
 * spu_decoder.c : spu decoder thread
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

/* repompé sur video_decoder.c
 * FIXME: passer en terminate/destroy avec les signaux supplémentaires ?? */

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>                                      /* malloc(), free() */
#include <sys/types.h>                        /* on BSD, uio.h needs types.h */
#include <sys/uio.h>                                          /* for input.h */
#include <unistd.h>                                              /* getpid() */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "plugins.h"

#include "intf_msg.h"
#include "debug.h"                                                 /* ASSERT */

#include "input.h"
#include "input_netlist.h"
#include "decoder_fifo.h"

#include "video.h"
#include "video_output.h"

#include "spu_decoder.h"

/*
 * Local prototypes
 */
static int      InitThread          ( spudec_thread_t *p_spudec );
static void     RunThread           ( spudec_thread_t *p_spudec );
static void     ErrorThread         ( spudec_thread_t *p_spudec );
static void     EndThread           ( spudec_thread_t *p_spudec );

#define GetWord( i ) \
    i  = GetByte( &p_spudec->bit_stream ) << 8; \
    i += GetByte( &p_spudec->bit_stream ); \
    i_index += 2;

/*****************************************************************************
 * spudec_CreateThread: create a spu decoder thread
 *****************************************************************************/
spudec_thread_t * spudec_CreateThread( input_thread_t * p_input )
{
    spudec_thread_t *     p_spudec;

    intf_DbgMsg("spudec debug: creating spu decoder thread\n");

    /* Allocate the memory needed to store the thread's structure */
    if ( (p_spudec = (spudec_thread_t *)malloc( sizeof(spudec_thread_t) )) == NULL )
    {
        intf_ErrMsg("spudec error: not enough memory for spudec_CreateThread() to create the new thread\n");
        return( NULL );
    }

    /*
     * Initialize the thread properties
     */
    p_spudec->b_die = 0;
    p_spudec->b_error = 0;

    /*
     * Initialize the input properties
     */
    /* Initialize the decoder fifo's data lock and conditional variable and set
     * its buffer as empty */
    vlc_mutex_init( &p_spudec->fifo.data_lock );
    vlc_cond_init( &p_spudec->fifo.data_wait );
    p_spudec->fifo.i_start = 0;
    p_spudec->fifo.i_end = 0;
    /* Initialize the bit stream structure */
    p_spudec->bit_stream.p_input = p_input;
    p_spudec->bit_stream.p_decoder_fifo = &p_spudec->fifo;
    p_spudec->bit_stream.fifo.buffer = 0;
    p_spudec->bit_stream.fifo.i_available = 0;

    /* Get the video output informations */
    p_spudec->p_vout = p_input->p_vout;

    /* Spawn the spu decoder thread */
    if ( vlc_thread_create(&p_spudec->thread_id, "spu decoder",
         (vlc_thread_func_t)RunThread, (void *)p_spudec) )
    {
        intf_ErrMsg("spudec error: can't spawn spu decoder thread\n");
        free( p_spudec );
        return( NULL );
    }

    intf_DbgMsg("spudec debug: spu decoder thread (%p) created\n", p_spudec);
    return( p_spudec );
}

/*****************************************************************************
 * spudec_DestroyThread: destroy a spu decoder thread
 *****************************************************************************
 * Destroy and terminate thread. This function will return 0 if the thread could
 * be destroyed, and non 0 else. The last case probably means that the thread
 * was still active, and another try may succeed.
 *****************************************************************************/
void spudec_DestroyThread( spudec_thread_t *p_spudec )
{
    intf_DbgMsg("spudec debug: requesting termination of spu decoder thread %p\n", p_spudec);

    /* Ask thread to kill itself */
    p_spudec->b_die = 1;

    /* Warn the decoder that we're quitting */
    vlc_mutex_lock( &p_spudec->fifo.data_lock );
    vlc_cond_signal( &p_spudec->fifo.data_wait );
    vlc_mutex_unlock( &p_spudec->fifo.data_lock );

    /* Waiting for the decoder thread to exit */
    /* Remove this as soon as the "status" flag is implemented */
    vlc_thread_join( p_spudec->thread_id );
}

/* following functions are local */

/*****************************************************************************
 * InitThread: initialize spu decoder thread
 *****************************************************************************
 * This function is called from RunThread and performs the second step of the
 * initialization. It returns 0 on success. Note that the thread's flag are not
 * modified inside this function.
 *****************************************************************************/
static int InitThread( spudec_thread_t *p_spudec )
{
    intf_DbgMsg("spudec debug: initializing spu decoder thread %p\n", p_spudec);

    /* Our first job is to initialize the bit stream structure with the
     * beginning of the input stream */
    vlc_mutex_lock( &p_spudec->fifo.data_lock );
    while ( DECODER_FIFO_ISEMPTY(p_spudec->fifo) )
    {
        if ( p_spudec->b_die )
        {
            vlc_mutex_unlock( &p_spudec->fifo.data_lock );
            return( 1 );
        }
        vlc_cond_wait( &p_spudec->fifo.data_wait, &p_spudec->fifo.data_lock );
    }

    p_spudec->bit_stream.p_ts = DECODER_FIFO_START( p_spudec->fifo )->p_first_ts;
    p_spudec->bit_stream.p_byte = p_spudec->bit_stream.p_ts->buffer + p_spudec->bit_stream.p_ts->i_payload_start;
    p_spudec->bit_stream.p_end = p_spudec->bit_stream.p_ts->buffer + p_spudec->bit_stream.p_ts->i_payload_end;
    vlc_mutex_unlock( &p_spudec->fifo.data_lock );

    /* Mark thread as running and return */
    intf_DbgMsg( "spudec debug: InitThread(%p) succeeded\n", p_spudec );
    return( 0 );
}

/*****************************************************************************
 * RunThread: spu decoder thread
 *****************************************************************************
 * spu decoder thread. This function does only return when the thread is
 * terminated.
 *****************************************************************************/
static void RunThread( spudec_thread_t *p_spudec )
{
    intf_DbgMsg("spudec debug: running spu decoder thread (%p) (pid == %i)\n",
        p_spudec, getpid());

    /*
     * Initialize thread and free configuration
     */
    p_spudec->b_error = InitThread( p_spudec );

    p_spudec->b_run = 1;

    /*
     * Main loop - it is not executed if an error occured during
     * initialization
     */
    while( (!p_spudec->b_die) && (!p_spudec->b_error) )
    {
        int i_spu_id;
        int i_packet_size;
        int i_rle_size;
        int i_index;
	int i_pes_size;
	boolean_t       b_finished;
        unsigned char * p_spu_data;
        subpicture_t  * p_spu;

        while( !DECODER_FIFO_ISEMPTY(p_spudec->fifo) )
        {
            printf( "*** tracking next SPU PES\n" );
            do
            {
                i_spu_id = GetByte( &p_spudec->bit_stream );
            }
            while( (i_spu_id & 0xe0) != 0x20 );
            i_pes_size = DECODER_FIFO_START(p_spudec->fifo)->i_pes_size;
            printf( "got it. size = 0x%.4x\n", i_pes_size );

            printf( "SPU id: 0x%.2x\n", i_spu_id );

            i_index = 0;

            GetWord( i_packet_size );
            printf( "total size:  0x%.4x\n", i_packet_size );

            GetWord( i_rle_size );
            printf( "RLE size:    0x%.4x\n", i_rle_size );

            /* we already read 4 bytes for the total size and the RLE size */

            p_spu = vout_CreateSubPicture( p_spudec->p_vout,
                                           DVD_SUBPICTURE, i_rle_size );
            p_spu_data = p_spu->p_data;

            if( (i_rle_size < i_packet_size)
                && ((i_spu_id & 0xe0) == 0x20) )
            {
                printf( "doing RLE stuff (%i bytes)\n", i_rle_size );
		printf( "index/size %i/%i\n", i_index, i_pes_size );
                while( i_index++ <i_rle_size )
                {
                    //*p_spu_data++ = GetByte( &p_spudec->bit_stream );
                    if (i_index == i_pes_size) printf ("\n **** \n");
		    /* kludge ??? */
                    if (i_index == i_pes_size) printf( "%.2x", *p_spu_data++ = GetByte( &p_spudec->bit_stream ) );
                    printf( "%.2x", *p_spu_data++ = GetByte( &p_spudec->bit_stream ) );
                }
		printf( "\nindex/size %i/%i\n", i_index, i_pes_size );
                //printf( "\n" );

		b_finished = 0;
                printf( "control stuff\n" );
		do
                {
                    unsigned char i_cmd;
                    unsigned int i_word;

                    GetWord( i_word );
                    printf( "date: 0x%.4x\n", i_word );

                    GetWord( i_word );
                    printf( "  next: 0x%.4x (i-5: %.4x)\n", i_word, i_index-5 );
		    b_finished = (i_index - 5 >= i_word );

		    do
		    {
                        i_cmd = GetByte( &p_spudec->bit_stream );
			i_index++;

			switch(i_cmd)
			{
                            case 0x00:
                                printf( "  00 (display now)\n" );
                                break;
                            case 0x01:
                                printf( "  01 (start displaying)\n" );
                                break;
                            case 0x02:
                                printf( "  02 (stop displaying)\n" );
                                break;
                            case 0x03:
				GetWord( i_word );
                                printf( "  03 (palette) - %.4x\n", i_word );
                                break;
                            case 0x04:
				GetWord( i_word );
                                printf( "  04 (alpha channel) - %.4x\n", i_word );
                                break;
                            case 0x05:
				GetWord( i_word );
                                printf( "  05 (coordinates) - %.4x", i_word );
				GetWord( i_word );
                                printf( "%.4x", i_word );
				GetWord( i_word );
                                printf( "%.4x\n", i_word );
                                break;
                            case 0x06:
				GetWord( i_word );
                                printf( "  06 (byte offsets) - %.4x", i_word );
				GetWord( i_word );
                                printf( "%.4x\n", i_word );
                                break;
                            case 0xff:
                                printf( "  ff (end)\n" );
                                break;
			    default:
                                printf( "  %.2x (unknown command)\n", i_cmd );
                                break;
			}

		    }
		    while( i_cmd != 0xff );

		}
		while( !b_finished );
                printf( "control stuff finished\n" );
                printf( "*** end of PES !\n\n" );
            }
            else 
            {
                printf( "*** invalid PES !\n\n" );
                /* trash the PES packet */
                /*vlc_mutex_lock( &p_spudec->fifo.data_lock );
                input_NetlistFreePES( p_spudec->bit_stream.p_input,
                                      DECODER_FIFO_START(p_spudec->fifo) );
                DECODER_FIFO_INCSTART( p_spudec->fifo );
                vlc_mutex_unlock( &p_spudec->fifo.data_lock );*/
            }

        }
        /* Waiting for the input thread to put new PES packets in the fifo */
        printf( "decoder fifo is empty\n" );
        vlc_cond_wait( &p_spudec->fifo.data_wait, &p_spudec->fifo.data_lock );
    }

    /*
     * Error loop
     */
    if( p_spudec->b_error )
    {
        ErrorThread( p_spudec );
    }

    p_spudec->b_run = 0;

    /* End of thread */
    EndThread( p_spudec );
}

/*****************************************************************************
 * ErrorThread: RunThread() error loop
 *****************************************************************************
 * This function is called when an error occured during thread main's loop. The
 * thread can still receive feed, but must be ready to terminate as soon as
 * possible.
 *****************************************************************************/
static void ErrorThread( spudec_thread_t *p_spudec )
{
    /* We take the lock, because we are going to read/write the start/end
     * indexes of the decoder fifo */
    vlc_mutex_lock( &p_spudec->fifo.data_lock );

    /* Wait until a `die' order is sent */
    while( !p_spudec->b_die )
    {
        /* Trash all received PES packets */
        while( !DECODER_FIFO_ISEMPTY(p_spudec->fifo) )
        {
            input_NetlistFreePES( p_spudec->bit_stream.p_input, DECODER_FIFO_START(p_spudec->fifo) );
            DECODER_FIFO_INCSTART( p_spudec->fifo );
        }

        /* Waiting for the input thread to put new PES packets in the fifo */
        vlc_cond_wait( &p_spudec->fifo.data_wait, &p_spudec->fifo.data_lock );
    }

    /* We can release the lock before leaving */
    vlc_mutex_unlock( &p_spudec->fifo.data_lock );
}

/*****************************************************************************
 * EndThread: thread destruction
 *****************************************************************************
 * This function is called when the thread ends after a sucessful
 * initialization.
 *****************************************************************************/
static void EndThread( spudec_thread_t *p_spudec )
{
    intf_DbgMsg( "spudec debug: destroying spu decoder thread %p\n", p_spudec );
    free( p_spudec );
    intf_DbgMsg( "spudec debug: spu decoder thread %p destroyed\n", p_spudec);
}
