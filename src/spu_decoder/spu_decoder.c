/*******************************************************************************
 * spu_decoder.c : spu decoder thread
 * (c)2000 VideoLAN
 *******************************************************************************/

/* repompé sur video_decoder.c
 * ?? passer en terminate/destroy avec les signaux supplémentaires */

/*******************************************************************************
 * Preamble
 *******************************************************************************/
//#include "vlc.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/uio.h>

#include "config.h"
#include "common.h"
#include "mtime.h"
#include "vlc_thread.h"

#include "intf_msg.h"
#include "debug.h"                    /* ?? temporaire, requis par netlist.h */

#include "input.h"
#include "input_netlist.h"
#include "decoder_fifo.h"

#include "spu_decoder.h"

/*
 * Local prototypes
 */
static int      InitThread          ( spudec_thread_t *p_spudec );
static void     RunThread           ( spudec_thread_t *p_spudec );
static void     ErrorThread         ( spudec_thread_t *p_spudec );
static void     EndThread           ( spudec_thread_t *p_spudec );

/******************************************************************************
 * spudec_CreateThread: create a spu decoder thread
 ******************************************************************************/
spudec_thread_t * spudec_CreateThread( input_thread_t * p_input )
{
    spudec_thread_t *     p_spudec;

    intf_DbgMsg("spudec debug: creating spu decoder thread\n");
    fprintf(stderr, "spudec debug: creating spu decoder thread\n");

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

/*******************************************************************************
 * spudec_DestroyThread: destroy a spu decoder thread
 *******************************************************************************
 * Destroy and terminate thread. This function will return 0 if the thread could
 * be destroyed, and non 0 else. The last case probably means that the thread
 * was still active, and another try may succeed.
 *******************************************************************************/
void spudec_DestroyThread( spudec_thread_t *p_spudec )
{
    intf_DbgMsg("spudec debug: requesting termination of spu decoder thread %p\n", p_spudec);
    fprintf(stderr, "spudec debug: requesting termination of spu decoder thread %p\n", p_spudec);

    /* Ask thread to kill itself */
    p_spudec->b_die = 1;

    /* Waiting for the decoder thread to exit */
    /* Remove this as soon as the "status" flag is implemented */
    vlc_thread_join( p_spudec->thread_id );
}

/* following functions are local */

/*******************************************************************************
 * InitThread: initialize spu decoder thread
 *******************************************************************************
 * This function is called from RunThread and performs the second step of the
 * initialization. It returns 0 on success. Note that the thread's flag are not
 * modified inside this function.
 *******************************************************************************/
static int InitThread( spudec_thread_t *p_spudec )
{

    intf_DbgMsg("spudec debug: initializing spu decoder thread %p\n", p_spudec);

    /* Mark thread as running and return */
    intf_DbgMsg("spudec debug: InitThread(%p) succeeded\n", p_spudec);    
    return( 0 );    
}

/*******************************************************************************
 * RunThread: spu decoder thread
 *******************************************************************************
 * spu decoder thread. This function does only return when the thread is
 * terminated. 
 *******************************************************************************/
static void RunThread( spudec_thread_t *p_spudec )
{
    intf_DbgMsg("spudec debug: running spu decoder thread (%p) (pid == %i)\n",
                p_spudec, getpid());

    /* 
     * Initialize thread and free configuration 
     */
    p_spudec->b_error = InitThread( p_spudec );
    if( p_spudec->b_error )
    {
        return;
    }
    p_spudec->b_run = 1;

    /*
     * Main loop - it is not executed if an error occured during
     * initialization
     */
    while( (!p_spudec->b_die) && (!p_spudec->b_error) )
    {
        fprintf(stderr, "I'm a spu decoder !\n");
	sleep(1);
    } 

    /*
     * Error loop
     */
    if( p_spudec->b_error )
    {
        ErrorThread( p_spudec );        
    }

    /* End of thread */
    EndThread( p_spudec );
    p_spudec->b_run = 0;
}

/*******************************************************************************
 * ErrorThread: RunThread() error loop
 *******************************************************************************
 * This function is called when an error occured during thread main's loop. The
 * thread can still receive feed, but must be ready to terminate as soon as
 * possible.
 *******************************************************************************/
static void ErrorThread( spudec_thread_t *p_spudec )
{
    /* Wait until a `die' order */
    while( !p_spudec->b_die )
    {
        // foo();
    }
}

/*******************************************************************************
 * EndThread: thread destruction
 *******************************************************************************
 * This function is called when the thread ends after a sucessfull 
 * initialization.
 *******************************************************************************/
static void EndThread( spudec_thread_t *p_spudec )
{
    intf_DbgMsg("spudec debug: EndThread(%p)\n", p_spudec);
}

