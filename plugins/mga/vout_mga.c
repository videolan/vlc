/*****************************************************************************
 * vout_mga.c: MGA video output display method
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: vout_mga.c,v 1.11 2001/12/30 07:09:55 sam Exp $
 *
 * Authors: Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *          Samuel Hocevar <sam@zoy.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>                                                 /* ENOMEM */
#include <unistd.h>                                               /* close() */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */
#include <fcntl.h>                                                 /* open() */
#include <sys/ioctl.h>                                            /* ioctl() */
#include <sys/mman.h>                                          /* PROT_WRITE */

#include <videolan/vlc.h>

#ifdef SYS_BSD
#include <sys/types.h>                                     /* typedef ushort */
#endif

#include "video.h"
#include "video_output.h"

#include "vout_mga.h"

/*****************************************************************************
 * vout_sys_t: video output MGA method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the MGA specific properties of an output thread.
 *****************************************************************************/
#ifndef __LINUX_MGAVID_H
#   define __LINUX_MGAVID_H
#   define MGA_VID_CONFIG _IOR('J', 1, mga_vid_config_t)
#   define MGA_VID_ON     _IO ('J', 2)
#   define MGA_VID_OFF    _IO ('J', 3)
#   define MGA_G200 0x1234
#   define MGA_G400 0x5678
typedef struct mga_vid_config_s
{
    u32     card_type;
    u32     ram_size;
    u32     src_width;
    u32     src_height;
    u32     dest_width;
    u32     dest_height;
    u32     x_org;
    u32     y_org;
    u8      colkey_on;
    u8      colkey_red;
    u8      colkey_green;
    u8      colkey_blue;
} mga_vid_config_t;
#endif

typedef struct vout_sys_s
{
    int                 i_page_size;
    byte_t             *p_video;

    /* MGA specific variables */
    int                 i_fd;
    int                 i_size;
    mga_vid_config_t    mga;
    byte_t *            p_mga_vid_base;
    boolean_t           b_g400;

} vout_sys_t;

#define DUMMY_WIDTH 16
#define DUMMY_HEIGHT 16
#define DUMMY_BITS_PER_PLANE 16
#define DUMMY_BYTES_PER_PIXEL 2

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  vout_Probe     ( probedata_t *p_data );
static int  vout_Create    ( struct vout_thread_s * );
static int  vout_Init      ( struct vout_thread_s * );
static void vout_End       ( struct vout_thread_s * );
static void vout_Destroy   ( struct vout_thread_s * );
static int  vout_Manage    ( struct vout_thread_s * );
static void vout_Display   ( struct vout_thread_s * );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( vout_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->pf_probe = vout_Probe;
    p_function_list->functions.vout.pf_create     = vout_Create;
    p_function_list->functions.vout.pf_init       = vout_Init;
    p_function_list->functions.vout.pf_end        = vout_End;
    p_function_list->functions.vout.pf_destroy    = vout_Destroy;
    p_function_list->functions.vout.pf_manage     = vout_Manage;
    p_function_list->functions.vout.pf_display    = vout_Display;
    p_function_list->functions.vout.pf_setpalette = NULL;
}

/*****************************************************************************
 * intf_Probe: return a score
 *****************************************************************************/
static int vout_Probe( probedata_t *p_data )
{
    if( TestMethod( VOUT_METHOD_VAR, "mga" ) )
    {
        return( 999 );
    }

    return( 10 );
}

/*****************************************************************************
 * vout_Create: allocates dummy video thread output method
 *****************************************************************************
 * This function allocates and initializes a dummy vout method.
 *****************************************************************************/
static int vout_Create( vout_thread_t *p_vout )
{
    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        intf_ErrMsg("vout error: %s", strerror(ENOMEM) );
        return( 1 );
    }

    if( (p_vout->p_sys->i_fd = open( "/dev/mga_vid", O_RDWR )) == -1 )
    {
        intf_ErrMsg( "vout error: can't open MGA driver /dev/mga_vid" );
        free( p_vout->p_sys );
        return( 1 );
    }

    p_vout->i_width            = DUMMY_WIDTH;
    p_vout->i_height           = DUMMY_HEIGHT;
    p_vout->i_screen_depth     = DUMMY_BITS_PER_PLANE;
    p_vout->i_bytes_per_pixel  = DUMMY_BYTES_PER_PIXEL;
    p_vout->i_bytes_per_line   = DUMMY_WIDTH * DUMMY_BYTES_PER_PIXEL;

    p_vout->p_sys->i_page_size = DUMMY_WIDTH * DUMMY_HEIGHT
                                  * DUMMY_BYTES_PER_PIXEL;

    /* Map two framebuffers a the very beginning of the fb */
    p_vout->p_sys->p_video = malloc( 2 * p_vout->p_sys->i_page_size );
    if( p_vout->p_sys->p_video == NULL )
    {
        intf_ErrMsg( "vout error: can't map video memory (%s)",
                     strerror(errno) );
        free( p_vout->p_sys );
        return( 1 );
    }

    /* Set and initialize buffers */
    p_vout->pf_setbuffers( p_vout, p_vout->p_sys->p_video,
                     p_vout->p_sys->p_video + p_vout->p_sys->i_page_size );

    return( 0 );
}

/*****************************************************************************
 * vout_Init: initialize dummy video thread output method
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
    /* create the MGA output */
    p_vout->p_sys->mga.src_width = p_vout->i_width;
    p_vout->p_sys->mga.src_height = p_vout->i_height;
    /* FIXME: we should initialize these ones according to the streams */
    p_vout->p_sys->mga.dest_width = p_vout->i_width;
    p_vout->p_sys->mga.dest_height = p_vout->i_height;
    //p_vout->p_sys->mga?dest_width = 400;
    //p_vout->p_sys->mga.dest_height = 300;
    p_vout->p_sys->mga.x_org = 100;
    p_vout->p_sys->mga.y_org = 100;
    p_vout->p_sys->mga.colkey_on = 0;

    if( ioctl(p_vout->p_sys->i_fd, MGA_VID_CONFIG, &p_vout->p_sys->mga) )
    {
        intf_ErrMsg("error in config ioctl");
    }

    if (p_vout->p_sys->mga.card_type == MGA_G200)
    {
        intf_Msg( "vout: detected MGA G200 (%d MB Ram)",
                  p_vout->p_sys->mga.ram_size );
        p_vout->p_sys->b_g400 = 0;
    }
    else
    {
        intf_Msg( "vout: detected MGA G400 (%d MB Ram)",
                  p_vout->p_sys->mga.ram_size );
        p_vout->p_sys->b_g400 = 1;
    }

    ioctl( p_vout->p_sys->i_fd, MGA_VID_ON, 0 );

    p_vout->p_sys->i_size = ( (p_vout->p_sys->mga.src_width + 31) & ~31 )
                             * p_vout->p_sys->mga.src_height;

    p_vout->p_sys->p_mga_vid_base = mmap( 0, p_vout->p_sys->i_size
                                             + p_vout->p_sys->i_size / 2,
                                          PROT_WRITE, MAP_SHARED,
                                          p_vout->p_sys->i_fd, 0 );

    memset( p_vout->p_sys->p_mga_vid_base,
            0x00, p_vout->p_sys->i_size );

    memset( p_vout->p_sys->p_mga_vid_base + p_vout->p_sys->i_size,
            0x80, p_vout->p_sys->i_size / 2 );

    return( 0 );
}

/*****************************************************************************
 * vout_End: terminate dummy video thread output method
 *****************************************************************************/
static void vout_End( vout_thread_t *p_vout )
{
    ioctl( p_vout->p_sys->i_fd, MGA_VID_OFF, 0 );
}

/*****************************************************************************
 * vout_Destroy: destroy dummy video thread output method
 *****************************************************************************
 * Terminate an output method created by DummyCreateOutputMethod
 *****************************************************************************/
static void vout_Destroy( vout_thread_t *p_vout )
{
    close( p_vout->p_sys->i_fd );

    free( p_vout->p_sys->p_video );
    free( p_vout->p_sys );
}

/*****************************************************************************
 * vout_Manage: handle dummy events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * console events. It returns a non null value on error.
 *****************************************************************************/
static int vout_Manage( vout_thread_t *p_vout )
{
    return( 0 );
}

/*****************************************************************************
 * vout_Display: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to dummy image, waits until
 * it is displayed and switch the two rendering buffers, preparing next frame.
 *****************************************************************************/
static void vout_Display( vout_thread_t *p_vout )
{
    ;
}

