/*******************************************************************************
 * vout_x11.c: X11 video output display method
 * (c)1998 VideoLAN
 *******************************************************************************
 * The X11 method for video output thread. It's properties (and the vout_x11_t
 * type) are defined in vout.h. The functions declared here should not be
 * needed by any other module than vout.c.
 *******************************************************************************/

/*******************************************************************************
 * Preamble
 *******************************************************************************/

#include "vlc.h"
/*
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/shm.h>
#include <sys/soundcard.h>
#include <sys/uio.h>


#include "config.h"
#include "common.h"
#include "mtime.h"
#include "vlc_thread.h"
#include "xutils.h"

#include "input.h"
#include "input_vlan.h"

#include "audio_output.h"

#include "video.h"
#include "video_output.h"
#include "video_sys.h"

#include "xconsole.h"
#include "interface.h"
#include "intf_msg.h"
*/

/*******************************************************************************
 * vout_sys_t: video output framebuffer method descriptor
 *******************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the FB specific properties of an output thread. FB video 
 * output is performed through regular resizable windows. Windows can be
 * dynamically resized to adapt to the size of the streams.
 *******************************************************************************/
typedef struct vout_sys_s
{
    int i_dummy;    

} vout_sys_t;

/*******************************************************************************
 * Local prototypes
 *******************************************************************************/


/*******************************************************************************
 * vout_SysCreate: allocate X11 video thread output method
 *******************************************************************************
 * This function allocate and initialize a X11 vout method.
 *******************************************************************************/
int vout_SysCreate( vout_thread_t *p_vout )
{
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );    
    if( p_vout->p_sys != NULL )
    {
        //??
        return( 0 );        
    }

    return( 1 );
}

/*******************************************************************************
 * vout_SysInit: initialize Sys video thread output method
 *******************************************************************************
 * This function create a Sys window according to the user configuration.
 *******************************************************************************/
int vout_SysInit( vout_thread_t *p_vout )
{
    //??
    intf_DbgMsg("%p -> success, depth=%d bpp",
                p_vout, p_vout->i_screen_depth );
    return( 0 );
}

/*******************************************************************************
 * vout_SysEnd: terminate Sys video thread output method
 *******************************************************************************
 * Terminate an output method created by vout_SysCreateOutputMethod
 *******************************************************************************/
void vout_SysEnd( vout_thread_t *p_vout )
{
    intf_DbgMsg("%p\n", p_vout );
    //??
}

/*******************************************************************************
 * vout_SysDestroy: destroy Sys video thread output method
 *******************************************************************************
 * Terminate an output method created by vout_SysCreateOutputMethod
 *******************************************************************************/
void vout_SysDestroy( vout_thread_t *p_vout )
{
    //??
    free( p_vout->p_sys );
}

/*******************************************************************************
 * vout_SysManage: handle Sys events
 *******************************************************************************
 * This function should be called regularly by video output thread. It manages
 * Sys events and allows window resizing. It returns a negative value if 
 * something happened which does not allow the thread to continue, and a 
 * positive one if the thread can go on, but the images have been modified and
 * therefore it is useless to display them.
 *******************************************************************************
 * Messages type: vout, major code: 103
 *******************************************************************************/
int vout_SysManage( vout_thread_t *p_vout )
{
    //??

    return( 0 );
}

/*******************************************************************************
 * vout_SysDisplay: displays previously rendered output
 *******************************************************************************
 * This function send the currently rendered image to Sys server, wait until
 * it is displayed and switch the two rendering buffer, preparing next frame.
 *******************************************************************************/
void vout_SysDisplay( vout_thread_t *p_vout )
{
    //??

    /* Swap buffers */
//??    p_vout->p_sys->i_buffer_index = ++p_vout->p_sys->i_buffer_index & 1;
}

