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
#include "main.h"

/*******************************************************************************
 * Macros
 *******************************************************************************/

/* CLIP_BYTE: return value if between 0 and 255, else return nearest boundary 
 * (0 or 255) */
#define CLIP_BYTE( i_val ) ( (i_val < 0) ? 0 : ((i_val > 255) ? 255 : i_val) )

/* YUV_GRAYSCALE: parametric macro for YUV grayscale transformation.
 * Due to the high performance need of this loop, all possible conditions 
 * evaluations are made outside the transformation loop. However, the code does 
 * not change much for two different loops. This macro allows to change slightly
 * the content of the loop without having to copy and paste code. It is used in 
 * RenderYUVPicture function. */
#define YUV_GRAYSCALE( TRANS_RED, TRANS_GREEN, TRANS_BLUE, P_PIC )      \
/* Main loop */                                                         \
for (i_pic_y=0; i_pic_y < p_pic->i_height ; i_pic_y++)                  \
{                                                                       \
    for (i_pic_x=0; i_pic_x< p_pic->i_width; i_pic_x++)                 \
    {                                                                   \
        i_y = *p_y++;                                                   \
        *P_PIC++ = TRANS_RED[i_y] | TRANS_GREEN[i_y] | TRANS_BLUE[i_y]; \
    }                                                                   \
    /* Skip until beginning of next line */                             \
    P_PIC += i_eol_offset;                                              \
}                                                                       

/* YUV_TRANSFORM: parametric macro for YUV transformation.
 * Due to the high performance need of this loop, all possible conditions 
 * evaluations are made outside the transformation loop. However, the code does 
 * not change much for two different loops. This macro allows to change slightly
 * the content of the loop without having to copy and paste code. It is used in 
 * RenderYUVPicture function. */
#define YUV_TRANSFORM( CHROMA, TRANS_RED, TRANS_GREEN, TRANS_BLUE, P_PIC ) \
/* Main loop */                                                         \
for (i_pic_y=0; i_pic_y < p_pic->i_height ; i_pic_y++)                  \
{                                                                       \
    for (i_pic_x=0; i_pic_x< p_pic->i_width; i_pic_x+=2)                \
    {                                                                   \
        /* First sample (complete) */                                   \
        i_y = 76309 * *p_y++ - 1188177;                                 \
        i_u = *p_u++ - 128;                                             \
        i_v = *p_v++ - 128;                                             \
        *P_PIC++ =                                                      \
            TRANS_RED   [(i_y+i_crv*i_v)                >>16] |         \
            TRANS_GREEN [(i_y-i_cgu*i_u-i_cgv*i_v)      >>16] |         \
            TRANS_BLUE  [(i_y+i_cbu*i_u)                >>16];          \
        i_y = 76309 * *p_y++ - 1188177;                                 \
        /* Second sample (partial) */                                   \
        if( CHROMA == 444 )                                             \
        {                                                               \
            i_u = *p_u++ - 128;                                         \
            i_v = *p_v++ - 128;                                         \
        }                                                               \
        *P_PIC++ =                                                      \
            TRANS_RED   [(i_y+i_crv*i_v)                >>16] |         \
            TRANS_GREEN [(i_y-i_cgu*i_u-i_cgv*i_v)      >>16] |         \
            TRANS_BLUE  [(i_y+i_cbu*i_u)                >>16];          \
    }                                                                   \
    if( (CHROMA == 420) && !(i_pic_y & 0x1) )                           \
    {                                                                   \
        p_u -= i_chroma_width;                                          \
        p_v -= i_chroma_width;                                          \
    }                                                                   \
    /* Skip until beginning of next line */                             \
    P_PIC += i_eol_offset;                                              \
}

/*******************************************************************************
 * Constants
 *******************************************************************************/

/* RGB/YUV inversion matrix (ISO/IEC 13818-2 section 6.3.6, table 6.9) */
const int MATRIX_COEFFICIENTS_TABLE[8][4] =
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
 * External prototypes
 *******************************************************************************/
#ifdef HAVE_MMX
/* YUV transformations for MMX - in yuv-mmx.S 
 *      p_y, p_u, p_v:          Y U and V planes
 *      i_width, i_height:      frames dimensions (pixels)
 *      i_ypitch, i_vpitch:     Y and V lines sizes (bytes)
 *      i_aspect:               vertical aspect factor
 *      pi_pic:                 RGB frame
 *      i_dci_offset:           ?? x offset for left image border
 *      i_offset_to_line_0:     ?? x offset for left image border
 *      i_pitch:                RGB line size (bytes)
 *      i_colortype:            0 for 565, 1 for 555 */
void vout_YUV420_16_MMX( u8* p_y, u8* p_u, u8 *p_v, 
                         unsigned int i_width, unsigned int i_height,
                         unsigned int i_ypitch, unsigned int i_vpitch,
                         unsigned int i_aspect, u8 *pi_pic, 
                         u32 i_dci_offset, u32 i_offset_to_line_0,
                         int CCOPitch, int i_colortype );
#endif

/*******************************************************************************
 * Local prototypes
 *******************************************************************************/
static int      InitThread              ( vout_thread_t *p_vout );
static void     RunThread               ( vout_thread_t *p_vout );
static void     ErrorThread             ( vout_thread_t *p_vout );
static void     EndThread               ( vout_thread_t *p_vout );
static void     RenderPicture           ( vout_thread_t *p_vout, picture_t *p_pic );
static void     RenderYUVGrayPicture    ( vout_thread_t *p_vout, picture_t *p_pic );
static void     RenderYUV16Picture      ( vout_thread_t *p_vout, picture_t *p_pic );
static void     RenderYUV32Picture      ( vout_thread_t *p_vout, picture_t *p_pic );
static void     RenderInfo              ( vout_thread_t *p_vout );

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

    /* Initialize some fields used by the system-dependant method - these fields will
     * probably be modified by the method */
    p_vout->b_info              = 0;    
    p_vout->b_grayscale         = main_GetIntVariable( VOUT_GRAYSCALE_VAR, 
                                                       VOUT_GRAYSCALE_DEFAULT );
    p_vout->i_width             = i_width;
    p_vout->i_height            = i_height;
    p_vout->i_bytes_per_line    = i_width * 2;    
    p_vout->i_screen_depth      = 15;
    p_vout->i_bytes_per_pixel   = 2;
    p_vout->f_x_ratio           = 1;
    p_vout->f_y_ratio           = 1;
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
  
    /* Terminate the initialization */
    p_vout->b_die               = 0;
    p_vout->b_error             = 0;    
    p_vout->b_active            = 0;
    p_vout->pi_status           = (pi_status != NULL) ? pi_status : &i_status;
    *p_vout->pi_status          = THREAD_CREATE;    
#ifdef STATS
    p_vout->c_loops             = 0;
    p_vout->c_idle_loops        = 0;
    p_vout->c_pictures          = 0;
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
 * display.
 *******************************************************************************/
void  vout_DisplayPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
#ifdef DEBUG_VIDEO
    char        psz_date[MSTRTIME_MAX_SIZE];         /* buffer for date string */
#endif
  
   vlc_mutex_lock( &p_vout->lock );

#ifdef DEBUG_VIDEO
   /* Check if picture status is valid */
   if( p_pic->i_status != RESERVED_PICTURE )
   {
       intf_DbgMsg("error: picture %d has invalid status %d\n", 
                   p_pic, p_pic->i_status );       
   }   
#endif

    /* Remove reservation flag */
    p_pic->i_status = READY_PICTURE;

#ifdef STATS
    /* Update stats */
    p_vout->c_pictures++;
#endif

#ifdef DEBUG_VIDEO
    /* Send picture informations */
    intf_DbgMsg("picture %p: type=%d, %dx%d, date=%s\n", p_pic, p_pic->i_type, 
                p_pic->i_width,p_pic->i_height, mstrtime( psz_date, p_pic->date ) );    
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
        case YUV_420_PICTURE:                   /* YUV picture: bits per pixel */
        case YUV_422_PICTURE:
        case YUV_444_PICTURE:
            p_free_picture->p_data = malloc( 3 * i_height * i_bytes_per_line );                
            p_free_picture->p_y = (yuv_data_t *) p_free_picture->p_data;
            p_free_picture->p_u = (yuv_data_t *)(p_free_picture->p_data + i_height * i_bytes_per_line);
            p_free_picture->p_v = (yuv_data_t *)(p_free_picture->p_data + i_height * i_bytes_per_line * 2);
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
            /* Copy picture informations */
            p_free_picture->i_type                      = i_type;
            p_free_picture->i_status                    = RESERVED_PICTURE;
            p_free_picture->i_width                     = i_width;
            p_free_picture->i_height                    = i_height;
            p_free_picture->i_bytes_per_line            = i_bytes_per_line;
            p_free_picture->i_refcount                  = 0;            
            p_free_picture->i_matrix_coefficients       = 1; 
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
 *******************************************************************************/
void vout_DestroyPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    vlc_mutex_lock( &p_vout->lock );

#ifdef DEBUG_VIDEO
   /* Check if picture status is valid */
   if( p_pic->i_status != RESERVED_PICTURE )
   {
       intf_DbgMsg("error: picture %d has invalid status %d\n", 
                   p_pic, p_pic->i_status );       
   }   
#endif

    p_pic->i_status = DESTROYED_PICTURE;

#ifdef DEBUG_VIDEO
    intf_DbgMsg("picture %p\n", p_pic);    
#endif

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

#ifdef DEBUG_VIDEO
    intf_DbgMsg("picture %p\n", p_pic);    
#endif

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

#ifdef DEBUG_VIDEO
    intf_DbgMsg("picture %p\n", p_pic);    
#endif

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
    p_vout->i_pictures = 0;
    for( i_index = 0; i_index < VOUT_MAX_PICTURES; i_index++)
    {
        p_vout->p_picture[i_index].i_type  = EMPTY_PICTURE;
        p_vout->p_picture[i_index].i_status= FREE_PICTURE;
    }

#ifdef STATS
    /* Initialize FPS index - since samples won't be used until a minimum of
     * pictures, they don't need to be initialized */
    p_vout->i_fps_index = 0;    
#endif

    /* Initialize output method - this function issues its own error messages */
    if( vout_SysInit( p_vout ) )
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
#ifndef DEBUG
    default:        
#endif
        i_pixel_size = sizeof( u32 );
        break;
#ifdef DEBUG
    default:
        intf_DbgMsg("error: invalid bytes_per_pixel %d\n", p_vout->i_bytes_per_pixel );
        i_pixel_size = sizeof( u32 );        
        break;              
#endif
    }
    p_vout->pi_trans32_red =   (u32 *)p_vout->pi_trans16_red =   
        (u16 *)malloc( 1024 * i_pixel_size );
    p_vout->pi_trans32_green = (u32 *)p_vout->pi_trans16_green = 
        (u16 *)malloc( 1024 * i_pixel_size );
    p_vout->pi_trans32_blue =  (u32 *)p_vout->pi_trans16_blue =  
        (u16 *)malloc( 1024 * i_pixel_size );
    if( (p_vout->pi_trans16_red == NULL) || 
        (p_vout->pi_trans16_green == NULL ) ||
        (p_vout->pi_trans16_blue == NULL ) )
    {
        intf_ErrMsg("error: %s\n", strerror(ENOMEM) );
        *p_vout->pi_status = THREAD_ERROR;   
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
            p_vout->pi_trans16_green[i_index]   = (CLIP_BYTE( i_index ) & 0xfc)<<3;
            p_vout->pi_trans16_blue[i_index]    =  CLIP_BYTE( i_index ) >> 3;
        }
        break;        
    case 24:
    case 32:        
        for( i_index = -384; i_index < 640; i_index++) 
        {
            p_vout->pi_trans32_red[i_index]     =  CLIP_BYTE( i_index ) <<16;
            p_vout->pi_trans32_green[i_index]   =  CLIP_BYTE( i_index ) <<8;
            p_vout->pi_trans32_blue[i_index]    =  CLIP_BYTE( i_index ) ;
        }
        break;        
#ifdef DEBUG
    default:
        intf_DbgMsg("error: invalid screen depth %d\n", p_vout->i_screen_depth );
        break;      
#endif
    }
    
    /* Mark thread as running and return */
    p_vout->b_active =          1;    
    *p_vout->pi_status =        THREAD_READY;    
    intf_DbgMsg("thread ready\n");    
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
    picture_t *     p_pic;                                  /* picture pointer */    
    mtime_t         pic_date = 0;                              /* picture date */    

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
	 * Find the picture to display - this is the only operation requiring
	 * the lock on the picture, since once a READY_PICTURE has been found,
	 * it can't be modified by the other threads (except if it is unliked,
	 * but its data remains)
	 */
        p_pic = NULL;         
        vlc_mutex_lock( &p_vout->lock );
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
	vlc_mutex_unlock( &p_vout->lock );

        /* 
	 * Render picture if any
	 */
        if( p_pic )
        {
#ifdef STATS
            /* Computes FPS rate */
            p_vout->fps_sample[ p_vout->i_fps_index++ ] = pic_date;
            if( p_vout->i_fps_index == VOUT_FPS_SAMPLES )
            {
                p_vout->i_fps_index = 0;                
            }                            
#endif
	    current_date = mdate();
	    if( pic_date < current_date )
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

#ifdef DEBUG_VIDEO
		intf_DbgMsg( "warning: late picture %p skipped\n", p_pic );
#endif
		vlc_mutex_unlock( &p_vout->lock );
		p_pic = NULL;
	    }
	    else if( pic_date > current_date + VOUT_DISPLAY_DELAY )
	    {
		/* A picture is ready to be rendered, but its rendering date is
		 * far from the current one so the thread will perform an empty loop
		 * as if no picture were found. The picture state is unchanged */
		p_pic = NULL;
	    }
	    else
	    {
		/* Picture has not yet been displayed, and has a valid display
		 * date : render it, then forget it */
		RenderPicture( p_vout, p_pic );
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

                /* Print additional informations */
                if( p_vout->b_info )
                {                    
                    RenderInfo( p_vout );
                }                
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
		mwait( pic_date );

		if( !i_err )
		{
		    vout_SysDisplay( p_vout );
		}
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

    /* Destroy translation tables - remeber these tables are translated */    
    free( p_vout->pi_trans16_red - 384 );
    free( p_vout->pi_trans16_green - 384 );
    free( p_vout->pi_trans16_blue - 384 );
    
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
 * ???? 24 and 32 bpp should probably be separated
 *******************************************************************************/
static void RenderPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
#ifdef DEBUG_VIDEO
    /* Send picture informations */
    intf_DbgMsg("picture %p\n", p_pic );

    /* Store rendering start date */
    p_vout->picture_render_time = mdate();    
#endif

    switch( p_pic->i_type )
    {
    case YUV_420_PICTURE:                   /* YUV picture: YUV transformation */        
    case YUV_422_PICTURE:
    case YUV_444_PICTURE:
        if( p_vout->b_grayscale )                                 /* grayscale */
        {
            RenderYUVGrayPicture( p_vout, p_pic );            
        }
        else if( p_vout->i_bytes_per_pixel == 2 )        /* color 15 or 16 bpp */
        {
            RenderYUV16Picture( p_vout, p_pic );        
        }
        else                                             /* color 24 or 32 bpp */
        {
            RenderYUV32Picture( p_vout, p_pic );            
        }
        break;        
#ifdef DEBUG
    default:        
        intf_DbgMsg("error: unknown picture type %d\n", p_pic->i_type );
        break;        
#endif
    }

#ifdef DEBUG_VIDEO
    /* Computes rendering time */
    p_vout->picture_render_time = mdate() - p_vout->picture_render_time;    
#endif
}

/*******************************************************************************
 * RenderYUVGrayPicture: render a 15, 16, 24 or 32 bpp YUV picture in grayscale
 *******************************************************************************
 * Performs the YUV convertion. The picture sent to this function should only
 * have YUV_420, YUV_422 or YUV_444 types.
 *******************************************************************************/
static void RenderYUVGrayPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    int         i_pic_x, i_pic_y;                /* x,y coordinates in picture */
    int         i_width, i_height;                             /* picture size */
    int         i_eol_offset;          /* pixels from end of line to next line */   
    yuv_data_t *p_y;                                     /* Y data base adress */
    yuv_data_t  i_y;                                               /* Y sample */
    u16 *       pi_pic16;                 /* destination picture, 15 or 16 bpp */
    u32 *       pi_pic32;                 /* destination picture, 24 or 32 bpp */
    u16 *       pi_trans16_red;                    /* red transformation table */
    u16 *       pi_trans16_green;                /* green transformation table */
    u16 *       pi_trans16_blue;                  /* blue transformation table */
    u32 *       pi_trans32_red;                    /* red transformation table */
    u32 *       pi_trans32_green;                /* green transformation table */
    u32 *       pi_trans32_blue;                  /* blue transformation table */
 
    /* Set the base pointers and transformation parameters */
    p_y =               p_pic->p_y;
    i_width =           p_pic->i_width;
    i_height =          p_pic->i_height;
    i_eol_offset =      p_vout->i_bytes_per_line / p_vout->i_bytes_per_pixel - i_width;

    /* Get base adress for destination image and translation tables, then
     * transform image */
    switch( p_vout->i_screen_depth )
    {
    case 15:
    case 16:
        pi_trans16_red =      p_vout->pi_trans16_red;
        pi_trans16_green =    p_vout->pi_trans16_green;
        pi_trans16_blue =     p_vout->pi_trans16_blue;        
        pi_pic16 = (u16 *) vout_SysGetPicture( p_vout );

        YUV_GRAYSCALE( pi_trans16_red, pi_trans16_green, pi_trans16_blue,
                       pi_pic16 );
        break;        
    case 24:        
    case 32:
        pi_trans32_red =      p_vout->pi_trans32_red;
        pi_trans32_green =    p_vout->pi_trans32_green;
        pi_trans32_blue =     p_vout->pi_trans32_blue;    
        pi_pic32 = (u32 *) vout_SysGetPicture( p_vout );

        YUV_GRAYSCALE( pi_trans32_red, pi_trans32_green, pi_trans32_blue,
                       pi_pic32 );
        break;        
#ifdef DEBUG
    default:
        intf_DbgMsg("error: invalid screen depth %d\n", p_vout->i_screen_depth );
        break;    
#endif      
    }
}


/*******************************************************************************
 * RenderYUV16Picture: render a 15 or 16 bpp YUV picture
 *******************************************************************************
 * Performs the YUV convertion. The picture sent to this function should only
 * have YUV_420, YUV_422 or YUV_444 types.
 *******************************************************************************/
static void RenderYUV16Picture( vout_thread_t *p_vout, picture_t *p_pic )
{
    int         i_crv, i_cbu, i_cgu, i_cgv;     /* transformation coefficients */
    int         i_pic_x, i_pic_y;                /* x,y coordinates in picture */
    int         i_y, i_u, i_v;                           /* Y, U and V samples */
    int         i_width, i_height;                             /* picture size */
    int         i_chroma_width;                                /* chroma width */    
    int         i_eol_offset;          /* pixels from end of line to next line */
    yuv_data_t *p_y;                                     /* Y data base adress */
    yuv_data_t *p_u;                                     /* U data base adress */
    yuv_data_t *p_v;                                     /* V data base adress */
    u16 *       pi_pic;                 /* base adress for destination picture */
    u16 *       pi_trans_red;                      /* red transformation table */
    u16 *       pi_trans_green;                  /* green transformation table */
    u16 *       pi_trans_blue;                    /* blue transformation table */
 
    /* Choose transformation matrix coefficients */
    i_crv = MATRIX_COEFFICIENTS_TABLE[p_pic->i_matrix_coefficients][0];
    i_cbu = MATRIX_COEFFICIENTS_TABLE[p_pic->i_matrix_coefficients][1];
    i_cgu = MATRIX_COEFFICIENTS_TABLE[p_pic->i_matrix_coefficients][2];
    i_cgv = MATRIX_COEFFICIENTS_TABLE[p_pic->i_matrix_coefficients][3];

    /* Choose the conversions tables */
    pi_trans_red =      p_vout->pi_trans16_red;
    pi_trans_green =    p_vout->pi_trans16_green;
    pi_trans_blue =     p_vout->pi_trans16_blue;    

    /* Set the base pointers and transformation parameters */
    p_y =               p_pic->p_y;
    p_u =               p_pic->p_u;
    p_v =               p_pic->p_v;
    i_width =           p_pic->i_width;
    i_height =          p_pic->i_height;
    i_chroma_width =    i_width / 2;
    i_eol_offset =      p_vout->i_bytes_per_line / 2 - i_width;    
        
    /* Get base adress for destination image */
    pi_pic = (u16 *)vout_SysGetPicture( p_vout );

    /* Do YUV transformation - the loops are repeated for optimization */
    switch( p_pic->i_type )
    {
    case YUV_420_PICTURE:                   /* 15 or 16 bpp 420 transformation */
#ifdef HAVE_MMX
        vout_YUV420_16_MMX( p_y, p_u, p_v, 
                            i_width, i_height, 
                            i_width, i_chroma_width,
                            0, (u8 *) pi_pic, 
                            0, 0, p_vout->i_bytes_per_line, 
                            p_vout->i_screen_depth == 15 );
#else
        YUV_TRANSFORM( 420,
                       pi_trans_red, 
                       pi_trans_green, 
                       pi_trans_blue,
                       pi_pic );            
#endif
        break;
    case YUV_422_PICTURE:                   /* 15 or 16 bpp 422 transformation */
        YUV_TRANSFORM( 422,
                       pi_trans_red, 
                       pi_trans_green, 
                       pi_trans_blue,
                       pi_pic );            
        break;
    case YUV_444_PICTURE:                   /* 15 or 16 bpp 444 transformation */
        YUV_TRANSFORM( 444,
                       pi_trans_red, 
                       pi_trans_green, 
                       pi_trans_blue,
                       pi_pic );            
        break;                 
    }
}

/*******************************************************************************
 * RenderYUV32Picture: render a 24 or 32 bpp YUV picture
 *******************************************************************************
 * Performs the YUV convertion. The picture sent to this function should only
 * have YUV_420, YUV_422 or YUV_444 types.
 *******************************************************************************/
static void RenderYUV32Picture( vout_thread_t *p_vout, picture_t *p_pic )
{
    int         i_crv, i_cbu, i_cgu, i_cgv;     /* transformation coefficients */
    int         i_pic_x, i_pic_y;                /* x,y coordinates in picture */
    int         i_y, i_u, i_v;                           /* Y, U and V samples */
    int         i_width, i_height;                             /* picture size */
    int         i_chroma_width;                                /* chroma width */    
    int         i_eol_offset;          /* pixels from end of line to next line */
    yuv_data_t *p_y;                                     /* Y data base adress */
    yuv_data_t *p_u;                                     /* U data base adress */
    yuv_data_t *p_v;                                     /* V data base adress */
    u32 *       pi_pic;                 /* base adress for destination picture */
    u32 *       pi_trans_red;                      /* red transformation table */
    u32 *       pi_trans_green;                  /* green transformation table */
    u32 *       pi_trans_blue;                    /* blue transformation table */
 
    /* Choose transformation matrix coefficients */
    i_crv = MATRIX_COEFFICIENTS_TABLE[p_pic->i_matrix_coefficients][0];
    i_cbu = MATRIX_COEFFICIENTS_TABLE[p_pic->i_matrix_coefficients][1];
    i_cgu = MATRIX_COEFFICIENTS_TABLE[p_pic->i_matrix_coefficients][2];
    i_cgv = MATRIX_COEFFICIENTS_TABLE[p_pic->i_matrix_coefficients][3];

    /* Choose the conversions tables */
    pi_trans_red =      p_vout->pi_trans32_red;
    pi_trans_green =    p_vout->pi_trans32_green;
    pi_trans_blue =     p_vout->pi_trans32_blue;    

    /* Set the base pointers and transformation parameters */
    p_y =               p_pic->p_y;
    p_u =               p_pic->p_u;
    p_v =               p_pic->p_v;
    i_width =           p_pic->i_width;
    i_height =          p_pic->i_height;
    i_chroma_width =    i_width / 2;
    i_eol_offset =      p_vout->i_bytes_per_line / p_vout->i_bytes_per_pixel - i_width;
        
    /* Get base adress for destination image */
    pi_pic = (u32 *)vout_SysGetPicture( p_vout );

    /* Do YUV transformation - the loops are repeated for optimization */
    switch( p_pic->i_type )
    {
    case YUV_420_PICTURE:                   /* 24 or 32 bpp 420 transformation */
        YUV_TRANSFORM( 420,
                       pi_trans_red, 
                       pi_trans_green, 
                       pi_trans_blue,
                       pi_pic );            
        break;
    case YUV_422_PICTURE:                   /* 24 or 32 bpp 422 transformation */
        YUV_TRANSFORM( 422,
                       pi_trans_red, 
                       pi_trans_green, 
                       pi_trans_blue,
                       pi_pic );            
        break;
    case YUV_444_PICTURE:                   /* 24 or 32 bpp 444 transformation */
        YUV_TRANSFORM( 444,
                       pi_trans_red, 
                       pi_trans_green, 
                       pi_trans_blue,
                       pi_pic );            
        break;                 
    }
}

/*******************************************************************************
 * RenderInfo: print additionnal informations on a picture
 *******************************************************************************
 * This function will add informations such as fps and buffer size on a picture
 *******************************************************************************/
static void RenderInfo( vout_thread_t *p_vout )
{
    char        psz_buffer[256];                              /* string buffer */

#ifdef STATS
    /* Print FPS rate */
    if( p_vout->c_pictures > VOUT_FPS_SAMPLES )
    {        
        sprintf( psz_buffer, "%.2f fps", (double) VOUT_FPS_SAMPLES * 1000000 /
                 ( p_vout->fps_sample[ (p_vout->i_fps_index + (VOUT_FPS_SAMPLES - 1)) % 
                                     VOUT_FPS_SAMPLES ] -
                   p_vout->fps_sample[ p_vout->i_fps_index ] ) );        
        vout_SysPrint( p_vout, p_vout->i_width, 0, 1, -1, psz_buffer );
    }

    /* Print statistics */
    sprintf( psz_buffer, "%ld pictures, %.1f %% idle loops", p_vout->c_pictures,
             (double) p_vout->c_idle_loops * 100 / p_vout->c_loops );    
    vout_SysPrint( p_vout, 0, 0, -1, -1, psz_buffer );    
#endif
    
#ifdef DEBUG
    /* Print heap size  */
    sprintf( psz_buffer, "video heap size: %d (%.1f %%)", p_vout->i_pictures,
             (double) p_vout->i_pictures * 100 / VOUT_MAX_PICTURES );
    vout_SysPrint( p_vout, 0, p_vout->i_height, -1, 1, psz_buffer );    
#endif

#ifdef DEBUG_VIDEO
    /* Print rendering statistics */
    sprintf( psz_buffer, "picture rendering time: %lu us", 
             (unsigned long) p_vout->picture_render_time );    
    vout_SysPrint( p_vout, p_vout->i_width, p_vout->i_height, 1, 1, psz_buffer );    
#endif
}


