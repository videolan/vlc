/*****************************************************************************
 * clone.c : Clone video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: clone.c,v 1.1 2002/05/13 19:30:40 sam Exp $
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
#include <errno.h>
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <videolan/vlc.h>

#include "video.h"
#include "video_output.h"

#include "filter_common.h"

/*****************************************************************************
 * Capabilities defined in the other files.
 *****************************************************************************/
static void vout_getfunctions( function_list_t * p_function_list );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( _("image clone video module") )
    /* Capability score set to 0 because we don't want to be spawned
     * as a video output unless explicitly requested to */
    ADD_CAPABILITY( VOUT, 0 )
    ADD_SHORTCUT( "clone" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    vout_getfunctions( &p_module->p_functions->vout );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * vout_sys_t: Clone video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the Clone specific properties of an output thread.
 *****************************************************************************/
typedef struct vout_sys_s
{
    int    i_clones;
    vout_thread_t **pp_vout;

} vout_sys_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  vout_Create    ( vout_thread_t * );
static int  vout_Init      ( vout_thread_t * );
static void vout_End       ( vout_thread_t * );
static void vout_Destroy   ( vout_thread_t * );
static int  vout_Manage    ( vout_thread_t * );
static void vout_Render    ( vout_thread_t *, picture_t * );
static void vout_Display   ( vout_thread_t *, picture_t * );

static void RemoveAllVout  ( vout_thread_t *p_vout );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static void vout_getfunctions( function_list_t * p_function_list )
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
 * vout_Create: allocates Clone video thread output method
 *****************************************************************************
 * This function allocates and initializes a Clone vout method.
 *****************************************************************************/
static int vout_Create( vout_thread_t *p_vout )
{
    char *psz_method, *psz_method_orig;

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        intf_ErrMsg( "vout error: out of memory" );
        return( 1 );
    }

    /* Look what method was requested */
    if( !(psz_method = psz_method_orig = config_GetPszVariable( "filter" )) )
    {
        intf_ErrMsg( "vout error: configuration variable %s empty", "filter" );
        return( 1 );
    }

    while( *psz_method && *psz_method != ':' )
    {
        psz_method++;
    }

    if( *psz_method )
    {
        p_vout->p_sys->i_clones = atoi( psz_method + 1 );
    }
    else
    {
        intf_ErrMsg( "vout error: "
                     "no valid clone count provided, using clone:2" );
        p_vout->p_sys->i_clones = 2;
    }

    p_vout->p_sys->i_clones = __MAX( 1, __MIN( 99, p_vout->p_sys->i_clones ) );

    intf_WarnMsg( 3, "vout info: spawning %i clone(s)",
                  p_vout->p_sys->i_clones );

    p_vout->p_sys->pp_vout = malloc( p_vout->p_sys->i_clones *
                                     sizeof(vout_thread_t *) );
    if( p_vout->p_sys->pp_vout == NULL )
    {
        intf_ErrMsg( "vout error: out of memory" );
        free( psz_method_orig );
        free( p_vout->p_sys );
        return( 1 );
    }

    free( psz_method_orig );

    return( 0 );
}

/*****************************************************************************
 * vout_Init: initialize Clone video thread output method
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
    int   i_index, i_vout;
    char *psz_filter;
    picture_t *p_pic;
    
    I_OUTPUTPICTURES = 0;

    /* Initialize the output structure */
    p_vout->output.i_chroma = p_vout->render.i_chroma;
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    /* Try to open the real video output */
    psz_filter = config_GetPszVariable( "filter" );
    config_PutPszVariable( "filter", NULL );

    intf_WarnMsg( 3, "vout info: spawning the real video outputs" );

    for( i_vout = 0; i_vout < p_vout->p_sys->i_clones; i_vout++ )
    {
        p_vout->p_sys->pp_vout[ i_vout ] =
                vout_CreateThread( NULL,
                            p_vout->render.i_width, p_vout->render.i_height,
                            p_vout->render.i_chroma, p_vout->render.i_aspect );
        if( p_vout->p_sys->pp_vout[ i_vout ] == NULL )
        {
            intf_ErrMsg( "vout error: failed to clone %i vout threads",
                         p_vout->p_sys->i_clones );
            p_vout->p_sys->i_clones = i_vout;
            RemoveAllVout( p_vout );
            config_PutPszVariable( "filter", psz_filter );
            if( psz_filter ) free( psz_filter );
            return 0;
        }
    }

    config_PutPszVariable( "filter", psz_filter );
    if( psz_filter ) free( psz_filter );

    ALLOCATE_DIRECTBUFFERS( VOUT_MAX_PICTURES );

    return( 0 );
}

/*****************************************************************************
 * vout_End: terminate Clone video thread output method
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
 * vout_Destroy: destroy Clone video thread output method
 *****************************************************************************
 * Terminate an output method created by CloneCreateOutputMethod
 *****************************************************************************/
static void vout_Destroy( vout_thread_t *p_vout )
{
    RemoveAllVout( p_vout );

    free( p_vout->p_sys->pp_vout );
    free( p_vout->p_sys );
}

/*****************************************************************************
 * vout_Manage: handle Clone events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * console events. It returns a non null value on error.
 *****************************************************************************/
static int vout_Manage( vout_thread_t *p_vout )
{
    return( 0 );
}

/*****************************************************************************
 * vout_Render: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to Clone image, waits
 * until it is displayed and switch the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static void vout_Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t *p_outpic = NULL;
    int i_vout, i_plane;

    for( i_vout = 0; i_vout < p_vout->p_sys->i_clones; i_vout++ )
    {
        while( ( p_outpic =
            vout_CreatePicture( p_vout->p_sys->pp_vout[ i_vout ], 0, 0, 0 )
               ) == NULL )
        {
            if( p_vout->b_die || p_vout->b_error )
            {
                vout_DestroyPicture(
                    p_vout->p_sys->pp_vout[ i_vout ], p_outpic );
                return;
            }

            msleep( VOUT_OUTMEM_SLEEP );
        }

        vout_DatePicture( p_vout->p_sys->pp_vout[ i_vout ],
                          p_outpic, p_pic->date );
        vout_LinkPicture( p_vout->p_sys->pp_vout[ i_vout ], p_outpic );

        for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
        {
            u8 *p_in, *p_in_end, *p_out;
            int i_in_pitch = p_pic->p[i_plane].i_pitch;
            const int i_out_pitch = p_outpic->p[i_plane].i_pitch;

            p_in = p_pic->p[i_plane].p_pixels;

            p_in_end = p_in + p_outpic->p[i_plane].i_lines
                               * p_pic->p[i_plane].i_pitch;

            p_out = p_outpic->p[i_plane].p_pixels;

            while( p_in < p_in_end )
            {
                FAST_MEMCPY( p_out, p_in, i_out_pitch );
                p_in += i_in_pitch;
                p_out += i_out_pitch;
            }
        }

        vout_UnlinkPicture( p_vout->p_sys->pp_vout[ i_vout ], p_outpic );
        vout_DisplayPicture( p_vout->p_sys->pp_vout[ i_vout ], p_outpic );
    }
}

/*****************************************************************************
 * vout_Display: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to Invert image, waits
 * until it is displayed and switch the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static void vout_Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    ;
}

/*****************************************************************************
 * RemoveAllVout: destroy all the child video output threads
 *****************************************************************************/
static void RemoveAllVout( vout_thread_t *p_vout )
{
    while( p_vout->p_sys->i_clones )
    {
         --p_vout->p_sys->i_clones;
         vout_DestroyThread(
                   p_vout->p_sys->pp_vout[ p_vout->p_sys->i_clones ], NULL );
    }
}

