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
static void     RenderBlank             ( vout_thread_t *p_vout );
static int      RenderPicture           ( vout_thread_t *p_vout, picture_t *p_pic, boolean_t b_blank );
static int      RenderPictureInfo       ( vout_thread_t *p_vout, picture_t *p_pic, boolean_t b_blank );
static int      RenderIdle              ( vout_thread_t *p_vout, boolean_t b_blank );
static int      RenderInfo              ( vout_thread_t *p_vout, boolean_t b_balnk );
static int      Manage                  ( vout_thread_t *p_vout );

/*******************************************************************************
 * vout_CreateThread: creates a new video output thread
 *******************************************************************************
 * This function creates a new video output thread, and returns a pointer
 * to its description. On error, it returns NULL.
 * If pi_status is NULL, then the function will block until the thread is ready.
 * If not, it will be updated using one of the THREAD_* constants.
 *******************************************************************************/
vout_thread_t * vout_CreateThread               ( char *psz_display, int i_root_window, 
                                                  int i_width, int i_height, int *pi_status )
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
    p_vout->b_grayscale         = main_GetIntVariable( VOUT_GRAYSCALE_VAR, 
                                                       VOUT_GRAYSCALE_DEFAULT );
    p_vout->i_width             = i_width;
    p_vout->i_height            = i_height;
    p_vout->i_bytes_per_line    = i_width * 2;    
    p_vout->i_screen_depth      = 15;
    p_vout->i_bytes_per_pixel   = 2;
    p_vout->f_gamma             = VOUT_GAMMA;    
#ifdef DEBUG
    p_vout->b_info              = 1;    
#else
    p_vout->b_info              = 0;    
#endif
    intf_DbgMsg("wished configuration: %dx%d,%d (%d bytes/pixel, %d bytes/line)\n",
                p_vout->i_width, p_vout->i_height, p_vout->i_screen_depth,
                p_vout->i_bytes_per_pixel, p_vout->i_bytes_per_line );
   
    /* Create and initialize system-dependant method - this function issues its
     * own error messages */
    if( vout_SysCreate( p_vout, psz_display, i_root_window ) )
    {
      free( p_vout );
      return( NULL );
    }
    intf_DbgMsg("actual configuration: %dx%d,%d (%d bytes/pixel, %d bytes/line)\n",
                p_vout->i_width, p_vout->i_height, p_vout->i_screen_depth,
                p_vout->i_bytes_per_pixel, p_vout->i_bytes_per_line );
 
#ifdef STATS
    /* Initialize statistics fields */
    p_vout->render_time           = 0;    
    p_vout->c_fps_samples       = 0;    
#endif      

    /* Initialize running properties */
    p_vout->i_changes           = 0;
    p_vout->last_picture_date   = 0;
    p_vout->last_display_date   = 0;

    /* Create thread and set locks */
    vlc_mutex_init( &p_vout->picture_lock );
    vlc_mutex_init( &p_vout->subtitle_lock );    
    vlc_mutex_init( &p_vout->change_lock );    
    vlc_mutex_lock( &p_vout->change_lock );    
    if( vlc_thread_create( &p_vout->thread_id, "video output", (void *) RunThread, (void *) p_vout) )
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
 * vout_DisplaySubtitle: display a subtitle
 *******************************************************************************
 * Remove the reservation flag of a subtitle, which will cause it to be ready for
 * display. The picture does not need to be locked, since it is ignored by
 * the output thread if is reserved.
 *******************************************************************************/
void  vout_DisplaySubtitle( vout_thread_t *p_vout, subtitle_t *p_sub )
{
#ifdef DEBUG_VIDEO
    char        psz_begin_date[MSTRTIME_MAX_SIZE];   /* buffer for date string */
    char        psz_end_date[MSTRTIME_MAX_SIZE];     /* buffer for date string */
#endif

#ifdef DEBUG
    /* Check if status is valid */
    if( p_sub->i_status != RESERVED_SUBTITLE )
    {
        intf_DbgMsg("error: subtitle %p has invalid status %d\n", p_sub, p_sub->i_status );       
    }   
#endif

    /* Remove reservation flag */
    p_sub->i_status = READY_SUBTITLE;

#ifdef DEBUG_VIDEO
    /* Send subtitle informations */
    intf_DbgMsg("subtitle %p: type=%d, begin date=%s, end date=%s\n", p_sub, p_sub->i_type, 
                mstrtime( psz_begin_date, p_sub->begin_date ), 
                mstrtime( psz_end_date, p_sub->end_date ) );    
#endif
}

/*******************************************************************************
 * vout_CreateSubtitle: allocate a subtitle in the video output heap.
 *******************************************************************************
 * This function create a reserved subtitle in the video output heap. 
 * A null pointer is returned if the function fails. This method provides an
 * already allocated zone of memory in the subtitle data fields. It needs locking
 * since several pictures can be created by several producers threads. 
 *******************************************************************************/
subtitle_t *vout_CreateSubtitle( vout_thread_t *p_vout, int i_type, 
                                 int i_size )
{
    //???
}

/*******************************************************************************
 * vout_DestroySubtitle: remove a permanent or reserved subtitle from the heap
 *******************************************************************************
 * This function frees a previously reserved subtitle.
 * It is meant to be used when the construction of a picture aborted.
 * This function does not need locking since reserved subtitles are ignored by
 * the output thread.
 *******************************************************************************/
void vout_DestroySubtitle( vout_thread_t *p_vout, subtitle_t *p_sub )
{
#ifdef DEBUG
   /* Check if subtitle status is valid */
   if( p_sub->i_status != RESERVED_SUBTITLE )
   {
       intf_DbgMsg("error: subtitle %p has invalid status %d\n", p_sub, p_sub->i_status );       
   }   
#endif

    p_sub->i_status = DESTROYED_SUBTITLE;

#ifdef DEBUG_VIDEO
    intf_DbgMsg("subtitle %p\n", p_sub);    
#endif
}

/*******************************************************************************
 * vout_DisplayPicture: display a picture
 *******************************************************************************
 * Remove the reservation flag of a picture, which will cause it to be ready for
 * display. The picture won't be displayed until vout_DatePicture has been 
 * called.
 *******************************************************************************/
void  vout_DisplayPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    vlc_mutex_lock( &p_vout->picture_lock );
    switch( p_pic->i_status )
    {
    case RESERVED_PICTURE:        
        p_pic->i_status = RESERVED_DISP_PICTURE;
        break;        
    case RESERVED_DATED_PICTURE:
        p_pic->i_status = READY_PICTURE;
        break;        
#ifdef DEBUG
    default:        
        intf_DbgMsg("error: picture %p has invalid status %d\n", p_pic, p_pic->i_status );    
        break;        
#endif
    }
    vlc_mutex_unlock( &p_vout->picture_lock );

#ifdef DEBUG_VIDEO
    intf_DbgMsg("picture %p\n", p_pic );
#endif
}

/*******************************************************************************
 * vout_DatePicture: date a picture
 *******************************************************************************
 * Remove the reservation flag of a picture, which will cause it to be ready for
 * display. The picture won't be displayed until vout_DisplayPicture has been 
 * called.
 *******************************************************************************/
void  vout_DatePicture( vout_thread_t *p_vout, picture_t *p_pic, mtime_t date )
{
    vlc_mutex_lock( &p_vout->picture_lock );
    p_pic->date = date;    
    switch( p_pic->i_status )
    {
    case RESERVED_PICTURE:        
        p_pic->i_status = RESERVED_DATED_PICTURE;
        break;        
    case RESERVED_DISP_PICTURE:
        p_pic->i_status = READY_PICTURE;
        break;        
#ifdef DEBUG
    default:        
        intf_DbgMsg("error: picture %p has invalid status %d\n", p_pic, p_pic->i_status );    
        break;        
#endif
    }
    vlc_mutex_unlock( &p_vout->picture_lock );

#ifdef DEBUG_VIDEO
    intf_DbgMsg("picture %p\n", p_pic);
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
    vlc_mutex_lock( &p_vout->picture_lock );

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
		vlc_mutex_unlock( &p_vout->picture_lock );
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
        vlc_mutex_unlock( &p_vout->picture_lock );
        return( p_free_picture );
    }
    
    // No free or destroyed picture could be found
    intf_DbgMsg( "warning: heap is full\n" );
    vlc_mutex_unlock( &p_vout->picture_lock );
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
   if( (p_pic->i_status != RESERVED_PICTURE) && 
       (p_pic->i_status != RESERVED_DATED_PICTURE) &&
       (p_pic->i_status != RESERVED_DISP_PICTURE) )
   {
       intf_DbgMsg("error: picture %p has invalid status %d\n", p_pic, p_pic->i_status );       
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
    vlc_mutex_lock( &p_vout->picture_lock );
    p_pic->i_refcount++;
    vlc_mutex_unlock( &p_vout->picture_lock );

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
    vlc_mutex_lock( &p_vout->picture_lock );
    p_pic->i_refcount--;

#ifdef DEBUG_VIDEO
    if( p_pic->i_refcount < 0 )
    {
        intf_DbgMsg("error: refcount < 0\n");
        p_pic->i_refcount = 0;        
    }    
#endif

    if( (p_pic->i_refcount == 0) && (p_pic->i_status == DISPLAYED_PICTURE) )
    {
	p_pic->i_status = DESTROYED_PICTURE;
    }
    vlc_mutex_unlock( &p_vout->picture_lock );

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

    /* Initialize pictures and subtitles */    
    for( i_index = 0; i_index < VOUT_MAX_PICTURES; i_index++)
    {
        p_vout->p_picture[i_index].i_type   = EMPTY_PICTURE;
        p_vout->p_picture[i_index].i_status = FREE_PICTURE;
        p_vout->p_subtitle[i_index].i_type  = EMPTY_SUBTITLE;
        p_vout->p_subtitle[i_index].i_status= FREE_SUBTITLE;
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
    mtime_t         current_date;                              /* current date */
    mtime_t         pic_date = 0;                              /* picture date */    
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
        current_date = mdate();
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
                vlc_mutex_lock( &p_vout->picture_lock );
                p_pic->i_status = p_pic->i_refcount ? DISPLAYED_PICTURE : DESTROYED_PICTURE;
                vlc_mutex_unlock( &p_vout->picture_lock );
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
        }
              
        /*
         * Perform rendering, sleep and display rendered picture
         */
        if( p_pic )
        {
            /* A picture is ready to be displayed : render it */
            if( p_vout->b_active )
            {                    
                b_display = RenderPicture( p_vout, p_pic, 1 );
                if( p_vout->b_info )
                {
                    b_display |= RenderPictureInfo( p_vout, p_pic, b_display );
                    b_display |= RenderInfo( p_vout, b_display );                    
                }                    
            }
            else
            {
                b_display = 0;                
            } 

            /* Remove picture from heap */
            vlc_mutex_lock( &p_vout->picture_lock );
            p_pic->i_status = p_pic->i_refcount ? DISPLAYED_PICTURE : DESTROYED_PICTURE;
            vlc_mutex_unlock( &p_vout->picture_lock );                          
        }            
        else
        {
            /* No picture. However, an idle screen may be ready to display */
            if( p_vout->b_active )
            {                
                b_display = RenderIdle( p_vout, 1 );
                if( p_vout->b_info )
                {                    
                    b_display |= RenderInfo( p_vout, b_display );
                }        
            }
            else
            {
                b_display = 0;                
            }            
        }

        /* Give back change lock */
        vlc_mutex_unlock( &p_vout->change_lock );        

        /* Sleep a while or until a given date */
        if( p_pic )
        {
            mwait( pic_date );
        }
        else
        {
            msleep( VOUT_IDLE_SLEEP );                
        }            

        /* On awakening, take back lock and send immediately picture to display */
        vlc_mutex_lock( &p_vout->change_lock );        
        if( b_display && p_vout->b_active && 
            !(p_vout->i_changes & VOUT_NODISPLAY_CHANGE) )
        {
            vout_SysDisplay( p_vout );
        }

        /*
         * Check events and manage thread
	 */
        if( vout_SysManage( p_vout ) | Manage( p_vout ) )
	{
	    /* A fatal error occured, and the thread must terminate immediately,
	     * without displaying anything - setting b_error to 1 cause the
	     * immediate end of the main while() loop. */
	    p_vout->b_error = 1;
	}  
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
 * RenderBlank: render a blank screen
 *******************************************************************************
 * This function is called by all other rendering functions when they arrive on
 * a non blanked screen.
 *******************************************************************************/
static void RenderBlank( vout_thread_t *p_vout )
{
    //?? toooooo slow
    int  i_index;                                    /* current 32 bits sample */    
    int  i_width;                                 /* number of 32 bits samples */    
    u32 *p_pic;                                  /* pointer to 32 bits samples */
    
    /* Initialize variables */
    p_pic =     vout_SysGetPicture( p_vout );
    i_width =   p_vout->i_bytes_per_line * p_vout->i_height / 128;

    /* Clear beginning of screen by 128 bytes blocks */
    for( i_index = 0; i_index < i_width; i_index++ )
    {
        *p_pic++ = 0;   *p_pic++ = 0;
        *p_pic++ = 0;   *p_pic++ = 0;
        *p_pic++ = 0;   *p_pic++ = 0;
        *p_pic++ = 0;   *p_pic++ = 0;
        *p_pic++ = 0;   *p_pic++ = 0;
        *p_pic++ = 0;   *p_pic++ = 0;
        *p_pic++ = 0;   *p_pic++ = 0;
        *p_pic++ = 0;   *p_pic++ = 0;
        *p_pic++ = 0;   *p_pic++ = 0;
        *p_pic++ = 0;   *p_pic++ = 0;
        *p_pic++ = 0;   *p_pic++ = 0;
        *p_pic++ = 0;   *p_pic++ = 0;
        *p_pic++ = 0;   *p_pic++ = 0;
        *p_pic++ = 0;   *p_pic++ = 0;
        *p_pic++ = 0;   *p_pic++ = 0;
        *p_pic++ = 0;   *p_pic++ = 0;
    }

    /* Clear last pixels */
    //??
}


/*******************************************************************************
 * RenderPicture: render a picture
 *******************************************************************************
 * This function convert a picture from a video heap to a pixel-encoded image
 * and copy it to the current rendering buffer. No lock is required, since the
 * rendered picture has been determined as existant, and will only be destroyed
 * by the vout thread later.
 *******************************************************************************/
static int RenderPicture( vout_thread_t *p_vout, picture_t *p_pic, boolean_t b_blank )
{
    int         i_display_height, i_display_width;       /* display dimensions */
    int         i_height, i_width;                /* source picture dimensions */
    int         i_scaled_height;               /* scaled height of the picture */   
    int         i_aspect_scale;                 /* aspect ratio vertical scale */
    int         i_eol;                        /* end of line offset for source */    
    byte_t *    p_convert_dst;                       /* convertion destination */        
    
#ifdef STATS
    /* Start recording render time */
    p_vout->render_time = mdate();
#endif

    /* Mark last picture date */
    p_vout->last_picture_date = p_pic->date;
    i_width =                   p_pic->i_width;    
    i_height =                  p_pic->i_height;
    i_display_width =           p_vout->i_width;    
    i_display_height =          p_vout->i_height;    

    /* Select scaling depending of aspect ratio */
    switch( p_pic->i_aspect_ratio )
    {
    case AR_3_4_PICTURE:
        i_aspect_scale = (4 * i_height - 3 * i_width) ? 
            1 + 3 * i_width / ( 4 * i_height - 3 * i_width ) : 0;
        break;
    case AR_16_9_PICTURE:
        i_aspect_scale = ( 16 * i_height - 9 * i_width ) ? 
            1 + 9 * i_width / ( 16 * i_height - 9 * i_width ) : 0;
        break;
    case AR_221_1_PICTURE:        
        i_aspect_scale = ( 221 * i_height - 100 * i_width ) ?
            1 + 100 * i_width / ( 221 * i_height - 100 * i_width ) : 0;
        break;               
    case AR_SQUARE_PICTURE:
    default:
        i_aspect_scale = 0;        
    }
    i_scaled_height = (i_aspect_scale ? i_height * (i_aspect_scale - 1) / i_aspect_scale : i_height);
    
    /* Crop picture if too large for the screen */
    if( i_width > i_display_width )
    {
        i_eol = i_width - i_display_width / 16 * 16;
        i_width = i_display_width / 16 * 16;        
    }
    else
    {
        i_eol = 0;        
    }
    if( i_scaled_height > i_display_height )
    {
        i_height = (i_aspect_scale * i_display_height / (i_aspect_scale - 1)) / 2 * 2;
        i_scaled_height = i_display_height;        
    }    
    p_convert_dst = vout_SysGetPicture( p_vout ) + 
        ( i_display_width - i_width ) / 2 * p_vout->i_bytes_per_pixel +
        ( i_display_height - i_scaled_height ) / 2 * p_vout->i_bytes_per_line;

    /*
     * Choose appropriate rendering function and render picture
     */
    switch( p_pic->i_type )
    {
    case YUV_420_PICTURE:
        p_vout->p_ConvertYUV420( p_vout, p_convert_dst, 
                                 p_pic->p_y, p_pic->p_u, p_pic->p_v,
                                 i_width, i_height, i_eol, 
                                 p_vout->i_bytes_per_line / p_vout->i_bytes_per_pixel - i_width,
                                 i_aspect_scale, p_pic->i_matrix_coefficients );
        break;        
    case YUV_422_PICTURE:
        p_vout->p_ConvertYUV422( p_vout, p_convert_dst, 
                                 p_pic->p_y, p_pic->p_u, p_pic->p_v,
                                 i_width, i_height, i_eol, 
                                 p_vout->i_bytes_per_line / p_vout->i_bytes_per_pixel - i_width,
                                 i_aspect_scale, p_pic->i_matrix_coefficients );
        break;        
    case YUV_444_PICTURE:
        p_vout->p_ConvertYUV444( p_vout, p_convert_dst, 
                                 p_pic->p_y, p_pic->p_u, p_pic->p_v,
                                 i_width, i_height, i_eol, 
                                 p_vout->i_bytes_per_line / p_vout->i_bytes_per_pixel - i_width,
                                 i_aspect_scale, p_pic->i_matrix_coefficients );
        break;                
#ifdef DEBUG
    default:        
        intf_DbgMsg("error: unknown picture type %d\n", p_pic->i_type );
        break;        
#endif
    }

#ifdef STATS
    /* End recording render time */
    p_vout->render_time = mdate() - p_vout->render_time;
#endif
    return( 1 );    
}

/*******************************************************************************
 * RenderPictureInfo: print additionnal informations on a picture
 *******************************************************************************
 * This function will print informations such as fps and other picture
 * dependant informations.
 *******************************************************************************/
static int RenderPictureInfo( vout_thread_t *p_vout, picture_t *p_pic, boolean_t b_blank )
{
    char        psz_buffer[256];                              /* string buffer */

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
     * Print frames count and loop time in upper left corner 
     */
    sprintf( psz_buffer, "%ld frames   render time: %lu us", 
             p_vout->c_fps_samples, (long unsigned) p_vout->render_time );
    vout_SysPrint( p_vout, 0, 0, -1, -1, psz_buffer );    
#endif

#ifdef DEBUG
    /*
     * Print picture information in lower right corner
     */
    sprintf( psz_buffer, "%s picture %dx%d (%dx%d%+d%+d %s)",
             (p_pic->i_type == YUV_420_PICTURE) ? "4:2:0" :
             ((p_pic->i_type == YUV_422_PICTURE) ? "4:2:2" :
              ((p_pic->i_type == YUV_444_PICTURE) ? "4:4:4" : "ukn-type")),
             p_pic->i_width, p_pic->i_height,
             p_pic->i_display_width, p_pic->i_display_height,
             p_pic->i_display_horizontal_offset, p_pic->i_display_vertical_offset,
             (p_pic->i_aspect_ratio == AR_SQUARE_PICTURE) ? "sq" :
             ((p_pic->i_aspect_ratio == AR_3_4_PICTURE) ? "4:3" :
              ((p_pic->i_aspect_ratio == AR_16_9_PICTURE) ? "16:9" :
               ((p_pic->i_aspect_ratio == AR_221_1_PICTURE) ? "2.21:1" : "ukn-ar" ))));    
    vout_SysPrint( p_vout, p_vout->i_width, p_vout->i_height, 1, 1, psz_buffer );
#endif
    
    return( 0 );    
}

/*******************************************************************************
 * RenderIdle: render idle picture
 *******************************************************************************
 * This function will clear the display or print a logo.
 *******************************************************************************/
static int RenderIdle( vout_thread_t *p_vout, boolean_t b_blank )
{
    /* Blank screen if required */
    if( (mdate() - p_vout->last_picture_date > VOUT_IDLE_DELAY) &&
        (p_vout->last_picture_date > p_vout->last_display_date) &&
        b_blank )
    {        
        RenderBlank( p_vout );
        p_vout->last_display_date = mdate();        
        vout_SysPrint( p_vout, p_vout->i_width / 2, p_vout->i_height / 2, 0, 0,
                       "no stream" );        
        return( 1 );        
    }

    return( 0 );    
}

/*******************************************************************************
 * RenderInfo: render additionnal informations
 *******************************************************************************
 * This function render informations which do not depend of the current picture
 * rendered.
 *******************************************************************************/
static int RenderInfo( vout_thread_t *p_vout, boolean_t b_blank )
{
    char        psz_buffer[256];                              /* string buffer */
#ifdef DEBUG
    int         i_ready_pic = 0;                             /* ready pictures */
    int         i_reserved_pic = 0;                       /* reserved pictures */
    int         i_picture;                                    /* picture index */
#endif

#ifdef DEBUG
    /* 
     * Print thread state in lower left corner  
     */
    for( i_picture = 0; i_picture < VOUT_MAX_PICTURES; i_picture++ )
    {
        switch( p_vout->p_picture[i_picture].i_status )
        {
        case RESERVED_PICTURE:
        case RESERVED_DATED_PICTURE:
        case RESERVED_DISP_PICTURE:
            i_reserved_pic++;            
            break;            
        case READY_PICTURE:
            i_ready_pic++;            
            break;            
        }        
    }
    sprintf( psz_buffer, "%dx%d:%d g%+.2f   pic: %d/%d/%d", 
             p_vout->i_width, p_vout->i_height, p_vout->i_screen_depth, 
             p_vout->f_gamma, i_reserved_pic, i_ready_pic,
             VOUT_MAX_PICTURES );
    vout_SysPrint( p_vout, 0, p_vout->i_height, -1, 1, psz_buffer );    
#endif
    return( 0 );    
}

/*******************************************************************************
 * Manage: manage thread
 *******************************************************************************
 * This function will handle changes in thread configuration.
 *******************************************************************************/
static int Manage( vout_thread_t *p_vout )
{
    /* On gamma or grayscale change, rebuild tables */
    if( p_vout->i_changes & (VOUT_GAMMA_CHANGE | VOUT_GRAYSCALE_CHANGE) )
    {
        vout_ResetTables( p_vout );        
    }    

    /* Clear changes flags which does not need management or have been handled */
    p_vout->i_changes &= ~(VOUT_GAMMA_CHANGE | VOUT_GRAYSCALE_CHANGE |
                           VOUT_INFO_CHANGE );

    /* Detect unauthorized changes */
    if( p_vout->i_changes )
    {
        /* Some changes were not acknowledged by vout_SysManage or this function,
         * it means they should not be authorized */
        intf_ErrMsg( "error: unauthorized changes in the video output thread\n" );        
        return( 1 );        
    }
    
    return( 0 );    
}
