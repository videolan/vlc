/*****************************************************************************
 * mga.c : Matrox Graphic Array plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 the VideoLAN team
 * $Id$
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

#include <vlc/vlc.h>
#include <vlc/vout.h>

#ifdef SYS_BSD
#include <sys/types.h>                                     /* typedef ushort */
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static void Display   ( vout_thread_t *, picture_t * );

static int  NewPicture     ( vout_thread_t *, picture_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Matrox Graphic Array video output") );
    set_capability( "video output", 10 );
    set_callbacks( Create, Destroy );
vlc_module_end();

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
#   define MGA_VID_FSEL   _IOR('J', 4, int)
#   define MGA_G200 0x1234
#   define MGA_G400 0x5678

#   define MGA_VID_FORMAT_YV12 0x32315659
#   define MGA_VID_FORMAT_IYUV (('I'<<24)|('Y'<<16)|('U'<<8)|'V')
#   define MGA_VID_FORMAT_I420 (('I'<<24)|('4'<<16)|('2'<<8)|'0')
#   define MGA_VID_FORMAT_YUY2 (('Y'<<24)|('U'<<16)|('Y'<<8)|'2')
#   define MGA_VID_FORMAT_UYVY (('U'<<24)|('Y'<<16)|('V'<<8)|'Y')

#   define MGA_VID_VERSION     0x0201

#   define MGA_NUM_FRAMES      1

typedef struct mga_vid_config_t
{
    uint16_t version;
    uint16_t card_type;
    uint32_t ram_size;
    uint32_t src_width;
    uint32_t src_height;
    uint32_t dest_width;
    uint32_t dest_height;
    uint32_t x_org;
    uint32_t y_org;
    uint8_t  colkey_on;
    uint8_t  colkey_red;
    uint8_t  colkey_green;
    uint8_t  colkey_blue;
    uint32_t format;
    uint32_t frame_size;
    uint32_t num_frames;
} mga_vid_config_t;
#endif

struct vout_sys_t
{
    mga_vid_config_t    mga;
    int                 i_fd;
    byte_t *            p_video;
};

struct picture_sys_t
{
    int     i_frame;
};

#define CEIL32(x) (((x)+31)&~31)

/*****************************************************************************
 * Create: allocates dummy video thread output method
 *****************************************************************************
 * This function allocates and initializes a dummy vout method.
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return( 1 );
    }

    p_vout->p_sys->i_fd = open( "/dev/mga_vid", O_RDWR );
    if( p_vout->p_sys->i_fd == -1 )
    {
        msg_Err( p_vout, "cannot open MGA driver /dev/mga_vid" );
        free( p_vout->p_sys );
        return( 1 );
    }

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = NULL;
    p_vout->pf_render = NULL;
    p_vout->pf_display = Display;

    return( 0 );
}

/*****************************************************************************
 * Init: initialize dummy video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic;

    I_OUTPUTPICTURES = 0;

    /* create the MGA output */
    p_vout->output.i_width = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    /* Set coordinates and aspect ratio */
    p_vout->p_sys->mga.src_width = CEIL32(p_vout->output.i_width);
    p_vout->p_sys->mga.src_height = p_vout->output.i_height;
    vout_PlacePicture( p_vout, 1024, 768,
                       &p_vout->p_sys->mga.x_org, &p_vout->p_sys->mga.y_org,
                       &p_vout->p_sys->mga.dest_width,
                       &p_vout->p_sys->mga.dest_height );

    /* Initialize a video buffer */
    p_vout->p_sys->mga.colkey_on = 0;
    p_vout->p_sys->mga.num_frames = MGA_NUM_FRAMES;
    p_vout->p_sys->mga.frame_size = CEIL32(p_vout->output.i_width)
                                     * p_vout->output.i_height * 2;
    p_vout->p_sys->mga.version = MGA_VID_VERSION;

    /* Assume we only do YMGA for the moment. XXX: mga_vid calls this
     * YV12, but it's actually some strange format with packed UV. */
    p_vout->output.i_chroma = VLC_FOURCC('Y','M','G','A');
    p_vout->p_sys->mga.format = MGA_VID_FORMAT_YV12;

    if( ioctl(p_vout->p_sys->i_fd, MGA_VID_CONFIG, &p_vout->p_sys->mga) )
    {
        msg_Err( p_vout, "MGA config ioctl failed" );
        return -1;
    }

    if( p_vout->p_sys->mga.card_type == MGA_G200 )
    {
        msg_Dbg( p_vout, "detected MGA G200 (%d MB Ram)",
                         p_vout->p_sys->mga.ram_size );
    }
    else
    {
        msg_Dbg( p_vout, "detected MGA G400/G450 (%d MB Ram)",
                         p_vout->p_sys->mga.ram_size );
    }

    p_vout->p_sys->p_video = mmap( 0, p_vout->p_sys->mga.frame_size
                                       * MGA_NUM_FRAMES,
                                   PROT_WRITE, MAP_SHARED,
                                   p_vout->p_sys->i_fd, 0 );

    /* Try to initialize up to MGA_NUM_FRAMES direct buffers */
    while( I_OUTPUTPICTURES < MGA_NUM_FRAMES )
    {
        p_pic = NULL;

        /* Find an empty picture slot */
        for( i_index = 0 ; i_index < VOUT_MAX_PICTURES ; i_index++ )
        {
            if( p_vout->p_picture[ i_index ].i_status == FREE_PICTURE )
            {
                p_pic = p_vout->p_picture + i_index;
                break;
            }
        }

        /* Allocate the picture */
        if( p_pic == NULL || NewPicture( p_vout, p_pic ) )
        {
            break;
        }

        p_pic->i_status = DESTROYED_PICTURE;
        p_pic->i_type   = DIRECT_PICTURE;

        PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;

        I_OUTPUTPICTURES++;
    }

    /* Blank the windows */
    for( i_index = 0; i_index < I_OUTPUTPICTURES; i_index++ )
    {
        memset( p_vout->p_sys->p_video
                 + p_vout->p_sys->mga.frame_size * i_index,
                0x00, p_vout->p_sys->mga.frame_size / 2 );
        memset( p_vout->p_sys->p_video
                 + p_vout->p_sys->mga.frame_size * ( 2*i_index + 1 ) / 2,
                0x80, p_vout->p_sys->mga.frame_size / 2 );
    }

    /* Display the image */
    ioctl( p_vout->p_sys->i_fd, MGA_VID_ON, 0 );

    return( 0 );
}

/*****************************************************************************
 * End: terminate dummy video thread output method
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
    int i_index;

    ioctl( p_vout->p_sys->i_fd, MGA_VID_OFF, 0 );

    /* Free the output buffers we allocated */
    for( i_index = I_OUTPUTPICTURES ; i_index ; )
    {
        i_index--;
    }
}

/*****************************************************************************
 * Destroy: destroy dummy video thread output method
 *****************************************************************************
 * Terminate an output method created by DummyCreateOutputMethod
 *****************************************************************************/
static void Destroy( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    close( p_vout->p_sys->i_fd );
    free( p_vout->p_sys );
}

/*****************************************************************************
 * Display: displays previously rendered output
 *****************************************************************************/
static void Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    ioctl( p_vout->p_sys->i_fd, MGA_VID_FSEL, &p_pic->p_sys->i_frame );
}

/* Following functions are local */

/*****************************************************************************
 * NewPicture: allocate a picture
 *****************************************************************************
 * Returns 0 on success, -1 otherwise
 *****************************************************************************/
static int NewPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    /* We know the chroma, allocate a buffer which will be used
     * directly by the decoder */
    p_pic->p_data = p_vout->p_sys->p_video + I_OUTPUTPICTURES
                                              * p_vout->p_sys->mga.frame_size;

    p_pic->p_sys = malloc( sizeof( picture_sys_t ) );

    if( p_pic->p_sys == NULL )
    {
        return -1;
    }

    p_pic->Y_PIXELS = p_pic->p_data;
    p_pic->p[Y_PLANE].i_lines = p_vout->output.i_height;
    p_pic->p[Y_PLANE].i_visible_lines = p_vout->output.i_height;
    p_pic->p[Y_PLANE].i_pitch = CEIL32( p_vout->output.i_width );
    p_pic->p[Y_PLANE].i_pixel_pitch = 1;
    p_pic->p[Y_PLANE].i_visible_pitch = p_vout->output.i_width;

    p_pic->U_PIXELS = p_pic->p_data + p_vout->p_sys->mga.frame_size * 2/4;
    p_pic->p[U_PLANE].i_lines = p_vout->output.i_height / 2;
    p_pic->p[U_PLANE].i_visible_lines = p_vout->output.i_height / 2;
    p_pic->p[U_PLANE].i_pitch = CEIL32( p_vout->output.i_width ) / 2;
    p_pic->p[U_PLANE].i_pixel_pitch = 1;
    p_pic->p[U_PLANE].i_visible_pitch = p_pic->p[U_PLANE].i_pitch;

    p_pic->V_PIXELS = p_pic->p_data + p_vout->p_sys->mga.frame_size * 3/4;
    p_pic->p[V_PLANE].i_lines = p_vout->output.i_height / 2;
    p_pic->p[V_PLANE].i_visible_lines = p_vout->output.i_height / 2;
    p_pic->p[V_PLANE].i_pitch = CEIL32( p_vout->output.i_width ) / 2;
    p_pic->p[V_PLANE].i_pixel_pitch = 1;
    p_pic->p[V_PLANE].i_visible_pitch = p_pic->p[V_PLANE].i_pitch;

    p_pic->p_sys->i_frame = I_OUTPUTPICTURES;

    p_pic->i_planes = 3;

    return 0;
}

