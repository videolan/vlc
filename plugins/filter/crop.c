/*****************************************************************************
 * crop.c : Crop video plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: crop.c,v 1.1.2.1 2002/06/02 00:43:43 sam Exp $
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
ADD_CATEGORY_HINT( N_("Miscellaneous"), NULL )
ADD_STRING ( "crop-geometry", NULL, NULL, N_("Crop geometry"), N_("Set the geometry of the zone to crop") )
//ADD_BOOL ( "autocrop", 0, NULL, N_("Automatic cropping"), N_("Activate automatic black border cropping") )
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( _("image crop video module") )
    /* Capability score set to 0 because we don't want to be spawned
     * as a video output unless explicitly requested to */
    ADD_CAPABILITY( VOUT, 0 )
    ADD_SHORTCUT( "crop" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    vout_getfunctions( &p_module->p_functions->vout );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * vout_sys_t: Crop video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the Crop specific properties of an output thread.
 *****************************************************************************/
typedef struct vout_sys_s
{
    vout_thread_t *p_vout;

    unsigned int i_x, i_y;
    unsigned int i_width, i_height, i_aspect;

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
 * vout_Create: allocates Crop video thread output method
 *****************************************************************************
 * This function allocates and initializes a Crop vout method.
 *****************************************************************************/
static int vout_Create( vout_thread_t *p_vout )
{
    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        intf_ErrMsg( "vout error: out of memory" );
        return 1;
    }

    return 0;
}

/*****************************************************************************
 * vout_Init: initialize Crop video thread output method
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
    int   i_index;
    char *psz_var;
    picture_t *p_pic;
    
    I_OUTPUTPICTURES = 0;

    /* Initialize the output structure */
    p_vout->output.i_chroma = p_vout->render.i_chroma;
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;
    p_vout->output.i_aspect = p_vout->render.i_aspect;

    /* Get geometry value from the user */
    psz_var = config_GetPszVariable( "crop-geometry" );
    if( psz_var )
    {
        char *psz_parser, *psz_tmp;

        psz_parser = psz_tmp = psz_var;
        while( *psz_tmp && *psz_tmp != 'x' ) psz_tmp++;

        if( *psz_tmp )
        {
            psz_tmp[0] = '\0';
            p_vout->p_sys->i_width = atoi( psz_parser );

            psz_parser = ++psz_tmp;
            while( *psz_tmp && *psz_tmp != '+' ) psz_tmp++;

            if( *psz_tmp )
            {
                psz_tmp[0] = '\0';
                p_vout->p_sys->i_height = atoi( psz_parser );

                psz_parser = ++psz_tmp;
                while( *psz_tmp && *psz_tmp != '+' ) psz_tmp++;

                if( *psz_tmp )
                {
                    psz_tmp[0] = '\0';
                    p_vout->p_sys->i_x = atoi( psz_parser );
                    p_vout->p_sys->i_y = atoi( ++psz_tmp );
                }
                else
                {
                    p_vout->p_sys->i_x = atoi( psz_parser );
                    p_vout->p_sys->i_y =
                     ( p_vout->output.i_height - p_vout->p_sys->i_height ) / 2;
                }
            }
            else
            {
                p_vout->p_sys->i_height = atoi( psz_parser );
                p_vout->p_sys->i_x =
                     ( p_vout->output.i_width - p_vout->p_sys->i_width ) / 2;
                p_vout->p_sys->i_y =
                     ( p_vout->output.i_height - p_vout->p_sys->i_height ) / 2;
            }
        }
        else
        {
            p_vout->p_sys->i_width = atoi( psz_parser );
            p_vout->p_sys->i_height = p_vout->output.i_height;
            p_vout->p_sys->i_x =
                     ( p_vout->output.i_width - p_vout->p_sys->i_width ) / 2;
            p_vout->p_sys->i_y =
                     ( p_vout->output.i_height - p_vout->p_sys->i_height ) / 2;
        }

        free( psz_var );
    }
    else
    {
        p_vout->p_sys->i_width  = p_vout->output.i_width;
        p_vout->p_sys->i_height = p_vout->output.i_height;
        p_vout->p_sys->i_x =
                     ( p_vout->output.i_width - p_vout->p_sys->i_width ) / 2;
        p_vout->p_sys->i_y =
                     ( p_vout->output.i_height - p_vout->p_sys->i_height ) / 2;
    }

    /* Check for validity */
    if( p_vout->p_sys->i_x + p_vout->p_sys->i_width > p_vout->output.i_width )
    {
        p_vout->p_sys->i_x = 0;
        if( p_vout->p_sys->i_width > p_vout->output.i_width )
        {
            p_vout->p_sys->i_width = p_vout->output.i_width;
        }
    }

    if( p_vout->p_sys->i_y + p_vout->p_sys->i_height > p_vout->output.i_height )
    {
        p_vout->p_sys->i_y = 0;
        if( p_vout->p_sys->i_height > p_vout->output.i_height )
        {
            p_vout->p_sys->i_height = p_vout->output.i_height;
        }
    }

    intf_WarnMsg( 3, "vout info: cropping at %ix%i+%i+%i",
                  p_vout->p_sys->i_width, p_vout->p_sys->i_height,
                  p_vout->p_sys->i_x, p_vout->p_sys->i_y );

    /* Set current output image properties */
    p_vout->p_sys->i_aspect = p_vout->output.i_aspect
                            * p_vout->output.i_height / p_vout->p_sys->i_height
                            * p_vout->p_sys->i_width / p_vout->output.i_width;

    /* Try to open the real video output */
    psz_var = config_GetPszVariable( "filter" );
    config_PutPszVariable( "filter", NULL );

    intf_WarnMsg( 3, "vout info: spawning the real video outputs" );

    p_vout->p_sys->p_vout =
        vout_CreateThread( NULL,
                    p_vout->p_sys->i_width, p_vout->p_sys->i_height,
                    p_vout->render.i_chroma, p_vout->p_sys->i_aspect );
    if( p_vout->p_sys->p_vout == NULL )
    {
        intf_ErrMsg( "vout error: failed to create vout" );
        config_PutPszVariable( "filter", psz_var );
        if( psz_var ) free( psz_var );
        return 0;
    }

    config_PutPszVariable( "filter", psz_var );
    if( psz_var ) free( psz_var );

    ALLOCATE_DIRECTBUFFERS( VOUT_MAX_PICTURES );

    return 0;
}

/*****************************************************************************
 * vout_End: terminate Crop video thread output method
 *****************************************************************************/
static void vout_End( vout_thread_t *p_vout )
{
    int i_index;

    /* Free the fake output buffers we allocated */
    for( i_index = I_OUTPUTPICTURES ; i_index ; )
    {
        i_index--;
        free( PP_OUTPUTPICTURE[ i_index ]->p_data_orig );
    }
}

/*****************************************************************************
 * vout_Destroy: destroy Crop video thread output method
 *****************************************************************************
 * Terminate an output method created by CropCreateOutputMethod
 *****************************************************************************/
static void vout_Destroy( vout_thread_t *p_vout )
{
    vout_DestroyThread( p_vout->p_sys->p_vout, NULL );
    free( p_vout->p_sys );
}

/*****************************************************************************
 * vout_Manage: handle Crop events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * console events. It returns a non null value on error.
 *****************************************************************************/
static int vout_Manage( vout_thread_t *p_vout )
{
    return 0;
}

/*****************************************************************************
 * vout_Render: display previously rendered output
 *****************************************************************************
 * This function sends the currently rendered image to Crop image, waits
 * until it is displayed and switches the two rendering buffers, preparing next
 * frame.
 *****************************************************************************/
static void vout_Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    picture_t *p_outpic = NULL;
    int i_plane;

    while( ( p_outpic =
                 vout_CreatePicture( p_vout->p_sys->p_vout, 0, 0, 0 )
           ) == NULL )
    {
        if( p_vout->b_die || p_vout->b_error )
        {
            vout_DestroyPicture( p_vout->p_sys->p_vout, p_outpic );
            return;
        }

        msleep( VOUT_OUTMEM_SLEEP );
    }

    vout_DatePicture( p_vout->p_sys->p_vout, p_outpic, p_pic->date );
    vout_LinkPicture( p_vout->p_sys->p_vout, p_outpic );

    for( i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {
        u8 *p_in, *p_out, *p_out_end;
        int i_in_pitch = p_pic->p[i_plane].i_pitch;
        const int i_out_pitch = p_outpic->p[i_plane].i_pitch;

        p_in = p_pic->p[i_plane].p_pixels
                /* Skip the right amount of lines */
                + i_in_pitch * ( p_pic->p[i_plane].i_lines * p_vout->p_sys->i_y
                                  / p_vout->output.i_height )
                /* Skip the right amount of columns */
                + i_in_pitch * p_vout->p_sys->i_x / p_vout->output.i_width;

        p_out = p_outpic->p[i_plane].p_pixels;
        p_out_end = p_out + i_out_pitch * p_outpic->p[i_plane].i_lines;

        while( p_out < p_out_end )
        {
            FAST_MEMCPY( p_out, p_in, i_out_pitch );
            p_in += i_in_pitch;
            p_out += i_out_pitch;
        }
    }

    vout_UnlinkPicture( p_vout->p_sys->p_vout, p_outpic );
    vout_DisplayPicture( p_vout->p_sys->p_vout, p_outpic );
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

