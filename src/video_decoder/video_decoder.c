/*******************************************************************************
 * video_decoder.c : video decoder thread
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
#include "debug.h"                      /* ?? temporaire, requis par netlist.h */

#include "input.h"
#include "input_netlist.h"
#include "decoder_fifo.h"
#include "video.h"
#include "video_output.h"
#include "video_parser.h"

#include "undec_picture.h"
#include "video_fifo.h"
#include "video_decoder.h"

/*
 * Local prototypes
 */
static int      InitThread          ( vdec_thread_t *p_vdec );
static void     RunThread           ( vdec_thread_t *p_vdec );
static void     ErrorThread         ( vdec_thread_t *p_vdec );
static void     EndThread           ( vdec_thread_t *p_vdec );
static void     DecodePicture       ( vdec_thread_t *p_vdec,
                                      undec_picture_t * p_undec_p );

/*******************************************************************************
 * vdec_CreateThread: create a video decoder thread
 *******************************************************************************
 * This function creates a new video decoder thread, and returns a pointer
 * to its description. On error, it returns NULL.
 * Following configuration properties are used:
 * ??
 *******************************************************************************/
vdec_thread_t * vdec_CreateThread( vpar_thread_t *p_vpar /*, int *pi_status */ )
{
    vdec_thread_t *     p_vdec;

    intf_DbgMsg("vdec debug: creating video decoder thread\n");

    /* Allocate the memory needed to store the thread's structure */
    if ( (p_vdec = (vdec_thread_t *)malloc( sizeof(vdec_thread_t) )) == NULL )
    {
        intf_ErrMsg("vdec error: not enough memory for vdec_CreateThread() to create the new thread\n");
        return( NULL );
    }

    /*
     * Initialize the thread properties
     */
    p_vdec->b_die = 0;
    p_vdec->b_error = 0;

    /*
     * Initialize the parser properties
     */
    p_vdec->p_vpar = p_vpar;

    /* Spawn the video decoder thread */
    if ( vlc_thread_create(&p_vdec->thread_id, "video decoder",
         (vlc_thread_func)RunThread, (void *)p_vdec) )
    {
        intf_ErrMsg("vdec error: can't spawn video decoder thread\n");
        free( p_vdec );
        return( NULL );
    }

    intf_DbgMsg("vdec debug: video decoder thread (%p) created\n", p_vdec);
    return( p_vdec );
}

/*******************************************************************************
 * vdec_DestroyThread: destroy a video decoder thread
 *******************************************************************************
 * Destroy and terminate thread. This function will return 0 if the thread could
 * be destroyed, and non 0 else. The last case probably means that the thread
 * was still active, and another try may succeed.
 *******************************************************************************/
void vdec_DestroyThread( vdec_thread_t *p_vdec /*, int *pi_status */ )
{
    intf_DbgMsg("vdec debug: requesting termination of video decoder thread %p\n", p_vdec);

    /* Ask thread to kill itself */
    p_vdec->b_die = 1;

    /* Waiting for the decoder thread to exit */
    /* Remove this as soon as the "status" flag is implemented */
    vlc_thread_join( p_vdec->thread_id );
}

/* following functions are local */

/*******************************************************************************
 * InitThread: initialize video decoder thread
 *******************************************************************************
 * This function is called from RunThread and performs the second step of the
 * initialization. It returns 0 on success. Note that the thread's flag are not
 * modified inside this function.
 *******************************************************************************/
static int InitThread( vdec_thread_t *p_vdec )
{
    intf_DbgMsg("vdec debug: initializing video decoder thread %p\n", p_vdec);

    /* Initialize other properties */
#ifdef STATS
    p_vdec->c_loops = 0;    
    p_vdec->c_idle_loops = 0;
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
 * RunThread: video decoder thread
 *******************************************************************************
 * Video decoder thread. This function does only return when the thread is
 * terminated. 
 *******************************************************************************/
static void RunThread( vdec_thread_t *p_vdec )
{
    intf_DbgMsg("vdec debug: running video decoder thread (%p) (pid == %i)\n",
                p_vdec, getpid());

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
        undec_picture_t *       p_undec_p;
        
        if( (p_undec_p = GetPicture( p_vdec->p_vpar->p_fifo )) != NULL )
        {
            DecodePicture( p_vdec, p_undec_p );
        }
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
    undec_picture_t *       p_undec_p;

    /* Wait until a `die' order */
    while( !p_vdec->b_die )
    {
        p_undec_p = GetPicture( p_vdec->p_vpar.vfifo );
        DestroyPicture( p_vdec->p_vpar.vfifo, p_undec_p );

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
    intf_DbgMsg("vdec debug: EndThread(%p)\n", p_vdec);
}

/*******************************************************************************
 * DecodePicture : decode a picture
 *******************************************************************************/
static void DecodePicture( vdec_thread_t *p_vdec, undec_picture_t * p_undec_p )
{
    static int              pi_chroma_nb_blocks[4] = {0, 1, 2, 4};
    static int              pi_chroma_nb_coeffs[4] = {0, 64, 128, 256};
    static f_motion_mb_t    ppf_chroma_motion[4] = { NULL,
                                                     &vdec_MotionMacroBlock420,
                                                     &vdec_MotionMacroBlock422,
                                                     &vdec_MotionMacroBlock444 };
    static f_motion_t       pppf_motion_forward[4][2] = {
                                {NULL, NULL} /* I picture */
                                {&vdec_MotionForward, &vdec_MotionForward} /* P */
                                {NULL, &vdec_MotionForward} /* B */
                                {NULL, NULL} /* D */ };
    static f_motion_t       pppf_motion_backward[4][2] = {
                                {NULL, NULL} /* I picture */
                                {NULL, NULL} /* P */
                                {NULL, &vdec_MotionBackward} /* B */
                                {NULL, NULL} /* D */ };
    static f_motion_t       ppf_motion[4] = { NULL,
                                              &vdec_MotionTopFirst,
                                              &vdec_MotionBottomFirst,
                                              &vdec_MotionFrame };

    int             i_mb, i_b, i_totb;
    coeff_t *       p_y, p_u, p_v;
    f_motion_mb_t   pf_chroma_motion;
    f_motion_t      pf_motion_forward, pf_motion_backward;
    int             i_chroma_nb_blocks, i_chroma_nb_coeffs;
    
    p_y = (coeff_t *)p_undec_p->p_picture->p_y;
    p_u = (coeff_t *)p_undec_p->p_picture->p_u;
    p_v = (coeff_t *)p_undec_p->p_picture->p_v;

#define I_chroma_format     p_undec_p->p_picture->i_chroma_format
    pf_chroma_motion = ppf_chroma_motion[I_chroma_format];
    pf_motion_forward
    pf_motion = ppf_motion[p_undec_p->i_structure];

    i_chroma_nb_blocks = pi_chroma_nb_blocks[I_chroma_format];
    i_chroma_nb_coeffs = pi_chroma_nb_coeffs[I_chroma_format];
#undef I_chroma_format

    for( i_mb = 0; i_mb < p_undec_p->i_mb_height*p_undec_p->i_mb_width; i_mb++ )
    {
#define P_mb_info           p_undec_p->p_mb_info[i_ref]

        /*
         * Inverse DCT (ISO/IEC 13818-2 section Annex A)
         */
        
        /* Luminance : always 4 blocks */
        for( i_b = 0; i_b < 4; i_b++ )
        {
            (*P_mb_info.p_idct_function[i_b])( p_y + i_b*64 );
        }
        i_totb = 4;
        
        /* Chrominance Cr */
        for( i_b = 0; i_b < i_chroma_nb_blocks; i_b++ )
        {
            (*P_mb_info.p_idct_function[i_totb + i_b])( p_u + i_b*64 );
        }
        i_totb += i_chroma_nb_blocks;
        
        /* Chrominance Cb */
        for( i_b = 0; i_b < i_chroma_nb_blocks; i_b++ )
        {
            (*P_mb_info.p_idct_function[i_totb + i_b])( p_v + i_b*64 );
        }

        /*
         * Motion Compensation (ISO/IEC 13818-2 section 7.6)
         */
        (*pf_motion)( p_vdec, p_undec_p, i_mb, pf_chroma_motion );

        p_y += 256;
        p_u += i_chroma_nb_coeffs;
        p_v += i_chroma_nb_coeffs;
#undef P_mb_info
    }

    /*
     * Decoding is finished, mark the picture ready for displaying and free
     * unneeded memory
     */
    vpar_ReleasePicture( p_vdec->p_vpar->p_fifo, p_undec_p );
}
