/*****************************************************************************
 * ggi.c : GGI plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: ggi.c,v 1.14 2002/02/19 00:50:19 sam Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>
#include <errno.h>                                                 /* ENOMEM */

#include <ggi/ggi.h>

#include <videolan/vlc.h>

#include "video.h"
#include "video_output.h"

#include "intf_msg.h"
#include "interface.h"

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
    SET_DESCRIPTION( "General Graphics Interface video output" )
    ADD_CAPABILITY( VOUT, 30 )
    ADD_SHORTCUT( "ggi" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    vout_getfunctions( &p_module->p_functions->vout );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * vout_sys_t: video output GGI method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the GGI specific properties of an output thread.
 *****************************************************************************/
typedef struct vout_sys_s
{
    /* GGI system informations */
    ggi_visual_t        p_display;                         /* display device */

    ggi_mode            mode;                             /* mode descriptor */
    int                 i_bits_per_pixel;

    /* Buffer information */
    ggi_directbuffer *  pp_buffer[2];                             /* buffers */
    int                 i_index;

    boolean_t           b_must_acquire;   /* must be acquired before writing */
} vout_sys_t;

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static void vout_getfunctions( function_list_t * p_function_list )
{
    p_function_list->functions.vout.pf_create  = vout_Create;
    p_function_list->functions.vout.pf_init    = vout_Init;
    p_function_list->functions.vout.pf_end     = vout_End;
    p_function_list->functions.vout.pf_destroy = vout_Destroy;
    p_function_list->functions.vout.pf_manage  = vout_Manage;
    p_function_list->functions.vout.pf_render  = vout_Render;
    p_function_list->functions.vout.pf_display = vout_Display;
}

/*****************************************************************************
 * vout_Create: allocate GGI video thread output method
 *****************************************************************************
 * This function allocate and initialize a GGI vout method. It uses some of the
 * vout properties to choose the correct mode, and change them according to the
 * mode actually used.
 *****************************************************************************/
int vout_Create( vout_thread_t *p_vout )
{
    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        intf_ErrMsg( "vout error: %s", strerror(ENOMEM) );
        return( 1 );
    }

    /* Open and initialize device */
    if( OpenDisplay( p_vout ) )
    {
        intf_ErrMsg( "vout error: can't initialize GGI display" );
        free( p_vout->p_sys );
        return( 1 );
    }

    return( 0 );
}

/*****************************************************************************
 * vout_Init: initialize GGI video thread output method
 *****************************************************************************
 * This function initialize the GGI display device.
 *****************************************************************************/
int vout_Init( vout_thread_t *p_vout )
{
    int i_index;
    picture_t *p_pic;

    I_OUTPUTPICTURES = 0;

    p_vout->output.i_width  = p_vout->p_sys->mode.visible.x;
    p_vout->output.i_height = p_vout->p_sys->mode.visible.y;
    p_vout->output.i_aspect = p_vout->p_sys->mode.visible.x
                               * VOUT_ASPECT_FACTOR
                               / p_vout->p_sys->mode.visible.y;

    switch( p_vout->p_sys->i_bits_per_pixel )
    {
        case 8: /* FIXME: set the palette */
            p_vout->output.i_chroma = FOURCC_BI_RGB; break;
        case 15:
            p_vout->output.i_chroma = FOURCC_RV15; break;
        case 16:
            p_vout->output.i_chroma = FOURCC_RV16; break;
        case 24:
            p_vout->output.i_chroma = FOURCC_BI_BITFIELDS; break;
        case 32:
            p_vout->output.i_chroma = FOURCC_BI_BITFIELDS; break;
        default:
            intf_ErrMsg( "vout error: unknown screen depth" );
            return 0;
    }

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
        return 0;
    }

#define p_b p_vout->p_sys->pp_buffer
    /* We know the chroma, allocate a buffer which will be used
     * directly by the decoder */
    p_vout->p_sys->i_index = 0;
    p_pic->p->p_pixels = p_b[ 0 ]->write;
    p_pic->p->i_pixel_bytes = p_b[ 0 ]->buffer.plb.pixelformat->size / 8;
    p_pic->p->i_lines = p_vout->p_sys->mode.visible.y;

    if( p_b[ 0 ]->buffer.plb.pixelformat->size / 8
         * p_vout->p_sys->mode.visible.x
        != p_b[ 0 ]->buffer.plb.stride )
    {
        p_pic->p->b_margin = 1;
        p_pic->p->b_hidden = 1;
        p_pic->p->i_pitch = p_b[ 0 ]->buffer.plb.stride;
        p_pic->p->i_visible_bytes = p_b[ 0 ]->buffer.plb.pixelformat->size
                                     / 8 * p_vout->p_sys->mode.visible.x;
    }
    else
    {
        p_pic->p->b_margin = 0;
        p_pic->p->i_pitch = p_b[ 0 ]->buffer.plb.stride;
    }

    /* Only useful for bits_per_pixel != 8 */
    p_pic->p->i_red_mask =   p_b[ 0 ]->buffer.plb.pixelformat->red_mask;
    p_pic->p->i_green_mask = p_b[ 0 ]->buffer.plb.pixelformat->green_mask;
    p_pic->p->i_blue_mask =  p_b[ 0 ]->buffer.plb.pixelformat->blue_mask;

    p_pic->i_planes = 1;

    p_pic->i_status = DESTROYED_PICTURE;
    p_pic->i_type   = DIRECT_PICTURE;

    PP_OUTPUTPICTURE[ I_OUTPUTPICTURES ] = p_pic;

    I_OUTPUTPICTURES++;

    /* Acquire first buffer */
    if( p_vout->p_sys->b_must_acquire )
    {
        ggiResourceAcquire( p_b[ 0 ]->resource,
                            GGI_ACTYPE_WRITE );
    }

    /* Listen to the keyboard and the mouse buttons */
    ggiSetEventMask( p_vout->p_sys->p_display,
                     emKeyboard | emPtrButtonPress | emPtrButtonRelease );

    /* Set asynchronous display mode -- usually quite faster */
    ggiAddFlags( p_vout->p_sys->p_display, GGIFLAG_ASYNC );

    return( 0 );
#undef p_b
}

/*****************************************************************************
 * vout_End: terminate GGI video thread output method
 *****************************************************************************
 * Terminate an output method created by vout_Create
 *****************************************************************************/
void vout_End( vout_thread_t *p_vout )
{
#define p_b p_vout->p_sys->pp_buffer
    /* Release buffer */
    if( p_vout->p_sys->b_must_acquire )
    {
        ggiResourceRelease( p_b[ p_vout->p_sys->i_index ]->resource );
    }
#undef p_b
}

/*****************************************************************************
 * vout_Destroy: destroy GGI video thread output method
 *****************************************************************************
 * Terminate an output method created by vout_Create
 *****************************************************************************/
void vout_Destroy( vout_thread_t *p_vout )
{
    CloseDisplay( p_vout );

    free( p_vout->p_sys );
}

/*****************************************************************************
 * vout_Manage: handle GGI events
 *****************************************************************************
 * This function should be called regularly by video output thread. It returns
 * a non null value if an error occured.
 *****************************************************************************/
int vout_Manage( vout_thread_t *p_vout )
{
    struct timeval tv = { 0, 1000 };                        /* 1 millisecond */
    gii_event_mask mask;
    gii_event      event;

    mask = emKeyboard | emPtrButtonPress | emPtrButtonRelease;

    ggiEventPoll( p_vout->p_sys->p_display, mask, &tv );
    
    while( ggiEventsQueued( p_vout->p_sys->p_display, mask) )
    {
        ggiEventRead( p_vout->p_sys->p_display, &event, mask);

        switch( event.any.type )
        {
            case evKeyRelease:

                switch( event.key.sym )
                {
                    case 'q':
                    case 'Q':
                    case GIIUC_Escape:
                        /* FIXME pass message ! */
                        p_main->p_intf->b_die = 1;
                        break;

                    default:
                        break;
                }
                break;

            case evPtrButtonRelease:

                switch( event.pbutton.button )
                {
                    case GII_PBUTTON_RIGHT:
                        /* FIXME: need locking ! */
                        p_main->p_intf->b_menu_change = 1;
                        break;
                }
                break;

            default:
                break;
        }
    }

    return( 0 );
}

/*****************************************************************************
 * vout_Render: displays previously rendered output
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
#define p_b p_vout->p_sys->pp_buffer
    p_pic->p->p_pixels = p_b[ p_vout->p_sys->i_index ]->write;

    /* Change display frame */
    if( p_vout->p_sys->b_must_acquire )
    {
        ggiResourceRelease( p_b[ p_vout->p_sys->i_index ]->resource );
    }
    ggiSetDisplayFrame( p_vout->p_sys->p_display,
                        p_b[ p_vout->p_sys->i_index ]->frame );

    /* Swap buffers and change write frame */
    p_vout->p_sys->i_index ^= 1;
    p_pic->p->p_pixels = p_b[ p_vout->p_sys->i_index ]->write;

    if( p_vout->p_sys->b_must_acquire )
    {
        ggiResourceAcquire( p_b[ p_vout->p_sys->i_index ]->resource,
                            GGI_ACTYPE_WRITE );
    }
    ggiSetWriteFrame( p_vout->p_sys->p_display,
                      p_b[ p_vout->p_sys->i_index ]->frame );

    /* Flush the output so that it actually displays */
    ggiFlush( p_vout->p_sys->p_display );
#undef p_b
}

/* following functions are local */

/*****************************************************************************
 * OpenDisplay: open and initialize GGI device
 *****************************************************************************
 * Open and initialize display according to preferences specified in the vout
 * thread fields.
 *****************************************************************************/
static int OpenDisplay( vout_thread_t *p_vout )
{
#define p_b p_vout->p_sys->pp_buffer
    ggi_color   col_fg;                                  /* foreground color */
    ggi_color   col_bg;                                  /* background color */
    int         i_index;                               /* all purposes index */
    char        *psz_display;

    /* Initialize library */
    if( ggiInit() )
    {
        intf_ErrMsg( "vout error: can't initialize GGI library" );
        return( 1 );
    }

    /* Open display */
    psz_display = main_GetPszVariable( VOUT_DISPLAY_VAR, NULL );

    p_vout->p_sys->p_display = ggiOpen( psz_display, NULL );

    if( p_vout->p_sys->p_display == NULL )
    {
        intf_ErrMsg( "vout error: can't open GGI default display" );
        ggiExit();
        return( 1 );
    }

    /* Find most appropriate mode */
    p_vout->p_sys->mode.frames =    2;                          /* 2 buffers */
    p_vout->p_sys->mode.visible.x = main_GetIntVariable( VOUT_WIDTH_VAR,
                                                         VOUT_WIDTH_DEFAULT );
    p_vout->p_sys->mode.visible.y = main_GetIntVariable( VOUT_HEIGHT_VAR,
                                                         VOUT_HEIGHT_DEFAULT );
    p_vout->p_sys->mode.virt.x =    GGI_AUTO;
    p_vout->p_sys->mode.virt.y =    GGI_AUTO;
    p_vout->p_sys->mode.size.x =    GGI_AUTO;
    p_vout->p_sys->mode.size.y =    GGI_AUTO;
    p_vout->p_sys->mode.graphtype = GT_15BIT;        /* minimum usable depth */
    p_vout->p_sys->mode.dpp.x =     GGI_AUTO;
    p_vout->p_sys->mode.dpp.y =     GGI_AUTO;
    ggiCheckMode( p_vout->p_sys->p_display, &p_vout->p_sys->mode );

    /* FIXME: Check that returned mode has some minimum properties */

    /* Set mode */
    if( ggiSetMode( p_vout->p_sys->p_display, &p_vout->p_sys->mode ) )
    {
        intf_ErrMsg( "vout error: can't set GGI mode" );
        ggiClose( p_vout->p_sys->p_display );
        ggiExit();
        return( 1 );
    }

    /* Check buffers properties */
    p_vout->p_sys->b_must_acquire = 0;
    for( i_index = 0; i_index < 2; i_index++ )
    {
        /* Get buffer address */
        p_vout->p_sys->pp_buffer[ i_index ] =
            (ggi_directbuffer *)ggiDBGetBuffer( p_vout->p_sys->p_display,
                                                i_index );
        if( p_b[ i_index ] == NULL )
        {
            intf_ErrMsg( "vout error: double buffering is not possible" );
            ggiClose( p_vout->p_sys->p_display );
            ggiExit();
            return( 1 );
        }

        /* Check buffer properties */
        if( ! ( p_b[ i_index ]->type & GGI_DB_SIMPLE_PLB )
           || ( p_b[ i_index ]->page_size != 0 )
           || ( p_b[ i_index ]->write == NULL )
           || ( p_b[ i_index ]->noaccess != 0 )
           || ( p_b[ i_index ]->align != 0 ) )
        {
            intf_ErrMsg( "vout error: incorrect video memory type" );
            ggiClose( p_vout->p_sys->p_display );
            ggiExit();
            return( 1 );
        }

        /* Check if buffer needs to be acquired before write */
        if( ggiResourceMustAcquire( p_b[ i_index ]->resource ) )
        {
            p_vout->p_sys->b_must_acquire = 1;
        }
    }

    /* Set graphic context colors */
    col_fg.r = col_fg.g = col_fg.b = -1;
    col_bg.r = col_bg.g = col_bg.b = 0;
    if( ggiSetGCForeground(p_vout->p_sys->p_display,
                           ggiMapColor(p_vout->p_sys->p_display,&col_fg)) ||
        ggiSetGCBackground(p_vout->p_sys->p_display,
                           ggiMapColor(p_vout->p_sys->p_display,&col_bg)) )
    {
        intf_ErrMsg( "vout error: can't set colors" );
        ggiClose( p_vout->p_sys->p_display );
        ggiExit();
        return( 1 );
    }

    /* Set clipping for text */
    if( ggiSetGCClipping( p_vout->p_sys->p_display, 0, 0,
                          p_vout->p_sys->mode.visible.x,
                          p_vout->p_sys->mode.visible.y ) )
    {
        intf_ErrMsg( "vout error: can't set clipping" );
        ggiClose( p_vout->p_sys->p_display );
        ggiExit();
        return( 1 );
    }

    /* FIXME: set palette in 8bpp */
    p_vout->p_sys->i_bits_per_pixel = p_b[ 0 ]->buffer.plb.pixelformat->depth;

    return( 0 );
#undef p_b
}

/*****************************************************************************
 * CloseDisplay: close and reset GGI device
 *****************************************************************************
 * This function returns all resources allocated by OpenDisplay and restore
 * the original state of the device.
 *****************************************************************************/
static void CloseDisplay( vout_thread_t *p_vout )
{
    /* Restore original mode and close display */
    ggiClose( p_vout->p_sys->p_display );

    /* Exit library */
    ggiExit();
}

