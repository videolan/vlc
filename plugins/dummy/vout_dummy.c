/*****************************************************************************
 * vout_dummy.c: Dummy video output display method for testing purposes
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: vout_dummy.c,v 1.20 2002/03/26 23:08:40 gbazin Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

#include <videolan/vlc.h>

#include "video.h"
#include "video_output.h"

#define DUMMY_WIDTH 16
#define DUMMY_HEIGHT 16
#define DUMMY_MAX_DIRECTBUFFERS 10

/*****************************************************************************
 * vout_sys_t: dummy video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the dummy specific properties of an output thread.
 *****************************************************************************/
typedef struct vout_sys_s
{
    /* Nothing needed here. Maybe stats ? */

    /* Prevent malloc(0) */
    int i_dummy;

} vout_sys_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  vout_Create    ( struct vout_thread_s * );
static int  vout_Init      ( struct vout_thread_s * );
static void vout_End       ( struct vout_thread_s * );
static void vout_Destroy   ( struct vout_thread_s * );
static int  vout_Manage    ( struct vout_thread_s * );
static void vout_Render    ( struct vout_thread_s *, struct picture_s * );
static void vout_Display   ( struct vout_thread_s *, struct picture_s * );

static int  DummyNewPicture( struct vout_thread_s *, struct picture_s * );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( vout_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->functions.vout.pf_create     = vout_Create;
    p_function_list->functions.vout.pf_init       = vout_Init;
    p_function_list->functions.vout.pf_end        = vout_End;
    p_function_list->functions.vout.pf_destroy    = vout_Destroy;
    p_function_list->functions.vout.pf_manage     = vout_Manage;
    p_function_list->functions.vout.pf_render     = vout_Render;
    p_function_list->functions.vout.pf_display    = vout_Display;
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
        intf_ErrMsg("error: %s", strerror(ENOMEM) );
        return( 1 );
    }

    return( 0 );
}

/*****************************************************************************
 * vout_Init: initialize dummy video thread output method
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic;
    
    I_OUTPUTPICTURES = 0;

    /* Initialize the output structure */
    switch( p_vout->render.i_chroma )
    {
        case FOURCC_I420:
        case FOURCC_IYUV:
        case FOURCC_YV12:
            p_vout->output.i_chroma = p_vout->render.i_chroma;
            p_vout->output.i_width  = p_vout->render.i_width;
            p_vout->output.i_height = p_vout->render.i_height;
            p_vout->output.i_aspect = p_vout->render.i_aspect;
            break;

        default:
            p_vout->output.i_chroma = FOURCC_RV16;
            p_vout->output.i_rmask  = 0xf800;
            p_vout->output.i_gmask  = 0x07e0;
            p_vout->output.i_bmask  = 0x001f;
            p_vout->output.i_width  = p_vout->render.i_width;
            p_vout->output.i_height = p_vout->render.i_height;
            p_vout->output.i_aspect = p_vout->render.i_aspect;
            break;
    }

    /* Try to initialize DUMMY_MAX_DIRECTBUFFERS direct buffers */
    while( I_OUTPUTPICTURES < DUMMY_MAX_DIRECTBUFFERS )
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
        if( p_pic == NULL || DummyNewPicture( p_vout, p_pic ) )
        {
            break;
        }

        p_pic->i_status = DESTROYED_PICTURE;
        p_pic->i_type   = DIRECT_PICTURE;

        PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;

        I_OUTPUTPICTURES++;
    }

    return( 0 );
}

/*****************************************************************************
 * vout_End: terminate dummy video thread output method
 *****************************************************************************/
static void vout_End( vout_thread_t *p_vout )
{
    int i_index;

    /* Free the fake output buffers we allocated */
    for( i_index = I_OUTPUTPICTURES ; i_index ; )
    {
        i_index--;
        free( PP_OUTPUTPICTURE[ i_index ]->p_data );
    }
}

/*****************************************************************************
 * vout_Destroy: destroy dummy video thread output method
 *****************************************************************************
 * Terminate an output method created by DummyCreateOutputMethod
 *****************************************************************************/
static void vout_Destroy( vout_thread_t *p_vout )
{
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
 * vout_Render: render previously calculated output
 *****************************************************************************/
static void vout_Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    /* No need to do anything, the fake direct buffers stay as they are */
}

/*****************************************************************************
 * vout_Display: displays previously rendered output
 *****************************************************************************/
static void vout_Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    /* No need to do anything, the fake direct buffers stay as they are */
}

/*****************************************************************************
 * DummyNewPicture: allocate a picture
 *****************************************************************************
 * Returns 0 on success, -1 otherwise
 *****************************************************************************/
static int DummyNewPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    int i_width  = p_vout->output.i_width;
    int i_height = p_vout->output.i_height;

    switch( p_vout->output.i_chroma )
    {
    /* We know this chroma, allocate a buffer which will be used
     * directly by the decoder */
    case FOURCC_I420:
    case FOURCC_IYUV:
    case FOURCC_YV12:

        /* Allocate the memory buffer */
        p_pic->p_data = memalign( 16, i_width * i_height * 3 / 2 );

        /* Y buffer */
        p_pic->Y_PIXELS = p_pic->p_data;
        p_pic->p[Y_PLANE].i_lines = i_height;
        p_pic->p[Y_PLANE].i_pitch = i_width;
        p_pic->p[Y_PLANE].i_pixel_bytes = 1;
        p_pic->p[Y_PLANE].b_margin = 0;

        /* U buffer */
        p_pic->U_PIXELS = p_pic->Y_PIXELS + i_height * i_width;
        p_pic->p[U_PLANE].i_lines = i_height / 2;
        p_pic->p[U_PLANE].i_pitch = i_width / 2;
        p_pic->p[U_PLANE].i_pixel_bytes = 1;
        p_pic->p[U_PLANE].b_margin = 0;

        /* V buffer */
        p_pic->V_PIXELS = p_pic->U_PIXELS + i_height * i_width / 4;
        p_pic->p[V_PLANE].i_lines = i_height / 2;
        p_pic->p[V_PLANE].i_pitch = i_width / 2;
        p_pic->p[V_PLANE].i_pixel_bytes = 1;
        p_pic->p[V_PLANE].b_margin = 0;

        /* We allocated 3 planes */
        p_pic->i_planes = 3;

        return( 0 );
        break;

    /* Unknown chroma, allocate an RGB buffer, the video output's job
     * will be to do the chroma->RGB conversion */
    case FOURCC_RV16:

        /* Allocate the memory buffer */
        p_pic->p_data = memalign( 16, i_width * i_height * 2 );

        /* Fill important structures */
        p_pic->p->p_pixels = p_pic->p_data;
        p_pic->p->i_lines = i_height;
        p_pic->p->i_pitch = i_width;
        p_pic->p->i_pixel_bytes = 2;
        p_pic->p->b_margin = 0;

        /* We allocated 1 plane */
        p_pic->i_planes = 1;

        return( 0 );
        break;

    default:
        p_pic->i_planes = 0;
        return( 0 );
        break;
    }
}

