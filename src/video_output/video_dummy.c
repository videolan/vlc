/******************************************************************************
 * vout_dummy.c: Dummy video output display method for testing purposes
 * (c)2000 VideoLAN
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

#define WIDTH 128
#define HEIGHT 64
#define BITS_PER_PLANE 16
#define BYTES_PER_PIXEL 2

/******************************************************************************
 * vout_sys_t: dummy video output method descriptor
 ******************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the dummy specific properties of an output thread.
 ******************************************************************************/
typedef struct vout_sys_s
{
    /* Dummy video memory */
    byte_t *                    p_video;                       /* base adress */    
    size_t                      i_page_size;                     /* page size */

} vout_sys_t;

/******************************************************************************
 * Local prototypes
 ******************************************************************************/
static int     DummyOpenDisplay   ( vout_thread_t *p_vout );
static void    DummyCloseDisplay  ( vout_thread_t *p_vout );

/******************************************************************************
 * vout_SysCreate: allocates dummy video thread output method
 ******************************************************************************
 * This function allocates and initializes a dummy vout method.
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
    if( DummyOpenDisplay( p_vout ) )
    {
        intf_ErrMsg("vout error: can't open display\n");
        free( p_vout->p_sys );
        return( 1 );
    }

    return( 0 );
}

/******************************************************************************
 * vout_SysInit: initialize dummy video thread output method
 ******************************************************************************/
int vout_SysInit( vout_thread_t *p_vout )
{
    return( 0 );
}

/******************************************************************************
 * vout_SysEnd: terminate dummy video thread output method
 ******************************************************************************/
void vout_SysEnd( vout_thread_t *p_vout )
{       
    ;    
}

/******************************************************************************
 * vout_SysDestroy: destroy dummy video thread output method
 ******************************************************************************
 * Terminate an output method created by vout_DummyCreateOutputMethod
 ******************************************************************************/
void vout_SysDestroy( vout_thread_t *p_vout )
{
    DummyCloseDisplay( p_vout );
    free( p_vout->p_sys );
}

/******************************************************************************
 * vout_SysManage: handle dummy events
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
 * This function send the currently rendered image to dummy image, waits until
 * it is displayed and switch the two rendering buffers, preparing next frame.
 ******************************************************************************/
void vout_SysDisplay( vout_thread_t *p_vout )
{
    ;
}

/* following functions are local */

/******************************************************************************
 * DummyOpenDisplay: open and initialize dummy device 
 ******************************************************************************
 * ?? The framebuffer mode is only provided as a fast and efficient way to
 * display video, providing the card is configured and the mode ok. It is
 * not portable, and is not supposed to work with many cards. Use at your
 * own risk !
 ******************************************************************************/

static int DummyOpenDisplay( vout_thread_t *p_vout )
{
    p_vout->i_width =                   WIDTH;
    p_vout->i_height =                  HEIGHT;
    p_vout->i_screen_depth =            BITS_PER_PLANE;
    p_vout->i_bytes_per_pixel =         BYTES_PER_PIXEL;
    p_vout->i_bytes_per_line =          WIDTH * BYTES_PER_PIXEL;

    p_vout->p_sys->i_page_size = WIDTH * HEIGHT * BYTES_PER_PIXEL;

    /* Map two framebuffers a the very beginning of the fb */
    p_vout->p_sys->p_video = malloc( p_vout->p_sys->i_page_size * 2 );
    if( (int)p_vout->p_sys->p_video == -1 )
    {
        intf_ErrMsg("vout error: can't map video memory (%s)\n", strerror(errno) );
        return( 1 );
    }

    /* Set and initialize buffers */
    vout_SetBuffers( p_vout, p_vout->p_sys->p_video, 
                     p_vout->p_sys->p_video + p_vout->p_sys->i_page_size );
    return( 0 );
}    

/******************************************************************************
 * DummyCloseDisplay: close and reset dummy device 
 ******************************************************************************
 * Returns all resources allocated by DummyOpenDisplay and restore the original
 * state of the device.
 ******************************************************************************/
static void DummyCloseDisplay( vout_thread_t *p_vout )
{
    free( p_vout->p_sys->p_video );
}

