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
#include "video_parser.h"

#include "undec_picture.h"
#include "video_fifo.h"
#include "video_decoder.h"

/*****************************************************************************
 * vpar_InitFIFO : initialize the video FIFO
 *****************************************************************************/
void vpar_InitFIFO( vpar_thread_t * p_vpar )
{
    int i_dummy;
    
    /* Initialize mutex and cond */
    vlc_mutex_init( p_vpar->vfifo.lock );
    vlc_cond_init( p_vpar->vfifo.wait );
    vlc_mutex_init( p_vpar->vbuffer.lock );
    
    /* Initialize FIFO properties */
    p_vpar->vfifo.i_start = p_vpar->vfifo.i_end = 0;
    p_vpar->vfifo.p_vpar = p_vpar;
    
    /* Initialize buffer properties */
    i_index = VFIFO_SIZE; /* all structures are available */
    for( i_dummy = 0; i_dummy < VFIFO_SIZE + 1; i_dummy++ )
    {
        p_vpar->vfifo.pp_undec_free[i_dummy] = p_vpar->vfifo.p_undec_p + i;
        p_vpar->vfifo.p_undec_p[i].p_mb_info = NULL;
    }
}

/*****************************************************************************
 * vpar_GetPicture : return a picture to be decoded
 *****************************************************************************/
undec_picture_t * vpar_GetPicture( video_fifo_t * p_fifo )
{
    undec_picture_t *   p_undec_p;

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
    
    p_undec_p = VIDEO_FIFO_START( *p_fifo );
    VIDEO_FIFO_INCSTART( *p_fifo );
    
    vlc_mutex_unlock( &p_fifo->lock );
    
    return( p_undec_p );
}

/*****************************************************************************
 * vpar_NewPicture : return a buffer for the parser
 *****************************************************************************/
undec_picture_t * vpar_NewPicture( video_fifo_t * p_fifo )
{
    undec_picture_t *   p_undec_p;

#define P_buffer p_fifo->p_vpar.vbuffer
    vlc_mutex_lock( &P_buffer->lock );
    if( P_buffer.i_index == -1 )
    {
        /* No more structures available. This should not happen ! */
        return NULL;
    }

    p_undec_p = P_buffer->pp_undec_free[ P_buffer->i_index-- ];
#undef P_buffer

    vlc_mutex_unlock( &P_buffer->lock );
    return( p_undec_p );
}

/*****************************************************************************
 * vpar_DecodePicture : put a picture in the video fifo, if it is decodable
 *****************************************************************************/
void vpar_DecodePicture( video_fifo_t * p_fifo, undec_picture_t * p_undec_p )
{
    boolean_t           b_decodable;
    
    switch( p_undec_p )
    {
    case B_CODING_TYPE:
        b_decodable = ((p_undec_p->p_backward_p != NULL) &&
                       (p_undec_p->p_forward_p != NULL));
        break;
    case P_CODING_TYPE:
        b_decodable = (p_undec_p->p_backward_p != NULL);
        break;
    case I_CODING_TYPE:
    case D_CODING_TYPE:
        b_decodable = TRUE;
        break;
    default:
        /* That should not happen */
    }

    if( b_decodable )
    {
        /* Place picture in the video FIFO */
        vlc_mutex_lock( &p_fifo->lock );
        
        /* By construction, the video FIFO cannot be full */
        VIDEO_FIFO_END( *p_fifo ) = p_undec_p;
        VIDEO_FIFO_INCEND( *p_fifo );
        
        vlc_mutex_unlock( &p_fifo->lock );
    }
}

/*****************************************************************************
 * vpar_ReleasePicture : put a picture in the video_output fifo, and update
 *                  links and buffers
 *****************************************************************************/
void vpar_ReleasePicture( video_fifo_t * p_fifo, undec_picture_t * p_undec_p )
{
    int         i_ref;
    
    /* Tell referencing pictures so that they can be decoded */
    for( i_ref = 0; p_undec_p->pp_referencing_undec[i_ref].p_undec != NULL
                    && i_ref < MAX_REFERENCING_UNDEC; i_ref++ )
    {
        *p_undec_p->pp_referencing_undec[i_ref].pp_frame = p_undec_p->p_picture;
        vout_LinkPicture( p_fifo->p_vpar.p_vout, p_picture );
        
        /* Try to put the referencing picture in the video FIFO */
        vpar_DecodePicture( p_fifo, p_undec_p->pp_referencing_undec[i_ref].p_undec );
    }

    /* Unlink referenced pictures */
    if( p_undec_p->p_forward_ref != NULL )
    {
        vout_UnlinkPicture( p_fifo->p_vpar.p_vout, p_undec_p->p_forward_ref );
        if( p_undec_p->p_backward_ref != NULL )
        {
            vout_UnlinkPicture( p_fifo->p_vpar.p_vout, p_undec_p->p_backward_ref );
        }
    }
    
    /* Mark the picture to be displayed */
    vout_DisplayPicture( p_fifo->p_vpar.p_vout, p_undec_p->p_picture );

    /* Release the undec_picture_t structure */
#define P_buffer p_fifo->p_vpar.vbuffer
    vlc_mutex_lock( &P_buffer->lock );

    P_buffer->pp_undec_free[ ++P_buffer->i_index ] = p_undec_p;
    
    vlc_mutex_unlock( &P_buffer->lock );
#undef P_buffer
    }
}

/*****************************************************************************
 * vpar_DestroyPicture : destroy a picture in case of error
 *****************************************************************************/
void vpar_DestroyPicture( video_fifo_t * p_fifo, undec_picture_t * p_undec_p )
{
    int         i_ref;
    
    /* Destroy referencing pictures */
    for( i_ref = 0; p_undec_p->pp_referencing_undec[i_ref].p_undec != NULL
                    && i_ref < MAX_REFERENCING_UNDEC; i_ref++ )
    {
        /* Try to put the referencing picture in the video FIFO */
        vpar_DestroyPicture( p_fifo, p_undec_p->pp_referencing_undec[i_ref].p_undec );
    }

    /* Unlink referenced pictures */
    if( p_undec_p->p_forward_ref != NULL )
    {
        vout_UnlinkPicture( p_fifo->p_vpar.p_vout, p_undec_p->p_forward_ref );
        if( p_undec_p->p_backward_ref != NULL )
        {
            vout_UnlinkPicture( p_fifo->p_vpar.p_vout, p_undec_p->p_backward_ref );
        }
    }
    
    /* Release the picture buffer */
    vout_DestroyPicture( p_fifo->p_vpar.p_vout, p_undec_p->p_picture );
    
    /* Release the undec_picture_t structure */
#define P_buffer p_fifo->p_vpar.vbuffer
    vlc_mutex_lock( &P_buffer->lock );

    P_buffer->pp_undec_free[ ++P_buffer->i_index ] = p_undec_p;
    
    vlc_mutex_unlock( &P_buffer->lock );
#undef P_buffer
    }
}
