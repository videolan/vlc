/*******************************************************************************
 * video_output.c : video output thread
 * (c)2000 VideoLAN
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

#ifdef VIDEO_X11
#include <X11/Xlib.h>                           /* for video_sys.h in X11 mode */
#endif

#include "common.h"
#include "config.h"
#include "mtime.h"
#include "vlc_thread.h"
#include "video.h"
#include "video_output.h"
#include "video_sys.h"
#include "video_yuv.h"
#include "intf_msg.h"
#include "main.h"

/*******************************************************************************
 * Local prototypes
 *******************************************************************************/
static int      InitThread              ( vout_thread_t *p_vout );
static void     RunThread               ( vout_thread_t *p_vout );
static void     ErrorThread             ( vout_thread_t *p_vout );
static void     EndThread               ( vout_thread_t *p_vout );
static void     RenderPicture           ( vout_thread_t *p_vout, picture_t *p_pic );
static void     RenderPictureInfo       ( vout_thread_t *p_vout, picture_t *p_pic );
static int      RenderIdle              ( vout_thread_t *p_vout, int i_level );

/*******************************************************************************
 * vout_CreateThread: creates a new video output thread
 *******************************************************************************
 * This function creates a new video output thread, and returns a pointer
 * to its description. On error, it returns NULL.
 * If pi_status is NULL, then the function will block until the thread is ready.
 * If not, it will be updated using one of the THREAD_* constants.
 *******************************************************************************/
vout_thread_t * vout_CreateThread               ( 
#ifdef VIDEO_X11
                                                  char *psz_display, Window root_window, 
#endif
                                                  int i_width, int i_height, int *pi_status 
                                                )
{
    vout_thread_t * p_vout;                               /* thread descriptor */
    int             i_status;                                 /* thread status */

    /* Allocate descriptor */
    p_vout = (vout_thread_t *) malloc( sizeof(vout_thread_t) );
    if( p_vout == NULL )
    {
        intf_ErrMsg("error: %s\n", strerror(ENOMEM));        
        return( NULL );
    }

    /* Initialize thread properties */
    p_vout->b_die               = 0;
    p_vout->b_error             = 0;    
    p_vout->b_active            = 0;
    p_vout->pi_status           = (pi_status != NULL) ? pi_status : &i_status;
    *p_vout->pi_status          = THREAD_CREATE;    

    /* Initialize some fields used by the system-dependant method - these fields will
     * probably be modified by the method, and are only preferences */
#ifdef DEBUG
    p_vout->b_info              = 1;    
#else
    p_vout->b_info              = 0;    
#endif
    p_vout->b_grayscale         = main_GetIntVariable( VOUT_GRAYSCALE_VAR, 
                                                       VOUT_GRAYSCALE_DEFAULT );
    p_vout->i_width             = i_width;
    p_vout->i_height            = i_height;
    p_vout->i_bytes_per_line    = i_width * 2;    
    p_vout->i_screen_depth      = 15;
    p_vout->i_bytes_per_pixel   = 2;
    p_vout->f_x_ratio           = 1;
    p_vout->f_y_ratio           = 1;
    p_vout->f_gamma             = VOUT_GAMMA;    
    intf_DbgMsg("wished configuration: %dx%d,%d (%d bytes/pixel, %d bytes/line), ratio %.2f:%.2f, gray=%d\n",
                p_vout->i_width, p_vout->i_height, p_vout->i_screen_depth,
                p_vout->i_bytes_per_pixel, p_vout->i_bytes_per_line,
                p_vout->f_x_ratio, p_vout->f_y_ratio, p_vout->b_grayscale );
   
    /* Create and initialize system-dependant method - this function issues its
     * own error messages */
    if( vout_SysCreate( p_vout
#if defined(VIDEO_X11)
                        , psz_display, root_window 
#endif
        ) )
    {
      free( p_vout );
      return( NULL );
    }
    intf_DbgMsg("actual configuration: %dx%d,%d (%d bytes/pixel, %d bytes/line), ratio %.2f:%.2f, gray=%d\n",
                p_vout->i_width, p_vout->i_height, p_vout->i_screen_depth,
                p_vout->i_bytes_per_pixel, p_vout->i_bytes_per_line,
                p_vout->f_x_ratio, p_vout->f_y_ratio, p_vout->b_grayscale );

    /* Initialize changement properties */
    p_vout->b_gamma_change      = 0;
    p_vout->i_new_width         = p_vout->i_width;
    p_vout->i_new_height        = p_vout->i_height; 

#ifdef STATS
    /* Initialize statistics fields */
    p_vout->c_loops             = 0;
    p_vout->c_idle_loops        = 0;
    p_vout->c_fps_samples       = 0;
#endif      

    /* Create thread and set locks */
    vlc_mutex_init( &p_vout->lock );
    if( vlc_thread_create( &p_vout->thread_id, "video output", 
			   (void *) RunThread, (void *) p_vout) )
    {
        intf_ErrMsg("error: %s\n", strerror(ENOMEM));
	vout_SysDestroy( p_vout );
        free( p_vout );
        return( NULL );
    }   

    intf_Msg("Video: display initialized (%dx%d, %d bpp)\n", 
             p_vout->i_width, p_vout->i_height, p_vout->i_screen_depth );    

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
            return( NULL );            
        }        
    }
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
    if( pi_status == NULL )
    {
        do
        {
            msleep( THREAD_SLEEP );
        }while( (i_status != THREAD_OVER) && (i_status != THREAD_ERROR) 
                && (i_status != THREAD_FATAL) );   
    }
}

/*******************************************************************************
 * vout_DisplayPicture: display a picture
 *******************************************************************************
 * Remove the reservation flag of a picture, which will cause it to be ready for
 * display. The picture does not need to be locked, since it is ignored by
 * the output thread if is reserved.
 *******************************************************************************/
void  vout_DisplayPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
#ifdef DEBUG_VIDEO
    char        psz_date[MSTRTIME_MAX_SIZE];         /* buffer for date string */
#endif

#ifdef DEBUG
    /* Check if picture status is valid */
    if( p_pic->i_status != RESERVED_PICTURE )
    {
        intf_DbgMsg("error: picture %d has invalid status %d\n", p_pic, p_pic->i_status );       
    }   
#endif

    /* Remove reservation flag */
    p_pic->i_status = READY_PICTURE;

#ifdef DEBUG_VIDEO
    /* Send picture informations */
    intf_DbgMsg("picture %p: type=%d, %dx%d, date=%s\n", p_pic, p_pic->i_type, 
                p_pic->i_width,p_pic->i_height, mstrtime( psz_date, p_pic->date ) );    
#endif
}

/*******************************************************************************
 * vout_CreatePicture: allocate a picture in the video output heap.
 *******************************************************************************
 * This function create a reserved image in the video output heap. 
 * A null pointer is returned if the function fails. This method provides an
 * already allocated zone of memory in the picture data fields. It needs locking
 * since several pictures can be created by several producers threads. 
 *******************************************************************************/
picture_t *vout_CreatePicture( vout_thread_t *p_vout, int i_type, 
			       int i_width, int i_height )
{
    int         i_picture;                                    /* picture index */
    int         i_chroma_width = 0;                            /* chroma width */    
    picture_t * p_free_picture = NULL;                   /* first free picture */    
    picture_t * p_destroyed_picture = NULL;         /* first destroyed picture */    

    /* Get lock */
    vlc_mutex_lock( &p_vout->lock );

    /* 
     * Look for an empty place 
     */
    for( i_picture = 0; 
         i_picture < VOUT_MAX_PICTURES; 
         i_picture++ )
    {
	if( p_vout->p_picture[i_picture].i_status == DESTROYED_PICTURE )
	{
	    /* Picture is marked for destruction, but is still allocated - note
             * that if width and type are the same for two pictures, chroma_width 
             * should also be the same */
	    if( (p_vout->p_picture[i_picture].i_type           == i_type)   &&
		(p_vout->p_picture[i_picture].i_height         == i_height) &&
		(p_vout->p_picture[i_picture].i_width          == i_width) )
	    {
		/* Memory size do match : memory will not be reallocated, and function
                 * can end immediately - this is the best possible case, since no
                 * memory allocation needs to be done */
		p_vout->p_picture[i_picture].i_status = RESERVED_PICTURE;
#ifdef DEBUG_VIDEO
                intf_DbgMsg("picture %p (in destroyed picture slot)\n", 
                            &p_vout->p_picture[i_picture] );                
#endif
		vlc_mutex_unlock( &p_vout->lock );
		return( &p_vout->p_picture[i_picture] );
	    }
	    else if( p_destroyed_picture == NULL )
	    {
		/* Memory size do not match, but picture index will be kept in
		 * case no other place are left */
		p_destroyed_picture = &p_vout->p_picture[i_picture];                
	    }	    
	}
        else if( (p_free_picture == NULL) && 
                 (p_vout->p_picture[i_picture].i_status == FREE_PICTURE ))
        {
	    /* Picture is empty and ready for allocation */
            p_free_picture = &p_vout->p_picture[i_picture];            
        }
    }

    /* If no free picture is available, use a destroyed picture */
    if( (p_free_picture == NULL) && (p_destroyed_picture != NULL ) )
    { 
	/* No free picture or matching destroyed picture has been found, but
	 * a destroyed picture is still avalaible */
        free( p_destroyed_picture->p_data );        
        p_free_picture = p_destroyed_picture;        
    }

    /*
     * Prepare picture
     */
    if( p_free_picture != NULL )
    {
        /* Allocate memory */
        switch( i_type )
        {
        case YUV_420_PICTURE:          /* YUV 420: 1,1/4,1/4 samples per pixel */
            i_chroma_width = i_width / 2;            
            p_free_picture->p_data = malloc( i_height * i_chroma_width * 3 * sizeof( yuv_data_t ) );
            p_free_picture->p_y = (yuv_data_t *)p_free_picture->p_data;
            p_free_picture->p_u = (yuv_data_t *)p_free_picture->p_data +i_height*i_chroma_width*4/2;
            p_free_picture->p_v = (yuv_data_t *)p_free_picture->p_data +i_height*i_chroma_width*5/2;
            break;
        case YUV_422_PICTURE:          /* YUV 422: 1,1/2,1/2 samples per pixel */
            i_chroma_width = i_width / 2;            
            p_free_picture->p_data = malloc( i_height * i_chroma_width * 4 * sizeof( yuv_data_t ) );
            p_free_picture->p_y = (yuv_data_t *)p_free_picture->p_data;
            p_free_picture->p_u = (yuv_data_t *)p_free_picture->p_data +i_height*i_chroma_width*2;
            p_free_picture->p_v = (yuv_data_t *)p_free_picture->p_data +i_height*i_chroma_width*3;
            break;
        case YUV_444_PICTURE:              /* YUV 444: 1,1,1 samples per pixel */
            i_chroma_width = i_width;            
            p_free_picture->p_data = malloc( i_height * i_chroma_width * 3 * sizeof( yuv_data_t ) );
            p_free_picture->p_y = (yuv_data_t *)p_free_picture->p_data;
            p_free_picture->p_u = (yuv_data_t *)p_free_picture->p_data +i_height*i_chroma_width;
            p_free_picture->p_v = (yuv_data_t *)p_free_picture->p_data +i_height*i_chroma_width*2;
            break;                
#ifdef DEBUG
        default:
            intf_DbgMsg("error: unknown picture type %d\n", i_type );
            p_free_picture->p_data   =  NULL;            
            break;            
#endif    
        }

        if( p_free_picture->p_data != NULL )
        {        
            /* Copy picture informations, set some default values */
            p_free_picture->i_type                      = i_type;
            p_free_picture->i_status                    = RESERVED_PICTURE;
            p_free_picture->i_matrix_coefficients       = 1; 
            p_free_picture->i_width                     = i_width;
            p_free_picture->i_height                    = i_height;
            p_free_picture->i_chroma_width              = i_chroma_width;            
            p_free_picture->i_display_horizontal_offset = 0;
            p_free_picture->i_display_vertical_offset   = 0;            
            p_free_picture->i_display_width             = i_width;
            p_free_picture->i_display_height            = i_height;
            p_free_picture->i_aspect_ratio              = AR_SQUARE_PICTURE;            
            p_free_picture->i_refcount                  = 0;            
        }
        else
        {
            /* Memory allocation failed : set picture as empty */
            p_free_picture->i_type   =  EMPTY_PICTURE;            
            p_free_picture->i_status =  FREE_PICTURE;            
            p_free_picture =            NULL;            
            intf_ErrMsg("warning: %s\n", strerror( ENOMEM ) );            
        }
        
#ifdef DEBUG_VIDEO
        intf_DbgMsg("picture %p (in free picture slot)\n", p_free_picture );        
#endif
        vlc_mutex_unlock( &p_vout->lock );
        return( p_free_picture );
    }
    
    // No free or destroyed picture could be found
    intf_DbgMsg( "warning: heap is full\n" );
    vlc_mutex_unlock( &p_vout->lock );
    return( NULL );
}

/*******************************************************************************
 * vout_DestroyPicture: remove a permanent or reserved picture from the heap
 *******************************************************************************
 * This function frees a previously reserved picture or a permanent
 * picture. It is meant to be used when the construction of a picture aborted.
 * Note that the picture will be destroyed even if it is linked !
 * This function does not need locking since reserved pictures are ignored by
 * the output thread.
 *******************************************************************************/
void vout_DestroyPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
#ifdef DEBUG
   /* Check if picture status is valid */
   if( p_pic->i_status != RESERVED_PICTURE )
   {
       intf_DbgMsg("error: picture %d has invalid status %d\n", p_pic, p_pic->i_status );       
   }   
#endif

    p_pic->i_status = DESTROYED_PICTURE;

#ifdef DEBUG_VIDEO
    intf_DbgMsg("picture %p\n", p_pic);    
#endif
}

/*******************************************************************************
 * vout_LinkPicture: increment reference counter of a picture
 *******************************************************************************
 * This function increment the reference counter of a picture in the video
 * heap. It needs a lock since several producer threads can access the picture.
 *******************************************************************************/
void vout_LinkPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    vlc_mutex_lock( &p_vout->lock );
    p_pic->i_refcount++;
    vlc_mutex_unlock( &p_vout->lock );

#ifdef DEBUG_VIDEO
    intf_DbgMsg("picture %p\n", p_pic);    
#endif
}

/*******************************************************************************
 * vout_UnlinkPicture: decrement reference counter of a picture
 *******************************************************************************
 * This function decrement the reference counter of a picture in the video heap.
 *******************************************************************************/
void vout_UnlinkPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    vlc_mutex_lock( &p_vout->lock );
    p_pic->i_refcount--;
    if( (p_pic->i_refcount == 0) && (p_pic->i_status == DISPLAYED_PICTURE) )
    {
	p_pic->i_status = DESTROYED_PICTURE;
    }
    vlc_mutex_unlock( &p_vout->lock );

#ifdef DEBUG_VIDEO
    intf_DbgMsg("picture %p\n", p_pic);    
#endif
}

/* following functions are local */

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

    /* Initialize output method - this function issues its own error messages */
    if( vout_SysInit( p_vout ) )
    {
        *p_vout->pi_status = THREAD_ERROR;        
        return( 1 );
    } 

    /* Initialize pictures */    
    for( i_index = 0; i_index < VOUT_MAX_PICTURES; i_index++)
    {
        p_vout->p_picture[i_index].i_type  = EMPTY_PICTURE;
        p_vout->p_picture[i_index].i_status= FREE_PICTURE;
    }

    /* Initialize convertion tables and functions */
    if( vout_InitTables( p_vout ) )
    {
        intf_ErrMsg("error: can't allocate translation tables\n");
        return( 1 );                
    }
    
    /* Mark thread as running and return */
    p_vout->b_active =          1;    
    *p_vout->pi_status =        THREAD_READY;    
    intf_DbgMsg("thread ready\n");    
    return( 0 );    
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
    int             i_picture;                                /* picture index */
    int             i_err;                                       /* error code */
    int             i_idle_level = 0;                            /* idle level */
    mtime_t         current_date;                              /* current date */
    mtime_t         pic_date = 0;                              /* picture date */    
    mtime_t         last_date = 0;                        /* last picture date */    
    boolean_t       b_display;                                 /* display flag */    
    picture_t *     p_pic;                                  /* picture pointer */
     
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
        /* 
	 * Find the picture to display - this operation does not need lock,
         * since only READY_PICTURES are handled 
         */
        p_pic = NULL;         
        for( i_picture = 0; i_picture < VOUT_MAX_PICTURES; i_picture++ )
	{
	    if( (p_vout->p_picture[i_picture].i_status == READY_PICTURE) &&
		( (p_pic == NULL) || 
		  (p_vout->p_picture[i_picture].date < pic_date) ) )
	    {
                p_pic = &p_vout->p_picture[i_picture];
                pic_date = p_pic->date;                
	    }
	}
        current_date = mdate();

        /* 
	 * Render picture if any
	 */
        if( p_pic )
        {
#ifdef STATS
            /* Computes FPS rate */
            p_vout->fps_sample[ p_vout->c_fps_samples++ % VOUT_FPS_SAMPLES ] = pic_date;
#endif	    
	    if( pic_date < current_date )
	    {
		/* Picture is late: it will be destroyed and the thread will sleep and
                 * go to next picture */
                vlc_mutex_lock( &p_vout->lock );
                p_pic->i_status = p_pic->i_refcount ? DISPLAYED_PICTURE : DESTROYED_PICTURE;
                vlc_mutex_unlock( &p_vout->lock );
#ifdef DEBUG_VIDEO
		intf_DbgMsg( "warning: late picture %p skipped\n", p_pic );
#endif
                p_pic =         NULL;                
	    }
	    else if( pic_date > current_date + VOUT_DISPLAY_DELAY )
	    {
		/* A picture is ready to be rendered, but its rendering date is
		 * far from the current one so the thread will perform an empty loop
		 * as if no picture were found. The picture state is unchanged */
                p_pic =         NULL;                
	    }
	    else
	    {
		/* Picture has not yet been displayed, and has a valid display
		 * date : render it, then mark it as displayed */
                if( p_vout->b_active )
                {                    
                    RenderPicture( p_vout, p_pic );
                    if( p_vout->b_info )
                    {
                        RenderPictureInfo( p_vout, p_pic );                        
                    }                    
                }                
                vlc_mutex_lock( &p_vout->lock );
                p_pic->i_status = p_pic->i_refcount ? DISPLAYED_PICTURE : DESTROYED_PICTURE;
                vlc_mutex_unlock( &p_vout->lock );
	    }
        }

        /* 
         * Rebuild tables if gamma has changed
         */
        if( p_vout->b_gamma_change )
        {
            //??
            p_vout->b_gamma_change = 0;            
            vout_ResetTables( p_vout );            // ?? test return value
        }        

        /*
         * Check events, sleep and display picture
	 */
        i_err = vout_SysManage( p_vout );
	if( i_err < 0 )
	{
	    /* A fatal error occured, and the thread must terminate immediately,
	     * without displaying anything - setting b_error to 1 cause the
	     * immediate end of the main while() loop. */
	    p_vout->b_error = 1;
	}
	else 
	{            
	    if( p_pic )
	    {
		/* A picture is ready to be displayed : remove blank screen flag */
                last_date =     pic_date;
                i_idle_level =  0;
                b_display =     1;                
                
                /* Sleep until its display date */
		mwait( pic_date );
	    }
	    else
	    {
                /* If last picture was a long time ago, increase idle level, reset
                 * date and render idle screen */
                if( !i_err && (current_date - last_date > VOUT_IDLE_DELAY) )
                {       
                    last_date = current_date;                    
                    b_display = p_vout->b_active && RenderIdle( p_vout, i_idle_level++ );
                }
                else
                {
                    b_display = 0;                    
                }
                
#ifdef STATS
		/* Update counters */
		p_vout->c_idle_loops++;
#endif

		/* Sleep to wait for new pictures */
		msleep( VOUT_IDLE_SLEEP );
	    }

            /* On awakening, send immediately picture to display */
            if( b_display && p_vout->b_active )
            {
                vout_SysDisplay( p_vout );
            }
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
    intf_DbgMsg( "thread end\n" );
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
    /* Wait until a `die' order */
    while( !p_vout->b_die )
    {
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
    int     i_picture;
        
    /* Store status */
    pi_status = p_vout->pi_status;    
    *pi_status = THREAD_END;    

    /* Destroy all remaining pictures */
    for( i_picture = 0; i_picture < VOUT_MAX_PICTURES; i_picture++ )
    {
	if( p_vout->p_picture[i_picture].i_status != FREE_PICTURE )
	{
            free( p_vout->p_picture[i_picture].p_data );
        }
    }

    /* Destroy translation tables */
    vout_EndTables( p_vout );
    
    /* Destroy thread structures allocated by InitThread */
    vout_SysEnd( p_vout );
    vout_SysDestroy( p_vout );
    free( p_vout );

    /* Update status */
    *pi_status = THREAD_OVER;    
}

/*******************************************************************************
 * RenderPicture: render a picture
 *******************************************************************************
 * This function convert a picture from a video heap to a pixel-encoded image
 * and copy it to the current rendering buffer. No lock is required, since the
 * rendered picture has been determined as existant, and will only be destroyed
 * by the vout thread later.
 *******************************************************************************/
static void RenderPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
#ifdef DEBUG_VIDEO
    /* Send picture informations and store rendering start date */
    intf_DbgMsg("picture %p\n", p_pic );
    p_vout->picture_render_time = mdate();    
#endif

    /* 
     * Prepare scaling 
     */
    if( (p_pic->i_width > p_vout->i_width) || (p_pic->i_height > p_vout->i_height) )
    {
#ifdef VIDEO_X11
        /* X11: window can be resized, so resize it - the picture won't be 
         * rendered since any alteration of the window size means recreating the
         * XImages */
        p_vout->i_new_width =   p_pic->i_width;
        p_vout->i_new_height =  p_pic->i_height;
        return;        
#else
        /* Other drivers: the video output thread can't change its size, so
         * we need to change the aspect ratio */
        //????
#endif
    }    

    /*
     * Choose appropriate rendering function and render picture
     */
    switch( p_pic->i_type )
    {
    case YUV_420_PICTURE:
        p_vout->p_ConvertYUV420( p_vout, vout_SysGetPicture( p_vout ),
                                 p_pic->p_y, p_pic->p_u, p_pic->p_v,
                                 p_pic->i_width, p_pic->i_height, 0, 0,
                                 4, p_pic->i_matrix_coefficients );
        break;        
    case YUV_422_PICTURE:
/*     ???   p_vout->p_convert_yuv_420( p_vout, 
                                   p_pic->p_y, p_pic->p_u, p_pic->p_v,
                                   i_chroma_width, i_chroma_height,
                                   p_vout->i_width / 2, p_vout->i_height,
                                   p_vout->i_bytes_per_line,
                                   0, 0, 0 );
  */      break;        
    case YUV_444_PICTURE:
/*  ???      p_vout->p_convert_yuv_420( p_vout, 
                                   p_pic->p_y, p_pic->p_u, p_pic->p_v,
                                   i_chroma_width, i_chroma_height,
                                   p_vout->i_width, p_vout->i_height,
                                   p_vout->i_bytes_per_line,
                                   0, 0, 0 );
  */      break;                
#ifdef DEBUG
    default:        
        intf_DbgMsg("error: unknown picture type %d\n", p_pic->i_type );
        break;        
#endif
    }

    /* 
     * Terminate scaling 
     */
    //??

#ifdef DEBUG_VIDEO
    /* Computes rendering time */
    p_vout->picture_render_time = mdate() - p_vout->picture_render_time;    
#endif
}



/*******************************************************************************
 * RenderPictureInfo: print additionnal informations on a picture
 *******************************************************************************
 * This function will add informations such as fps and buffer size on a picture
 *******************************************************************************/
static void RenderPictureInfo( vout_thread_t *p_vout, picture_t *p_pic )
{
    char        psz_buffer[256];                              /* string buffer */
#ifdef DEBUG
    int         i_ready_pic = 0;                             /* ready pictures */
    int         i_reserved_pic = 0;                       /* reserved pictures */
    int         i_picture;                                    /* picture index */
#endif

#ifdef STATS
    /* 
     * Print FPS rate in upper right corner 
     */
    if( p_vout->c_fps_samples > VOUT_FPS_SAMPLES )
    {        
        sprintf( psz_buffer, "%.2f fps", (double) VOUT_FPS_SAMPLES * 1000000 /
                 ( p_vout->fps_sample[ (p_vout->c_fps_samples - 1) % VOUT_FPS_SAMPLES ] -
                   p_vout->fps_sample[ p_vout->c_fps_samples % VOUT_FPS_SAMPLES ] ) );        
        vout_SysPrint( p_vout, p_vout->i_width, 0, 1, -1, psz_buffer );
    }

    /* 
     * Print statistics in upper left corner 
     */
    sprintf( psz_buffer, "gamma=%.2f   %ld frames (%.1f %% idle)", 
             p_vout->f_gamma, p_vout->c_fps_samples, p_vout->c_loops ? 
             (double ) p_vout->c_idle_loops * 100 / p_vout->c_loops : 100. );    
    vout_SysPrint( p_vout, 0, 0, -1, -1, psz_buffer );    
#endif
    
#ifdef DEBUG
    /* 
     * Print heap state in lower left corner  
     */
    for( i_picture = 0; i_picture < VOUT_MAX_PICTURES; i_picture++ )
    {
        switch( p_vout->p_picture[i_picture].i_status )
        {
        case RESERVED_PICTURE:
            i_reserved_pic++;            
            break;            
        case READY_PICTURE:
            i_ready_pic++;            
            break;            
        }        
    }
    sprintf( psz_buffer, "video heap: %d/%d/%d", i_reserved_pic, i_ready_pic, 
             VOUT_MAX_PICTURES );
    vout_SysPrint( p_vout, 0, p_vout->i_height, -1, 1, psz_buffer );    
#endif

#ifdef DEBUG_VIDEO
    /* 
     * Print picture info in lower right corner 
     */
    switch( p_pic->i_type )
    {
    case YUV_420_PICTURE:
        sprintf( psz_buffer, "YUV 4:2:0 picture, rendering time: %lu us", 
                 (unsigned long) p_vout->picture_render_time );
        break;        
    case YUV_422_PICTURE:
        sprintf( psz_buffer, "YUV 4:2:2 picture, rendering time: %lu us", 
                 (unsigned long) p_vout->picture_render_time );
        break;        
    case YUV_444_PICTURE:
        sprintf( psz_buffer, "YUV 4:4:4 picture, rendering time: %lu us", 
                 (unsigned long) p_vout->picture_render_time );
        break;
    default:
        sprintf( psz_buffer, "unknown picture, rendering time: %lu us", 
                 (unsigned long) p_vout->picture_render_time );    
        break;        
    }    
    vout_SysPrint( p_vout, p_vout->i_width, p_vout->i_height, 1, 1, psz_buffer );    
#endif
}

/*******************************************************************************
 * RenderIdle: render idle picture
 *******************************************************************************
 * This function will clear the display or print a logo. Level will vary from 0
 * to a very high value that noone should never reach. It returns non 0 if 
 * something needs to be displayed and 0 if the previous picture can be kept.
 *******************************************************************************/
static int RenderIdle( vout_thread_t *p_vout, int i_level )
{
    byte_t      *pi_pic;                            /* pointer to picture data */
    
    /* Get frame pointer and clear display */
    pi_pic = vout_SysGetPicture( p_vout );    
     
    
    switch( i_level )
    {
    case 0:                                           /* level 0: clear screen */
        memset( pi_pic, 0, p_vout->i_bytes_per_line * p_vout->i_height );
        break;                
    case 1:                                            /* level 1: "no stream" */        
        memset( pi_pic, 0, p_vout->i_bytes_per_line * p_vout->i_height );
        vout_SysPrint( p_vout, p_vout->i_width / 2, p_vout->i_height / 2,
                       0, 0, "no stream" );     
        break;
    case 50:                                    /* level 50: copyright message */
        memset( pi_pic, 0, p_vout->i_bytes_per_line * p_vout->i_height );
        vout_SysPrint( p_vout, p_vout->i_width / 2, p_vout->i_height / 2,
                       0, 0, COPYRIGHT_MESSAGE );        
        break; 
    default:                            /* other levels: keep previous picture */
        return( 0 );
        break;        
    }

    return( 1 );    
}

