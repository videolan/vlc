/*****************************************************************************
 * vout_aa.c: Aa video output display method for testing purposes
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: aa.c,v 1.3 2002/04/19 13:56:10 sam Exp $
 *
 * Authors: Sigmund Augdal <sigmunau@idi.ntnu.no>
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

#include <aalib.h>

#include <videolan/vlc.h>

#include "video.h"
#include "video_output.h"

/*****************************************************************************
 * Capabilities defined in the other files.
 *****************************************************************************/
static void vout_getfunctions  ( function_list_t * p_function_list );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP


MODULE_INIT_START
    SET_DESCRIPTION( _("ASCII-art video output module") )
    ADD_CAPABILITY( VOUT, 10 )
    ADD_SHORTCUT( "aa" )
    ADD_SHORTCUT( "aalib" )
MODULE_INIT_STOP


MODULE_ACTIVATE_START
    vout_getfunctions( &p_module->p_functions->vout );
MODULE_ACTIVATE_STOP


MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * vout_sys_t: aa video output method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the aa specific properties of an output thread.
 *****************************************************************************/
typedef struct vout_sys_s
{
    struct aa_context*  aa_context;
    aa_palette          palette;
    int                 i_width;                     /* width of main window */
    int                 i_height;                   /* height of main window */

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

static void SetPalette     ( struct vout_thread_s *, u16 *, u16 *, u16 * );

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
 * vout_Create: allocates aa video thread output method
 *****************************************************************************
 * This function allocates and initializes a aa vout method.
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

    /* Don't parse any options, but take $AAOPTS into account */
    aa_parseoptions( NULL, NULL, NULL, NULL );

    if (!(p_vout->p_sys->aa_context = aa_autoinit(&aa_defparams)))
    {
        intf_ErrMsg( "vout error: cannot initialize AA-lib. Sorry" );
        return( 1 );
    }
    p_vout->p_sys->i_width = aa_imgwidth(p_vout->p_sys->aa_context);
    p_vout->p_sys->i_height = aa_imgheight(p_vout->p_sys->aa_context);

    return( 0 );
}

/*****************************************************************************
 * vout_Init: initialize aa video thread output method
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic = NULL;

    I_OUTPUTPICTURES = 0;

    p_vout->output.i_chroma = FOURCC_RGB2;
    p_vout->output.i_width = p_vout->p_sys->i_width;
    p_vout->output.i_height = p_vout->p_sys->i_height;
    p_vout->output.i_aspect = p_vout->p_sys->i_width
                               * VOUT_ASPECT_FACTOR / p_vout->p_sys->i_height;
    p_vout->output.pf_setpalette = SetPalette;

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

    /* Allocate the picture */
    p_pic->p->p_pixels = aa_image( p_vout->p_sys->aa_context );
    p_pic->p->i_pixel_bytes = 1;
    p_pic->p->i_lines = p_vout->p_sys->i_height;
    p_pic->p->i_pitch = p_vout->p_sys->i_width;
    p_pic->p->b_margin = 0;
    p_pic->i_planes = 1;

    p_pic->i_status = DESTROYED_PICTURE;
    p_pic->i_type   = DIRECT_PICTURE;

    PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;
    I_OUTPUTPICTURES++;

    return 0;
}

/*****************************************************************************
 * vout_End: terminate aa video thread output method
 *****************************************************************************/
static void vout_End( vout_thread_t *p_vout )
{
    ;
}

/*****************************************************************************
 * vout_Destroy: destroy aa video thread output method
 *****************************************************************************
 * Terminate an output method created by AaCreateOutputMethod
 *****************************************************************************/
static void vout_Destroy( vout_thread_t *p_vout )
{
    aa_close(p_vout->p_sys->aa_context);
    free( p_vout->p_sys );
}

/*****************************************************************************
 * vout_Manage: handle aa events
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
    ;
}

/*****************************************************************************
 * vout_Display: displays previously rendered output
 *****************************************************************************/
static void vout_Display( vout_thread_t *p_vout, picture_t *p_pic )
{
    /* No need to do anything, the fake direct buffers stay as they are */
    int i_width, i_height, i_x, i_y;

    vout_PlacePicture( p_vout, p_vout->p_sys->i_width, p_vout->p_sys->i_height,
                       &i_x, &i_y, &i_width, &i_height );
    //    p_vout->p_sys->aa_context->imagebuffer = p_pic->p_data;
    aa_fastrender( p_vout->p_sys->aa_context, 0, 0,
                   aa_scrwidth(p_vout->p_sys->aa_context),
                   aa_scrheight(p_vout->p_sys->aa_context) );

    aa_flush(p_vout->p_sys->aa_context);
}

/*****************************************************************************
 * SetPalette: set the 8bpp palette
 *****************************************************************************/
static void SetPalette( vout_thread_t *p_vout, u16 *red, u16 *green, u16 *blue )
{
    int i;

    /* Fill colors with color information */
    for( i = 0; i < 256; i++ )
    {
        aa_setpalette( p_vout->p_sys->palette, 256 -i,
                       red[ i ], green[ i ], blue[ i ] );
    }
}

