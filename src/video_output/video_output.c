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

#ifdef VIDEO_X11
#include <X11/Xlib.h>
#endif

#include "common.h"
#include "config.h"
#include "mtime.h"
#include "vlc_thread.h"
#include "video.h"
#include "video_output.h"
#include "video_sys.h"
#include "intf_msg.h"

/*******************************************************************************
 * Macros
 *******************************************************************************/
#define CLIP_BYTE( i_val ) ( (i_val < 0) ? 0 : ((i_val > 255) ? 255 : i_val) )

/*******************************************************************************
 * Constants
 *******************************************************************************/

/* RGB/YUV inversion matrix (ISO/IEC 13818-2 section 6.3.6, table 6.9) */
int matrix_coefficients[8][4] =
{
  {117504, 138453, 13954, 34903},       /* no sequence_display_extension */
  {117504, 138453, 13954, 34903},       /* ITU-R Rec. 709 (1990) */
  {104597, 132201, 25675, 53279},       /* unspecified */
  {104597, 132201, 25675, 53279},       /* reserved */
  {104448, 132798, 24759, 53109},       /* FCC */
  {104597, 132201, 25675, 53279},       /* ITU-R Rec. 624-4 System B, G */
  {104597, 132201, 25675, 53279},       /* SMPTE 170M */
  {117579, 136230, 16907, 35559}        /* SMPTE 240M (1987) */
};

/*******************************************************************************
 * Local prototypes
 *******************************************************************************/
static int      InitThread          ( vout_thread_t *p_vout );
static void     RunThread           ( vout_thread_t *p_vout );
static void     ErrorThread         ( vout_thread_t *p_vout );
static void     EndThread           ( vout_thread_t *p_vout );

static void     RenderPicture   ( vout_thread_t *p_vout, picture_t *p_pic );

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
vout_thread_t * vout_CreateThread               ( 
#if defined(VIDEO_X11)
                                                  char *psz_display, Window root_window, 
#elif defined(VIDEO_FB)
                                                  //??
#endif
                                                  int i_width, int i_height, int *pi_status 
                                                )
{
    vout_thread_t * p_vout;                               /* thread descriptor */
    int             i_status;                                 /* thread status */

    /* Allocate descriptor and create method */
    p_vout = (vout_thread_t *) malloc( sizeof(vout_thread_t) );
    if( p_vout == NULL )                                              /* error */
    {
        return( NULL );
    }
    intf_DbgMsg( "0x%x\n", p_vout );
    if( vout_SysCreate( p_vout
#if defined(VIDEO_X11)
                        , psz_display, root_window 
#elif defined(VIDEO_FB)
                        //??
#endif
        ) )
    {
      free( p_vout );
      return( NULL );
    }
  
    /* Initialize */
    p_vout->i_width            = i_width;
    p_vout->i_height           = i_height;
    p_vout->pi_status          = (pi_status != NULL) ? pi_status : &i_status;
    *p_vout->pi_status         = THREAD_CREATE;
    p_vout->b_die              = 0;
    p_vout->b_error            = 0;    
    p_vout->b_active           = 0;

    /* Create thread and set locks */
    vlc_mutex_init( &p_vout->lock );
    if( vlc_thread_create( &p_vout->thread_id, "video output", 
			   (void *) RunThread, (void *) p_vout) )
    {
        intf_ErrMsg("vout error: %s\n", strerror(ENOMEM));
	vout_SysDestroy( p_vout );
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

    intf_DbgMsg( "0x%x\n", p_vout );

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
 * display.
 *******************************************************************************/
void  vout_DisplayPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    vlc_mutex_lock( &p_vout->lock );

    /* Remove reservation flag */
    p_pic->i_status = READY_PICTURE;

#ifdef STATS
    /* Update stats */
    p_vout->c_pictures++;
#endif

    vlc_mutex_unlock( &p_vout->lock );
}

/*******************************************************************************
 * vout_CreatePicture: allocate a picture in the video output heap.
 *******************************************************************************
 * This function create a reserved image in the video output heap. 
 * A null pointer is returned if the function fails. This method provides an
 * already allocated zone of memory in the picture data fields.
 *******************************************************************************/
picture_t *vout_CreatePicture( vout_thread_t *p_vout, int i_type, 
			       int i_width, int i_height, int i_bytes_per_line )
{
    int         i_picture;                                    /* picture index */
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
	    /* Picture is marked for destruction, but is still allocated */
	    if( (p_vout->p_picture[i_picture].i_type           == i_type)   &&
		(p_vout->p_picture[i_picture].i_height         == i_height) &&
		(p_vout->p_picture[i_picture].i_bytes_per_line == i_bytes_per_line) )
	    {
		/* Memory size do match : memory will not be reallocated, and function
                 * can end immediately - this is the best possible case, since no
                 * memory allocation needs to be done */
		p_vout->p_picture[i_picture].i_width  = i_width;
		p_vout->p_picture[i_picture].i_status = RESERVED_PICTURE;
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
        case YUV_420_PICTURE:           /* YUV picture: 3*16 ?? bits per pixel */
        case YUV_422_PICTURE:
        case YUV_444_PICTURE:
            p_free_picture->p_data = malloc( 3 * i_height * i_bytes_per_line );                
            p_free_picture->p_y = (yuv_data_t *) p_free_picture->p_data;
            p_free_picture->p_u = (yuv_data_t *)(p_free_picture->p_data + i_height * i_bytes_per_line);
            p_free_picture->p_v = (yuv_data_t *)(p_free_picture->p_data + i_height * i_bytes_per_line * 2);
            break;                
#ifdef DEBUG
        default:
            intf_DbgMsg("0x%x error: unknown picture type %d\n", p_vout, i_type );
            p_free_picture->p_data = NULL;            
#endif    
        }

        if( p_free_picture->p_data != NULL )
        {        
            /* Copy picture informations */
            p_free_picture->i_type           = i_type;
            p_free_picture->i_status         = RESERVED_PICTURE;
            p_free_picture->i_width          = i_width;
            p_free_picture->i_height         = i_height;
            p_free_picture->i_bytes_per_line = i_bytes_per_line;
            p_free_picture->i_refcount       = 0;            
        }
        else
        {
            /* Memory allocation failed : set picture as empty */
            p_free_picture->i_type   = EMPTY_PICTURE;            
            p_free_picture->i_status = FREE_PICTURE;            
            p_free_picture = NULL;            
            intf_DbgMsg("0x%x malloc for new picture failed\n");            
        }
        
        vlc_mutex_unlock( &p_vout->lock );
        return( p_free_picture );
    }
    
    // No free or destroyed picture could be found
    intf_DbgMsg("0x%x no picture available\n");
    vlc_mutex_unlock( &p_vout->lock );
    return( NULL );
}

/*******************************************************************************
 * vout_RemovePicture: remove a permanent or reserved picture from the heap
 *******************************************************************************
 * This function frees a previously reserved picture or a permanent
 * picture. It is meant to be used when the construction of a picture aborted.
 * Note that the picture will be destroyed even if it is linked !
 *******************************************************************************/
void vout_DestroyPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    vlc_mutex_lock( &p_vout->lock );

    /* Mark picture for destruction */
    p_pic->i_status = DESTROYED_PICTURE;
    intf_DbgMsg("%p -> picture %p destroyed\n", p_vout, p_pic );

    vlc_mutex_unlock( &p_vout->lock );
}

/*******************************************************************************
 * vout_LinkPicture: increment reference counter of a picture
 *******************************************************************************
 * This function increment the reference counter of a picture in the video
 * heap.
 *******************************************************************************/
void vout_LinkPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    vlc_mutex_lock( &p_vout->lock );
    p_pic->i_refcount++;
    vlc_mutex_unlock( &p_vout->lock );
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
    int     i_pixel_size;     /* pixel size, in bytes, for translations tables */    

    /* Update status */
    *p_vout->pi_status = THREAD_START;    
    
    /* Initialize pictures */    
    for( i_index = 0; i_index < VOUT_MAX_PICTURES; i_index++)
    {
        p_vout->p_picture[i_index].i_type  = EMPTY_PICTURE;
        p_vout->p_picture[i_index].i_status= FREE_PICTURE;
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
    if( vout_SysInit( p_vout ) )                        /* error */
    {
        *p_vout->pi_status = THREAD_ERROR;        
        return( 1 );
    } 

    /* Allocate translation tables */
    switch( p_vout->i_bytes_per_pixel )
    {
    case 2:                   /* 15 or 16 bpp, use 16 bits translations tables */        
        i_pixel_size = sizeof( u16 );        
        break;                
    case 3:                   /* 24 or 32 bpp, use 32 bits translations tables */        
    case 4:
        i_pixel_size = sizeof( u32 );
        break;        
    }
    p_vout->pi_trans16_red = p_vout->pi_trans32_red =     malloc( 1024 * i_pixel_size );
    p_vout->pi_trans16_green = p_vout->pi_trans32_green = malloc( 1024 * i_pixel_size );
    p_vout->pi_trans16_blue = p_vout->pi_trans32_blue =   malloc( 1024 * i_pixel_size );
    if( (p_vout->pi_trans16_red == NULL) || 
        (p_vout->pi_trans16_green == NULL ) ||
        (p_vout->pi_trans16_blue == NULL ) )
    {
        if( p_vout->pi_trans16_red != NULL )
        {
            free( p_vout->pi_trans16_red );
        }
        if( p_vout->pi_trans16_green != NULL )
        {
            free( p_vout->pi_trans16_green );
        }
        if( p_vout->pi_trans16_blue != NULL )
        {
            free( p_vout->pi_trans16_blue );
        }
        intf_ErrMsg("vout error: %s\n", strerror(ENOMEM) );
        *p_vout->pi_status = THREAD_ERROR;        
        return( 1 );
    }        
    
    /* Translate translation tables */
    p_vout->pi_trans16_red      += 384;
    p_vout->pi_trans16_green    += 384;
    p_vout->pi_trans16_blue     += 384;
    p_vout->pi_trans32_red      += 384;
    p_vout->pi_trans32_green    += 384;
    p_vout->pi_trans32_blue     += 384;

    /* Build translation tables */
    switch( p_vout->i_screen_depth )
    {
    case 15:
        for( i_index = -384; i_index < 640; i_index++) 
        {
            p_vout->pi_trans16_red[i_index]     = (CLIP_BYTE( i_index ) & 0xf8)<<7;
            p_vout->pi_trans16_green[i_index]   = (CLIP_BYTE( i_index ) & 0xf8)<<2;
            p_vout->pi_trans16_blue[i_index]    =  CLIP_BYTE( i_index ) >> 3;
        }
        break;        
    case 16:
        for( i_index = -384; i_index < 640; i_index++) 
        {
            p_vout->pi_trans16_red[i_index]     = (CLIP_BYTE( i_index ) & 0xf8)<<8;
            p_vout->pi_trans16_green[i_index]   = (CLIP_BYTE( i_index ) & 0xf8)<<3;
            p_vout->pi_trans16_blue[i_index]    =  CLIP_BYTE( i_index ) >> 3;
        }
    case 24:
    case 32:        
        for( i_index = -384; i_index < 640; i_index++) 
        {
            p_vout->pi_trans32_red[i_index]     =  CLIP_BYTE( i_index ) <<16;
            p_vout->pi_trans32_green[i_index]   =  CLIP_BYTE( i_index ) <<8;
            p_vout->pi_trans32_blue[i_index]    =  CLIP_BYTE( i_index ) ;
        }
        break;        
    }
    
    //????

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
    int             i_picture;                                /* picture index */
    int             i_err;                                       /* error code */
    mtime_t         current_date;                              /* current date */
    picture_t *     p_pic = NULL;
#ifdef VOUT_DEBUG
    char            sz_date[MSTRTIME_MAX_SIZE];                 /* date buffer */
#endif

    /* 
     * Initialize thread and free configuration 
     */
    intf_DbgMsg( "0x%x begin\n", p_vout );
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
	 * Find the picture to display - this is the only operation requiring
	 * the lock on the picture, since once a READY_PICTURE has been found,
	 * it can't be modified by the other threads (except if it is unliked,
	 * but its data remains)
	 */
        vlc_mutex_lock( &p_vout->lock );

	for( i_picture = 0; i_picture < VOUT_MAX_PICTURES; i_picture++ )
	{
	    if( (p_vout->p_picture[i_picture].i_status == READY_PICTURE) &&
		( (p_pic == NULL) || 
		  (p_vout->p_picture[i_picture].date < p_pic->date) ) )
	    {
		p_pic = &p_vout->p_picture[i_picture];
	    }
	}

	vlc_mutex_unlock( &p_vout->lock );

        /* 
	 * Render picture if any
	 */
        if( p_pic )
        {
	    current_date = mdate();
	    if( p_pic->date < current_date )
	    {
		/* Picture is late: it will be destroyed and the thread will go
		 * immediately to next picture */
		vlc_mutex_lock( &p_vout->lock );
		if( p_pic->i_refcount )
		{
		    p_pic->i_status = DISPLAYED_PICTURE;
		}
		else
		{
		    p_pic->i_status = DESTROYED_PICTURE;
		}
		vlc_mutex_unlock( &p_vout->lock );
		intf_ErrMsg( "vout error: picture %p was late - skipped\n", p_pic );
		p_pic = NULL;
	    }
	    else if( p_pic->date > current_date + VOUT_DISPLAY_DELAY )
	    {
		/* A picture is ready to be rendered, but its rendering date is
		 * far from the current one so the thread will perform an empty loop
		 * as if no picture were found. The picture state is unchanged */
		p_pic = NULL;
	    }
	    else
	    {
		/* Picture has not yet been displayed, and has a valid display
		 * date : render it */
		RenderPicture( p_vout, p_pic );
	    }
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
		/* A picture is ready to be displayed : sleep until its display date */
		mwait( p_pic->date );

		if( !i_err )
		{
		    vout_SysDisplay( p_vout );
		}

		/* Picture has been displayed : destroy it */
		vlc_mutex_lock( &p_vout->lock );
		if( p_pic->i_refcount )
		{
		    p_pic->i_status = DISPLAYED_PICTURE;
		}
		else
		{
		    p_pic->i_status = DESTROYED_PICTURE;
		}
		vlc_mutex_unlock( &p_vout->lock );
	    }
	    else
	    {
		/* Sleep to wait for new pictures */
		msleep( VOUT_IDLE_SLEEP );
#ifdef STATS
		/* Update counters */
		p_vout->c_idle_loops++;
#endif
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
    intf_DbgMsg( "0x%x end\n", p_vout );
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

    /* Destroy translation tables - remeber these tables are translated */    
    free( p_vout->pi_trans16_red - 384 );
    free( p_vout->pi_trans16_green - 384 );
    free( p_vout->pi_trans16_blue - 384 );
    
    /* Destroy thread structures allocated by InitThread */
    vout_SysEnd( p_vout );
    vout_SysDestroy( p_vout );            /* destroy output method */
    free( p_vout );

    /* Update status */
    *pi_status = THREAD_OVER;    
    intf_DbgMsg("%p\n", p_vout);
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
    intf_DbgMsg("0x%x Picture 0x%x type=%d, %dx%d\n", 
                p_vout, p_pic, p_pic->i_type, p_pic->i_width, p_pic->i_height );
    
    /*???*/
}

