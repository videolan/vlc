/*******************************************************************************
 * video_decoder.c : video decoder thread
 * (c)1999 VideoLAN
 *******************************************************************************/

/* ?? passer en terminate/destroy avec les signaux supplémentaires */

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
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

#include "config.h"
#include "common.h"
#include "mtime.h"
#include "vlc_thread.h"

#include "intf_msg.h"
#include "debug.h"                    /* ?? temporaire, requis par netlist.h */

#include "input.h"
#include "input_netlist.h"
#include "decoder_fifo.h"
#include "video.h"
#include "video_output.h"

#include "vdec_idct.h"
#include "video_decoder.h"
#include "vdec_motion.h"

#include "vpar_blocks.h"
#include "vpar_headers.h"
#include "video_fifo.h"
#include "vpar_synchro.h"
#include "video_parser.h"

/*
 * Local prototypes
 */
static int      InitThread          ( vdec_thread_t *p_vdec );
static void     RunThread           ( vdec_thread_t *p_vdec );
static void     ErrorThread         ( vdec_thread_t *p_vdec );
static void     EndThread           ( vdec_thread_t *p_vdec );
static void     DecodeMacroblock    ( vdec_thread_t *p_vdec,
                                      macroblock_t * p_mb );

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
         (vlc_thread_func_t)RunThread, (void *)p_vdec) )
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
#ifdef MPEG2_COMPLIANT
    int i_dummy;
#endif

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

#ifdef MPEG2_COMPLIANT
    /* Init crop table */
    p_vdec->pi_crop = p_vdec->pi_crop_buf + (VDEC_CROPRANGE >> 1);
    for( i_dummy = -VDEC_CROPRANGE; i_dummy < -256; i_dummy ++ )
    {
        p_vdec->pi_crop[i_dummy] = -256;
    }
    for( ; i_dummy < 255; i_dummy ++ )
    {
        p_vdec->pi_crop[i_dummy] = i_dummy;
    }
    for( ; i_dummy < (VDEC_CROPRANGE >> 1) -1; i_dummy++ )
    {
        p_vdec->pi_crop[i_dummy] = 255;
    }
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
        macroblock_t *          p_mb;
        
        if( (p_mb = vpar_GetMacroblock( &p_vdec->p_vpar->vfifo )) != NULL )
        {
            DecodeMacroblock( p_vdec, p_mb );
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
    macroblock_t *       p_mb;

    /* Wait until a `die' order */
    while( !p_vdec->b_die )
    {
        p_mb = vpar_GetMacroblock( &p_vdec->p_vpar->vfifo );
        vpar_DestroyMacroblock( &p_vdec->p_vpar->vfifo, p_mb );

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
 * DecodeMacroblock : decode a macroblock of a picture
 *******************************************************************************/
static void DecodeMacroblock( vdec_thread_t *p_vdec, macroblock_t * p_mb )
{
    int             i_b;

    /*
     * Motion Compensation (ISO/IEC 13818-2 section 7.6)
     */
    (*p_mb->pf_motion)( p_mb );

    /* luminance */
    for( i_b = 0; i_b < 4; i_b++ )
    {
        /*
         * Inverse DCT (ISO/IEC 13818-2 section Annex A)
         */
        (p_mb->pf_idct[i_b])( p_vdec, p_mb->ppi_blocks[i_b],
                              p_mb->pi_sparse_pos[i_b] );

        /*
         * Adding prediction and coefficient data (ISO/IEC 13818-2 section 7.6.8)
         */
        (p_mb->pf_addb[i_b])( p_mb->ppi_blocks[i_b],
                              p_mb->p_data[i_b], p_mb->i_l_stride );
    }

    /* chrominance */
    for( i_b = 4; i_b < 4 + 2*p_mb->i_chroma_nb_blocks; i_b++ )
    {
        /*
         * Inverse DCT (ISO/IEC 13818-2 section Annex A)
         */
        (p_mb->pf_idct[i_b])( p_vdec, p_mb->ppi_blocks[i_b],
                              p_mb->pi_sparse_pos[i_b] );

        /*
         * Adding prediction and coefficient data (ISO/IEC 13818-2 section 7.6.8)
         */
        (p_mb->pf_addb[i_b])( (elem_t*)p_mb->ppi_blocks[i_b],
                              p_mb->p_data[i_b], p_mb->i_c_stride );
    }

    /*
     * Decoding is finished, release the macroblock and free
     * unneeded memory.
     */
    vpar_ReleaseMacroblock( &p_vdec->p_vpar->vfifo, p_mb );
}

/*******************************************************************************
 * vdec_AddBlock : add a block
 *******************************************************************************/
void vdec_AddBlock( elem_t * p_block, yuv_data_t * p_data, int i_incr )
{
    int i_x, i_y;
    
    for( i_y = 0; i_y < 8; i_y++ )
    {
        for( i_x = 0; i_x < 8; i_x++ )
        {
#ifdef MPEG2_COMPLIANT
            *p_data = p_vdec->pi_clip[*p_data + *p_block++];
            p_data++;
#else
            *p_data++ += *p_block++;
#endif
        }
        p_data += i_incr;
    }
}

/*******************************************************************************
 * vdec_CopyBlock : copy a block
 *******************************************************************************/
void vdec_CopyBlock( elem_t * p_block, yuv_data_t * p_data, int i_incr )
{
    int i_y;
    
    for( i_y = 0; i_y < 8; i_y++ )
    {
#ifndef VDEC_DFT
        /* elem_t and yuv_data_t are the same */
        memcpy( p_data, p_block, 8*sizeof(yuv_data_t) );
        p_data += i_incr+8;
        p_block += 8;
#else
        int i_x;

        for( i_x = 0; i_x < 8; i_x++ )
        {
            /* ??? Need clip to be MPEG-2 compliant */
            /* ??? Why does the reference decoder add 128 ??? */
            *p_data++ = *p_block++;
        }
        p_data += i_incr;
#endif
    }
}

/*******************************************************************************
 * vdec_DummyBlock : dummy function that does nothing
 *******************************************************************************/
void vdec_DummyBlock( elem_t * p_block, yuv_data_t * p_data, int i_incr )
{
}
