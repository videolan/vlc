/*******************************************************************************
 * video_decoder.c : video decoder thread
 * (c)1999 VideoLAN
 *******************************************************************************/

/* ?? passer en terminate/destroy avec les signaux supplémentaires */

/*******************************************************************************
 * Preamble
 *******************************************************************************/
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/uio.h>
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

#include "config.h"
#include "common.h"
#include "mtime.h"

#include "intf_msg.h"
#include "debug.h"                      /* ?? temporaire, requis par netlist.h */

#include "input.h"
#include "input_netlist.h"
#include "decoder_fifo.h"
#include "video.h"
#include "video_output.h"
#include "video_decoder.h"

/*
 * Local prototypes
 */
//static int      CheckConfiguration  ( video_cfg_t *p_cfg );
static int      InitThread          ( vdec_thread_t *p_vdec );
static void     RunThread           ( vdec_thread_t *p_vdec );
static void     ErrorThread         ( vdec_thread_t *p_vdec );
static void     EndThread           ( vdec_thread_t *p_vdec );

/*******************************************************************************
 * vdec_CreateThread: create a generic decoder thread
 *******************************************************************************
 * This function creates a new video decoder thread, and returns a pointer
 * to its description. On error, it returns NULL.
 * Following configuration properties are used:
 * ??
 *******************************************************************************/
vdec_thread_t * vdec_CreateThread( /* video_cfg_t *p_cfg, */ input_thread_t *p_input /*,
                                   vout_thread_t *p_vout, int *pi_status */ )
{
    vdec_thread_t *     p_vdec;

    intf_DbgMsg("vdec debug: creating video decoder thread\n");

    /* Allocate the memory needed to store the thread's structure */
    if ( (p_vdec = (vdec_thread_t *)malloc( sizeof(vdec_thread_t) )) == NULL )
    {
        intf_ErrMsg("adec error: not enough memory for vdec_CreateThread() to create the new thread\n");
        return( NULL );
    }

    /*
     * Initialize the thread properties
     */
    p_vdec->b_die = 0;
    p_vdec->b_error = 0;

    /*
     * Initialize the input properties
     */
    /* Initialize the decoder fifo's data lock and conditional variable and set     * its buffer as empty */
    pthread_mutex_init( &p_vdec->fifo.data_lock, NULL );
    pthread_cond_init( &p_vdec->fifo.data_wait, NULL );
    p_vdec->fifo.i_start = 0;
    p_vdec->fifo.i_end = 0;
    /* Initialize the bit stream structure */
    p_vdec->bit_stream.p_input = p_input;
    p_vdec->bit_stream.p_decoder_fifo = &p_vdec->fifo;
    p_vdec->bit_stream.fifo.buffer = 0;
    p_vdec->bit_stream.fifo.i_available = 0;

    /* Spawn the video decoder thread */
    if ( pthread_create(&p_vdec->thread_id, NULL, (void *)RunThread, (void *)p_vdec) )
    {
        intf_ErrMsg("vdec error: can't spawn video decoder thread\n");
        free( p_vdec );
        return( NULL );
    }

    intf_DbgMsg("vdec debug: video decoder thread (%p) created\n", p_vdec);
    return( p_vdec );
}

/*******************************************************************************
 * vdec_DestroyThread: destroy a generic decoder thread
 *******************************************************************************
 * Destroy a terminated thread. This function will return 0 if the thread could
 * be destroyed, and non 0 else. The last case probably means that the thread
 * was still active, and another try may succeed.
 *******************************************************************************/
void vdec_DestroyThread( vdec_thread_t *p_vdec /*, int *pi_status */ )
{
    intf_DbgMsg("vdec debug: requesting termination of video decoder thread %p\n", p_vdec);

    /* Ask thread to kill itself */
    p_vdec->b_die = 1;
    /* Make sure the decoder thread leaves the GetByte() function */
    pthread_mutex_lock( &(p_vdec->fifo.data_lock) );
    pthread_cond_signal( &(p_vdec->fifo.data_wait) );
    pthread_mutex_unlock( &(p_vdec->fifo.data_lock) );

    /* Waiting for the decoder thread to exit */
    /* Remove this as soon as the "status" flag is implemented */
    pthread_join( p_vdec->thread_id, NULL );
}

/* following functions are local */

/*******************************************************************************
 * CheckConfiguration: check vdec_CreateThread() configuration
 *******************************************************************************
 * Set default parameters where required. In DEBUG mode, check if configuration
 * is valid.
 *******************************************************************************/
#if 0
static int CheckConfiguration( video_cfg_t *p_cfg )
{
    /* ?? */

    return( 0 );
}
#endif

/*******************************************************************************
 * InitThread: initialize vdec output thread
 *******************************************************************************
 * This function is called from RunThread and performs the second step of the
 * initialization. It returns 0 on success. Note that the thread's flag are not
 * modified inside this function.
 *******************************************************************************/
static int InitThread( vdec_thread_t *p_vdec )
{

    intf_DbgMsg("vdec debug: initializing video decoder thread %p\n", p_vdec);

    /* Our first job is to initialize the bit stream structure with the
     * beginning of the input stream */
    pthread_mutex_lock( &p_vdec->fifo.data_lock );
    while ( DECODER_FIFO_ISEMPTY(p_vdec->fifo) )
    {
        pthread_cond_wait( &p_vdec->fifo.data_wait, &p_vdec->fifo.data_lock );
    }
    p_vdec->bit_stream.p_ts = DECODER_FIFO_START( p_vdec->fifo )->p_first_ts;
    p_vdec->bit_stream.i_byte = p_vdec->bit_stream.p_ts->i_payload_start;
    pthread_mutex_unlock( &p_vdec->fifo.data_lock );

#if 0
    /* ?? */
    /* Create video stream */
    p_vdec->i_stream =  vout_CreateStream( p_vdec->p_vout );
    if( p_vdec->i_stream < 0 )                                        /* error */
    {
        return( 1 );        
    }
    
    /* Initialize decoding data */    
    /* ?? */
#endif

    /* Initialize other properties */
#ifdef STATS
    p_vdec->c_loops = 0;    
    p_vdec->c_idle_loops = 0;
    p_vdec->c_pictures = 0;
    p_vdec->c_i_pictures = 0;
    p_vdec->c_p_pictures = 0;
    p_vdec->c_b_pictures = 0;
    p_vdec->c_decoded_pictures = 0;
    p_vdec->c_decoded_i_pictures = 0;
    p_vdec->c_decoded_p_pictures = 0;
    p_vdec->c_decoded_b_pictures = 0;
#endif

    /* Mark thread as running and return */
    intf_DbgMsg("vdec debug: InitThread(%p) succeeded\n", p_vdec);    
    return( 0 );    
}

/*******************************************************************************
 * RunThread: generic decoder thread
 *******************************************************************************
 * Generic decoder thread. This function does only returns when the thread is
 * terminated. 
 *******************************************************************************/
static void RunThread( vdec_thread_t *p_vdec )
{

    intf_DbgMsg("vdec debug: running video decoder thread (%p) (pid == %i)\n", p_vdec, getpid());

    /* 
     * Initialize thread and free configuration 
     */
    p_vdec->b_error = InitThread( p_vdec );
    if( p_vdec->b_error )
    {
        return;
    }
    p_vdec->b_run = 1;

/* REMOVE ME !!!!! */
p_vdec->b_error = 1;

    /*
     * Main loop - it is not executed if an error occured during
     * initialization
     */
    while( (!p_vdec->b_die) && (!p_vdec->b_error) )
    {
        /* ?? */
    } 

    /*
     * Error loop
     */
    if( p_vdec->b_error )
    {
        ErrorThread( p_vdec );        
    }

    /* End of thread */
    EndThread( p_vdec );
    p_vdec->b_run = 0;
}

/*******************************************************************************
 * ErrorThread: RunThread() error loop
 *******************************************************************************
 * This function is called when an error occured during thread main's loop. The
 * thread can still receive feed, but must be ready to terminate as soon as
 * possible.
 *******************************************************************************/
static void ErrorThread( vdec_thread_t *p_vdec )
{
    /* Wait until a `die' order */
    while( !p_vdec->b_die )
    {
        /* We take the lock, because we are going to read/write the start/end
         * indexes of the decoder fifo */
        pthread_mutex_lock( &p_vdec->fifo.data_lock );

        /* ?? trash all trashable PES packets */
        while( !DECODER_FIFO_ISEMPTY(p_vdec->fifo) )
        {
            input_NetlistFreePES( p_vdec->bit_stream.p_input, DECODER_FIFO_START(p_vdec->fifo) );
            DECODER_FIFO_INCSTART( p_vdec->fifo );
        }

        pthread_mutex_unlock( &p_vdec->fifo.data_lock );
        /* Sleep a while */
        msleep( VDEC_IDLE_SLEEP );                
    }
}

/*******************************************************************************
 * EndThread: thread destruction
 *******************************************************************************
 * This function is called when the thread ends after a sucessfull 
 * initialization.
 *******************************************************************************/
static void EndThread( vdec_thread_t *p_vdec )
{
    intf_DbgMsg("vdec debug: destroying video decoder thread %p\n", p_vdec);

#ifdef DEBUG
    /* Check for remaining PES packets */
    /* ?? */
#endif

    /* Destroy thread structures allocated by InitThread */
//    vout_DestroyStream( p_vdec->p_vout, p_vdec->i_stream );
    /* ?? */

    intf_DbgMsg("vdec debug: EndThread(%p)\n", p_vdec);
}
