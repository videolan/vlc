/*****************************************************************************
 * transform.c : transform image plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: transform.c,v 1.1 2001/12/19 03:50:22 sam Exp $
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

#define MODULE_NAME filter_transform
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

#include "filter_common.h"

#include "modules.h"
#include "modules_export.h"

#define TRANSFORM_MODE_HFLIP   1
#define TRANSFORM_MODE_VFLIP   2
#define TRANSFORM_MODE_90      3
#define TRANSFORM_MODE_180     4
#define TRANSFORM_MODE_270     5

/*****************************************************************************
 * Capabilities defined in the other files.
 *****************************************************************************/
static void vout_getfunctions( function_list_t * p_function_list );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
ADD_WINDOW( "Configuration for transform module" )
    ADD_COMMENT( "Ha, ha -- nothing to configure yet" )
MODULE_CONFIG_STOP

MODULE_INIT_START
    p_module->i_capabilities = MODULE_CAPABILITY_NULL
                                | MODULE_CAPABILITY_VOUT;
    p_module->psz_longname = "image transformation module";
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    vout_getfunctions( &p_module->p_functions->vout );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * vout_sys_t: Transform video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the Transform specific properties of an output thread.
 *****************************************************************************/
typedef struct vout_sys_s
{
    int i_mode;
    boolean_t b_rotation;
    struct vout_thread_s *p_vout;

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
    if( TestMethod( VOUT_FILTER_VAR, "transform" ) )
    {
        return( 999 );
    }

    /* If we weren't asked to filter, don't filter. */
    return( 0 );
}

/*****************************************************************************
 * vout_Create: allocates Transform video thread output method
 *****************************************************************************
 * This function allocates and initializes a Transform vout method.
 *****************************************************************************/
static int vout_Create( vout_thread_t *p_vout )
{
    char *psz_method;

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        intf_ErrMsg( "error: %s", strerror(ENOMEM) );
        return( 1 );
    }

    /* Look what method was requested */
    psz_method = main_GetPszVariable( VOUT_FILTER_VAR, "" );

    while( *psz_method && *psz_method != ':' )
    {
        psz_method++;
    }

    if( !strcmp( psz_method, ":hflip" ) )
    {
        p_vout->p_sys->i_mode = TRANSFORM_MODE_HFLIP;
        p_vout->p_sys->b_rotation = 0;
    }
    else if( !strcmp( psz_method, ":vflip" ) )
    {
        p_vout->p_sys->i_mode = TRANSFORM_MODE_VFLIP;
        p_vout->p_sys->b_rotation = 0;
    }
    else if( !strcmp( psz_method, ":90" ) )
    {
        p_vout->p_sys->i_mode = TRANSFORM_MODE_90;
        p_vout->p_sys->b_rotation = 1;
    }
    else if( !strcmp( psz_method, ":180" ) )
    {
        p_vout->p_sys->i_mode = TRANSFORM_MODE_180;
        p_vout->p_sys->b_rotation = 0;
    }
    else if( !strcmp( psz_method, ":270" ) )
    {
        p_vout->p_sys->i_mode = TRANSFORM_MODE_270;
        p_vout->p_sys->b_rotation = 1;
    }
    else
    {
        intf_ErrMsg( "filter error: no valid transform mode provided, "
                     "using transform:90" );
        p_vout->p_sys->i_mode = TRANSFORM_MODE_90;
        p_vout->p_sys->b_rotation = 1;
    }

    return( 0 );
}

/*****************************************************************************
 * vout_Init: initialize Transform video thread output method
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
    int i_index;
    char *psz_filter;
    picture_t *p_pic;
    
    I_OUTPUTPICTURES = 0;

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
            return( 0 ); /* unknown chroma */
            break;
    }

    /* Try to open the real video output */
    psz_filter = main_GetPszVariable( VOUT_FILTER_VAR, "" );
    main_PutPszVariable( VOUT_FILTER_VAR, "" );

    intf_WarnMsg( 1, "filter: spawning the real video output" );

    if( p_vout->p_sys->b_rotation )
    {
        p_vout->p_sys->p_vout =
            vout_CreateThread( NULL,
                           p_vout->render.i_height, p_vout->render.i_width,
                           p_vout->render.i_chroma,
                           (u64)VOUT_ASPECT_FACTOR * (u64)VOUT_ASPECT_FACTOR
                               / (u64)p_vout->render.i_aspect );
    }
    else
    {
        p_vout->p_sys->p_vout =
            vout_CreateThread( NULL,
                           p_vout->render.i_width, p_vout->render.i_height,
                           p_vout->render.i_chroma, p_vout->render.i_aspect );
    }

    /* Everything failed */
    if( p_vout->p_sys->p_vout == NULL )
    {
        intf_ErrMsg( "filter error: can't open vout, aborting" );
        return( 0 );
    }
 
    main_PutPszVariable( VOUT_FILTER_VAR, psz_filter );

    ALLOCATE_DIRECTBUFFERS( VOUT_MAX_PICTURES );

    return( 0 );
}

/*****************************************************************************
 * vout_End: terminate Transform video thread output method
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
 * vout_Destroy: destroy Transform video thread output method
 *****************************************************************************
 * Terminate an output method created by TransformCreateOutputMethod
 *****************************************************************************/
static void vout_Destroy( vout_thread_t *p_vout )
{
    vout_DestroyThread( p_vout->p_sys->p_vout, NULL );

    free( p_vout->p_sys );
}

/*****************************************************************************
 * vout_Manage: handle Transform events
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
 * This function send the currently rendered image to Transform image, waits
 * until it is displayed and switch the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static void vout_Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t *p_outpic;
    int i_index;

    /* This is a new frame. Get a structure from the video_output. */
    while( ( p_outpic = vout_CreatePicture( p_vout->p_sys->p_vout, 0, 0, 0 ) )
              == NULL )
    {
        if( p_vout->b_die || p_vout->b_error )
        {
            return;
        }
        msleep( VOUT_OUTMEM_SLEEP );
    }   

    vout_DatePicture( p_vout->p_sys->p_vout, p_outpic, mdate() + 50000 );
    vout_LinkPicture( p_vout->p_sys->p_vout, p_outpic );

    switch( p_vout->p_sys->i_mode )
    {
        case TRANSFORM_MODE_90:
            for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
            {
                int i_line_bytes = p_pic->planes[ i_index ].i_line_bytes;

                pixel_data_t *p_in = p_pic->planes[ i_index ].p_data;

                pixel_data_t *p_out = p_outpic->planes[ i_index ].p_data;
                pixel_data_t *p_out_end = p_out
                                       + p_outpic->planes[ i_index ].i_bytes;

                for( ; p_out < p_out_end ; )
                {
                    pixel_data_t *p_line_end;

                    p_line_end = p_in + p_pic->planes[ i_index ].i_bytes;

                    for( ; p_in < p_line_end ; )
                    {
                        p_line_end -= i_line_bytes;
                        *(--p_out_end) = *p_line_end;
                    }

                    p_in++;
                }
            }
            break;

        case TRANSFORM_MODE_180:
            for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
            {
                pixel_data_t *p_in = p_pic->planes[ i_index ].p_data;
                pixel_data_t *p_in_end = p_in + p_pic->planes[ i_index ].i_bytes;

                pixel_data_t *p_out = p_outpic->planes[ i_index ].p_data;

                for( ; p_in < p_in_end ; )
                {
                    *p_out++ = *(--p_in_end);
                }
            }
            break;

        case TRANSFORM_MODE_270:
            for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
            {
                int i_line_bytes = p_pic->planes[ i_index ].i_line_bytes;

                pixel_data_t *p_in = p_pic->planes[ i_index ].p_data;

                pixel_data_t *p_out = p_outpic->planes[ i_index ].p_data;
                pixel_data_t *p_out_end = p_out
                                       + p_outpic->planes[ i_index ].i_bytes;

                for( ; p_out < p_out_end ; )
                {
                    pixel_data_t *p_in_end;

                    p_in_end = p_in + p_pic->planes[ i_index ].i_bytes;

                    for( ; p_in < p_in_end ; )
                    {
                        p_in_end -= i_line_bytes;
                        *p_out++ = *p_in_end;
                    }

                    p_in++;
                }
            }
            break;

        case TRANSFORM_MODE_VFLIP:
            for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
            {
                pixel_data_t *p_in = p_pic->planes[ i_index ].p_data;
                pixel_data_t *p_in_end = p_in + p_pic->planes[ i_index ].i_bytes;

                pixel_data_t *p_out = p_outpic->planes[ i_index ].p_data;

                for( ; p_in < p_in_end ; )
                {
                    p_in_end -= p_pic->planes[ i_index ].i_line_bytes;
                    p_main->fast_memcpy( p_out, p_in_end,
                                     p_pic->planes[ i_index ].i_line_bytes );
                    p_out += p_pic->planes[ i_index ].i_line_bytes;
                }
            }
            break;

        case TRANSFORM_MODE_HFLIP:
            for( i_index = 0 ; i_index < p_pic->i_planes ; i_index++ )
            {
                pixel_data_t *p_in = p_pic->planes[ i_index ].p_data;
                pixel_data_t *p_in_end = p_in + p_pic->planes[ i_index ].i_bytes;

                pixel_data_t *p_out = p_outpic->planes[ i_index ].p_data;

                for( ; p_in < p_in_end ; )
                {
                    pixel_data_t *p_line_end = p_in
                            + p_pic->planes[ i_index ].i_line_bytes;

                    for( ; p_in < p_line_end ; )
                    {
                        *p_out++ = *(--p_line_end);
                    }

                    p_in += p_pic->planes[ i_index ].i_line_bytes;
                }
            }
            break;

        default:
            break;
    }

    vout_UnlinkPicture( p_vout->p_sys->p_vout, p_outpic );

    vout_DisplayPicture( p_vout->p_sys->p_vout, p_outpic );
}

