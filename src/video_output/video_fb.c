/******************************************************************************
 * vout_fb.c: Linux framebuffer video output display method
 * (c)1998 VideoLAN
 ******************************************************************************/

/******************************************************************************
 * Preamble
 ******************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/uio.h>                                        /* for input.h */

#include "config.h"
#include "common.h"
#include "mtime.h"
#include "vlc_thread.h"

#include "input.h"
#include "video.h"
#include "video_output.h"
#include "video_sys.h"
#include "intf_msg.h"
#include "main.h"

/******************************************************************************
 * vout_sys_t: video output framebuffer method descriptor
 ******************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the FB specific properties of an output thread.
 ******************************************************************************/
typedef struct vout_sys_s
{
    /* System informations */
    int                         i_fb_dev;        /* framebuffer device handle */
    size_t                      i_page_size;                     /* page size */
    struct fb_var_screeninfo    var_info;    /* framebuffer mode informations */

    /* Video memory */
    byte_t *                    p_video;

    /* User settings */
    boolean_t           b_shm;                /* shared memory extension flag */

    /* Font information */
    int                 i_char_bytes_per_line;     /* character width (bytes) */
    int                 i_char_height;            /* character height (lines) */
    int                 i_char_interspacing;/* space between centers (pixels) */
    byte_t *            pi_font;                      /* pointer to font data */

    /* Display buffers information */
    int                 i_buffer_index;                       /* buffer index */
    void *              p_image[2];                                  /* image */

} vout_sys_t;

/******************************************************************************
 * Local prototypes
 ******************************************************************************/
static int     FBOpenDisplay   ( vout_thread_t *p_vout );
static void    FBCloseDisplay  ( vout_thread_t *p_vout );
static void    FBBlankDisplay  ( vout_thread_t *p_vout );


/******************************************************************************
 * vout_SysCreate: allocates FB video thread output method
 ******************************************************************************
 * This function allocates and initializes a FB vout method.
 ******************************************************************************/
int vout_SysCreate( vout_thread_t *p_vout, char *psz_display, int i_root_window )
{
    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {   
        intf_ErrMsg("error: %s\n", strerror(ENOMEM) );
        return( 1 );
    }

    /* Open and initialize device */
    if( FBOpenDisplay( p_vout ) )
    {
        intf_ErrMsg("vout error: can't open display\n");
        free( p_vout->p_sys );
        return( 1 );
    }

    return( 0 );
}

/******************************************************************************
 * vout_SysInit: initialize framebuffer video thread output method
 ******************************************************************************
 * This function creates the images needed by the output thread. It is called
 * at the beginning of the thread, but also each time the display is resized.
 ******************************************************************************/
int vout_SysInit( vout_thread_t *p_vout )
{
    // Blank both screens
    memset( p_vout->p_sys->p_video, 0x00, 2*p_vout->p_sys->i_page_size );
    //memset( p_vout->p_sys->p_image[0], 0xf0, p_vout->p_sys->i_page_size );
    //memset( p_vout->p_sys->p_image[1], 0x0f, p_vout->p_sys->i_page_size );

    /* Set buffer index to 0 */
    p_vout->p_sys->i_buffer_index = 0;

    return( 0 );
}

/******************************************************************************
 * vout_SysEnd: terminate FB video thread output method
 ******************************************************************************
 * Destroy the FB images created by vout_SysInit. It is called at the end of 
 * the thread, but also each time the window is resized.
 ******************************************************************************/
void vout_SysEnd( vout_thread_t *p_vout )
{
    intf_DbgMsg("%p\n", p_vout );
}

/******************************************************************************
 * vout_SysDestroy: destroy FB video thread output method
 ******************************************************************************
 * Terminate an output method created by vout_FBCreateOutputMethod
 ******************************************************************************/
void vout_SysDestroy( vout_thread_t *p_vout )
{
    FBCloseDisplay( p_vout );
    free( p_vout->p_sys );
}

/******************************************************************************
 * vout_SysManage: handle FB events
 ******************************************************************************
 * This function should be called regularly by video output thread. It manages
 * console events and allows screen resizing. It returns a non null value on 
 * error.
 ******************************************************************************/
int vout_SysManage( vout_thread_t *p_vout )
{
    /* XXX */
    if( p_vout->i_changes & VOUT_SIZE_CHANGE )
    {
        intf_DbgMsg("resizing window\n");
        p_vout->i_changes &= ~VOUT_SIZE_CHANGE;
	FBBlankDisplay( p_vout );
    }
					
    return 0;
}

/******************************************************************************
 * vout_SysDisplay: displays previously rendered output
 ******************************************************************************
 * This function send the currently rendered image to FB image, waits until
 * it is displayed and switch the two rendering buffers, preparing next frame.
 ******************************************************************************/
void vout_SysDisplay( vout_thread_t *p_vout )
{
    /* Swap buffers */
    //p_vout->p_sys->i_buffer_index = ++p_vout->p_sys->i_buffer_index & 1;
    p_vout->p_sys->i_buffer_index = 0;

    /* tout est bien affiché, on peut échanger les 2 écrans */
    p_vout->p_sys->var_info.xoffset = 0;
    p_vout->p_sys->var_info.yoffset =
        0;
        //p_vout->p_sys->i_buffer_index ? 0 : p_vout->p_sys->var_info.yres;

    //ioctl( p_vout->p_sys->i_fb_dev, FBIOPUT_VSCREENINFO, &p_vout->p_sys->var_info );	
    ioctl( p_vout->p_sys->i_fb_dev, FBIOPAN_DISPLAY, &p_vout->p_sys->var_info );	
}

/******************************************************************************
 * vout_SysGetPicture: get current display buffer informations
 ******************************************************************************
 * This function returns the address of the current display buffer.
 ******************************************************************************/
void * vout_SysGetPicture( vout_thread_t *p_vout )
{
    return( p_vout->p_sys->p_image[ p_vout->p_sys->i_buffer_index ] );
}

/* following functions are local */

/******************************************************************************
 * FBOpenDisplay: open and initialize framebuffer device 
 ******************************************************************************
 * ?? The framebuffer mode is only provided as a fast and efficient way to
 * display video, providing the card is configured and the mode ok. It is
 * not portable, and is not supposed to work with many cards. Use at your
 * own risk !
 ******************************************************************************/

static int FBOpenDisplay( vout_thread_t *p_vout )
{
    char *psz_device;                               /* framebuffer device path */
    struct fb_fix_screeninfo    fix_info;       /* framebuffer fix information */

    /* Open framebuffer device */
    psz_device = main_GetPszVariable( VOUT_FB_DEV_VAR, VOUT_FB_DEV_DEFAULT );
    p_vout->p_sys->i_fb_dev = open( psz_device, O_RDWR);
    if( p_vout->p_sys->i_fb_dev == -1 )
    {
        intf_ErrMsg("vout error: can't open %s (%s)\n", psz_device, strerror(errno) );
        return( 1 );
    }

    // ?? here would be the code used to save the current mode and 
    // ?? change to the most appropriate mode...

    /* Get framebuffer device informations */
    if( ioctl( p_vout->p_sys->i_fb_dev, FBIOGET_VSCREENINFO, &p_vout->p_sys->var_info ) )
    {
        intf_ErrMsg("vout error: can't get framebuffer informations (%s)\n", strerror(errno) );
        close( p_vout->p_sys->i_fb_dev );
        return( 1 );
    }

    /* Framebuffer must have some basic properties to be usable */
    //??

    /* Set some attributes */
    p_vout->p_sys->var_info.activate = FB_ACTIVATE_NXTOPEN;
    p_vout->p_sys->var_info.xoffset =  0;
    p_vout->p_sys->var_info.yoffset =  0;
    fprintf(stderr, "ypanstep is %i\n", fix_info.ypanstep);
    //??ask sam p_vout->p_sys->mode_info.sync = FB_SYNC_VERT_HIGH_ACT;
    //???
    if( ioctl( p_vout->p_sys->i_fb_dev, FBIOPUT_VSCREENINFO, &p_vout->p_sys->var_info ) )
    {
        intf_ErrMsg("vout error: can't set framebuffer informations (%s)\n", strerror(errno) );
        close( p_vout->p_sys->i_fb_dev );
        return( 1 );
    }
    
    /* Get some informations again, in the definitive configuration */
    if( ioctl( p_vout->p_sys->i_fb_dev, FBIOGET_FSCREENINFO, &fix_info ) ||
        ioctl( p_vout->p_sys->i_fb_dev, FBIOGET_VSCREENINFO, &p_vout->p_sys->var_info ) )
    {
        intf_ErrMsg("vout error: can't get framebuffer informations (%s)\n", strerror(errno) );
        // ?? restore fb config
        close( p_vout->p_sys->i_fb_dev );
        return( 1 );
    }

    p_vout->i_width =                   p_vout->p_sys->var_info.xres;
    p_vout->i_height =                  p_vout->p_sys->var_info.yres;
    p_vout->i_bytes_per_line =          p_vout->i_width * 2;
    p_vout->i_screen_depth =            p_vout->p_sys->var_info.bits_per_pixel;
    switch( p_vout->i_screen_depth )
    {
    case 15:                        /* 15 bpp (16bpp with a missing green bit) */
    case 16:                                          /* 16 bpp (65536 colors) */
        p_vout->i_bytes_per_pixel = 2;
        break;

    case 24:                                    /* 24 bpp (millions of colors) */
        p_vout->i_bytes_per_pixel = 3;
        break;

    case 32:                                    /* 32 bpp (millions of colors) */
        p_vout->i_bytes_per_pixel = 4;
        break;

    default:                                       /* unsupported screen depth */
        intf_ErrMsg("vout error: screen depth %i is not supported\n",
		                     p_vout->i_screen_depth);
        return( 1  );
        break;
    }
    p_vout->p_sys->i_page_size = p_vout->p_sys->var_info.xres *
                p_vout->p_sys->var_info.yres * p_vout->i_bytes_per_pixel;

    /* Map two framebuffers a the very beginning of the fb */
    p_vout->p_sys->p_video = mmap(0, p_vout->p_sys->i_page_size * 2,
                                  PROT_READ | PROT_WRITE, MAP_SHARED,
                                  p_vout->p_sys->i_fb_dev, 0 );
    if( (int)p_vout->p_sys->p_video == -1 ) //?? according to man, it is -1. What about NULL ?
    {
        intf_ErrMsg("vout error: can't map video memory (%s)\n", strerror(errno) );
        // ?? restore fb config
        close( p_vout->p_sys->i_fb_dev );
        return( 1 );
    }
    p_vout->p_sys->p_image[ 0 ] = p_vout->p_sys->p_video;
    p_vout->p_sys->p_image[ 1 ] = p_vout->p_sys->p_video + p_vout->p_sys->i_page_size;

    intf_DbgMsg("framebuffer type=%d, visual=%d, ypanstep=%d, ywrap=%d, accel=%d\n",
                fix_info.type, fix_info.visual, fix_info.ypanstep, fix_info.ywrapstep, fix_info.accel );
    intf_Msg("vout: framebuffer display initialized (%s), %dx%d depth=%d bpp\n",
             fix_info.id, p_vout->i_width, p_vout->i_height, p_vout->i_screen_depth );

    return( 0 );
}    

/******************************************************************************
 * FBCloseDisplay: close and reset framebuffer device 
 ******************************************************************************
 * Returns all resources allocated by FBOpenDisplay and restore the original
 * state of the device.
 ******************************************************************************/
static void FBCloseDisplay( vout_thread_t *p_vout )
{
    // Free font info
    free( p_vout->p_sys->pi_font );    

    // Destroy window and close display
    close( p_vout->p_sys->i_fb_dev );    
}

/******************************************************************************
 * FBBlankDisplay: render a blank screen
 ******************************************************************************
 * This function is called by all other rendering functions when they arrive on
 * a non blanked screen.
 ******************************************************************************/
static void FBBlankDisplay( vout_thread_t *p_vout )
{
    memset( p_vout->p_sys->p_video, 0x00, 2*p_vout->p_sys->i_page_size );
}
