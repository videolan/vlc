/*****************************************************************************
 * wall.c : Wall video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: wall.c,v 1.1 2001/12/17 03:38:21 sam Exp $
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

#define MODULE_NAME filter_wall
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <errno.h>
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include "common.h"                                     /* boolean_t, byte_t */
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "video.h"
#include "video_output.h"

#include "modules.h"
#include "modules_export.h"

#define WALL_MAX_DIRECTBUFFERS 8

/*****************************************************************************
 * Capabilities defined in the other files.
 *****************************************************************************/
static void vout_getfunctions( function_list_t * p_function_list );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
ADD_WINDOW( "Configuration for Wall module" )
    ADD_COMMENT( "Ha, ha -- nothing to configure yet" )
MODULE_CONFIG_STOP

MODULE_INIT_START
    p_module->i_capabilities = MODULE_CAPABILITY_NULL
                                | MODULE_CAPABILITY_VOUT;
    p_module->psz_longname = "image wall video module";
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    vout_getfunctions( &p_module->p_functions->vout );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * vout_sys_t: Wall video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the Wall specific properties of an output thread.
 *****************************************************************************/
typedef struct vout_sys_s
{
    struct vout_thread_s *p_vout_top;
    struct vout_thread_s *p_vout_bottom;

} vout_sys_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  vout_Probe     ( probedata_t *p_data );
static int  vout_Create    ( struct vout_thread_s * );
static int  vout_Init      ( struct vout_thread_s * );
static void vout_End       ( struct vout_thread_s * );
static void vout_Destroy   ( struct vout_thread_s * );
static int  vout_Manage    ( struct vout_thread_s * );
static void vout_Display   ( struct vout_thread_s *, struct picture_s * );

static int  WallNewPicture ( struct vout_thread_s *, struct picture_s * );

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
    p_function_list->functions.vout.pf_display    = vout_Display;
    p_function_list->functions.vout.pf_setpalette = NULL;
}

/*****************************************************************************
 * intf_Probe: return a score
 *****************************************************************************/
static int vout_Probe( probedata_t *p_data )
{
    if( TestMethod( VOUT_FILTER_VAR, "wall" ) )
    {
        return( 999 );
    }

    /* If we weren't asked to filter, don't filter. */
    return( 0 );
}

/*****************************************************************************
 * vout_Create: allocates Wall video thread output method
 *****************************************************************************
 * This function allocates and initializes a Wall vout method.
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
 * vout_Init: initialize Wall video thread output method
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
    int i_index;
    char *psz_filter;
    picture_t *p_pic;
    
    I_OUTPUTPICTURES = 0;

    /* Try to open the real video output */
    psz_filter = main_GetPszVariable( VOUT_FILTER_VAR, "" );
    main_PutPszVariable( VOUT_FILTER_VAR, "" );

    intf_WarnMsg( 1, "filter: spawning the real video outputs" );

    p_vout->p_sys->p_vout_top =
        vout_CreateThread( NULL,
                           p_vout->render.i_width, p_vout->render.i_height / 2,
                           p_vout->render.i_chroma, p_vout->render.i_aspect * 2);

    /* Everything failed */
    if( p_vout->p_sys->p_vout_top == NULL )
    {
        intf_ErrMsg( "filter error: can't open top vout, aborting" );

        return( 0 );
    }

    p_vout->p_sys->p_vout_bottom =
        vout_CreateThread( NULL,
                           p_vout->render.i_width, p_vout->render.i_height / 2,
                           p_vout->render.i_chroma, p_vout->render.i_aspect * 2 );

    /* Everything failed */
    if( p_vout->p_sys->p_vout_bottom == NULL )
    {
        intf_ErrMsg( "filter error: can't open bottom vout, aborting" );
        vout_DestroyThread( p_vout->p_sys->p_vout_top, NULL );

        return( 0 );
    }
 
    main_PutPszVariable( VOUT_FILTER_VAR, psz_filter );

    /* Initialize the output structure */
    switch( p_vout->render.i_chroma )
    {
        case YUV_420_PICTURE:
            p_vout->output.i_chroma = p_vout->render.i_chroma;
            p_vout->output.i_width  = p_vout->render.i_width;
            p_vout->output.i_height = p_vout->render.i_height;
            p_vout->output.i_aspect = p_vout->render.i_aspect;
            break;

        default:
            p_vout->output.i_chroma = EMPTY_PICTURE; /* unknown chroma */
            break;
    }

    /* Try to initialize WALL_MAX_DIRECTBUFFERS direct buffers */
    while( I_OUTPUTPICTURES < WALL_MAX_DIRECTBUFFERS )
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
        if( WallNewPicture( p_vout, p_pic ) )
        {
            break;
        }

        p_pic->i_status        = DESTROYED_PICTURE;
        p_pic->i_type          = DIRECT_PICTURE;

        p_pic->i_left_margin   =
        p_pic->i_right_margin  =
        p_pic->i_top_margin    =
        p_pic->i_bottom_margin = 0;

        PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;

        I_OUTPUTPICTURES++;
    }

    return( 0 );
}

/*****************************************************************************
 * vout_End: terminate Wall video thread output method
 *****************************************************************************/
static void vout_End( vout_thread_t *p_vout )
{
    int i_index;

    /* Free the fake output buffers we allocated */
    for( i_index = I_OUTPUTPICTURES ; i_index ; )
    {
        i_index--;
        free( PP_OUTPUTPICTURE[ i_index ]->planes[ 0 ].p_data );
    }
}

/*****************************************************************************
 * vout_Destroy: destroy Wall video thread output method
 *****************************************************************************
 * Terminate an output method created by WallCreateOutputMethod
 *****************************************************************************/
static void vout_Destroy( vout_thread_t *p_vout )
{
    vout_DestroyThread( p_vout->p_sys->p_vout_top, NULL );
    vout_DestroyThread( p_vout->p_sys->p_vout_bottom, NULL );

    free( p_vout->p_sys );
}

/*****************************************************************************
 * vout_Manage: handle Wall events
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
 * This function send the currently rendered image to Wall image, waits
 * until it is displayed and switch the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static void vout_Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t *p_outpic_top, *p_outpic_bottom;
    int i_index;
    mtime_t i_date = mdate() + 50000;

    while( ( p_outpic_top
              = vout_CreatePicture( p_vout->p_sys->p_vout_top, 0, 0, 0 ) )
            == NULL )
    {
        if( p_vout->b_die || p_vout->b_error )
        {
            return;
        }
        msleep( VOUT_OUTMEM_SLEEP );
    }   

    while( ( p_outpic_bottom
              = vout_CreatePicture( p_vout->p_sys->p_vout_bottom, 0, 0, 0 ) )
            == NULL )
    {
        if( p_vout->b_die || p_vout->b_error )
        {
            vout_DestroyPicture( p_vout->p_sys->p_vout_top, p_outpic_top );
            return;
        }
        msleep( VOUT_OUTMEM_SLEEP );
    }   

    vout_DatePicture( p_vout->p_sys->p_vout_top, p_outpic_top, i_date );
    vout_DatePicture( p_vout->p_sys->p_vout_bottom, p_outpic_bottom, i_date );

    vout_LinkPicture( p_vout->p_sys->p_vout_top, p_outpic_top );
    vout_LinkPicture( p_vout->p_sys->p_vout_bottom, p_outpic_bottom );

    for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
    {
        p_main->fast_memcpy( p_outpic_top->planes[ i_index ].p_data,
                             p_pic->planes[ i_index ].p_data,
                             p_pic->planes[ i_index ].i_bytes / 2 );

        p_main->fast_memcpy( p_outpic_bottom->planes[ i_index ].p_data,
                             p_pic->planes[ i_index ].p_data
                              + p_pic->planes[ i_index ].i_bytes / 2,
                             p_pic->planes[ i_index ].i_bytes / 2 );
    }

    vout_UnlinkPicture( p_vout->p_sys->p_vout_top, p_outpic_top );
    vout_UnlinkPicture( p_vout->p_sys->p_vout_bottom, p_outpic_bottom );

    vout_DisplayPicture( p_vout->p_sys->p_vout_top, p_outpic_top );
    vout_DisplayPicture( p_vout->p_sys->p_vout_bottom, p_outpic_bottom );
}


/*****************************************************************************
 * WallNewPicture: allocate a picture
 *****************************************************************************
 * Returns 0 on success, -1 otherwise
 *****************************************************************************/
static int WallNewPicture( vout_thread_t *p_vout, picture_t *p_pic )
{
    int i_luma_bytes, i_chroma_bytes;

    int i_width  = p_vout->output.i_width;
    int i_height = p_vout->output.i_height;

    switch( p_vout->output.i_chroma )
    {
    /* We know this chroma, allocate a buffer which will be used
     * directly by the decoder */
    case YUV_420_PICTURE:

        /* Precalculate some values */
        p_pic->i_size         = i_width * i_height;
        p_pic->i_chroma_width = i_width / 2;
        p_pic->i_chroma_size  = i_height * ( i_width / 2 );

        /* Allocate the memory buffer */
        i_luma_bytes = p_pic->i_size * sizeof(pixel_data_t);
        i_chroma_bytes = p_pic->i_chroma_size * sizeof(pixel_data_t);

        /* Y buffer */
        p_pic->planes[ Y_PLANE ].p_data = malloc( i_luma_bytes + 2 * i_chroma_bytes );
        p_pic->planes[ Y_PLANE ].i_bytes = i_luma_bytes;
        p_pic->planes[ Y_PLANE ].i_line_bytes = i_width * sizeof(pixel_data_t);

        /* U buffer */
        p_pic->planes[ U_PLANE ].p_data = p_pic->planes[ Y_PLANE ].p_data + i_height * i_width;
        p_pic->planes[ U_PLANE ].i_bytes = i_chroma_bytes / 2;
        p_pic->planes[ U_PLANE ].i_line_bytes = p_pic->i_chroma_width * sizeof(pixel_data_t);

        /* V buffer */
        p_pic->planes[ V_PLANE ].p_data = p_pic->planes[ U_PLANE ].p_data + i_height * p_pic->i_chroma_width;
        p_pic->planes[ V_PLANE ].i_bytes = i_chroma_bytes / 2;
        p_pic->planes[ V_PLANE ].i_line_bytes = p_pic->i_chroma_width * sizeof(pixel_data_t);

        /* We allocated 3 planes */
        p_pic->i_planes = 3;

        return( 0 );
        break;

    /* Unknown chroma, do nothing */
    default:

        return( 0 );
        break;
    }
}

