/*******************************************************************************
 * video_parser.c : video parser thread
 * (c)1999 VideoLAN
 *******************************************************************************/

/* ?? passer en terminate/destroy avec les signaux supplémentaires */

/*******************************************************************************
 * Preamble
 *******************************************************************************/
#include <errno.h>
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
#include "vlc_thread.h"

#include "intf_msg.h"
#include "debug.h"                     /* ?? temporaire, requis par netlist.h */

#include "input.h"
#include "input_netlist.h"
#include "decoder_fifo.h"
#include "video.h"
#include "video_output.h"
#include "video_decoder.h"
#include "video_parser.h"
#include "parser_fifo.h"

/*
 * Local prototypes
 */
//static int      CheckConfiguration  ( video_cfg_t *p_cfg );
static int      InitThread          ( vpar_thread_t *p_vpar );
static void     RunThread           ( vpar_thread_t *p_vpar );
static void     ErrorThread         ( vpar_thread_t *p_vpar );
static void     EndThread           ( vpar_thread_t *p_vpar );

/*******************************************************************************
 * vpar_CreateThread: create a generic parser thread
 *******************************************************************************
 * This function creates a new video parser thread, and returns a pointer
 * to its description. On error, it returns NULL.
 * Following configuration properties are used:
 * ??
 *******************************************************************************/
vpar_thread_t * vpar_CreateThread( /* video_cfg_t *p_cfg, */ input_thread_t *p_input /*,
                                   vout_thread_t *p_vout, int *pi_status */ )
{
    vpar_thread_t *     p_vpar;

    intf_DbgMsg("vpar debug: creating video parser thread\n");

    /* Allocate the memory needed to store the thread's structure */
    if ( (p_vpar = (vpar_thread_t *)malloc( sizeof(vpar_thread_t) )) == NULL )
    {
        intf_ErrMsg("vpar error: not enough memory for vpar_CreateThread() to create the new thread\n");
        return( NULL );
    }

    /*
     * Initialize the thread properties
     */
    p_vpar->b_die = 0;
    p_vpar->b_error = 0;

    /*
     * Initialize the input properties
     */
    /* Initialize the parser fifo's data lock and conditional variable and set     * its buffer as empty */
    vlc_mutex_init( &p_vpar->fifo.data_lock );
    vlc_cond_init( &p_vpar->fifo.data_wait );
    p_vpar->fifo.i_start = 0;
    p_vpar->fifo.i_end = 0;
    /* Initialize the bit stream structure */
    p_vpar->bit_stream.p_input = p_input;
    p_vpar->bit_stream.p_parser_fifo = &p_vpar->fifo;
    p_vpar->bit_stream.fifo.buffer = 0;
    p_vpar->bit_stream.fifo.i_available = 0;

    /* Spawn the video parser thread */
    if ( vlc_thread_create(&p_vpar->thread_id, "video parser", (vlc_thread_func)RunThread, (void *)p_vpar) )
    {
        intf_ErrMsg("vpar error: can't spawn video parser thread\n");
        free( p_vpar );
        return( NULL );
    }

    intf_DbgMsg("vpar debug: video parser thread (%p) created\n", p_vpar);
    return( p_vpar );
}

/*******************************************************************************
 * vpar_DestroyThread: destroy a generic parser thread
 *******************************************************************************
 * Destroy a terminated thread. This function will return 0 if the thread could
 * be destroyed, and non 0 else. The last case probably means that the thread
 * was still active, and another try may succeed.
 *******************************************************************************/
void vpar_DestroyThread( vpar_thread_t *p_vpar /*, int *pi_status */ )
{
    intf_DbgMsg("vpar debug: requesting termination of video parser thread %p\n", p_vpar);

    /* Ask thread to kill itself */
    p_vpar->b_die = 1;
    /* Make sure the parser thread leaves the GetByte() function */
    vlc_mutex_lock( &(p_vpar->fifo.data_lock) );
    vlc_cond_signal( &(p_vpar->fifo.data_wait) );
    vlc_mutex_unlock( &(p_vpar->fifo.data_lock) );

    /* Waiting for the parser thread to exit */
    /* Remove this as soon as the "status" flag is implemented */
    vlc_thread_join( p_vpar->thread_id );
}

/* following functions are local */

/*******************************************************************************
 * CheckConfiguration: check vpar_CreateThread() configuration
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
 * InitThread: initialize vpar output thread
 *******************************************************************************
 * This function is called from RunThread and performs the second step of the
 * initialization. It returns 0 on success. Note that the thread's flag are not
 * modified inside this function.
 *******************************************************************************/
static int InitThread( vpar_thread_t *p_vpar )
{
    int     i_dummy;

    intf_DbgMsg("vpar debug: initializing video parser thread %p\n", p_vpar);

    /* Our first job is to initialize the bit stream structure with the
     * beginning of the input stream */
    vlc_mutex_lock( &p_vpar->fifo.data_lock );
    while ( DECODER_FIFO_ISEMPTY(p_vpar->fifo) )
    {
        vlc_cond_wait( &p_vpar->fifo.data_wait, &p_vpar->fifo.data_lock );
    }
    p_vpar->bit_stream.p_ts = DECODER_FIFO_START( p_vpar->fifo )->p_first_ts;
    p_vpar->bit_stream.i_byte = p_vpar->bit_stream.p_ts->i_payload_start;
    vlc_mutex_unlock( &p_vpar->fifo.data_lock );

#if 0
    /* ?? */
    /* Create video stream */
    p_vpar->i_stream =  vout_CreateStream( p_vpar->p_vout );
    if( p_vpar->i_stream < 0 )                                        /* error */
    {
        return( 1 );
    }
#endif

    /* Initialize parsing data */
    p_vpar->sequence.p_forward = p_vpar->sequence.p_backward = NULL;
    p_vpar->sequence.p_frame_lum_lookup
        = p_vpar->sequence.p_field_lum_lookup
        = p_vpar->sequence.p_frame_chroma_lookup
        = p_vpar->sequence.p_field_chroma_lookup
        = NULL;
    p_vpar->sequence.intra_quant.b_allocated = FALSE;
    p_vpar->sequence.nonintra_quant.b_allocated = FALSE;
    p_vpar->sequence.chroma_intra_quant.b_allocated = FALSE;
    p_vpar->sequence.chroma_nonintra_quant.b_allocated = FALSE;
    p_vpar->sequence.i_frame_number = 0;

    /* Initialize other properties */
#ifdef STATS
    p_vpar->c_loops = 0;    
    p_vpar->c_idle_loops = 0;
    p_vpar->c_pictures = 0;
    p_vpar->c_i_pictures = 0;
    p_vpar->c_p_pictures = 0;
    p_vpar->c_b_pictures = 0;
    p_vpar->c_decoded_pictures = 0;
    p_vpar->c_decoded_i_pictures = 0;
    p_vpar->c_decoded_p_pictures = 0;
    p_vpar->c_decoded_b_pictures = 0;
#endif

    /* Initialize video FIFO */
    vpar_InitFIFO( p_vpar );
    
    bzero( p_vpar->p_vdec, MAX_VDEC*sizeof(vdec_thread_t *) );
    
    /* Spawn video_decoder threads */
    /* ??? modify the number of vdecs at runtime ? */
    for( i_dummy = 0; i_dummy < NB_VDEC; i_dummy++ )
    {
        if( (p_vpar->p_vdec[i_dummy] = vdec_CreateThread( p_vpar )) == NULL )
        {
            return( 1 );
        }
    }

    /* Mark thread as running and return */
    intf_DbgMsg("vpar debug: InitThread(%p) succeeded\n", p_vpar);
    return( 0 );
}

/*******************************************************************************
 * RunThread: generic parser thread
 *******************************************************************************
 * Video parser thread. This function does only returns when the thread is
 * terminated. 
 *******************************************************************************/
static void RunThread( vpar_thread_t *p_vpar )
{
    int i_dummy;

    intf_DbgMsg("vpar debug: running video parser thread (%p) (pid == %i)\n", p_vpar, getpid());

    /* 
     * Initialize thread 
     */
    p_vpar->b_error = InitThread( p_vpar );
    if( p_vpar->b_error )
    {
        return;
    }
    p_vpar->b_run = 1;

    /*
     * Main loop - it is not executed if an error occured during
     * initialization
     */
    while( (!p_vpar->b_die) && (!p_vpar->b_error) )
    {
        /* Find the next sequence header in the stream */
        p_vpar->b_error = vpar_NextSequenceHeader( p_vpar );

#ifdef STATS
        p_vpar->c_sequences++;
#endif

        while( (!p_vpar->b_die) && (!p_vpar->b_error) )
        {
            /* Parse the next sequence, group or picture header */
            if( vpar_ParseHeader( p_vpar ) )
            {
                /* End of sequence */
                break;
            };
        }
    } 

    /*
     * Error loop
     */
    if( p_vpar->b_error )
    {
        ErrorThread( p_vpar );
    }

    /* End of thread */
    EndThread( p_vpar );
    p_vpar->b_run = 0;
}

/*******************************************************************************
 * ErrorThread: RunThread() error loop
 *******************************************************************************
 * This function is called when an error occured during thread main's loop. The
 * thread can still receive feed, but must be ready to terminate as soon as
 * possible.
 *******************************************************************************/
static void ErrorThread( vpar_thread_t *p_vpar )
{
    /* Wait until a `die' order */
    while( !p_vpar->b_die )
    {
        /* We take the lock, because we are going to read/write the start/end
         * indexes of the parser fifo */
        vlc_mutex_lock( &p_vpar->fifo.data_lock );

        /* ?? trash all trashable PES packets */
        while( !DECODER_FIFO_ISEMPTY(p_vpar->fifo) )
        {
            input_NetlistFreePES( p_vpar->bit_stream.p_input,
                                  DECODER_FIFO_START(p_vpar->fifo) );
            DECODER_FIFO_INCSTART( p_vpar->fifo );
        }

        vlc_mutex_unlock( &p_vpar->fifo.data_lock );
        /* Sleep a while */
        msleep( VPAR_IDLE_SLEEP );
    }
}

/*******************************************************************************
 * EndThread: thread destruction
 *******************************************************************************
 * This function is called when the thread ends after a sucessfull 
 * initialization.
 *******************************************************************************/
static void EndThread( vpar_thread_t *p_vpar )
{
    int i_dummy;

    intf_DbgMsg("vpar debug: destroying video parser thread %p\n", p_vpar);

#ifdef DEBUG
    /* Check for remaining PES packets */
    /* ?? */
#endif

    /* Destroy thread structures allocated by InitThread */
//    vout_DestroyStream( p_vpar->p_vout, p_vpar->i_stream );
    /* ?? */

    /* Destroy vdec threads */
    for( i_dummy = 0; i_dummy < NB_VDEC; i_dummy++ )
    {
        if( p_vpar->p_vdec[i_dummy] != NULL )
            vdec_DestroyThread( p_vpar->p_vdec[i_dummy] );
        else
            break;
    }

    intf_DbgMsg("vpar debug: EndThread(%p)\n", p_vpar);
}
