/*****************************************************************************
 * vout_ggi.c: GGI video output display method
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000, 2001 VideoLAN
 * $Id: vout_ggi.c,v 1.9 2001/04/06 09:15:47 sam Exp $
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

#define MODULE_NAME ggi
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

#include <ggi/ggi.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"
#include "modules.h"

#include "video.h"
#include "video_output.h"

#include "intf_msg.h"
#include "interface.h"

#include "main.h"

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

    /* Buffer information */
    ggi_directbuffer *  p_buffer[2];                              /* buffers */
    boolean_t           b_must_acquire;   /* must be acquired before writing */
} vout_sys_t;

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  vout_Probe     ( probedata_t *p_data );
static int  vout_Create    ( struct vout_thread_s * );
static int  vout_Init      ( struct vout_thread_s * );
static void vout_End       ( struct vout_thread_s * );
static void vout_Destroy   ( struct vout_thread_s * );
static int  vout_Manage    ( struct vout_thread_s * );
static void vout_Display   ( struct vout_thread_s * );

static int  GGIOpenDisplay ( vout_thread_t *p_vout );
static void GGICloseDisplay( vout_thread_t *p_vout );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( vout_getfunctions )( function_list_t * p_function_list )
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
 * vout_Probe: probe the video driver and return a score
 *****************************************************************************
 * This function tries to initialize GGI and returns a score to the
 * plugin manager so that it can select the best plugin.
 *****************************************************************************/
static int vout_Probe( probedata_t *p_data )
{
    if( TestMethod( VOUT_METHOD_VAR, "ggi" ) )
    {
        return( 999 );
    }

    return( 40 );
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
    if( GGIOpenDisplay( p_vout ) )
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
#define p_b p_vout->p_sys->p_buffer
    /* Acquire first buffer */
    if( p_vout->p_sys->b_must_acquire )
    {
        ggiResourceAcquire( p_b[ p_vout->i_buffer_index ]->resource,
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
#define p_b p_vout->p_sys->p_buffer
    /* Release buffer */
    if( p_vout->p_sys->b_must_acquire )
    {
        ggiResourceRelease( p_b[ p_vout->i_buffer_index ]->resource );
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
    GGICloseDisplay( p_vout );

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
 * vout_Display: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to the display, wait until
 * it is displayed and switch the two rendering buffer, preparing next frame.
 *****************************************************************************/
void vout_Display( vout_thread_t *p_vout )
{
#define p_b p_vout->p_sys->p_buffer
    /* Change display frame */
    if( p_vout->p_sys->b_must_acquire )
    {
        ggiResourceRelease( p_b[ p_vout->i_buffer_index ]->resource );
    }
    ggiSetDisplayFrame( p_vout->p_sys->p_display,
                        p_b[ p_vout->i_buffer_index ]->frame );

    /* Swap buffers and change write frame */
    if( p_vout->p_sys->b_must_acquire )
    {
        ggiResourceAcquire( p_b[ (p_vout->i_buffer_index + 1) & 1]->resource,
                            GGI_ACTYPE_WRITE );
    }
    ggiSetWriteFrame( p_vout->p_sys->p_display,
                      p_b[ (p_vout->i_buffer_index + 1) & 1]->frame );

    /* Flush the output so that it actually displays */
    ggiFlush( p_vout->p_sys->p_display );
#undef p_b
}

/* following functions are local */

/*****************************************************************************
 * GGIOpenDisplay: open and initialize GGI device
 *****************************************************************************
 * Open and initialize display according to preferences specified in the vout
 * thread fields.
 *****************************************************************************/
static int GGIOpenDisplay( vout_thread_t *p_vout )
{
#define p_b p_vout->p_sys->p_buffer
    ggi_mode    mode;                                     /* mode descriptor */
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
    mode.frames =       2;                                      /* 2 buffers */
    mode.visible.x =    main_GetIntVariable( VOUT_WIDTH_VAR,
                                             VOUT_WIDTH_DEFAULT );
    mode.visible.y =    main_GetIntVariable( VOUT_HEIGHT_VAR,
                                             VOUT_HEIGHT_DEFAULT );
    mode.virt.x =       GGI_AUTO;
    mode.virt.y =       GGI_AUTO;
    mode.size.x =       GGI_AUTO;
    mode.size.y =       GGI_AUTO;
    mode.graphtype =    GT_15BIT;             /* minimum usable screen depth */
    mode.dpp.x =        GGI_AUTO;
    mode.dpp.y =        GGI_AUTO;
    ggiCheckMode( p_vout->p_sys->p_display, &mode );

    /* FIXME: Check that returned mode has some minimum properties */

    /* Set mode */
    if( ggiSetMode( p_vout->p_sys->p_display, &mode ) )
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
        p_vout->p_sys->p_buffer[ i_index ] =
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

    if( p_vout->p_sys->b_must_acquire )
    {
        intf_DbgMsg("buffers must be acquired");
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
    if( ggiSetGCClipping(p_vout->p_sys->p_display, 0, 0,
                         mode.visible.x, mode.visible.y ) )
    {
        intf_ErrMsg( "vout error: can't set clipping" );
        ggiClose( p_vout->p_sys->p_display );
        ggiExit();
        return( 1 );
    }

    /* Set thread information */
    p_vout->i_width =           mode.visible.x;
    p_vout->i_height =          mode.visible.y;
    p_vout->i_bytes_per_line =  p_b[ 0 ]->buffer.plb.stride;
    p_vout->i_screen_depth =    p_b[ 0 ]->buffer.plb.pixelformat->depth;
    p_vout->i_bytes_per_pixel = p_b[ 0 ]->buffer.plb.pixelformat->size / 8;
    p_vout->i_red_mask =        p_b[ 0 ]->buffer.plb.pixelformat->red_mask;
    p_vout->i_green_mask =      p_b[ 0 ]->buffer.plb.pixelformat->green_mask;
    p_vout->i_blue_mask =       p_b[ 0 ]->buffer.plb.pixelformat->blue_mask;

    /* FIXME: set palette in 8bpp */

    /* Set and initialize buffers */
    vout_SetBuffers( p_vout, p_b[ 0 ]->write, p_b[ 1 ]->write );

    return( 0 );
#undef p_b
}

/*****************************************************************************
 * GGICloseDisplay: close and reset GGI device
 *****************************************************************************
 * This function returns all resources allocated by GGIOpenDisplay and restore
 * the original state of the device.
 *****************************************************************************/
static void GGICloseDisplay( vout_thread_t *p_vout )
{
    /* Restore original mode and close display */
    ggiClose( p_vout->p_sys->p_display );

    /* Exit library */
    ggiExit();
}

