/*****************************************************************************
 * glide.c : 3dfx Glide plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: glide.c,v 1.17 2002/07/23 00:39:17 sam Exp $
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/vout.h>

#ifndef __linux__
#   include <conio.h>                                         /* for glide ? */
#endif
#include <glide.h>
#include <linutil.h>                            /* Glide kbhit() and getch() */

#define GLIDE_WIDTH 800
#define GLIDE_HEIGHT 600
#define GLIDE_BITS_PER_PLANE 16
#define GLIDE_BYTES_PER_PIXEL 2

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void vout_getfunctions( function_list_t * p_function_list );

static int  vout_Create    ( vout_thread_t * );
static int  vout_Init      ( vout_thread_t * );
static void vout_End       ( vout_thread_t * );
static void vout_Destroy   ( vout_thread_t * );
static int  vout_Manage    ( vout_thread_t * );
static void vout_Render    ( vout_thread_t *, picture_t * );
static void vout_Display   ( vout_thread_t *, picture_t * );

static int  OpenDisplay    ( vout_thread_t * );
static void CloseDisplay   ( vout_thread_t * );

/*****************************************************************************
 * Building configuration tree
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( _("3dfx Glide module") )
    ADD_CAPABILITY( VOUT, 20 )
    ADD_SHORTCUT( "3dfx" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    vout_getfunctions( &p_module->p_functions->vout );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * vout_sys_t: Glide video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the Glide specific properties of an output thread.
 *****************************************************************************/
struct vout_sys_t
{
    GrLfbInfo_t                 p_buffer_info;           /* back buffer info */

    u8* pp_buffer[2];
    int i_index;
};

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
 * vout_Create: allocates Glide video thread output method
 *****************************************************************************
 * This function allocates and initializes a Glide vout method.
 *****************************************************************************/
int vout_Create( vout_thread_t *p_vout )
{
    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return( 1 );
    }

    /* Open and initialize device */
    if( OpenDisplay( p_vout ) )
    {
        msg_Err( p_vout, "cannot open display" );
        free( p_vout->p_sys );
        return( 1 );
    }

    return( 0 );
}

/*****************************************************************************
 * vout_Init: initialize Glide video thread output method
 *****************************************************************************/
int vout_Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic;

    /* FIXME: we don't set i_chroma !! */
    p_vout->output.i_rmask = 0xf800;
    p_vout->output.i_gmask = 0x07e0;
    p_vout->output.i_bmask = 0x001f;

    I_OUTPUTPICTURES = 0;

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

    if( p_pic == NULL )
    {
        return -1;
    }

    /* We know the chroma, allocate a buffer which will be used
     * directly by the decoder */
    p_pic->i_planes = 1;

    p_pic->p->p_pixels = p_vout->p_sys->pp_buffer[p_vout->p_sys->i_index];
    p_pic->p->i_lines = GLIDE_HEIGHT;
    p_pic->p->i_pitch = p_vout->p_sys->p_buffer_info.strideInBytes;
                         /*1024 * GLIDE_BYTES_PER_PIXEL*/
    p_pic->p->i_pixel_pitch = GLIDE_BYTES_PER_PIXEL;
    p_pic->p->i_visible_pitch = GLIDE_WIDTH * GLIDE_BYTES_PER_PIXEL;

    p_pic->i_status = DESTROYED_PICTURE;
    p_pic->i_type   = DIRECT_PICTURE;

    PP_OUTPUTPICTURE[ 0 ] = p_pic;

    I_OUTPUTPICTURES = 1;

    return 0;
}

/*****************************************************************************
 * vout_End: terminate Glide video thread output method
 *****************************************************************************/
void vout_End( vout_thread_t *p_vout )
{
    ;
}

/*****************************************************************************
 * vout_Destroy: destroy Glide video thread output method
 *****************************************************************************
 * Terminate an output method created by vout_CreateOutputMethod
 *****************************************************************************/
void vout_Destroy( vout_thread_t *p_vout )
{
    CloseDisplay( p_vout );
    free( p_vout->p_sys );
}

/*****************************************************************************
 * vout_Manage: handle Glide events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * console events. It returns a non null value on error.
 *****************************************************************************/
int vout_Manage( vout_thread_t *p_vout )
{
    int buf;

    /* very Linux specific - see tlib.c in Glide for other versions */
    while( kbhit() )
    {
        buf = getch();

        switch( (char)buf )
        {
        case 'q':
            p_vout->p_vlc->b_die = 1;
            break;

        default:
            break;
        }
    }

    return 0;
}

/*****************************************************************************
 * vout_Render: renders previously calculated output
 *****************************************************************************/
void vout_Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    ;
}
        
/*****************************************************************************
 * vout_Display: displays previously rendered output
 *****************************************************************************/
void vout_Display( vout_thread_t *p_vout, picture_t *p_pic )

{
    grLfbUnlock( GR_LFB_WRITE_ONLY, GR_BUFFER_BACKBUFFER );

    grBufferSwap( 0 );

    if ( grLfbLock(GR_LFB_WRITE_ONLY, GR_BUFFER_BACKBUFFER,
                   GR_LFBWRITEMODE_565, GR_ORIGIN_UPPER_LEFT, FXFALSE,
                   &p_vout->p_sys->p_buffer_info) == FXFALSE )
    {
        msg_Err( p_vout, "cannot take 3dfx back buffer lock" );
    }
}

/* following functions are local */

/*****************************************************************************
 * OpenDisplay: open and initialize 3dfx device
 *****************************************************************************/

static int OpenDisplay( vout_thread_t *p_vout )
{
    static char version[80];
    GrHwConfiguration hwconfig;
    GrScreenResolution_t resolution = GR_RESOLUTION_800x600;
    GrLfbInfo_t p_front_buffer_info;                    /* front buffer info */

    grGlideGetVersion( version );
    grGlideInit();

    if( !grSstQueryHardware(&hwconfig) )
    {
        msg_Err( p_vout, "cannot get 3dfx hardware config" );
        return( 1 );
    }

    grSstSelect( 0 );
    if( !grSstWinOpen( 0, resolution, GR_REFRESH_60Hz,
                       GR_COLORFORMAT_ABGR, GR_ORIGIN_UPPER_LEFT, 2, 1 ) )
    {
        msg_Err( p_vout, "cannot open 3dfx screen" );
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
        msg_Err( p_vout, "cannot take 3dfx front buffer lock" );
        grGlideShutdown();
        return( 1 );
    }
    grLfbUnlock( GR_LFB_WRITE_ONLY, GR_BUFFER_FRONTBUFFER );

    if ( grLfbLock(GR_LFB_WRITE_ONLY, GR_BUFFER_BACKBUFFER,
                   GR_LFBWRITEMODE_565, GR_ORIGIN_UPPER_LEFT, FXFALSE,
                   &p_vout->p_sys->p_buffer_info) == FXFALSE )
    {
        msg_Err( p_vout, "cannot take 3dfx back buffer lock" );
        grGlideShutdown();
        return( 1 );
    }
    grLfbUnlock( GR_LFB_WRITE_ONLY, GR_BUFFER_BACKBUFFER );
    
    grBufferClear( 0, 0, 0 );

    p_vout->p_sys->pp_buffer[0] = p_vout->p_sys->p_buffer_info.lfbPtr;
    p_vout->p_sys->pp_buffer[1] = p_front_buffer_info.lfbPtr;
    p_vout->p_sys->i_index = 0;

    return( 0 );
}

/*****************************************************************************
 * CloseDisplay: close and reset 3dfx device
 *****************************************************************************
 * Returns all resources allocated by OpenDisplay and restore the original
 * state of the device.
 *****************************************************************************/
static void CloseDisplay( vout_thread_t *p_vout )
{
    /* unlock the hidden buffer */
    grLfbUnlock( GR_LFB_WRITE_ONLY, GR_BUFFER_BACKBUFFER );

    /* shutdown Glide */
    grGlideShutdown();
}

