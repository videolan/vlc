/*****************************************************************************
 * video_fifo.h : FIFO for the pool of video_decoders
 * (c)1999 VideoLAN
 *****************************************************************************
 *****************************************************************************
 * Requires:
 *  "config.h"
 *  "common.h"
 *  "vlc_thread.h"
 *  "video_parser.h"
 *  "undec_picture.h"
 *****************************************************************************/

/*****************************************************************************
 * Macros
 *****************************************************************************/

/* ?? move to inline functions */
#define VIDEO_FIFO_ISEMPTY( fifo )    ( (fifo).i_start == (fifo).i_end )
#define VIDEO_FIFO_ISFULL( fifo )     ( ( ( (fifo).i_end + 1 - (fifo).i_start ) \
                                          & VFIFO_SIZE ) == 0 )
#define VIDEO_FIFO_START( fifo )      ( (fifo).buffer[ (fifo).i_start ] )
#define VIDEO_FIFO_INCSTART( fifo )   ( (fifo).i_start = ((fifo).i_start + 1) \
                                                           & VFIFO_SIZE ) 
#define VIDEO_FIFO_END( fifo )        ( (fifo).buffer[ (fifo).i_end ] )
#define VIDEO_FIFO_INCEND( fifo )     ( (fifo).i_end = ((fifo).i_end + 1) \
                                                         & VFIFO_SIZE )

/*****************************************************************************
 * vpar_GetMacroblock : return a macroblock to be decoded
 *****************************************************************************/
static __inline__ macroblock_t * vpar_GetMacroblock( video_fifo_t * p_fifo )
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
static __inline__ macroblock_t * vpar_NewMacroblock( video_fifo_t * p_fifo )
{
    macroblock_t *      p_mb;

#define P_buffer p_fifo->p_vpar->vbuffer
    vlc_mutex_lock( &P_buffer.lock );
    while( P_buffer.i_index == -1 )
    {
        /* No more structures available. This should not happen ! */
        intf_DbgMsg("vpar debug: macroblock list is empty, delaying\n");
        vlc_mutex_unlock( &P_buffer.lock );
        msleep(VPAR_OUTMEM_SLEEP);
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
static __inline__ void vpar_DecodeMacroblock( video_fifo_t * p_fifo,
                                              macroblock_t * p_mb )
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
static __inline__ void vpar_ReleaseMacroblock( video_fifo_t * p_fifo,
                                               macroblock_t * p_mb )
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
//fprintf(stderr, "Image decodee\n");
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
static __inline__ void vpar_DestroyMacroblock( video_fifo_t * p_fifo,
                                               macroblock_t * p_mb )
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

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
void vpar_InitFIFO( struct vpar_thread_s * p_vpar );
