/*****************************************************************************
 * video_fifo.h : FIFO for the pool of video_decoders
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: video_fifo.h,v 1.4 2001/02/23 14:07:25 massiot Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/
 
/*****************************************************************************
 * Requires:
 *  "config.h"
 *  "common.h"
 *  "threads.h"
 *  "video_parser.h"
 *  "undec_picture.h"
 *****************************************************************************/

/*****************************************************************************
 * Macros
 *****************************************************************************/

#ifdef VDEC_SMP
/* FIXME: move to inline functions ??*/
#define VIDEO_FIFO_ISEMPTY( fifo )    ( (fifo).i_start == (fifo).i_end )
#define VIDEO_FIFO_ISFULL( fifo )     ( ( ( (fifo).i_end + 1 - (fifo).i_start )\
                                          & VFIFO_SIZE ) == 0 )
#define VIDEO_FIFO_START( fifo )      ( (fifo).buffer[ (fifo).i_start ] )
#define VIDEO_FIFO_INCSTART( fifo )   ( (fifo).i_start = ((fifo).i_start + 1) \
                                                           & VFIFO_SIZE )
#define VIDEO_FIFO_END( fifo )        ( (fifo).buffer[ (fifo).i_end ] )
#define VIDEO_FIFO_INCEND( fifo )     ( (fifo).i_end = ((fifo).i_end + 1) \
                                                         & VFIFO_SIZE )
#endif

/*****************************************************************************
 * vpar_GetMacroblock : return a macroblock to be decoded
 *****************************************************************************/
static __inline__ macroblock_t * vpar_GetMacroblock( video_fifo_t * p_fifo )
{
#ifdef VDEC_SMP
    macroblock_t *      p_mb;

    vlc_mutex_lock( &p_fifo->lock );
    while( VIDEO_FIFO_ISEMPTY( *p_fifo ) )
    {
        vlc_cond_wait( &p_fifo->wait, &p_fifo->lock );
        if( p_fifo->p_vpar->p_fifo->b_die )
        {
            vlc_mutex_unlock( &p_fifo->lock );
            return( NULL );
        }
    }

    p_mb = VIDEO_FIFO_START( *p_fifo );
    VIDEO_FIFO_INCSTART( *p_fifo );

    vlc_mutex_unlock( &p_fifo->lock );

    return( p_mb );
#else
    /* Shouldn't normally be used without SMP. */
    return NULL;
#endif
}

/*****************************************************************************
 * vpar_NewMacroblock : return a buffer for the parser
 *****************************************************************************/
static __inline__ macroblock_t * vpar_NewMacroblock( video_fifo_t * p_fifo )
{
#ifdef VDEC_SMP
    macroblock_t *      p_mb;

#define P_buffer p_fifo->p_vpar->vbuffer
    vlc_mutex_lock( &P_buffer.lock );
    while( P_buffer.i_index == -1 )
    {
        /* No more structures available. This should not happen ! */
        intf_DbgMsg("vpar debug: macroblock list is empty, delaying");
        vlc_mutex_unlock( &P_buffer.lock );
        if( p_fifo->p_vpar->p_fifo->b_die )
        {
            return( NULL );
        }
        msleep(VPAR_OUTMEM_SLEEP);
        vlc_mutex_lock( &P_buffer.lock );
    }

    p_mb = P_buffer.pp_mb_free[ P_buffer.i_index-- ];

    vlc_mutex_unlock( &P_buffer.lock );
#undef P_buffer
    return( p_mb );
#else
    return( &p_fifo->buffer );
#endif
}

/*****************************************************************************
 * vpar_DecodeMacroblock : put a macroblock in the video fifo
 *****************************************************************************/
static __inline__ void vpar_DecodeMacroblock( video_fifo_t * p_fifo,
                                              macroblock_t * p_mb )
{
#ifdef VDEC_SMP
    /* Place picture in the video FIFO */
    vlc_mutex_lock( &p_fifo->lock );

    /* By construction, the video FIFO cannot be full */
    VIDEO_FIFO_END( *p_fifo ) = p_mb;
    VIDEO_FIFO_INCEND( *p_fifo );

    vlc_mutex_unlock( &p_fifo->lock );
#else
    vdec_DecodeMacroblockC( p_fifo->p_vpar->pp_vdec[0], p_mb );
#endif
}

/*****************************************************************************
 * vpar_ReleaseMacroblock : release a macroblock and put the picture in the
 *                          video output heap, if it is finished
 *****************************************************************************/
static __inline__ void vpar_ReleaseMacroblock( video_fifo_t * p_fifo,
                                               macroblock_t * p_mb )
{
#ifdef VDEC_SMP
    boolean_t      b_finished;

    /* Unlink picture buffer */
    vlc_mutex_lock( &p_mb->p_picture->lock_deccount );
    p_mb->p_picture->i_deccount--;
    b_finished = (p_mb->p_picture->i_deccount == 1);
    vlc_mutex_unlock( &p_mb->p_picture->lock_deccount );
//intf_DbgMsg( "%d ", p_mb->p_picture->i_deccount );
    /* Test if it was the last block of the picture */
    if( b_finished )
    {
//intf_DbgMsg( "Image decodee" );
        /* Mark the picture to be displayed */
        vout_DisplayPicture( p_fifo->p_vpar->p_vout, p_mb->p_picture );

        /* Warn Synchro for its records. */
        vpar_SynchroEnd( p_fifo->p_vpar, 0 );

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

#else
    p_mb->p_picture->i_deccount--;
    if( p_mb->p_picture->i_deccount == 1 )
    {
        /* Mark the picture to be displayed */
        vout_DisplayPicture( p_fifo->p_vpar->p_vout, p_mb->p_picture );

        /* Warn Synchro for its records. */
        vpar_SynchroEnd( p_fifo->p_vpar, 0 );
    }
#endif
}

/*****************************************************************************
 * vpar_DestroyMacroblock : destroy a macroblock in case of error
 *****************************************************************************/
static __inline__ void vpar_DestroyMacroblock( video_fifo_t * p_fifo,
                                               macroblock_t * p_mb )
{
#ifdef VDEC_SMP
    boolean_t       b_finished;

    /* Unlink picture buffer */
    vlc_mutex_lock( &p_mb->p_picture->lock_deccount );
    p_mb->p_picture->i_deccount--;
    b_finished = (p_mb->p_picture->i_deccount == 1);
    vlc_mutex_unlock( &p_mb->p_picture->lock_deccount );

    /* Test if it was the last block of the picture */
    if( b_finished )
    {
        intf_DbgMsg( "Image trashee" );
        /* Mark the picture to be trashed */
        vout_DestroyPicture( p_fifo->p_vpar->p_vout, p_mb->p_picture );

        /* Warn Synchro for its records. */
        vpar_SynchroEnd( p_fifo->p_vpar, 1 );

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

#else
    p_mb->p_picture->i_deccount--;
    if( p_mb->p_picture->i_deccount == 1 )
    {
        /* Mark the picture to be trashed */
        vout_DestroyPicture( p_fifo->p_vpar->p_vout, p_mb->p_picture );

        /* Warn Synchro for its records. */
        vpar_SynchroEnd( p_fifo->p_vpar, 1 );
    }
#endif
}

/*****************************************************************************
 * vpar_FreeMacroblock : destroy a macroblock in case of error, without
 * updating the macroblock counters
 *****************************************************************************/
static __inline__ void vpar_FreeMacroblock( video_fifo_t * p_fifo,
                                            macroblock_t * p_mb )
{
#ifdef VDEC_SMP
    /* Release the macroblock_t structure */
#define P_buffer p_fifo->p_vpar->vbuffer
    vlc_mutex_lock( &P_buffer.lock );
    P_buffer.pp_mb_free[ ++P_buffer.i_index ] = p_mb;
    vlc_mutex_unlock( &P_buffer.lock );
#undef P_buffer

#else
    ;
#endif
}
/*****************************************************************************
 * Prototypes
 *****************************************************************************/
void vpar_InitFIFO( struct vpar_thread_s * p_vpar );
