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
static int      CheckConfiguration  ( video_cfg_t *p_cfg );
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
vdec_thread_t * vdec_CreateThread( video_cfg_t *p_cfg, input_thread_t *p_input,
                                   vout_thread_t *p_vout, int *pi_status )
{
    /* ?? */
}

/*******************************************************************************
 * vdec_DestroyThread: destroy a generic decoder thread
 *******************************************************************************
 * Destroy a terminated thread. This function will return 0 if the thread could
 * be destroyed, and non 0 else. The last case probably means that the thread
 * was still active, and another try may succeed.
 *******************************************************************************/
void vdec_DestroyThread( vdec_thread_t *p_vdec, int *pi_status )
{
    /* ?? */
}

/* following functions are local */

/*******************************************************************************
 * CheckConfiguration: check vdec_CreateThread() configuration
 *******************************************************************************
 * Set default parameters where required. In DEBUG mode, check if configuration
 * is valid.
 *******************************************************************************/
static int CheckConfiguration( video_cfg_t *p_cfg )
{
    /* ?? */

    return( 0 );
}

/*******************************************************************************
 * InitThread: initialize vdec output thread
 *******************************************************************************
 * This function is called from RunThread and performs the second step of the
 * initialization. It returns 0 on success. Note that the thread's flag are not
 * modified inside this function.
 *******************************************************************************/
static int InitThread( vdec_thread_t *p_vdec )
{
    /* ?? */
    /* Create video stream */
    p_vdec->i_stream =  vout_CreateStream( p_vdec->p_vout );
    if( p_vdec->i_stream < 0 )                                        /* error */
    {
        return( 1 );        
    }
    
    /* Initialize decoding data */    
    /* ?? */

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
    /* 
     * Initialize thread and free configuration 
     */
    p_vdec->b_error = InitThread( p_vdec );
    if( p_vdec->b_error )
    {
        return;
    }
    p_vdec->b_run = 1;

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
        /* ?? trash all trashable PES packets */

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
#ifdef DEBUG
    /* Check for remaining PES packets */
    /* ?? */
#endif

    /* Destroy thread structures allocated by InitThread */
    vout_DestroyStream( p_vdec->p_vout, p_vdec->i_stream );
    /* ?? */

    intf_DbgMsg("vdec debug: EndThread(%p)\n", p_vdec);
}


