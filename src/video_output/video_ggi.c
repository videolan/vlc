/*******************************************************************************
 * vout_ggi.c: GGI video output display method
 * (c)1998 VideoLAN
 *******************************************************************************/

/*******************************************************************************
 * Preamble
 *******************************************************************************/
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <ggi/ggi.h>

#include "config.h"
#include "common.h"
#include "mtime.h"
#include "vlc_thread.h"

#include "video.h"
#include "video_output.h"
#include "video_sys.h"
#include "intf_msg.h"

/*******************************************************************************
 * vout_sys_t: video output GGI method descriptor
 *******************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the GGI specific properties of an output thread. 
 *******************************************************************************/
typedef struct vout_sys_s
{ 
    /* GGI system informations */
    ggi_visual_t        p_display;                           /* display device */

    /* Buffer index */
    int                 i_buffer_index;    
} vout_sys_t;

/*******************************************************************************
 * Local prototypes
 *******************************************************************************/
static int     GGIOpenDisplay   ( vout_thread_t *p_vout );
static void    GGICloseDisplay  ( vout_thread_t *p_vout );

/*******************************************************************************
 * vout_SysCreate: allocate GGI video thread output method
 *******************************************************************************
 * This function allocate and initialize a GGI vout method. It uses some of the
 * vout properties to choose the correct mode, and change them according to the 
 * mode actually used.
 *******************************************************************************/
int vout_SysCreate( vout_thread_t *p_vout )
{    
    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );    
    if( p_vout->p_sys == NULL )
    {
        intf_ErrMsg("error: %s\n", strerror(ENOMEM) );
        return( 1 );
    }

    /* Open and initialize device */
    if( GGIOpenDisplay( p_vout ) )
    {
        intf_ErrMsg("error: can't initialize GGI display\n");        
        free( p_vout->p_sys );
        return( 1 );        
    }
    
    return( 0 );
}

/*******************************************************************************
 * vout_SysInit: initialize GGI video thread output method
 *******************************************************************************
 * This function initialize the GGI display device.
 *******************************************************************************/
int vout_SysInit( vout_thread_t *p_vout )
{
    p_vout->p_sys->i_buffer_index = 0;
    return( 0 );
}

/*******************************************************************************
 * vout_SysEnd: terminate Sys video thread output method
 *******************************************************************************
 * Terminate an output method created by vout_SysCreateOutputMethod
 *******************************************************************************/
void vout_SysEnd( vout_thread_t *p_vout )
{
    ;
}

/*******************************************************************************
 * vout_SysDestroy: destroy Sys video thread output method
 *******************************************************************************
 * Terminate an output method created by vout_SysCreateOutputMethod
 *******************************************************************************/
void vout_SysDestroy( vout_thread_t *p_vout )
{
    GGICloseDisplay( p_vout );
    free( p_vout->p_sys );
}

/*******************************************************************************
 * vout_SysManage: handle Sys events
 *******************************************************************************
 * This function should be called regularly by video output thread. It returns 
 * a negative value if something happened which does not allow the thread to 
 * continue, and a positive one if the thread can go on, but the images have 
 * been modified and therefore it is useless to display them.
 *******************************************************************************/
int vout_SysManage( vout_thread_t *p_vout )
{
    //??

    return( 0 );
}

/*******************************************************************************
 * vout_SysDisplay: displays previously rendered output
 *******************************************************************************
 * This function send the currently rendered image to the display, wait until
 * it is displayed and switch the two rendering buffer, preparing next frame.
 *******************************************************************************/
void vout_SysDisplay( vout_thread_t *p_vout )
{
    /* Change display frame */
    ggiFlush( p_vout->p_sys->p_display ); // ??    
    ggiSetDisplayFrame( p_vout->p_sys->p_display, p_vout->p_sys->i_buffer_index );      
        
    /* Swap buffers and change write frame */
    p_vout->p_sys->i_buffer_index = ++p_vout->p_sys->i_buffer_index & 1;
    ggiSetWriteFrame( p_vout->p_sys->p_display, p_vout->p_sys->i_buffer_index );        
}

/*******************************************************************************
 * vout_SysGetPicture: get current display buffer informations
 *******************************************************************************
 * This function returns the address of the current display buffer.
 *******************************************************************************/
byte_t * vout_SysGetPicture( vout_thread_t *p_vout )
{
//????
//    return( p_vout->p_sys->p_ximage[ p_vout->p_sys->i_buffer_index ].data );        
}

/* following functions are local */

/*******************************************************************************
 * GGIOpenDisplay: open and initialize GGI device 
 *******************************************************************************
 * Open and initialize display according to preferences specified in the vout
 * thread fields.
 *******************************************************************************/
static int GGIOpenDisplay( vout_thread_t *p_vout )
{
    ggi_mode    mode;                                       /* mode descriptor */    
    
    /* Initialize library */
    if( ggiInit() )
    {
        intf_ErrMsg("error: can't initialize GGI library\n");
        return( 1 );        
    }

    /* Open display */
    p_vout->p_sys->p_display = ggiOpen( NULL );
    if( p_vout->p_sys->p_display == NULL )
    {
        intf_ErrMsg("error: can't open GGI default display\n");
        ggiExit();
        return( 1 );        
    }
    
    /* Find most appropriate mode */
    mode.frames =       2;                                        /* 2 buffers */
    mode.visible.x =    p_vout->i_width;                      /* minimum width */
    mode.visible.y =    p_vout->i_width;                      /* maximum width */    
    mode.virt.x =       GGI_AUTO;
    mode.virt.y =       GGI_AUTO;
    mode.size.x =       GGI_AUTO;
    mode.size.y =       GGI_AUTO;
    mode.graphtype =    GT_15BIT;               /* minimum usable screen depth */
    mode.dpp.x =        GGI_AUTO;
    mode.dpp.y =        GGI_AUTO;    
    ggiCheckMode( p_vout->p_sys->p_display, &mode );

    /* Check that returned mode has some minimum properties */
    //??
    
    /* Set mode */
    if( ggiSetMode( p_vout->p_sys->p_display, &mode ) )
    {
        intf_ErrMsg("error: can't set GGI mode\n");
        ggiClose( p_vout->p_sys->p_display );        
        ggiExit();
        return( 1 );        
    }            

    /* Set thread information */
    p_vout->i_width =           mode.visible.x;    
    p_vout->i_height =          mode.visible.y;
    switch( mode.graphtype )
    {
    case GT_15BIT:
        p_vout->i_screen_depth =        15;
        p_vout->i_bytes_per_pixel =     2;
        break;        
    case GT_16BIT:
        p_vout->i_screen_depth =        16;
        p_vout->i_bytes_per_pixel =     2;
        break;        
    case GT_24BIT:
        p_vout->i_screen_depth =        24;
        p_vout->i_bytes_per_pixel =     3;
        break;        
    case GT_32BIT:
        p_vout->i_screen_depth =        32;
        p_vout->i_bytes_per_pixel =     4;
       break;        
    default:
        intf_ErrMsg("error: unsupported screen depth\n");        
        ggiClose( p_vout->p_sys->p_display );
        ggiExit();        
        return( 1 );        
        break;        
    }

    return( 0 );    
}

/*******************************************************************************
 * GGICloseDisplay: close and reset GGI device 
 *******************************************************************************
 * This function returns all resources allocated by GGIOpenDisplay and restore
 * the original state of the device.
 *******************************************************************************/
static void GGICloseDisplay( vout_thread_t *p_vout )
{
    // Restore original mode and close display
    ggiClose( p_vout->p_sys->p_display );    

    // Exit library
    ggiExit();    
}

