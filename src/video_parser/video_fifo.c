/*******************************************************************************
 * video_fifo.c : video FIFO management
 * (c)1999 VideoLAN
 *******************************************************************************/

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

#include "vdec_idct.h"
#include "video_decoder.h"
#include "vdec_motion.h"

#include "vpar_blocks.h"
#include "vpar_headers.h"
#include "video_fifo.h"
#include "vpar_synchro.h"
#include "video_parser.h"

/*****************************************************************************
 * vpar_InitFIFO : initialize the video FIFO
 *****************************************************************************/
void vpar_InitFIFO( vpar_thread_t * p_vpar )
{
    int                 i_dummy;
    
    /* Initialize mutex and cond */
    vlc_mutex_init( &p_vpar->vfifo.lock );
    vlc_cond_init( &p_vpar->vfifo.wait );
    vlc_mutex_init( &p_vpar->vbuffer.lock );
    
    /* Initialize FIFO properties */
    p_vpar->vfifo.i_start = p_vpar->vfifo.i_end = 0;
    p_vpar->vfifo.p_vpar = p_vpar;
    
    /* Initialize buffer properties */
    p_vpar->vbuffer.i_index = VFIFO_SIZE; /* all structures are available */
    for( i_dummy = 0; i_dummy < VFIFO_SIZE + 1; i_dummy++ )
    {
        p_vpar->vbuffer.pp_mb_free[i_dummy] = p_vpar->vbuffer.p_macroblocks
                                               + i_dummy;
    }
}

/*****************************************************************************
 * vpar_GetMacroblock : return a macroblock to be decoded
 *****************************************************************************/
macroblock_t * vpar_GetMacroblock( video_fifo_t * p_fifo )
{
    macroblock_t *      p_mb;

    vlc_mutex_lock( &p_fifo->lock );
    while( VIDEO_FIFO_ISEMPTY( *p_fifo ) )
    {
        vlc_cond_wait( &p_fifo->wait, &p_fifo->lock );
        if( p_fifo->p_vpar->b_die )
        {
            vlc_mutex_unlock( &p_fifo->lock );
            return( NULL );
        }
    }
    
    p_mb = VIDEO_FIFO_START( *p_fifo );
    VIDEO_FIFO_INCSTART( *p_fifo );

    vlc_mutex_unlock( &p_fifo->lock );
    
    return( p_mb );
}

/*****************************************************************************
 * vpar_NewMacroblock : return a buffer for the parser
 *****************************************************************************/
macroblock_t * vpar_NewMacroblock( video_fifo_t * p_fifo )
{
    macroblock_t *      p_mb;

#define P_buffer p_fifo->p_vpar->vbuffer
    vlc_mutex_lock( &P_buffer.lock );
    while( P_buffer.i_index == -1 )
    {
        /* No more structures available. This should not happen ! */
        intf_DbgMsg("vpar debug: macroblock list is empty, delaying\n");
        vlc_mutex_unlock( &P_buffer.lock );
        msleep(VPAR_IDLE_SLEEP);
        vlc_mutex_lock( &P_buffer.lock );
    }

    p_mb = P_buffer.pp_mb_free[ P_buffer.i_index-- ];

    vlc_mutex_unlock( &P_buffer.lock );
#undef P_buffer
    return( p_mb );
}

/*****************************************************************************
 * vpar_DecodeMacroblock : put a macroblock in the video fifo
 *****************************************************************************/
void vpar_DecodeMacroblock( video_fifo_t * p_fifo, macroblock_t * p_mb )
{
    /* Place picture in the video FIFO */
    vlc_mutex_lock( &p_fifo->lock );
        
    /* By construction, the video FIFO cannot be full */
    VIDEO_FIFO_END( *p_fifo ) = p_mb;
    VIDEO_FIFO_INCEND( *p_fifo );

    vlc_mutex_unlock( &p_fifo->lock );
}

/*****************************************************************************
 * vpar_ReleaseMacroblock : release a macroblock and put the picture in the
 *                          video output heap, if it is finished
 *****************************************************************************/
void vpar_ReleaseMacroblock( video_fifo_t * p_fifo, macroblock_t * p_mb )
{
    boolean_t      b_finished;

    /* Unlink picture buffer */
    vlc_mutex_lock( &p_mb->p_picture->lock_deccount );
    p_mb->p_picture->i_deccount--;
    b_finished = (p_mb->p_picture->i_deccount == 1);
    vlc_mutex_unlock( &p_mb->p_picture->lock_deccount );
//fprintf(stderr, "%d ", p_mb->p_picture->i_deccount);
    /* Test if it was the last block of the picture */
    if( b_finished )
    {
fprintf(stderr, "Image decodee\n");
        /* Mark the picture to be displayed */
        vout_DisplayPicture( p_fifo->p_vpar->p_vout, p_mb->p_picture );

        /* Warn Synchro for its records. */
        vpar_SynchroEnd( p_fifo->p_vpar );
     
        /* Unlink referenced pictures */
        if( p_mb->p_forward != NULL )
        {
	        vout_UnlinkPicture( p_fifo->p_vpar->p_vout, p_mb->p_forward );
        }
        if( p_mb->p_backward != NULL )
        {
            vout_UnlinkPicture( p_fifo->p_vpar->p_vout, p_mb->p_backward );
        }
    }

    /* Release the macroblock_t structure */
#define P_buffer p_fifo->p_vpar->vbuffer
    vlc_mutex_lock( &P_buffer.lock );
    P_buffer.pp_mb_free[ ++P_buffer.i_index ] = p_mb;
    vlc_mutex_unlock( &P_buffer.lock );
#undef P_buffer
}

/*****************************************************************************
 * vpar_DestroyMacroblock : destroy a macroblock in case of error
 *****************************************************************************/
void vpar_DestroyMacroblock( video_fifo_t * p_fifo, macroblock_t * p_mb )
{
    boolean_t       b_finished;

    /* Unlink picture buffer */
    vlc_mutex_lock( &p_mb->p_picture->lock_deccount );
    p_mb->p_picture->i_deccount--;
    b_finished = (p_mb->p_picture->i_deccount == 0);
    vlc_mutex_unlock( &p_mb->p_picture->lock_deccount );

    /* Test if it was the last block of the picture */
    if( b_finished )
    {
fprintf(stderr, "Image trashee\n");
        /* Mark the picture to be displayed */
        vout_DestroyPicture( p_fifo->p_vpar->p_vout, p_mb->p_picture );

        /* Warn Synchro for its records. */
        vpar_SynchroEnd( p_fifo->p_vpar );

        /* Unlink referenced pictures */
        if( p_mb->p_forward != NULL )
        {
            vout_UnlinkPicture( p_fifo->p_vpar->p_vout, p_mb->p_forward );
        }
        if( p_mb->p_backward != NULL )
        {
            vout_UnlinkPicture( p_fifo->p_vpar->p_vout, p_mb->p_backward );
        }
    }

    /* Release the macroblock_t structure */
#define P_buffer p_fifo->p_vpar->vbuffer
    vlc_mutex_lock( &P_buffer.lock );
    P_buffer.pp_mb_free[ ++P_buffer.i_index ] = p_mb;
    vlc_mutex_unlock( &P_buffer.lock );
#undef P_buffer
}
