/*****************************************************************************
 * vout_3dfx.c: 3dfx video output display method for 3dfx cards
 * (c)2000 VideoLAN
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#ifndef __linux__
#include <conio.h>                                            /* for glide ? */
#endif
#include <sys/uio.h>                                          /* for input.h */
#include <glide.h>

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

#define WIDTH 640
#define HEIGHT 480
#define BITS_PER_PLANE 16
#define BYTES_PER_PIXEL 2

/*****************************************************************************
 * vout_sys_t: 3dfx video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the 3dfx specific properties of an output thread.
 *****************************************************************************/
typedef struct vout_sys_s
{
    GrLfbInfo_t                 p_buffer_info;           /* back buffer info */

    /* Dummy video memory */
    byte_t *                    p_video;                      /* base adress */
    size_t                      i_page_size;                    /* page size */

} vout_sys_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int     GlideOpenDisplay   ( vout_thread_t *p_vout );
static void    GlideCloseDisplay  ( vout_thread_t *p_vout );

/*****************************************************************************
 * vout_SysCreate: allocates 3dfx video thread output method
 *****************************************************************************
 * This function allocates and initializes a 3dfx vout method.
 *****************************************************************************/
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
    if( GlideOpenDisplay( p_vout ) )
    {
        intf_ErrMsg("vout error: can't open display\n");
        free( p_vout->p_sys );
        return( 1 );
    }

    return( 0 );
}

/*****************************************************************************
 * vout_SysInit: initialize 3dfx video thread output method
 *****************************************************************************/
int vout_SysInit( vout_thread_t *p_vout )
{
    return( 0 );
}

/*****************************************************************************
 * vout_SysEnd: terminate 3dfx video thread output method
 *****************************************************************************/
void vout_SysEnd( vout_thread_t *p_vout )
{
    ;
}

/*****************************************************************************
 * vout_SysDestroy: destroy 3dfx video thread output method
 *****************************************************************************
 * Terminate an output method created by vout_CreateOutputMethod
 *****************************************************************************/
void vout_SysDestroy( vout_thread_t *p_vout )
{
    GlideCloseDisplay( p_vout );
    free( p_vout->p_sys );
}

/*****************************************************************************
 * vout_SysManage: handle 3dfx events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * console events. It returns a non null value on error.
 *****************************************************************************/
int vout_SysManage( vout_thread_t *p_vout )
{
    return 0;
}

/*****************************************************************************
 * vout_SysDisplay: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to 3dfx image, waits until
 * it is displayed and switch the two rendering buffers, preparing next frame.
 *****************************************************************************/
void vout_SysDisplay( vout_thread_t *p_vout )
{
    grLfbUnlock( GR_LFB_WRITE_ONLY, GR_BUFFER_BACKBUFFER );

    grBufferSwap( 0 );

    if ( grLfbLock(GR_LFB_WRITE_ONLY, GR_BUFFER_BACKBUFFER,
                   GR_LFBWRITEMODE_565, GR_ORIGIN_UPPER_LEFT, FXFALSE,
                   &p_vout->p_sys->p_buffer_info) == FXFALSE )
    {
        intf_ErrMsg( "vout error: can't take 3dfx back buffer lock\n" );
    }
}

/* following functions are local */

/*****************************************************************************
 * GlideOpenDisplay: open and initialize 3dfx device
 *****************************************************************************/

static int GlideOpenDisplay( vout_thread_t *p_vout )
{
    static char version[80];
    GrHwConfiguration hwconfig;
    GrScreenResolution_t resolution = GR_RESOLUTION_640x480;
    GrLfbInfo_t p_front_buffer_info;                    /* front buffer info */

    p_vout->i_width =                   WIDTH;
    p_vout->i_height =                  HEIGHT;
    p_vout->i_screen_depth =            BITS_PER_PLANE;
    p_vout->i_bytes_per_pixel =         BYTES_PER_PIXEL;
    p_vout->i_bytes_per_line =          1024 * BYTES_PER_PIXEL;

    p_vout->p_sys->i_page_size = WIDTH * HEIGHT * BYTES_PER_PIXEL;

    p_vout->i_red_mask =   0xf800;
    p_vout->i_green_mask = 0x07e0;
    p_vout->i_blue_mask =  0x001f;

    /* Map two framebuffers a the very beginning of the fb */
    p_vout->p_sys->p_video = malloc( p_vout->p_sys->i_page_size * 2 );
    if( (int)p_vout->p_sys->p_video == -1 )
    {
        intf_ErrMsg( "vout error: can't map video memory (%s)\n", strerror(errno) );
        return( 1 );
    }

    grGlideGetVersion( version );
    grGlideInit();

    if( !grSstQueryHardware(&hwconfig) )
    {
        intf_ErrMsg( "vout error: can't get 3dfx hardware config\n" );
        return( 1 );
    }

    grSstSelect( 0 );
    if( !grSstWinOpen(0, resolution, GR_REFRESH_60Hz,
                        GR_COLORFORMAT_ABGR, GR_ORIGIN_UPPER_LEFT, 2, 1) )
    {
        intf_ErrMsg( "vout error: can't open 3dfx screen\n" );
        return( 1 );
    }

    /* disable dithering */
    //grDitherMode( GR_DITHER_DISABLE );

    /* clear both buffers */
    grRenderBuffer( GR_BUFFER_BACKBUFFER );
    grBufferClear( 0, 0, 0 );
    grRenderBuffer( GR_BUFFER_FRONTBUFFER );
    grBufferClear( 0, 0, 0 );
    grRenderBuffer( GR_BUFFER_BACKBUFFER );

    p_vout->p_sys->p_buffer_info.size = sizeof( GrLfbInfo_t );
    p_front_buffer_info.size          = sizeof( GrLfbInfo_t );

    /* lock the buffers to find their adresses */
    if ( grLfbLock(GR_LFB_WRITE_ONLY, GR_BUFFER_FRONTBUFFER,
                   GR_LFBWRITEMODE_565, GR_ORIGIN_UPPER_LEFT, FXFALSE,
                   &p_front_buffer_info) == FXFALSE )
    {
        intf_ErrMsg( "vout error: can't take 3dfx front buffer lock\n" );
        grGlideShutdown();
        return( 1 );
    }
    grLfbUnlock( GR_LFB_WRITE_ONLY, GR_BUFFER_FRONTBUFFER );

    if ( grLfbLock(GR_LFB_WRITE_ONLY, GR_BUFFER_BACKBUFFER,
                   GR_LFBWRITEMODE_565, GR_ORIGIN_UPPER_LEFT, FXFALSE,
                   &p_vout->p_sys->p_buffer_info) == FXFALSE )
    {
        intf_ErrMsg( "vout error: can't take 3dfx back buffer lock\n" );
        grGlideShutdown();
        return( 1 );
    }

    grBufferClear( 0, 0, 0 );

    /* Set and initialize buffers */
    vout_SetBuffers( p_vout, p_vout->p_sys->p_buffer_info.lfbPtr,
                     p_front_buffer_info.lfbPtr );

    return( 0 );
}

/*****************************************************************************
 * GlideCloseDisplay: close and reset 3dfx device
 *****************************************************************************
 * Returns all resources allocated by GlideOpenDisplay and restore the original
 * state of the device.
 *****************************************************************************/
static void GlideCloseDisplay( vout_thread_t *p_vout )
{
    /* unlock the hidden buffer */
    grLfbUnlock( GR_LFB_WRITE_ONLY, GR_BUFFER_BACKBUFFER );

    /* shutdown Glide */
    grGlideShutdown();
    free( p_vout->p_sys->p_video );
}

