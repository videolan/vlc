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
    struct fb_var_screeninfo    var_info;    /* framebuffer mode informations */

    /* Video memory */
    byte_t *                    p_video;                       /* base adress */    
    size_t                      i_page_size;                     /* page size */

    struct fb_cmap              fb_cmap;                 /* original colormap */
    unsigned short              *fb_palette;              /* original palette */

} vout_sys_t;

/******************************************************************************
 * Local prototypes
 ******************************************************************************/
static int     FBOpenDisplay   ( vout_thread_t *p_vout );
static void    FBCloseDisplay  ( vout_thread_t *p_vout );
static void    FBSetPalette    ( p_vout_thread_t p_vout,
                                 u16 *red, u16 *green, u16 *blue, u16 *transp );

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
 ******************************************************************************/
int vout_SysInit( vout_thread_t *p_vout )
{
    p_vout->p_set_palette       = FBSetPalette;
    return( 0 );
}

/******************************************************************************
 * vout_SysEnd: terminate FB video thread output method
 ******************************************************************************/
void vout_SysEnd( vout_thread_t *p_vout )
{       
    ;    
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
 * console events. It returns a non null value on error.
 ******************************************************************************/
int vout_SysManage( vout_thread_t *p_vout )
{
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
    /* tout est bien affiché, on peut échanger les 2 écrans */
    p_vout->p_sys->var_info.xoffset = 0;
    p_vout->p_sys->var_info.yoffset = p_vout->i_buffer_index ? p_vout->p_sys->var_info.yres : 0;

    //ioctl( p_vout->p_sys->i_fb_dev, FBIOPUT_VSCREENINFO, &p_vout->p_sys->var_info );	
    ioctl( p_vout->p_sys->i_fb_dev, FBIOPAN_DISPLAY, &p_vout->p_sys->var_info );	
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
                                            /* framebuffer palette information */
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
    p_vout->i_screen_depth =            p_vout->p_sys->var_info.bits_per_pixel;
    switch( p_vout->i_screen_depth )
    {
    case 8:                                                          /* 8 bpp */
        p_vout->p_sys->fb_palette = malloc( 8 * 256 * sizeof(unsigned short) );
        p_vout->p_sys->fb_cmap.start = 0;
        p_vout->p_sys->fb_cmap.len = 256;
        p_vout->p_sys->fb_cmap.red = p_vout->p_sys->fb_palette;
        p_vout->p_sys->fb_cmap.green = p_vout->p_sys->fb_palette + 256 * sizeof(unsigned short);
        p_vout->p_sys->fb_cmap.blue = p_vout->p_sys->fb_palette + 2 * 256 * sizeof(unsigned short);
        p_vout->p_sys->fb_cmap.transp = p_vout->p_sys->fb_palette + 3 * 256 * sizeof(unsigned short);

        ioctl( p_vout->p_sys->i_fb_dev, FBIOGETCMAP, &p_vout->p_sys->fb_cmap );

        /* initializes black & white palette */
        //FBInitRGBPalette( p_vout );
	//FBInitBWPalette( p_vout );

        p_vout->i_bytes_per_pixel = 1;
        p_vout->i_bytes_per_line = p_vout->i_width;
        break;

    case 15:                       /* 15 bpp (16bpp with a missing green bit) */
    case 16:                                         /* 16 bpp (65536 colors) */
        p_vout->i_bytes_per_pixel = 2;
        p_vout->i_bytes_per_line = p_vout->i_width * 2;
        break;

    case 24:                                   /* 24 bpp (millions of colors) */
        p_vout->i_bytes_per_pixel = 3;
        p_vout->i_bytes_per_line = p_vout->i_width * 3;
        break;

    case 32:                                   /* 32 bpp (millions of colors) */
        p_vout->i_bytes_per_pixel = 4;
        p_vout->i_bytes_per_line = p_vout->i_width * 4;
        break;

    default:                                      /* unsupported screen depth */
        intf_ErrMsg("vout error: screen depth %d is not supported\n",
		                     p_vout->i_screen_depth);
        return( 1  );
        break;
    }

    switch( p_vout->i_screen_depth )
    {
    case 15:
    case 16:
    case 24:
    case 32:
        p_vout->i_red_mask =    ( (1 << p_vout->p_sys->var_info.red.length) - 1 )
                                    << p_vout->p_sys->var_info.red.offset;
        p_vout->i_green_mask =    ( (1 << p_vout->p_sys->var_info.green.length) - 1 )
                                    << p_vout->p_sys->var_info.green.offset;
        p_vout->i_blue_mask =    ( (1 << p_vout->p_sys->var_info.blue.length) - 1 )
                                    << p_vout->p_sys->var_info.blue.offset;
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

    /* Set and initialize buffers */
    vout_SetBuffers( p_vout, p_vout->p_sys->p_video, 
                     p_vout->p_sys->p_video + p_vout->p_sys->i_page_size );
    intf_DbgMsg("framebuffer type=%d, visual=%d, ypanstep=%d, ywrap=%d, accel=%d\n",
                fix_info.type, fix_info.visual, fix_info.ypanstep, fix_info.ywrapstep, fix_info.accel );
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
    /* Restore palette */
    if( p_vout->i_screen_depth == 8 );
    {
        ioctl( p_vout->p_sys->i_fb_dev, FBIOPUTCMAP, &p_vout->p_sys->fb_cmap );
        free( p_vout->p_sys->fb_palette );
    }

    // Destroy window and close display
    close( p_vout->p_sys->i_fb_dev );    
}

/******************************************************************************
 * FBSetPalette: sets an 8 bpp palette
 ******************************************************************************
 * This function sets the palette given as an argument. It does not return
 * anything, but could later send information on which colors it was unable
 * to set.
 ******************************************************************************/
static void    FBSetPalette   ( p_vout_thread_t p_vout,
                                u16 *red, u16 *green, u16 *blue, u16 *transp )
{
    struct fb_cmap cmap = { 0, 256, red, green, blue, transp };
    ioctl( p_vout->p_sys->i_fb_dev, FBIOPUTCMAP, &cmap );
}

