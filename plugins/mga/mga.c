/*****************************************************************************
 * mga.c : Matrox Graphic Array plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: mga.c,v 1.10 2002/01/05 03:49:18 sam Exp $
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

/*****************************************************************************
 * Capabilities defined in the other files.
 *****************************************************************************/
static void vout_getfunctions( function_list_t * p_function_list );

static int  vout_Probe     ( probedata_t *p_data );
static int  vout_Create    ( vout_thread_t * );
static int  vout_Init      ( vout_thread_t * );
static void vout_End       ( vout_thread_t * );
static void vout_Destroy   ( vout_thread_t * );
static int  vout_Manage    ( vout_thread_t * );
static void vout_Render    ( vout_thread_t *, picture_t * );
static void vout_Display   ( vout_thread_t *, picture_t * );

/*****************************************************************************
 * Building configuration tree
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( "Matrox Graphic Array video module" )
    ADD_CAPABILITY( VOUT, 10 )
    ADD_SHORTCUT( "mga" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    vout_getfunctions( &p_module->p_functions->vout );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

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
/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static void vout_getfunctions( function_list_t * p_function_list )
{
    p_function_list->pf_probe = vout_Probe;
    p_function_list->functions.vout.pf_create     = vout_Create;
    p_function_list->functions.vout.pf_init       = vout_Init;
    p_function_list->functions.vout.pf_end        = vout_End;
    p_function_list->functions.vout.pf_destroy    = vout_Destroy;
    p_function_list->functions.vout.pf_manage     = vout_Manage;
    p_function_list->functions.vout.pf_render     = vout_Render;
    p_function_list->functions.vout.pf_display    = vout_Display;
}

/*****************************************************************************
 * intf_Probe: return a score
 *****************************************************************************/
static int vout_Probe( probedata_t *p_data )
{
    int i_fd;

    i_fd = open( "/dev/mga_vid", O_RDWR );

    if( i_fd == -1 )
    {
        return 0;
    }

    close( i_fd );

    return 10;
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

    return( 0 );
}

/*****************************************************************************
 * vout_Init: initialize dummy video thread output method
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
    /* create the MGA output */
    p_vout->output.i_width = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    /* FIXME: we should initialize these ones according to the streams */
    p_vout->p_sys->mga.src_width = p_vout->output.i_width;
    p_vout->p_sys->mga.src_height = p_vout->output.i_height;
    p_vout->p_sys->mga.dest_width = 900;
    p_vout->p_sys->mga.dest_height = 700;
    p_vout->p_sys->mga.x_org = 50;
    p_vout->p_sys->mga.y_org = 50;
    p_vout->p_sys->mga.colkey_on = 0;

    if( ioctl(p_vout->p_sys->i_fd, MGA_VID_CONFIG, &p_vout->p_sys->mga) )
    {
        intf_ErrMsg( "vout error: MGA config ioctl failed" );
    }

    if( p_vout->p_sys->mga.card_type == MGA_G200 )
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
 * vout_Render: displays previously calculated output
 *****************************************************************************/
static void vout_Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    ;
}

/*****************************************************************************
 * vout_Display: displays previously rendered output
 *****************************************************************************/
static void vout_Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    ;
}

