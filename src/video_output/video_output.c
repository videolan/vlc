/*******************************************************************************
 * video_output.c : video output thread
 * (c)1999 VideoLAN
 *******************************************************************************
 * This module describes the programming interface for video output threads.
 * It includes functions allowing to open a new thread, send pictures to a
 * thread, and destroy a previously oppenned video output thread.
 *******************************************************************************/

/*******************************************************************************
 * Preamble
 *******************************************************************************/
#include <errno.h> 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

#include "common.h"
#include "config.h"
#include "mtime.h"
#include "vlc_thread.h"
#include "thread.h"

#include "video.h"
#include "video_graphics.h"
#include "video_output.h"
#include "video_x11.h"

#include "intf_msg.h"

/*
 * Local prototypes
 */
static int      CheckConfiguration  ( video_cfg_t *p_cfg );
static int      InitThread          ( vout_thread_t *p_vout );
static void     RunThread           ( vout_thread_t *p_vout );
static void     ErrorThread         ( vout_thread_t *p_vout );
static void     EndThread           ( vout_thread_t *p_vout );

static picture_t *  FindPicture     ( vout_thread_t *p_vout );
static void         EmptyPicture    ( vout_thread_t *p_vout, picture_t *p_pic );
static mtime_t      FirstPass       ( vout_thread_t *p_vout, mtime_t current_date );
static void         SecondPass      ( vout_thread_t *p_vout, mtime_t display_date );
static void         SortPictures    ( vout_thread_t *p_vout, picture_t **pp_picture );
static void         RenderPicture   ( vout_thread_t *p_vout, picture_t *p_pic );
static void         ClipPicture     ( int *pi_ofs, int i_size, int *pi_pic_ofs, int *pi_pic_size,
                                      int i_ratio, int i_placement );

/*******************************************************************************
 * vout_CreateThread: creates a new video output thread
 *******************************************************************************
 * This function creates a new video output thread, and returns a pointer
 * to its description. On error, it returns NULL.
 * Following configuration properties are used:
 *  VIDEO_CFG_SIZE      video heap maximal size
 *  VIDEO_CFG_WIDTH     window width
 *  VIDEO_CFG_HEIGHT    window height 
 * Using X11 display method (the only one supported yet):
 *  VIDEO_CFG_DISPLAY   display used
 *  VIDEO_CFG_TITLE     window title
 *  VIDEO_CFG_SHM_EXT   try to use XShm extension
 * If pi_status is NULL, then the function will block until the thread is ready.
 * If not, pi_error will be updated using one of the THREAD_* constants.
 *******************************************************************************/
vout_thread_t *vout_CreateThread( video_cfg_t *p_cfg, int *pi_status )
{
    vout_thread_t * p_vout;                               /* thread descriptor */
    int             i_status;                                 /* thread status */

    /*
     * Check configuration 
     */
    if( CheckConfiguration( p_cfg ) )
    {
        return( NULL );
    }

    /* Allocate descriptor and initialize flags */
    p_vout = (vout_thread_t *) malloc( sizeof(vout_thread_t) );
    if( p_vout == NULL )                                              /* error */
    {
        return( NULL );
    }
    if( vout_X11AllocOutputMethod( p_vout, p_cfg ) )
    {
        free( p_vout );
        return( NULL );        
    }

    /* Copy configuration */
    p_vout->i_max_pictures = p_cfg->i_size;    
    p_vout->i_width = p_cfg->i_width;
    p_vout->i_height = p_cfg->i_height;
   
    /* Set status */
    p_vout->pi_status = (pi_status != NULL) ? pi_status : &i_status;
    *p_vout->pi_status = THREAD_CREATE;
    
    /* Initialize flags */
    p_vout->b_die = 0;
    p_vout->b_error = 0;    
    p_vout->b_active = 0;

    /* Create thread and set locks */
    vlc_mutex_init( &p_vout->streams_lock );
    vlc_mutex_init( &p_vout->pictures_lock );
    if( vlc_thread_create( &p_vout->thread_id, "video output", (vlc_thread_func)RunThread, (void *) p_vout) )
    {
        intf_ErrMsg("vout error: %s\n", strerror(ENOMEM));
        intf_DbgMsg("failed\n");        
        vout_X11FreeOutputMethod( p_vout );        
        free( p_vout );
        return( NULL );
    }   

    /* If status is NULL, wait until the thread is created */
    if( pi_status == NULL )
    {
        do
        {            
            msleep( THREAD_SLEEP );
        }while( (i_status != THREAD_READY) && (i_status != THREAD_ERROR) 
                && (i_status != THREAD_FATAL) );
        if( i_status != THREAD_READY )
        {
            intf_DbgMsg("failed\n");            
            return( NULL );            
        }        
    }

    intf_DbgMsg("succeeded -> %p\n", p_vout);    
    return( p_vout );
}

/*******************************************************************************
 * vout_DestroyThread: destroys a previously created thread
 *******************************************************************************
 * Destroy a terminated thread. 
 * The function will request a destruction of the specified thread. If pi_error
 * is NULL, it will return once the thread is destroyed. Else, it will be 
 * update using one of the THREAD_* constants.
 *******************************************************************************/
void vout_DestroyThread( vout_thread_t *p_vout, int *pi_status )
{  
    int     i_status;                                         /* thread status */

    /* Set status */
    p_vout->pi_status = (pi_status != NULL) ? pi_status : &i_status;
    *p_vout->pi_status = THREAD_DESTROY;
     
    /* Request thread destruction */
    p_vout->b_die = 1;

    /* If status is NULL, wait until thread has been destroyed */
    if( pi_status )
    {
        do
        {
            msleep( THREAD_SLEEP );
        }while( (i_status != THREAD_OVER) && (i_status != THREAD_ERROR) 
                && (i_status != THREAD_FATAL) );   
    }
    
    intf_DbgMsg("%p -> succeeded\n", p_vout);
}

/*******************************************************************************
 * vout_DisplayPicture: add a picture to a thread
 *******************************************************************************
 * The picture will be placed in an empty place of the video heap of the thread.
 * If this is impossible, NULL will be returned. Else, the original picture
 * is destroyed.
 *******************************************************************************/
picture_t * vout_DisplayPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t * p_newpic;                                 /* new picture index */

    p_newpic = FindPicture( p_vout );
    if( p_newpic != NULL )
    {        
        /* Copy picture information */
        video_CopyPictureDescriptor( p_newpic, p_pic );
        p_newpic->p_data = p_pic->p_data;
        free( p_pic );

        /* Update heap size and stats */
        p_vout->i_pictures++;
#ifdef STATS
        p_vout->c_pictures++;
        p_vout->p_stream[ p_newpic->i_stream ].c_pictures++;
#endif
    }

    /* Release lock and return */
    vlc_mutex_unlock( &p_vout->pictures_lock );
    return( p_newpic );
}

/*******************************************************************************
 * vout_DisplayPictureCopy: copy a picture to a thread
 *******************************************************************************
 * The picture will be copied in an empty place of the video heap of the thread.
 * If this is impossible, NULL will be returned. The picture used is the copy
 * of the picture passed as argument, which remains usable.
 * The picture data are copied only if the original picture owns its own data.
 * The link reference counter is set to 0.
 *******************************************************************************/
picture_t * vout_DisplayPictureCopy( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t * p_newpic;                                 /* new picture index */

    p_newpic = FindPicture( p_vout );
    if( p_newpic != NULL )
    {        
        /* Copy picture information */
        video_CopyPictureDescriptor( p_newpic, p_pic );

        /* If source picture owns its data, make a copy */
        if( p_pic->i_flags & OWNER_PICTURE )
        {      
            p_newpic->p_data = malloc( p_pic->i_height * p_pic->i_bytes_per_line );
            if( p_newpic->p_data == NULL )                            /* error */
            {
                intf_ErrMsg("vout error: %s\n", strerror(ENOMEM) );

                /* Restore type and flags */
                p_newpic->i_type = EMPTY_PICTURE;
                p_newpic->i_flags = 0;
                vlc_mutex_unlock( &p_vout->pictures_lock );            
                return( NULL );
            }
            memcpy( p_newpic->p_data, p_pic->p_data,
                    p_pic->i_height * p_pic->i_bytes_per_line );
        }
        /* If source picture does not owns its data, copy the reference */
        else              
        {
            p_newpic->p_data = p_pic->p_data;
        }
        p_newpic->i_refcount = 0;
            
        /* Update heap size and stats */
        p_vout->i_pictures++;
#ifdef STATS
        p_vout->c_pictures++;
        p_vout->p_stream[ p_newpic->i_stream ].c_pictures++;
#endif
    }

    /* Release lock and return */
    vlc_mutex_unlock( &p_vout->pictures_lock );
    return( p_newpic );
}

/*******************************************************************************
 * vout_DisplayPictureReplicate: copy a picture to a thread
 *******************************************************************************
 * The picture will be copied in an empty place of the video heap of the thread.
 * If this is impossible, NULL will be returned. The picture used is the copy
 * of the picture passed as argument, which remains usable.
 * The picture data are a reference to original picture (the picture in the heap
 * does not owns its data). Link reference counter is set to 0.
 *******************************************************************************/
picture_t * vout_DisplayPictureReplicate( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t * p_newpic;                            /* new picture descrpitor */

    p_newpic = FindPicture( p_vout );
    if( p_newpic != NULL )
    {        
        /* Copy picture information */
        video_CopyPictureDescriptor( p_newpic, p_pic );   
        p_newpic->i_flags &= ~OWNER_PICTURE;
        p_newpic->p_data = p_pic->p_data;
        p_newpic->i_refcount = 0;
            
        /* Update heap size and stats */
        p_vout->i_pictures++;
#ifdef STATS
        p_vout->c_pictures++;
        p_vout->p_stream[ p_newpic->i_stream ].c_pictures++;
#endif
    }

   /* Release lock and return */
    vlc_mutex_unlock( &p_vout->pictures_lock );
    return( p_newpic );
}

/*******************************************************************************
 * vout_DisplayReservedPicture: add a reserved picture to a thread
 *******************************************************************************
 * The picture will be placed in an empty place of the video heap of the thread.
 * If the picture has been declared on reservation as permanent, it remains
 * usable. Else, it will be destroyed by this call.
 * This function can't fail, since it is the purpose of picture reservation to
 * provide such a robust mechanism.
 *******************************************************************************/
picture_t * vout_DisplayReservedPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    vlc_mutex_lock( &p_vout->pictures_lock );

    /* Remove reservation flag */
    p_pic->i_flags &= ~RESERVED_PICTURE;

#ifdef STATS
    /* Update stats */
    p_vout->c_pictures++;
    p_vout->p_stream[ p_pic->i_stream ].c_pictures++;
#endif

    vlc_mutex_unlock( &p_vout->pictures_lock );
    return( p_pic );
}

/*******************************************************************************
 * vout_CreateReservedPicture: reserve a place for a new picture in heap
 *******************************************************************************
 * This function create a reserved image in the video output heap. 
 * A null pointer is returned if the function fails. 
 * Following configuration properties are used:
 *  VIDEO_CFG_WIDTH     picture width (required)
 *  VIDEO_CFG_HEIGHT    picture height (required)
 *  VIDEO_CFG_TYPE      picture type (required)
 *  VIDEO_CFG_FLAGS     flags
 *  VIDEO_CFG_BPP       padded bpp (required for pixel pictures)
 *  VIDEO_CFG_POSITION  position in output window
 *  VIDEO_CFG_ALIGN     base position in output window
 *  VIDEO_CFG_RATIO     display ratio
 *  VIDEO_CFG_LEVEL     overlay hierarchical level  
 *  VIDEO_CFG_REFCOUNT  links reference counter
 *  VIDEO_CFG_STREAM    video stream id
 *  VIDEO_CFG_DATE      display date
 *  VIDEO_CFG_DURATION  duration for overlay pictures
 *  VIDEO_CFG_PIXEL     pixel value for mask pictures
 *  VIDEO_CFG_DATA      picture data (required if not owner and non blank)
 * Pictures created using this function are always reserved, even if they did
 * not had the RESERVED_PICTURE flag set.
 * This function is very similar to the video_CreatePicture function.
 *******************************************************************************/
picture_t *vout_CreateReservedPicture( vout_thread_t *p_vout, video_cfg_t *p_cfg )
{
    picture_t * p_newpic;                                 /* new picture index */

    p_newpic = FindPicture( p_vout );
    if( p_newpic != NULL )
    {        
        /* Create new picture */
        if( video_CreatePictureBody( p_newpic, p_cfg ) )
        {   
            intf_ErrMsg("vout error: %s\n", strerror(ENOMEM));             
            /* Error: restore type and flags */
            p_newpic->i_type = EMPTY_PICTURE;
            p_newpic->i_flags = 0;
            vlc_mutex_unlock( &p_vout->pictures_lock );
            return( NULL );
        }
        p_newpic->i_flags |= RESERVED_PICTURE;

        /* Update heap size, release lock and return */
        p_vout->i_pictures++;
    }

    /* Release lock and return */
    vlc_mutex_unlock( &p_vout->pictures_lock );
    return( p_newpic );
}

/*******************************************************************************
 * vout_ReservePicture: reserve a place for a picture in heap
 *******************************************************************************
 * This function transforms an existing picture in a reserved picture in a
 * video heap. The picture will be placed in an empty place of the video heap 
 * of the thread. If this is impossible, NULL will be returned. Else, the 
 * original picture is destroyed.
 *******************************************************************************/
picture_t *vout_ReservePicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t * p_newpic;                                 /* new picture index */

    p_newpic = FindPicture( p_vout );
    if( p_newpic != NULL )
    {        
        /* Copy picture information */
        video_CopyPictureDescriptor( p_newpic, p_pic );
        p_newpic->p_data = p_pic->p_data;
        p_newpic->i_flags |= RESERVED_PICTURE;
        free( p_pic );           

        /* Update heap size */
        p_vout->i_pictures++;
    }

    /* Release lock and return */
    vlc_mutex_unlock( &p_vout->pictures_lock );
    return( p_newpic );
}

/*******************************************************************************
 * vout_ReservePictureCopy: reserve a place for a picture in heap
 *******************************************************************************
 * This function returns a pointer to a picture which can be processed. The
 * returned picture is a copy of the one passed as parameter.
 * This function returns NULL if the reservation fails.
 *******************************************************************************/
picture_t *vout_ReservePictureCopy( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t * p_newpic;                                 /* new picture index */

    p_newpic = FindPicture( p_vout );
    if( p_newpic != NULL )
    {        
        /* Copy picture information */
        video_CopyPictureDescriptor( p_newpic, p_pic );

        /* If source picture owns its data, make a copy */
        if( p_pic->i_flags & OWNER_PICTURE )
        {              
            p_newpic->p_data = malloc( p_pic->i_height * p_pic->i_bytes_per_line );
            if( p_newpic->p_data == NULL )                            /* error */
            {
                intf_ErrMsg("vout error: %s\n", strerror(ENOMEM));                    
                /* Restore type and flags */
                p_newpic->i_type = EMPTY_PICTURE;
                p_newpic->i_flags = 0;
                vlc_mutex_unlock( &p_vout->pictures_lock );   
                return( NULL );
            }
            memcpy( p_newpic->p_data, p_pic->p_data,
                    p_pic->i_height * p_pic->i_bytes_per_line );
        }   
        /* If source picture does not owns its data, copy the reference */
        else              
        {
            p_newpic->p_data = p_pic->p_data;
        }
        p_newpic->i_refcount = 0;
        p_newpic->i_flags |= RESERVED_PICTURE;

        /* Update heap size */
        p_vout->i_pictures++;
    }

    /* Release lock and return */
    vlc_mutex_unlock( &p_vout->pictures_lock );
    return( p_newpic );
}

/*******************************************************************************
 * vout_ReservePictureReplicate: reserve a place for a picture in heap
 *******************************************************************************
 * This function returns a pointer to a picture which can be processed. The
 * returned picture is a copy of the one passed as parameter.
 * This function returns NULL if the reservation fails.
 *******************************************************************************/
picture_t *vout_ReservePictureReplicate( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t * p_newpic;                                 /* new picture index */

    p_newpic = FindPicture( p_vout );
    if( p_newpic != NULL )
    {        
        /* Copy picture information */
        video_CopyPictureDescriptor( p_newpic, p_pic );
        p_newpic->p_data = p_pic->p_data;
        p_newpic->i_refcount = 0;
        p_newpic->i_flags &= ~OWNER_PICTURE;
        p_newpic->i_flags |= RESERVED_PICTURE;

        /* Update heap size */
        p_vout->i_pictures++;
    }

    /* Release lock and return */
    vlc_mutex_unlock( &p_vout->pictures_lock );
    return( p_newpic );
}

/*******************************************************************************
 * vout_RemovePicture: remove a permanent or reserved picture from the heap
 *******************************************************************************
 * This function frees a previously reserved picture or a permanent
 * picture. The picture data is destroyed if required. 
 *******************************************************************************/
void vout_RemovePicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    vlc_mutex_lock( &p_vout->pictures_lock );

    /* Mark picture for destruction */
    p_pic->i_flags |= DESTROY_PICTURE;
    /* Since permanent pictures can normally not be destroyed by the vout thread,   
     * the permanent flag needs to be removed */
    p_pic->i_flags &= ~PERMANENT_PICTURE;
    intf_DbgMsg("%p -> picture %p removing requested\n", p_vout, p_pic );

    vlc_mutex_unlock( &p_vout->pictures_lock );
}

/*******************************************************************************
 * vout_RefreshPermanentPicture: 
 *******************************************************************************
 * Set the DISPLAYED_PICTURE flag of a permanent picture to 0, allowing to
 * display it again if it not overlaid. The display date is also updated.
 *******************************************************************************/
void vout_RefreshPermanentPicture( vout_thread_t *p_vout, picture_t *p_pic, 
                                   mtime_t display_date )
{
    vlc_mutex_lock( &p_vout->pictures_lock );
    p_pic->i_flags &= ~DISPLAYED_PICTURE;
    p_pic->date = display_date;
    vlc_mutex_lock( &p_vout->pictures_lock );
}

/*******************************************************************************
 * vout_LinkPicture: increment reference counter of a picture
 *******************************************************************************
 * This function increment the reference counter of a picture in the video
 * heap.
 *******************************************************************************/
void vout_LinkPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    vlc_mutex_lock( &p_vout->pictures_lock );
    p_pic->i_refcount++;
    vlc_mutex_unlock( &p_vout->pictures_lock );
}

/*******************************************************************************
 * vout_UnlinkPicture: decrement reference counter of a picture
 *******************************************************************************
 * This function decrement the reference counter of a picture in the video heap.
 *******************************************************************************/
void vout_UnlinkPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    vlc_mutex_lock( &p_vout->pictures_lock );
    p_pic->i_refcount--;
#ifdef DEBUG
    if( p_pic->i_refcount < 0 )
    {
        intf_DbgMsg("%p -> picture %p i_refcount < 0\n", p_vout, p_pic);
    }
#endif
    vlc_mutex_unlock( &p_vout->pictures_lock );    
}

/*******************************************************************************
 * vout_CreateStream: get a new stream id
 *******************************************************************************
 * Stream ids are used to create list of pictures in a video output thread.
 * The function returns an unused stream id, or a negative number on error.
 * Stream id 0 is never returned, since it is not a stream descriptor but a set 
 * of independant images.
 *******************************************************************************/
int vout_CreateStream( vout_thread_t *p_vout )
{
    int     i_index;                                           /* stream index */

    /* Find free (inactive) stream id */
    vlc_mutex_lock( &p_vout->streams_lock );                   /* get lock */
    for( i_index = 1; i_index < VOUT_MAX_STREAMS; i_index++ )  /* find free id */
    {
        if( p_vout->p_stream[i_index].i_status == VOUT_INACTIVE_STREAM )
        {
            /* Initialize stream */
            p_vout->p_stream[i_index].i_status = VOUT_ACTIVE_STREAM; 
#ifdef STATS
            p_vout->p_stream[i_index].c_pictures = 0;
            p_vout->p_stream[i_index].c_rendered_pictures = 0;
#endif
            
            /* Return stream id */
            intf_DbgMsg("%p -> stream %i created\n", p_vout, i_index);
            vlc_mutex_unlock( &p_vout->streams_lock );     /* release lock */
            return( i_index );                                    /* return id */
        }
    }
   
    /* Failure: all streams id are already active */
    intf_DbgMsg("%p -> failed\n", p_vout);    
    vlc_mutex_unlock( &p_vout->streams_lock );    
    return( -1 );
}

/*******************************************************************************
 * vout_EndStream: free a previously allocated stream
 *******************************************************************************
 * This function must be called once a stream is no more used. It can be called
 * even if there are remaining pictures in the video heap, since the stream will
 * only be destroyed once all pictures of the stream have been displayed.
 *******************************************************************************/
void vout_EndStream( vout_thread_t *p_vout, int i_stream )
{
    vlc_mutex_lock( &p_vout->streams_lock );                   /* get lock */
    p_vout->p_stream[i_stream].i_status = VOUT_ENDING_STREAM;   /* mark stream */
    vlc_mutex_unlock( &p_vout->streams_lock );             /* release lock */
    intf_DbgMsg("%p -> stream %d\n", p_vout, i_stream);
}

/*******************************************************************************
 * vout_DestroyStream: free a previously allocated stream
 *******************************************************************************
 * This function must be called once a stream is no more used. It can be called
 * even if there are remaining pictures in the video heap, since all pictures
 * will be removed before the stream is destroyed.
 *******************************************************************************/
void vout_DestroyStream( vout_thread_t *p_vout, int i_stream )
{
    vlc_mutex_lock( &p_vout->streams_lock );                   /* get lock */
    p_vout->p_stream[i_stream].i_status = VOUT_DESTROYED_STREAM;/* mark stream */
    vlc_mutex_unlock( &p_vout->streams_lock );             /* release lock */
    intf_DbgMsg("%p -> stream %d\n", p_vout, i_stream);
}

/* following functions are debugging functions */

/*******************************************************************************
 * vout_PrintHeap: display heap state (debugging function)
 *******************************************************************************
 * This functions, which is only defined if DEBUG is defined, can be used 
 * for debugging purposes. It prints on debug stream the current state of the   
 * heap. The second parameter is used to identify the output.
 *******************************************************************************/
#ifdef DEBUG
void vout_PrintHeap( vout_thread_t *p_vout, char *psz_str )
{
    int         i_picture;                                    /* picture index */

    intf_Msg("vout: --- thread %p heap status (%s) ---\n", p_vout, psz_str );

    /* Browse all pictures in heap */
    for( i_picture = 0; i_picture < p_vout->i_max_pictures; i_picture++ )
    {
        if( p_vout->p_picture[i_picture].i_type != EMPTY_PICTURE )
        {
            video_PrintPicture( &p_vout->p_picture[i_picture], "vout: ..." );
        }
    }   

    intf_Msg("vout: --- end ---\n");
}
#endif

/* following functions are local */

/*******************************************************************************
 * CheckConfiguration: check vout_CreateThread() configuration
 *******************************************************************************
 * Set default parameters where required. In DEBUG mode, check if configuration
 * is valid.
 *******************************************************************************/
static int CheckConfiguration( video_cfg_t *p_cfg )
{
    /* Heap size */
    if( !( p_cfg->i_properties & VIDEO_CFG_SIZE ) )
    {
        p_cfg->i_size = VOUT_HEAP_SIZE;
    }

    return( 0 );
}

/*******************************************************************************
 * InitThread: initialize video output thread
 *******************************************************************************
 * This function is called from RunThread and performs the second step of the
 * initialization. It returns 0 on success. Note that the thread's flag are not
 * modified inside this function.
 *******************************************************************************/
static int InitThread( vout_thread_t *p_vout )
{
    int     i_index;                                          /* generic index */    

    /* Update status */
    *p_vout->pi_status = THREAD_START;    
    
    /* Allocate video heap */
    p_vout->p_picture = 
        (picture_t *) malloc( sizeof(picture_t) * p_vout->i_max_pictures );
    if( !p_vout->p_picture )                                          /* error */
    {        
        intf_ErrMsg("vout error: %s\n", strerror(ENOMEM));
        intf_DbgMsg("%p -> failed\n", p_vout);
        *p_vout->pi_status = THREAD_ERROR;        
        return( 1 );
    }

    /* Initialize pictures */    
    for( i_index = 0; i_index < p_vout->i_max_pictures; i_index++)
    {
        p_vout->p_picture[i_index].i_type = EMPTY_PICTURE;
        p_vout->p_picture[i_index].i_flags = 0;
    }

    /* Initialize video streams - note that stream 0 is always active */
    p_vout->p_stream[0].i_status = VOUT_ACTIVE_STREAM; 
#ifdef STATS
    p_vout->p_stream[0].c_pictures = 0;
    p_vout->p_stream[0].c_rendered_pictures = 0;
#endif
    for( i_index = 1; i_index < VOUT_MAX_STREAMS; i_index++ )
    {
        p_vout->p_stream[i_index].i_status = VOUT_INACTIVE_STREAM;
    }
 
    /* Initialize other properties */
    p_vout->i_pictures = 0;
#ifdef STATS
    p_vout->c_loops = 0;
    p_vout->c_idle_loops = 0;
    p_vout->c_pictures = 0;
    p_vout->c_rendered_pictures = 0;
#endif

    /* Initialize output method - width, height, screen depth and bytes per 
     * pixel are initialized by this call. */
    if( vout_X11CreateOutputMethod( p_vout ) )                        /* error */
    {
        free( p_vout->p_picture );
        *p_vout->pi_status = THREAD_ERROR;        
        return( 1 );
    } 

    /* Mark thread as running and return */
    *p_vout->pi_status = THREAD_READY;    
    intf_DbgMsg("%p -> succeeded\n", p_vout);    
    return(0);    
}

/*******************************************************************************
 * RunThread: video output thread
 *******************************************************************************
 * Video output thread. This function does only returns when the thread is
 * terminated. It handles the pictures arriving in the video heap and the
 * display device events.
 *******************************************************************************/
static void RunThread( vout_thread_t *p_vout)
{
    int             i_stream;                                  /* stream index */
    int             i_picture;                                /* picture index */
    int             i_active_streams;              /* number of active streams */
    int             i_err;                                       /* error code */
    mtime_t         display_date;                              /* display date */    
    mtime_t         current_date;                              /* current date */
    picture_t *     pp_sorted_picture[VOUT_MAX_PICTURES];   /* sorted pictures */
#ifdef VOUT_DEBUG
    char            sz_date[MSTRTIME_MAX_SIZE];                 /* date buffer */
#endif

    /* 
     * Initialize thread and free configuration 
     */
    p_vout->b_error = InitThread( p_vout );
    if( p_vout->b_error )
    {
        free( p_vout );                                  /* destroy descriptor */
        return;        
    }    

    /*
     * Main loop - it is not executed if an error occured during
     * initialization
     */
    while( (!p_vout->b_die) && (!p_vout->b_error) )
    {
        /* Get locks on pictures and streams */
        vlc_mutex_lock( &p_vout->streams_lock );
        vlc_mutex_lock( &p_vout->pictures_lock );
        
        /* Initialise streams: clear images from all streams */
        for( i_stream = 0; i_stream < VOUT_MAX_STREAMS; i_stream++ )
        {
            p_vout->p_stream[i_stream].p_next_picture = NULL;
        }

        /* First pass: a set of pictures is selected - all unselected pictures
         * which should be removed are removed, and a display date is computed.
         * If a stream has no next_picture after this step, then the heap does
         * not include any picture from that stream. */
        current_date = mdate();
        display_date = FirstPass( p_vout, current_date );

        /* Stream management: streams which are in ENDING or DESTROYED state and
         * which have no more pictures are destroyed */
        for( i_active_streams = i_stream = 1; 
             i_stream < VOUT_MAX_STREAMS; 
             i_stream++ )
        {    
            switch( p_vout->p_stream[i_stream].i_status )
            {
            case VOUT_ACTIVE_STREAM:                          /* active stream */
                i_active_streams++;
                break;

            case VOUT_ENDING_STREAM:            /* ending or destroyed streams */
            case VOUT_DESTROYED_STREAM:
                /* Those streams can be destroyed if there are no more picture
                 * remaining. This should always be the case for destroyed
                 * streams, except if it includes permanent pictures */
                if( p_vout->p_stream[i_stream].p_next_picture == NULL )
                {
                    p_vout->p_stream[i_stream].i_status = VOUT_INACTIVE_STREAM;
#ifdef STATS                   
                    intf_DbgMsg("%p -> stream %d destroyed %d pictures, %d rendered pictures\n", 
                                p_vout, i_stream, p_vout->p_stream[i_stream].c_pictures, 
                                p_vout->p_stream[i_stream].c_rendered_pictures );
#else
                    intf_DbgMsg("%p -> stream %d destroyed\n", p_vout, i_stream );
#endif
                }
                /* If the stream can't be destroyed, it means it is still 
                 * active - in that case, the next image pointer needs to be
                 * cleard */
                else
                {
                    i_active_streams++;
                    p_vout->p_stream[i_stream].p_next_picture = NULL;
                }
                break;
            }
        }
      
        /* From now until next loop, only next_picture field in streams
         * will be used, and selected pictures structures won't modified.
         * Therefore, locks can be released */
        vlc_mutex_unlock( &p_vout->pictures_lock );
        vlc_mutex_unlock( &p_vout->streams_lock );

        /* If there are some pictures to display, then continue */
        if( display_date != LAST_MDATE )
        {
            /* Increase display_date if it is too low */
            if( display_date < current_date + VOUT_DISPLAY_DELAY )
            {
                intf_Msg("vout: late picture(s) detected\n");
                display_date = current_date + VOUT_DISPLAY_DELAY;
            }
#ifdef VOUT_DEBUG
            intf_DbgMsg("%p -> display_date is %s\n", p_vout, 
                        mstrtime( sz_date, display_date ));
#endif

            /* Second pass: a maximum of one picture per stream is selected
             * (except for stream 0), and some previously selected pictures may
             * be removed */
            SecondPass( p_vout, display_date );
#ifdef VOUT_DEBUG
            vout_PrintHeap( p_vout, "ready for rendering" );
#endif
 
            /* Rendering: sort pictures and render them */
            SortPictures( p_vout, pp_sorted_picture );
            for( i_picture = 0; pp_sorted_picture[i_picture] != NULL; i_picture++ )
            {
                /* Render picture */
                RenderPicture( p_vout, pp_sorted_picture[i_picture] );             
            }
            
            /* Handle output method events - this function can return a negative
             * value, which means a fatal error and the end of the display loop 
             * (i.e. the X11 window has been destroyed), a positive one, meaning
             * a modification occured in the pictures and they do have need to 
             * be displayed, or 0 */
            i_err = vout_X11ManageOutputMethod( p_vout );
            if( !i_err )
            {
                /* Sleep and display */
                mwait( display_date );
                vout_X11DisplayOutput( p_vout );
            }
            else if( i_err < 0 )
            {                
                /* An error occured, and the thread must terminate immediately,
                 * without displaying anything - setting b_error to 1 cause the
                 * immediate end of the main while() loop. */
                p_vout->b_error = 1;
            }
        }        
        /* If there is nothing to display, just handle events and sleep a while, 
         * hopping there will be something to do later */
        else    
        {
            if( vout_X11ManageOutputMethod( p_vout ) < 0)
            {
                p_vout->b_error = 1;
                
            }
            else
            {   
                msleep( VOUT_IDLE_SLEEP );
            }            
#ifdef STATS
            /* Update counters */
            p_vout->c_idle_loops++;
#endif
        }        

#ifdef STATS
        /* Update counters */
        p_vout->c_loops++;
#endif
    } 

    /*
     * Error loop
     */
    if( p_vout->b_error )
    {
        ErrorThread( p_vout );        
    }

    /* End of thread */
    EndThread( p_vout );
}

/*******************************************************************************
 * ErrorThread: RunThread() error loop
 *******************************************************************************
 * This function is called when an error occured during thread main's loop. The
 * thread can still receive feed, but must be ready to terminate as soon as
 * possible.
 *******************************************************************************/
static void ErrorThread( vout_thread_t *p_vout )
{
    int i_picture;                                            /* picture index */    
    
    /* Wait until a `die' order */
    while( !p_vout->b_die )
    {
        /* Get lock on pictures */
        vlc_mutex_lock( &p_vout->pictures_lock );     

        /* Try to remove all pictures - only removable pictures will be
         * removed */
        for( i_picture = 0; i_picture < p_vout->i_max_pictures; i_picture++ )
        {   
            if( p_vout->p_picture[i_picture].i_type != EMPTY_PICTURE )
            {
                EmptyPicture( p_vout, &p_vout->p_picture[i_picture] );
            }
        }
        
        /* Release locks on pictures */
        vlc_mutex_unlock( &p_vout->pictures_lock );

        /* Sleep a while */
        msleep( VOUT_IDLE_SLEEP );                
    }
}

/*******************************************************************************
 * EndThread: thread destruction
 *******************************************************************************
 * This function is called when the thread ends after a sucessfull 
 * initialization.
 *******************************************************************************/
static void EndThread( vout_thread_t *p_vout )
{
    int *   pi_status;                                        /* thread status */
#ifdef DEBUG
    int     i_stream;                                    /* video stream index */
#endif
        
    /* Store status */
    pi_status = p_vout->pi_status;    
    *pi_status = THREAD_END;    
    
#ifdef DEBUG
    /* Check for remaining pictures or video streams */
    if( p_vout->i_pictures )
    {
        intf_DbgMsg("%p -> remaining pictures\n", p_vout);        
    }   
    for( i_stream = 1; 
         (i_stream < VOUT_MAX_STREAMS) && (p_vout->p_stream[i_stream].i_status == VOUT_INACTIVE_STREAM);
         i_stream++ )
    {
        ;        
    }    
    if( i_stream != VOUT_MAX_STREAMS )
    {
        intf_DbgMsg("%p -> remaining video streams\n", p_vout);        
    }    
#endif

    /* Destroy thread structures allocated by InitThread */
    vout_X11DestroyOutputMethod( p_vout );            /* destroy output method */
    free( p_vout->p_picture );                                    /* free heap */
    free( p_vout );

    /* Update status */
    *pi_status = THREAD_OVER;    
    intf_DbgMsg("%p\n", p_vout);
}

/*******************************************************************************
 * FindPicture: find an empty picture in a video heap
 *******************************************************************************
 * This function is used by most of the vout_*Picture*() functions. It locks the
 * video heap, look for an empty picture in it and return a pointer to this
 * picture, or NULL if the heap is full.
 * Not that the heap is not unlocked when this function returns, and it needs 
 * to be donne by the calling function.
 *******************************************************************************/
static picture_t *FindPicture( vout_thread_t *p_vout )
{
    int         i_picture;                                    /* picture index */

    /* Get lock */
    vlc_mutex_lock( &p_vout->pictures_lock );

    /* Look for an empty place */
    for( i_picture = 0; i_picture < p_vout->i_max_pictures; i_picture++ )
    {
        /* An empty place has been found: return */
        if( p_vout->p_picture[i_picture].i_type == EMPTY_PICTURE )
        {
            return( &p_vout->p_picture[i_picture] );
        }
    }

    /* Nothing has been found */
    return( NULL );    
}

/*******************************************************************************
 * EmptyPicture: empty a picture without destroying its descriptor
 *******************************************************************************
 * When a picture is no more used in video heap or must be destroyed, this
 * function is called to destroy associated data and set picture type to empty.
 * Only non permanent and unlinked pictures can be destroyed. 
 * Only non-empty pictures should be sent to this function.
 * All picture flags are cleared, and the heap size is decreased.
 *******************************************************************************
 * Messages type: vout, major code 15
 *******************************************************************************/
static void EmptyPicture( vout_thread_t *p_vout, picture_t *p_pic )
{    
#ifdef DEBUG
    /* Check if picture is non-empty */
    if( p_pic->i_type == EMPTY_PICTURE )
    {
        intf_DbgMsg("%p -> trying to remove empty picture %p\n",
                    p_vout, p_pic);
        return;
    }
#endif

    /* Mark as ready for destruction */
    p_pic->i_flags |= DESTROY_PICTURE;
    p_pic->i_flags &= ~DISPLAY_PICTURE;

    /* If picture is no more referenced and is not permanent, destroy */
    if( (p_pic->i_refcount == 0) && !(p_pic->i_flags & PERMANENT_PICTURE) )
    {
        /* If picture owns its data, free them */
        if( p_pic->i_flags & OWNER_PICTURE )
        {
            free( p_pic->p_data );
        }           
        p_pic->i_type = EMPTY_PICTURE;
        p_pic->i_flags = 0;  
        p_vout->i_pictures--;
        intf_DbgMsg("%p -> picture %p removed\n", p_vout, p_pic);
    }
}

/*******************************************************************************
 * FirstPass: first pass of the vout thread on pictures
 *******************************************************************************
 * A set of pictures is selected according to the current date and their display
 * date. Some unselected and already displayed pictures are removed, and the
 * next display date is computed. 
 * The next_picture fields of all the streams are updated. Note that streams
 * are initialized before this function is called.
 *******************************************************************************
 * Messages type: vout, major code 18
 *******************************************************************************/
static mtime_t FirstPass( vout_thread_t *p_vout, mtime_t current_date )
{
    mtime_t         display_date;                              /* display date */
    int             i_picture;                                /* picture index */
    picture_t *     p_picture;                   /* picture pointer (shortcut) */
 
    /* Set up date */
    display_date = LAST_MDATE;
        
    /* Browse all pictures */
    for( i_picture = 0; i_picture < p_vout->i_max_pictures; i_picture++ )
    {
        p_picture = &p_vout->p_picture[i_picture];

        /* Empty pictures are never selected */
        if( p_picture->i_type == EMPTY_PICTURE )
        {
            ;
        }               
        /* Destroyed pictures are never selected, they can be emptied if needed */
        else if( p_picture->i_flags & DESTROY_PICTURE )
        {
            EmptyPicture( p_vout, p_picture );
        }  
#ifdef DEBUG
        /* Pictures in an inactive stream are submitted to a deletion attempt,
         * and nether selected - this case should never happen */
        else if( p_vout->p_stream[ p_picture->i_stream ].i_status == VOUT_INACTIVE_STREAM )
        {   
            intf_DbgMsg("%p -> picture %p belongs to an inactive stream\n", 
                        p_vout, p_picture);
            EmptyPicture( p_vout, p_picture );
        }  
#endif
        /* Pictures in a destroyed stream are submitted to a deletion attempt,
         * and nether selected */
        else if( p_vout->p_stream[ p_picture->i_stream ].i_status == VOUT_DESTROYED_STREAM )
        {   
            EmptyPicture( p_vout, p_picture );
        }
        /* Reserved pictures are never selected */
        else if( p_picture->i_flags & RESERVED_PICTURE )
        {
            p_picture->i_flags &= ~DISPLAY_PICTURE;
        }
        /* Overlay pictures */
        else if( p_picture->i_flags & OVERLAY_PICTURE )
        {
            /* A picture is outdated if it is not permanent and if it's maximal
             * date is passed */
            if( !(p_picture->i_flags & PERMANENT_PICTURE ) 
                && (p_picture->date + p_picture->duration < current_date) )
            {
                p_picture->i_flags &= ~DISPLAY_PICTURE;      
                EmptyPicture( p_vout, p_picture );
            }
            /* Else if picture is in stream 0, it will always be selected */
            else if( p_picture->i_stream == 0 )
            {
                p_picture->i_flags |= DISPLAY_PICTURE;   
                /* Update display_date if picture has never been displayed */
                if( !(p_picture->i_flags & DISPLAYED_PICTURE) && (p_picture->date < display_date) )
                {
                    display_date = p_picture->date;
                }    
            }
            /* The picture can be considered as a regular picture, because it
             * has never been displayed */
            else if( !(p_picture->i_flags & DISPLAYED_PICTURE) )
            {                
                /* Select picture if can be updated */
                if( (p_vout->p_stream[ p_picture->i_stream].p_next_picture == NULL)
                    || (p_vout->p_stream[p_picture->i_stream].p_next_picture->date > p_picture->date))
                {
                    p_picture->i_flags |= DISPLAY_PICTURE;   
                    p_vout->p_stream[p_picture->i_stream].p_next_picture = p_picture;
                    /* Update display_date */
                    if( p_picture->date < display_date )
                    {
                        display_date = p_picture->date;
                    }
                }
            }
            /* In other cases (overlay pictures which have already been displayed), 
             * the picture is always selected */
            else
            {
                p_picture->i_flags |= DISPLAY_PICTURE;
                /* The stream is only updated if there is no picture in that stream */
                if( p_vout->p_stream[ p_picture->i_stream].p_next_picture == NULL )
                {
                    p_vout->p_stream[p_picture->i_stream].p_next_picture = p_picture;
                }
            }
        }
        /* Non overlay pictures are deleted if they have been displayed, and
         * selected if there date is valid */
        else
        {
            /* Remove already displayed pictures */
            if( p_picture->i_flags & DISPLAYED_PICTURE )
            {
                EmptyPicture( p_vout, p_picture );
            }
            /* Remove passed pictures, The picture is marked as displayed in case it 
             * could not be removed */
            else if( p_picture->date < current_date )
            {
                intf_DbgMsg("%p -> picture %p detected late\n", 
                            p_vout, p_picture );
                p_picture->i_flags |= DISPLAYED_PICTURE;
                EmptyPicture( p_vout, p_picture );
            }
            /* Update streams for other pictures */
            else
            {
                p_picture->i_flags |= DISPLAY_PICTURE;
                /* Update 'next picture' field in stream descriptor */
                if( (p_vout->p_stream[ p_picture->i_stream].p_next_picture == NULL)
                    || (p_vout->p_stream[p_picture->i_stream].p_next_picture->date > p_picture->date))
                {
                   p_vout->p_stream[p_picture->i_stream].p_next_picture = p_picture;
                   /* Update display_date */
                   if( p_picture->date < display_date )
                   {
                       display_date = p_picture->date;
                   }
                }
            }
        }
    }

    return( display_date );
}

/*******************************************************************************
 * SecondPass: second pass of the vout thread on pictures
 *******************************************************************************
 * Select only one picture per stream other than stream 0, and remove pictures
 * which should have been displayed.... but arrived too late. Note that nothing
 * is locked when this function is called, and therefore pictures should not
 * be removed here.
 * Only previously selected pictures are processed.
 *******************************************************************************
 * Messages type: vout, major code 19
 *******************************************************************************/
static void SecondPass( vout_thread_t *p_vout, mtime_t display_date )
{
    int         i_picture;                                    /* picture index */
    picture_t * p_picture;                       /* picture pointer (shortcut) */

    for( i_picture = 0; i_picture < p_vout->i_max_pictures; i_picture++ )
    {
        p_picture = &p_vout->p_picture[i_picture];
        
        /* Process only previously selected pictures */
        if( p_picture->i_flags & DISPLAY_PICTURE )
        {
            /* Deselect picture if it is too youg */
            if( p_picture->date > display_date + VOUT_DISPLAY_TOLERANCE )
            {
                p_picture->i_flags &= ~DISPLAY_PICTURE;
            }
            /* Pictures in stream 0 are selected depending only of their date */
            else if( p_picture->i_stream == 0 )
            {
                /* Overlay pictures are not selected if they are 
                 * outdated */
                if( p_picture->i_flags & OVERLAY_PICTURE )
                {
                    if( !(p_picture->i_flags & PERMANENT_PICTURE)
                    && (display_date > p_picture->date + p_picture->duration) )
                    {
                        p_picture->i_flags |= DESTROY_PICTURE;
                        p_picture->i_flags &= ~DISPLAY_PICTURE;
                    }
                }
                /* Regular pictures are selected if they are not too late */
                else if( p_picture->date < display_date - VOUT_DISPLAY_TOLERANCE )
                {
                    intf_DbgMsg("%p -> picture %p detected late\n",
                                p_vout, p_picture );
                    p_picture->i_flags |= DISPLAYED_PICTURE | DESTROY_PICTURE;
                    p_picture->i_flags &= ~DISPLAY_PICTURE;
                }
            }
            /* Overlay pictures which have been displayed have special 
             * processing */
            else if( (p_picture->i_flags & OVERLAY_PICTURE) 
                     && (p_picture->i_flags & DISPLAYED_PICTURE) )
            {
                /* If the stream is not empty, or the picture has been 
                 * outdated, de select */
                if( (p_vout->p_stream[p_picture->i_stream].p_next_picture != NULL)
                    || (!(p_picture->i_flags & PERMANENT_PICTURE)
                        && (display_date > p_picture->date + p_picture->duration) ) )
                {
                    p_picture->i_flags |= DESTROY_PICTURE;
                    p_picture->i_flags &= ~DISPLAY_PICTURE;
                }
            }
            /* Other pictures are 'regular' pictures */
            else 
            {
                /* If the stream is empty, always select */
                if( p_vout->p_stream[p_picture->i_stream].p_next_picture == NULL)
                {
                    p_vout->p_stream[p_picture->i_stream].p_next_picture = p_picture;
                }
                /* Else, remove and mark as displayed (skip) if the picture is too old */
                else if( p_picture->date < display_date - VOUT_DISPLAY_TOLERANCE )
                {
                    intf_DbgMsg("%p -> picture %p detected late\n",
                                p_vout, p_picture );
                    p_picture->i_flags |= DISPLAYED_PICTURE | DESTROY_PICTURE;
                    p_picture->i_flags &= ~DISPLAY_PICTURE;
                }
                /* Else, select if the picture is younger than the current selected one */
                else if( p_picture->date 
                          < p_vout->p_stream[p_picture->i_stream].p_next_picture->date )
                {
                    /* Deselect the previous picture */
                    p_vout->p_stream[p_picture->i_stream].p_next_picture->i_flags 
                        &= ~DISPLAY_PICTURE; 
                    /* Select the current one */
                    p_vout->p_stream[p_picture->i_stream].p_next_picture = p_picture;     
                }
                /* Else, select if the current selected one is an already displayed overlay */
                else if( (p_vout->p_stream[p_picture->i_stream].p_next_picture->i_flags 
                          & ( OVERLAY_PICTURE | DISPLAYED_PICTURE )) 
                         == (OVERLAY_PICTURE | DISPLAYED_PICTURE) ) 

                {
                    /* Deselect and remove the previous picture */
                    p_picture->i_flags |= DESTROY_PICTURE;
                    p_picture->i_flags &= ~DISPLAY_PICTURE;
                    /* Select the current one */
                    p_vout->p_stream[p_picture->i_stream].p_next_picture = p_picture;     
                }
                /* Else, deselect the picture */
                else
                {
                    p_picture->i_flags &= ~DISPLAY_PICTURE;
                }
            }
        }
    }
}

/*******************************************************************************
 * SortPictures: sort pictures ready for display by hierarchical level
 *******************************************************************************
 * Build an array of pictures in rendering order, starting from minimal to
 * level to maximal level. A NULL pointer always ends the array, which size is
 * limited to VOUT_MAX_PICTURES.
 *******************************************************************************
 * Messages type: vout, major code 26
 *******************************************************************************/
static void SortPictures( vout_thread_t *p_vout, picture_t **pp_picture )
{
    int             i_picture;                /* picture index in sorted array */
    int             i_heap_picture;                   /* picture index in heap */
    picture_t *     p_previous_picture;                /* first shift register */
    picture_t *     p_current_picture;                /* second shift register */

    /* Initialize array */
    pp_picture[0] = NULL;

    /* Copy and sort pictures */
    for( i_heap_picture = 0; i_heap_picture < p_vout->i_max_pictures ; i_heap_picture++ )
    {
        /* Sort only selected pictures */
        if( p_vout->p_picture[ i_heap_picture ].i_flags & DISPLAY_PICTURE )
        {            
            /* Find picture position */
            for( i_picture = 0; (pp_picture[i_picture] != NULL) 
                     && (pp_picture[i_picture]->i_level 
                         <= p_vout->p_picture[ i_heap_picture ].i_level);
                 i_picture++ )
            {
                ;
            }
            p_current_picture = &p_vout->p_picture[ i_heap_picture ];

            /* Insert picture and shift end of array */
            for( ; p_previous_picture != NULL; i_picture++ )
            {
                p_previous_picture = p_current_picture;
                p_current_picture = pp_picture[i_picture];
                if( i_picture == VOUT_MAX_PICTURES - 1 )
                {
                    p_previous_picture = NULL;
                }
                pp_picture[i_picture] = p_previous_picture; 
            }
        }
    }

}

/*******************************************************************************
 * RenderPicture: render a picture
 *******************************************************************************
 * This function convert a picture from a video heap to a pixel-encoded image
 * and copy it to the current rendering buffer. No lock is required, since the
 * rendered picture has been determined as existant, and will only be destroyed
 * by the vout_Thread() later.
 *******************************************************************************
 * Messages types: vout, major code 27
 *******************************************************************************/
static void RenderPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    int     i_x, i_y;                                   /* position in display */
    int     i_pic_x;                            /* x offset in clipped picture */
    int     i_pic_y;                            /* y offset in clipped picture */
    int     i_height, i_width;                   /* clipped picture dimensions */
    int     i_line_width;                                        /* line width */
    void    (*RenderLine)(vout_thread_t *p_vout, picture_t *p_pic, /* renderer */
                          int i_x, int i_y, int i_pic_x, int i_pic_y, int i_width,
                          int i_line_width, int i_ratio );

    /* Update counters - this is done now since for blank pictures, the function 
     * may never reach it's regular end */
    p_pic->i_flags |= DISPLAYED_PICTURE;
#ifdef STATS
    p_vout->c_rendered_pictures++;
    p_vout->p_stream[ p_pic->i_stream ].c_rendered_pictures++;
#endif                
    
    /* Computes position and size of the picture zone to render */
    i_x = p_pic->i_x;
    i_y = p_pic->i_y;
    i_width = p_pic->i_width;
    i_height = p_pic->i_height;
    ClipPicture( &i_x, p_vout->i_width, &i_pic_x, &i_width, p_pic->i_h_ratio, p_pic->i_h_align );
    ClipPicture( &i_y, p_vout->i_height, &i_pic_y, &i_height, p_pic->i_v_ratio, p_pic->i_v_align );
#ifdef VOUT_DEBUG
   intf_DbgMsg("%p -> picture %p, (%d-%d:%d-%d, %dx%d)\n", 
               p_vout, p_pic, i_x, i_pic_x, i_y, i_pic_y, i_width, i_height );

#endif

    /* If there is noting to display, returns immediately */
    if( (i_pic_x >= i_width) || (i_pic_y >= i_height) )
    {
        return;
    }
    
    /* Determine method used to render line. This function is choosed here to
     * avoid multiple conditions tests later */
    switch( p_pic->i_type )
    {
    case RGB_BLANK_PICTURE:         /* picture is blank, RGB encoded (no data) */
    case PIXEL_BLANK_PICTURE:     /* picture is blank, pixel encoded (no data) */
        /* Blank pictures are displayed immediately - dimensions are real ones,
         * and should be recalculated */
        switch( p_pic->i_h_ratio )                   
        {
        case DISPLAY_RATIO_HALF:                              /* 1:2 half size */
            i_width = (i_width - i_pic_x) / 2;
            break;     
        case DISPLAY_RATIO_NORMAL:                          /* 1:1 normal size */
            i_width -= i_pic_x;
            break;
        case DISPLAY_RATIO_DOUBLE:                          /* 2:1 double size */   
            i_width = (i_width - i_pic_x) * 2;
            break;
            /* ?? add others display ratio */
        }
        switch( p_pic->i_v_ratio )                   
        {
        case DISPLAY_RATIO_HALF:                              /* 1:2 half size */
            i_height = (i_height - i_pic_y) / 2;
            break;     
        case DISPLAY_RATIO_NORMAL:                          /* 1:1 normal size */
            i_height -= i_pic_y;
            break;
        case DISPLAY_RATIO_DOUBLE:                          /* 2:1 double size */   
            i_height = (i_height - i_pic_y) * 2;
            break;
            /* ?? add others display ratio */
        }
        if( p_pic->i_type == RGB_BLANK_PICTURE )          /* RGB blank picture */
        {            
            p_vout->RenderRGBBlank( p_vout, p_pic->pixel, i_x, i_y, i_width, i_height );
        }
        else                                            /* pixel blank picture */
        {
            p_vout->RenderPixelBlank( p_vout, p_pic->pixel, i_x, i_y, i_width, i_height );
        }        
        return;
        break;
    case RGB_PICTURE:                        /* picture is 24 bits rgb encoded */
        RenderLine = p_vout->RenderRGBLine;
        break;
    case PIXEL_PICTURE:                            /* picture is pixel encoded */
        RenderLine = p_vout->RenderPixelLine;
        break;
    case RGB_MASK_PICTURE:                      /* picture is a 1 rgb bpp mask */
        RenderLine = p_vout->RenderRGBMaskLine;
        break;
    case PIXEL_MASK_PICTURE:                  /* picture is a 1 pixel bpp mask */
        RenderLine = p_vout->RenderPixelMaskLine;
        break;
        /* ?? add YUV types */
#ifdef DEBUG
    default:                      /* internal error, which should never happen */
        intf_DbgMsg("%p -> unknown type for picture %p\n", p_vout, p_pic);
        break;  
#endif  
    }

    /* For non blank pictures, loop on lines */
    for( ; i_pic_y < i_height; i_pic_y++ )
    {
        /* First step: check if line has to be rendered. This is not obvious since
         * if display ratio is less than 1:1, some of the lines don't need to
         * be displayed, and therefore do not need to be rendered. */
        switch( p_pic->i_v_ratio )
        {
        case DISPLAY_RATIO_HALF:                              /* 1:2 half size */
            /* Only even lines are rendered, and copied once */
            i_line_width = i_pic_y % 2;
            break;

        case DISPLAY_RATIO_NORMAL:                          /* 1:1 normal size */
            /* All lines are rendered and copied once */
            i_line_width = 1;
            break;

        case DISPLAY_RATIO_DOUBLE:                          /* 2:1 double size */
            /* All lines are rendered and copied twice */
            i_line_width = 2;
            break;
        }

        if( i_line_width )
        {
            /* Computed line width can be reduced if it would cause to render 
             * outside the display */
            if( i_y + i_line_width > p_vout->i_height )
            {
                i_line_width = p_vout->i_height - i_y;
            }

            /* Second step: render line. Since direct access to the rendering 
             * buffer is required, functions in charge of rendering the line
             * are part of the display driver. Because this step is critical
             * and require high optimization, different methods are used 
             * depending of the horizontal display ratio, the image type and 
             * the screen depth. */
            RenderLine( p_vout, p_pic, i_x, i_y, i_pic_x, i_pic_y,
                        i_width, i_line_width, p_pic->i_h_ratio );
            
            /* Increment display line index */
            i_y += i_line_width;
        }
    }
}

/*******************************************************************************
 * ClipPicture: clip a picture in display window
 *******************************************************************************
 * This function computes picture placement in display window and rendering
 * zone, according to wished picture placement and display ratio. It must be
 * called twice, once and with the x coordinates and once with the y
 * coordinates. 
 * The pi_pic_ofs parameter is only written, but pi_ofs and pi_pic_size must
 * be valid arguments. Note that *pi_pic_size is still the rendering zone size,
 * starting from picture offset 0 and not the size starting from *pi_pic_ofs
 * (same thing for i_size).
 *******************************************************************************
 * Messages types: vout, major code 28
 *******************************************************************************/
static void ClipPicture( int *pi_ofs, int i_size, int *pi_pic_ofs, int *pi_pic_size,
                         int i_ratio, int i_placement )
{
    int i_ofs;                                   /* temporary picture position */

    /* Computes base picture position */
    switch( i_placement )
    {
    case -1:                                               /* left/top aligned */
        i_ofs = *pi_ofs;
        break;
    case 0:                                                        /* centered */
        i_ofs = *pi_ofs + (i_size - *pi_pic_size) / 2;
        break;
    case 1:                                            /* right/bottom aligned */
        i_ofs = *pi_ofs + i_size - *pi_pic_size;
        break;
#ifdef DEBUG
    default:                      /* internal error, which should never happen */
        intf_DbgMsg("invalid placement\n");
        break;
#endif
    }

    /* Computes base rendering position and update picture position - i_ofs is
     * the picture position, and can be negative */
    if( i_ofs < 0 )                       /* picture starts outside the screen */
    {
        switch( i_ratio )                   
        {
        case DISPLAY_RATIO_HALF:                              /* 1:2 half size */
            *pi_pic_ofs = - *pi_ofs * 2;
            break;     
        case DISPLAY_RATIO_NORMAL:                          /* 1:1 normal size */
            *pi_pic_ofs = -*pi_ofs;
            break;
        case DISPLAY_RATIO_DOUBLE:                          /* 2:1 double size */
            *pi_pic_ofs = -CEIL(*pi_ofs, 2);
            break;
#ifdef DEBUG
        default:                  /* internal error, which should never happen */
            intf_DbgMsg("unsupported ratio\n");
            break;  
#endif  
        }
        *pi_ofs = 0;
    }
    else                                   /* picture starts inside the screen */
    {
        *pi_pic_ofs = 0;
        *pi_ofs = i_ofs;
    }

    /* Computes rendering size - i_ofs is the picture position, and can be 
     * negative, *pi_ofs is the real picture position, and is always positive */
    switch( i_ratio )                   
    {
    case DISPLAY_RATIO_HALF:                                  /* 1:2 half size */
        if( i_ofs + CEIL(*pi_pic_size, 2) > i_size )
        {
            *pi_pic_size = ( i_size - i_ofs ) * 2;
        }
        break;     
    case DISPLAY_RATIO_NORMAL:                              /* 1:1 normal size */
        if( i_ofs + *pi_pic_size > i_size )
        {
            *pi_pic_size = i_size - i_ofs;
        }
        break;
    case DISPLAY_RATIO_DOUBLE:                              /* 2:1 double size */
        if( *pi_ofs + *pi_pic_size * 2 > i_size )
        {
            *pi_pic_size = ( i_size - i_ofs ) / 2;
        }
        break;
#ifdef DEBUG
    default:                  /* internal error, which should never happen */
        intf_DbgMsg("unsupported ratio\n");
        break;  
#endif  
    }     
}

