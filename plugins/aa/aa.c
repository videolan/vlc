/*****************************************************************************
 * vout_aa.c: Aa video output display method for testing purposes
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: aa.c,v 1.6 2002/06/02 09:03:53 sam Exp $
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

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/intf.h>

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
struct vout_sys_s
{
    struct aa_context*  aa_context;
    aa_palette          palette;
    int                 i_width;                     /* width of main window */
    int                 i_height;                   /* height of main window */
};

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

static void SetPalette     ( vout_thread_t *, u16 *, u16 *, u16 * );

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
        msg_Err( p_vout, "out of memory" );
        return( 1 );
    }

    /* Don't parse any options, but take $AAOPTS into account */
    aa_parseoptions( NULL, NULL, NULL, NULL );

    if (!(p_vout->p_sys->aa_context = aa_autoinit(&aa_defparams)))
    {
        msg_Err( p_vout, "cannot initialize aalib" );
        return( 1 );
    }

    p_vout->p_sys->i_width = aa_imgwidth(p_vout->p_sys->aa_context);
    p_vout->p_sys->i_height = aa_imgheight(p_vout->p_sys->aa_context);
    aa_autoinitkbd( p_vout->p_sys->aa_context, 0 );
    aa_autoinitmouse( p_vout->p_sys->aa_context, AA_MOUSEPRESSMASK );
    aa_hidemouse( p_vout->p_sys->aa_context );
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
    aa_close( p_vout->p_sys->aa_context );
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
    int event, x, y, b;
    event = aa_getevent( p_vout->p_sys->aa_context, 0 );
    switch ( event )
    {
    case AA_MOUSE:
        aa_getmouse( p_vout->p_sys->aa_context, &x, &y, &b );
        if ( b & AA_BUTTON3 )
        {
            intf_thread_t *p_intf;
            p_intf = vlc_object_find( p_vout, VLC_OBJECT_INTF, FIND_ANYWHERE );
            if( p_intf )
            {
                p_intf->b_menu_change = 1;
                vlc_object_release( p_intf );
            }
        }
        break;
    case AA_RESIZE:
        p_vout->i_changes |= VOUT_SIZE_CHANGE;
        aa_resize( p_vout->p_sys->aa_context );
        p_vout->p_sys->i_width = aa_imgwidth( p_vout->p_sys->aa_context );
        p_vout->p_sys->i_height = aa_imgheight( p_vout->p_sys->aa_context );
        break;
    default:
        break;
    }
    return( 0 );
}

/*****************************************************************************
 * vout_Render: render previously calculated output
 *****************************************************************************/
static void vout_Render( vout_thread_t *p_vout, picture_t *p_pic )
{
  aa_fastrender( p_vout->p_sys->aa_context, 0, 0,
                 aa_imgwidth( p_vout->p_sys->aa_context ),
                 aa_imgheight( p_vout->p_sys->aa_context ) );
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

