/*****************************************************************************
 * vout_directx.c: Windows DirectX video output display method
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: vout_directx.c,v 1.6 2001/07/08 17:45:52 gbazin Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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

#define MODULE_NAME directx
#include "modules_inner.h"

/* To be fixed:
 *
 * When the option: "Display full screen when dragging window" is enabled in
 * Windows display properties, the overlay surface coordinates won't be updated
 * (but it won't crash anymore ;-) I know where the problem is in the code, but * I just don't know yet of a nice way to fix this.
 * 
 * Double buffering
 *
 * Port this plugin to Video Output IV
 */

/*****************************************************************************
 * Preamble:
 *
 * This plugin will use YUV overlay if supported, using overlay will result in
 * the best video quality (hardware interpolation when rescaling the picture)
 * and the fastest display as it requires less processing.
 *
 * If YUV overlay is not supported the plugin will use an RGB offscreen video
 * surface that will be blitted onto the primary surface (display) to
 * effectively display the picture. this fallback method enables us to display
 * video in window mode.
 * Another fallback method (which isn't implemented yet) would be to use the
 * primary surface as the video buffer. This would allow for better
 * performance but this is restricted to fullscreen video. In short,
 * implementing this is not considered high priority.
 * 
 *****************************************************************************/
#include "defs.h"

#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

#include <windows.h>
#include <windowsx.h>
#include <directx.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"
#include "netutils.h"

#include "video.h"
#include "video_output.h"

#include "intf_msg.h"
#include "interface.h"
#include "main.h"

#include "modules.h"
#include "modules_export.h"

/*****************************************************************************
 * vout_sys_t: video output DirectX method descriptor
 *****************************************************************************
 * This structure is part of the video output thread descriptor.
 * It describes the DirectX specific properties of an output thread.
 *****************************************************************************/
typedef struct vout_sys_s
{

    LPDIRECTDRAW2        p_ddobject;                    /* DirectDraw object */
    LPDIRECTDRAWSURFACE3 p_display;                        /* Display device */
    LPDIRECTDRAWSURFACE3 p_surface;    /* surface where we display the video */
    LPDIRECTDRAWCLIPPER  p_clipper;             /* clipper used for blitting */
    HINSTANCE            hddraw_dll;       /* handle of the opened ddraw dll */
    HBRUSH               hbrush;           /* window backgound brush (color) */
    HWND                 hwnd;                  /* Handle of the main window */

    int         i_image_width;                  /* size of the decoded image */
    int         i_image_height;
    int         i_window_width;               /* size of the displayed image */
    int         i_window_height;

    int         i_colorkey;          /* colorkey used to display the overlay */
 
    boolean_t   b_display_enabled;
    boolean_t   b_overlay;
    boolean_t   b_cursor;

    boolean_t   b_cursor_autohidden;
    mtime_t     i_lastmoved;

    char       *p_directx_buf[2];                      /* Buffer information */

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
static void vout_SetPalette( p_vout_thread_t p_vout, u16 *red, u16 *green,
                             u16 *blue, u16 *transp );

static int  DirectXCreateWindow   ( vout_thread_t *p_vout );
static int  DirectXInitDDraw      ( vout_thread_t *p_vout );
static int  DirectXCreateDisplay  ( vout_thread_t *p_vout );
static int  DirectXCreateSurface  ( vout_thread_t *p_vout );
static int  DirectXCreateClipper  ( vout_thread_t *p_vout );
static int  DirectXUpdateOverlay  ( vout_thread_t *p_vout );
static void DirectXCloseDDraw     ( vout_thread_t *p_vout );
static void DirectXCloseWindow    ( vout_thread_t *p_vout );
static void DirectXCloseDisplay   ( vout_thread_t *p_vout );
static void DirectXCloseSurface   ( vout_thread_t *p_vout );
static void DirectXKeepAspectRatio( vout_thread_t *p_vout, RECT *coordinates );

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
    p_function_list->functions.vout.pf_setpalette = vout_SetPalette;
}

/*****************************************************************************
 * vout_Probe: probe the video driver and return a score
 *****************************************************************************
 * This function tries to initialize Windows DirectX and returns a score to
 * the plugin manager so that it can select the best plugin.
 *****************************************************************************/
static int vout_Probe( probedata_t *p_data )
{

    if( TestMethod( VOUT_METHOD_VAR, "directx" ) )
    {
        return( 999 );
    }

    /* Check that at least DirectX5 is installed on the computer */
    /* Fixme */

    return( 400 );
}

/*****************************************************************************
 * vout_Create: allocate DirectX video thread output method
 *****************************************************************************
 * This function allocates and initialize the DirectX vout method.
 *****************************************************************************/
static int vout_Create( vout_thread_t *p_vout )
{
    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        intf_ErrMsg( "vout error: can't create p_sys (%s)", strerror(ENOMEM) );
        return( 1 );
    }

    /* Initialisations */
    p_vout->p_sys->p_ddobject = NULL;
    p_vout->p_sys->p_display = NULL;
    p_vout->p_sys->p_surface = NULL;
    p_vout->p_sys->p_clipper = NULL;
    p_vout->p_sys->hbrush = NULL;
    p_vout->p_sys->hwnd = NULL;
    p_vout->p_sys->b_display_enabled = 0;
    p_vout->b_need_render = 0;      /* by default try an YUV overlay display */

    p_vout->p_sys->b_cursor = 1; /* TODO should be done with a main_GetInt.. */

    p_vout->p_sys->b_cursor_autohidden = 0;
    p_vout->p_sys->i_lastmoved = mdate();

    p_vout->b_fullscreen = main_GetIntVariable( VOUT_FULLSCREEN_VAR,
                                                VOUT_FULLSCREEN_DEFAULT );
    p_vout->p_sys->b_overlay = main_GetIntVariable( VOUT_OVERLAY_VAR,
                                                    VOUT_OVERLAY_DEFAULT );
    p_vout->p_sys->i_window_width = main_GetIntVariable( VOUT_WIDTH_VAR,
                                                         VOUT_WIDTH_DEFAULT );
    p_vout->p_sys->i_window_height = main_GetIntVariable( VOUT_HEIGHT_VAR,
                                                         VOUT_HEIGHT_DEFAULT );
    /* We don't know yet the dimensions of the video so the best guess is to
     * pick the same as the window */
    p_vout->p_sys->i_image_width = p_vout->p_sys->i_window_width;
    p_vout->p_sys->i_image_height = p_vout->p_sys->i_window_height;

    /* Create a window for the video */
    /* Creating a window under Windows also initializes the thread's event
     * message qeue */
    if( DirectXCreateWindow( p_vout ) )
    {
        intf_ErrMsg( "vout error: can't create window" );
        free( p_vout->p_sys );
        return ( 1 );
    }

    /* Initialise DirectDraw */
    if( DirectXInitDDraw( p_vout ) )
    {
        intf_ErrMsg( "vout error: can't initialise DirectDraw" );
        DirectXCloseWindow( p_vout );
        free( p_vout->p_sys );
        return ( 1 );
    }

    /* Create the directx display */
    if( DirectXCreateDisplay( p_vout ) )
    {
        intf_ErrMsg( "vout error: can't initialise DirectDraw" );
        DirectXCloseDDraw( p_vout );
        DirectXCloseWindow( p_vout );
        free( p_vout->p_sys );
        return ( 1 );
    }

    return( 0 );
}

/*****************************************************************************
 * vout_Init: initialize DirectX video thread output method
 *****************************************************************************
 *
 *****************************************************************************/
static int vout_Init( vout_thread_t *p_vout )
{
    return( 0 );
}

/*****************************************************************************
 * vout_End: terminate Sys video thread output method
 *****************************************************************************
 * Terminate an output method created by vout_Create.
 * It is called at the end of the thread.
 *****************************************************************************/
static void vout_End( vout_thread_t *p_vout )
{
    return;
}

/*****************************************************************************
 * vout_Destroy: destroy Sys video thread output method
 *****************************************************************************
 * Terminate an output method created by vout_Create
 *****************************************************************************/
static void vout_Destroy( vout_thread_t *p_vout )
{
    intf_WarnMsg( 3, "vout: vout_Destroy" );
    DirectXCloseDisplay( p_vout );
    DirectXCloseDDraw( p_vout );
    DirectXCloseWindow( p_vout );

    if( p_vout->p_sys != NULL )
    {
        free( p_vout->p_sys );
        p_vout->p_sys = NULL;
    }

}

/*****************************************************************************
 * vout_Manage: handle Sys events
 *****************************************************************************
 * This function should be called regularly by video output thread. It returns
 * a non null value if an error occured.
 *****************************************************************************/
static int vout_Manage( vout_thread_t *p_vout )
{
    MSG             msg;
    WINDOWPLACEMENT window_placement;
    boolean_t       b_dispatch_msg = TRUE;

    while( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
    {
        switch( msg.message )
        {

        case WM_CLOSE:
            intf_WarnMsg( 4, "vout: vout_Manage WM_CLOSE" );
            p_vout->b_die = 1;
            break;
          
        case WM_QUIT:
            intf_WarnMsg( 4, "vout: vout_Manage WM_QUIT" );
            p_main->p_intf->b_die = 1;
            break;
          
        case WM_MOVE:
            intf_WarnMsg( 3, "vout: vout_Manage WM_MOVE" );
            if( !p_vout->b_need_render )
            {
                p_vout->i_changes |= VOUT_SIZE_CHANGE;
            }
            /* don't create a never ending loop */
            b_dispatch_msg = FALSE;
            break;
          
        case WM_APP:
            intf_WarnMsg( 3, "vout: vout_Manage WM_APP" );
            if( !p_vout->b_need_render )
            {
                p_vout->i_changes |= VOUT_SIZE_CHANGE;
            }
            /* don't create a never ending loop */
            b_dispatch_msg = FALSE;
            break;
          
#if 0
        case WM_PAINT:
            intf_WarnMsg( 4, "vout: vout_Manage WM_PAINT" );
            break;
          
        case WM_ERASEBKGND:
            intf_WarnMsg( 4, "vout: vout_Manage WM_ERASEBKGND" );
            break;
#endif
          
        case WM_MOUSEMOVE:
            intf_WarnMsg( 4, "vout: vout_Manage WM_MOUSEMOVE" );
            if( p_vout->p_sys->b_cursor )
            {
                if( p_vout->p_sys->b_cursor_autohidden )
                {
                    p_vout->p_sys->b_cursor_autohidden = 0;
                    p_vout->p_sys->i_lastmoved = mdate();
                    ShowCursor( TRUE );
                }
                else
                {
                    p_vout->p_sys->i_lastmoved = mdate();
                }
            }               
            break;
          
        case WM_KEYDOWN:
            /* the key events are first processed here. The next
             * message processed by this main message loop will be the
             * char translation of the key event */
            intf_WarnMsg( 3, "vout: vout_Manage WM_KEYDOWN" );
            switch( msg.wParam )
            {
            case VK_ESCAPE:
            case VK_F12:
                p_main->p_intf->b_die = 1;
                break;
            }
            TranslateMessage(&msg);
            b_dispatch_msg = FALSE;
            break;
          
        case WM_CHAR:
            intf_WarnMsg( 3, "vout: vout_Manage WM_CHAR" );
            switch( msg.wParam )
            {
            case 'q':
            case 'Q':
                p_main->p_intf->b_die = 1;
                break;
              
            case 'f':                                /* switch to fullscreen */
            case 'F':
                p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
                break;
              
            case 'y':                                  /* switch to hard YUV */
            case 'Y':
                p_vout->i_changes |= VOUT_YUV_CHANGE;
                break;
              
            case 'c':                                    /* toggle grayscale */
            case 'C':
                p_vout->b_grayscale = ! p_vout->b_grayscale;
                p_vout->i_changes |= VOUT_GRAYSCALE_CHANGE;
                break;
              
            case 'i':                                         /* toggle info */
            case 'I':
                p_vout->b_info = ! p_vout->b_info;
                p_vout->i_changes |= VOUT_INFO_CHANGE;
                break;
              
            case 's':                                      /* toggle scaling */
            case 'S':
                p_vout->b_scale = ! p_vout->b_scale;
                p_vout->i_changes |= VOUT_SCALE_CHANGE;
                break;
              
            case ' ':                                    /* toggle interface */
                p_vout->b_interface = ! p_vout->b_interface;
                p_vout->i_changes |= VOUT_INTF_CHANGE;
                break;
              
            case '0': network_ChannelJoin( 0 ); break;
            case '1': network_ChannelJoin( 1 ); break;
            case '2': network_ChannelJoin( 2 ); break;
            case '3': network_ChannelJoin( 3 ); break;
            case '4': network_ChannelJoin( 4 ); break;
            case '5': network_ChannelJoin( 5 ); break;
            case '6': network_ChannelJoin( 6 ); break;
            case '7': network_ChannelJoin( 7 ); break;
            case '8': network_ChannelJoin( 8 ); break;
            case '9': network_ChannelJoin( 9 ); break;
              
            default:
                if( intf_ProcessKey( p_main->p_intf,
                                     (char )msg.wParam ) )
                {
                    intf_DbgMsg( "unhandled key '%c' (%i)",
                                 (char)msg.wParam, msg.wParam );
                }
                break;
            }

#if 0          
        default:
            intf_WarnMsg( 4, "vout: vout_Manage WM Default %i",
                          msg.message );
            break;
#endif

        } /* End Switch */

        /* don't create a never ending loop */
        if( b_dispatch_msg )
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        b_dispatch_msg = TRUE;

    } /* End While() */


    /*
     * Scale Change 
     */
    if( p_vout->i_changes & VOUT_SCALE_CHANGE )
    {
        intf_WarnMsg( 3, "vout: vout_Manage Scale Change" );
        if( DirectXUpdateOverlay( p_vout ) )
            /* failed so try again next time */
            PostMessage( p_vout->p_sys->hwnd, WM_CHAR, (WPARAM)'S', 0);
        p_vout->i_changes &= ~VOUT_SCALE_CHANGE;
    }

    /*
     * Size Change 
     */
    if( p_vout->i_changes & VOUT_SIZE_CHANGE )
    {
        intf_WarnMsg( 3, "vout: vout_Manage Size Change" );
        if( DirectXUpdateOverlay( p_vout ) )
            /* failed so try again next time */
	    PostMessage( p_vout->p_sys->hwnd, WM_APP, 0, 0);
        p_vout->i_changes &= ~VOUT_SIZE_CHANGE;
    }

    /*
     * YUV Change 
     */
    if( p_vout->i_changes & VOUT_YUV_CHANGE )
    {
        p_vout->b_need_render = ! p_vout->b_need_render;
        
        /* Need to reopen display */
        DirectXCloseSurface( p_vout );
        if( DirectXCreateSurface( p_vout ) )
        {
          intf_ErrMsg( "error: can't reopen display after YUV change" );
          return( 1 );
        }

        /* Repaint the window background (needed by the overlay surface) */
        if( !p_vout->b_need_render )
        {
            InvalidateRect( p_vout->p_sys->hwnd, NULL, TRUE );
            p_vout->p_sys->b_display_enabled = 1;
            if( DirectXUpdateOverlay( p_vout ) )
                /* failed so try again next time */
                PostMessage( p_vout->p_sys->hwnd, WM_CHAR, (WPARAM)'S', 0);
        }
        p_vout->i_changes &= ~VOUT_YUV_CHANGE;
    }

    /*
     * Fullscreen change
     */
    if( p_vout->i_changes & VOUT_FULLSCREEN_CHANGE )
    {
        p_vout->b_fullscreen = ! p_vout->b_fullscreen;

        /* We need to switch between Maximized and Normal sized window */
        window_placement.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement( p_vout->p_sys->hwnd, &window_placement );
        if( p_vout->b_fullscreen )
        {
            /* Maximized window */
            window_placement.showCmd = SW_SHOWMAXIMIZED;
            /* Change window style, no borders and no title bar */
            SetWindowLong( p_vout->p_sys->hwnd, GWL_STYLE, 0 );

        }
        else
        {
            /* Normal window */
            window_placement.showCmd = SW_SHOWNORMAL;
            /* Change window style, borders and title bar */
            SetWindowLong( p_vout->p_sys->hwnd, GWL_STYLE,
                           WS_OVERLAPPEDWINDOW | WS_SIZEBOX | WS_VISIBLE );
        }

        SetWindowPlacement( p_vout->p_sys->hwnd, &window_placement );

        p_vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
    }

    /*
     * Pointer change
     */
    if( ! p_vout->p_sys->b_cursor_autohidden &&
        ( mdate() - p_vout->p_sys->i_lastmoved > 5000000 ) )
    {
        /* Hide the mouse automatically */
        p_vout->p_sys->b_cursor_autohidden = 1;
        ShowCursor( FALSE );
    }

    if( p_vout->i_changes & VOUT_CURSOR_CHANGE )
    {
        p_vout->p_sys->b_cursor = ! p_vout->p_sys->b_cursor;

        ShowCursor( p_vout->p_sys->b_cursor &&
                     ! p_vout->p_sys->b_cursor_autohidden );

        p_vout->i_changes &= ~VOUT_CURSOR_CHANGE;
    }

    return( 0 );
}

/*****************************************************************************
 * vout_SetPalette: sets an 8 bpp palette
 *****************************************************************************
 * This function sets the palette given as an argument. It does not return
 * anything, but could later send information on which colors it was unable
 * to set.
 *****************************************************************************/
static void vout_SetPalette( p_vout_thread_t p_vout, u16 *red, u16 *green,
                         u16 *blue, u16 *transp)
{
    /* Nothing yet */
    return;
}

/*****************************************************************************
 * vout_Display: displays previously rendered output
 *****************************************************************************
 * This function send the currently rendered image to the display, wait until
 * it is displayed and switch the two rendering buffer, preparing next frame.
 *****************************************************************************/
static void vout_Display( vout_thread_t *p_vout )
{
    DDSURFACEDESC ddsd;
    HRESULT       dxresult;
    int           i;
    int           i_image_width;
    int           i_image_height;

    if( (p_vout->p_sys->p_display == NULL) )
    {
        intf_WarnMsg( 3, "vout error: vout_Display no display!!" );
        return;
    }

    /* if the size of the decoded pictures has changed then we close the
     * video surface (which doesn't have the right size anymore). */
    i_image_width = ( p_vout->p_rendered_pic ) ?
      p_vout->p_rendered_pic->i_width : p_vout->p_sys->i_image_width;
    i_image_height = ( p_vout->p_rendered_pic ) ?
      p_vout->p_rendered_pic->i_height : p_vout->p_sys->i_image_height;

    if( p_vout->p_sys->i_image_width != i_image_width
        || p_vout->p_sys->i_image_height != i_image_height )
    {
        intf_WarnMsg( 3, "vout: video surface size changed" );
        p_vout->p_sys->i_image_width = i_image_width;
        p_vout->p_sys->i_image_height = i_image_height;
        DirectXCloseSurface( p_vout );
    }

    if( p_vout->b_need_render )
    {
        RECT  rect_window;
        POINT point_window;
  
        /* Nothing yet */
        if( p_vout->p_sys->p_surface == NULL )
        {
            intf_WarnMsg( 3, "vout: no video surface, open one..." );
            if( DirectXCreateSurface( p_vout ) )
            {
                intf_WarnMsg( 3, "vout: cannot open a new video surface !!" );
                return;
            }
            /* Display the surface */
            p_vout->p_sys->b_display_enabled = 1;
        }

        /* Now get the coordinates of the window. We don't actually want the
         * window coordinates but these of the usable surface inside the window
         * By specification GetClientRect will always set rect_window.left and
         * rect_window.top to 0 because the Client area is always relative to
         * the container window */
        GetClientRect(p_vout->p_sys->hwnd, &rect_window);
        
        point_window.x = 0;
        point_window.y = 0;
        ClientToScreen(p_vout->p_sys->hwnd, &point_window);
        rect_window.left = point_window.x;
        rect_window.top = point_window.y;
        
        point_window.x = rect_window.right;
        point_window.y = rect_window.bottom;
        ClientToScreen(p_vout->p_sys->hwnd, &point_window);
        rect_window.right = point_window.x;
        rect_window.bottom = point_window.y;

        /* We want to keep the aspect ratio of the video */
        if( p_vout->b_scale )
        {
            DirectXKeepAspectRatio( p_vout, &rect_window );
        }

        /* Blit video surface to display */
        dxresult = IDirectDrawSurface3_Blt(p_vout->p_sys->p_display,
                                           &rect_window,
                                           p_vout->p_sys->p_surface,
                                           NULL,
                                           0, NULL );
        if( dxresult != DD_OK )
        {
            intf_WarnMsg( 3, "vout: could not Blit the surface" );
            return;
        }

    }
    else
    {
        /*
         * p_vout->p_rendered_pic->p_y/u/v contains the YUV buffers to
         * render
         */
        /* TODO: support for streams other than 4:2:0 */

        if( p_vout->p_sys->p_surface == NULL )
        {
            intf_WarnMsg( 3, "vout: no video surface, open one..." );
            if( DirectXCreateSurface( p_vout ) )
            {
                intf_WarnMsg( 3, "vout: cannot open a new video surface !!" );
                return;
            }
        }

        /* Lock the overlay surface */
        memset( &ddsd, 0, sizeof( DDSURFACEDESC ));
        ddsd.dwSize = sizeof(DDSURFACEDESC);
        dxresult = IDirectDrawSurface3_Lock(p_vout->p_sys->p_surface, NULL,
                                            &ddsd, DDLOCK_NOSYSLOCK, NULL);
        if ( dxresult == DDERR_SURFACELOST )
        {
            /* Your surface can be lost (thanks to windows) so be sure
             * to check this and restore it if needed */
            dxresult = IDirectDrawSurface3_Restore( p_vout->p_sys->p_surface );
            dxresult = IDirectDrawSurface3_Lock( p_vout->p_sys->p_surface,
                                                 NULL, &ddsd, DDLOCK_NOSYSLOCK
                                                 | DDLOCK_WAIT, NULL);
        }
        if( dxresult != DD_OK )
        {
            intf_WarnMsg( 3, "vout: could not lock the surface" );
            return;
        }

        /* Now we can do the actual image copy.
         * The copy has to be done line by line because of the special case
         * when the Pitch does not equal the width of the picture */
        for( i=0; i < ddsd.dwHeight/2; i++)
        {
#ifdef NONAMELESSUNION
            /* copy Y, we copy two lines at once */
            memcpy(ddsd.lpSurface + i*2*ddsd.u1.lPitch,
                   p_vout->p_rendered_pic->p_y + i*2*i_image_width,
                   i_image_width);
            memcpy(ddsd.lpSurface + (i*2+1)*ddsd.u1.lPitch,
                   p_vout->p_rendered_pic->p_y + (i*2+1)*i_image_width,
                   i_image_width);
            /* then V */
            memcpy((ddsd.lpSurface + ddsd.dwHeight * ddsd.u1.lPitch)
                      + i * ddsd.u1.lPitch/2,
                   p_vout->p_rendered_pic->p_v + i*i_image_width/2,
                   i_image_width/2);
            /* and U */
            memcpy((ddsd.lpSurface + ddsd.dwHeight * ddsd.u1.lPitch)
                      + (ddsd.dwHeight * ddsd.u1.lPitch/4)
                      + i * ddsd.u1.lPitch/2,
                   p_vout->p_rendered_pic->p_u + i*i_image_width/2,
                   i_image_width/2);
#else
            /* copy Y, we copy two lines at once */
            memcpy(ddsd.lpSurface + i*2*ddsd.lPitch,
                   p_vout->p_rendered_pic->p_y + i*2*i_image_width,
                   i_image_width);
            memcpy(ddsd.lpSurface + (i*2+1)*ddsd.lPitch,
                   p_vout->p_rendered_pic->p_y + (i*2+1)*i_image_width,
                   i_image_width);
            /* then V */
            memcpy((ddsd.lpSurface + ddsd.dwHeight * ddsd.lPitch)
                      + i * ddsd.lPitch/2,
                   p_vout->p_rendered_pic->p_v + i*i_image_width/2,
                   i_image_width/2);
            /* and U */
            memcpy((ddsd.lpSurface + ddsd.dwHeight * ddsd.lPitch)
                      + (ddsd.dwHeight * ddsd.lPitch/4)
                      + i * ddsd.lPitch/2,
                   p_vout->p_rendered_pic->p_u + i*i_image_width/2,
                   i_image_width/2);
#endif /* NONAMELESSUNION */

        }

        /* Unlock the Surface */
        dxresult = IDirectDrawSurface3_Unlock(p_vout->p_sys->p_surface,
                                              ddsd.lpSurface );

        /* If display not enabled yet then enable */
        if( !p_vout->p_sys->b_display_enabled )
        {
            p_vout->p_sys->b_display_enabled = 1;
            DirectXUpdateOverlay( p_vout );
        }

    }

    /* The first time this function is called it enables the display */
    p_vout->p_sys->b_display_enabled = 1;

}


/* following functions are local */


/*****************************************************************************
 * DirectXEventProc: This is the window event processing function.
 *****************************************************************************
 * On Windows, when you create a window you have to attach an event processing
 * function to it. The aim of this function is to manage "Queued Messages" and
 * "Nonqueued Messages".
 * Queued Messages are those picked up and retransmitted by vout_Manage
 * (using the GetMessage function).
 * Nonqueued Messages are those that Windows will send directly to this
 * function (like WM_DESTROY, WM_WINDOWPOSCHANGED...)
 *****************************************************************************/
long FAR PASCAL DirectXEventProc( HWND hwnd, UINT message,
                                  WPARAM wParam, LPARAM lParam )
{
    switch( message )
    {

#if 0
    case WM_APP:
        intf_WarnMsg( 3, "vout: WinProc WM_APP" );
        break;

    case WM_ACTIVATE:
        intf_WarnMsg( 4, "vout: WinProc WM_ACTIVED" );
        break;

    case WM_CREATE:
        intf_WarnMsg( 4, "vout: WinProc WM_CREATE" );
        break;

    /* the user wants to close the window */
    case WM_CLOSE:
        intf_WarnMsg( 4, "vout: WinProc WM_CLOSE" );
        break;
#endif

    /* the window has been closed so shut down everything now */
    case WM_DESTROY:
        intf_WarnMsg( 4, "vout: WinProc WM_DESTROY" );
        PostQuitMessage( 0 );
        break;

    case WM_SYSCOMMAND:
        switch (wParam)
        {
            case SC_SCREENSAVE:                     /* catch the screensaver */
            case SC_MONITORPOWER:              /* catch the monitor turn-off */
            intf_WarnMsg( 3, "vout: WinProc WM_SYSCOMMAND" );
            return 0;                      /* this stops them from happening */
        }
        break;

#if 0
    case WM_MOVE:
        intf_WarnMsg( 4, "vout: WinProc WM_MOVE" );
        break;

    case WM_SIZE:
        intf_WarnMsg( 4, "vout: WinProc WM_SIZE" );
        break;

    case WM_MOVING:
        intf_WarnMsg( 4, "vout: WinProc WM_MOVING" );
        break;

    case WM_ENTERSIZEMOVE:
        intf_WarnMsg( 4, "vout: WinProc WM_ENTERSIZEMOVE" );
        break;

    case WM_SIZING:
        intf_WarnMsg( 4, "vout: WinProc WM_SIZING" );
        break;
#endif

    case WM_WINDOWPOSCHANGED:
        intf_WarnMsg( 3, "vout: WinProc WM_WINDOWPOSCHANGED" );
        PostMessage( hwnd, WM_APP, 0, 0);
        break;

#if 0
    case WM_WINDOWPOSCHANGING:
        intf_WarnMsg( 3, "vout: WinProc WM_WINDOWPOSCHANGING" );
        break;

    case WM_PAINT:
        intf_WarnMsg( 4, "vout: WinProc WM_PAINT" );
        break;

    case WM_ERASEBKGND:
        intf_WarnMsg( 4, "vout: WinProc WM_ERASEBKGND" );
        break;

    default:
        intf_WarnMsg( 4, "vout: WinProc WM Default %i", message );
        break;
#endif
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

/*****************************************************************************
 * DirectXCreateWindow: create a windows window where the video will play.
 *****************************************************************************
 * Before creating a direct draw surface, we need to create a window in which
 * the video will be displayed. This window will also allow us to capture the
 * events.
 *****************************************************************************/
static int DirectXCreateWindow( vout_thread_t *p_vout )
{
    HINSTANCE  hInstance;
    WNDCLASSEX wc;                                /* window class components */
    RECT       rect_window;
    COLORREF   colorkey; 
    HDC        hdc;

    intf_WarnMsg( 3, "vout: WinDX WinDXCreateWindow" );

    /* get this module's instance */
    hInstance = GetModuleHandle(NULL);

    /* Create a BRUSH that will be used by Windows to paint the window
     * background.
     * This window background is important for us as it will be used by the
     * graphics card to display the overlay.
     * This is why we carefully choose the color for this background, the goal
     * being to choose a color which isn't complete black but nearly. We
     * obviously don't want to use black as a colorkey for the overlay because
     * black is one of the most used color and thus would give us undesirable
     * effects */
    /* the first step is to find the colorkey we want to use. The difficulty
     * comes from the potential dithering (depends on the display depth)
     * because we need to know the real RGB value of the chosen colorkey */
    hdc = GetDC( GetDesktopWindow() );
    for( colorkey = 5; colorkey < 0xFF /*all shades of red*/; colorkey++ )
    {
        if( colorkey == GetNearestColor( hdc, colorkey ) )
          break;
    }
    intf_WarnMsg(3,"vout: DirectXCreateWindow background color:%i", colorkey);
    ReleaseDC( p_vout->p_sys->hwnd, hdc );

    /* create the actual brush */  
    p_vout->p_sys->hbrush = CreateSolidBrush(colorkey);
    p_vout->p_sys->i_colorkey = (int)colorkey;

    /* fill in the window class structure */
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = 0;                               /* no special styles */
    wc.lpfnWndProc   = (WNDPROC)DirectXEventProc;           /* event handler */
    wc.cbClsExtra    = 0;                             /* no extra class data */
    wc.cbWndExtra    = 0;                            /* no extra window data */
    wc.hInstance     = hInstance;                                /* instance */
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION); /* load the vlc icon */
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW); /* load a default cursor */
    wc.hbrBackground = p_vout->p_sys->hbrush;            /* background color */
    wc.lpszMenuName  = NULL;                                      /* no menu */
    wc.lpszClassName = "VLC DirectX";                 /* use a special class */
    wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION); /* load the vlc icon */

    /* register the window class */
    if (!RegisterClassEx(&wc))
    {
        intf_WarnMsg( 3, "vout: DirectXCreateWindow register window FAILED" );
        return (1);
    }

    /* when you create a window you give the dimensions you wish it to have.
     * Unfortunatly these dimensions will include the borders and title bar.
     * We use the following function to find out the size of the window
     * corresponding to the useable surface we want */
    rect_window.top    = 10;
    rect_window.left   = 10;
    rect_window.right  = rect_window.left + p_vout->p_sys->i_window_width;
    rect_window.bottom = rect_window.top + p_vout->p_sys->i_window_height;
    AdjustWindowRect( &rect_window, WS_OVERLAPPEDWINDOW|WS_SIZEBOX, 0 );

    /* create the window */
    p_vout->p_sys->hwnd = CreateWindow("VLC DirectX",/* name of window class */
                    "VLC DirectX",                  /* window title bar text */
                    WS_OVERLAPPEDWINDOW
                    | WS_SIZEBOX | WS_VISIBLE,               /* window style */
                    10,                              /* default X coordinate */
                    10,                              /* default Y coordinate */
                    rect_window.right - rect_window.left,    /* window width */
                    rect_window.bottom - rect_window.top,   /* window height */
                    NULL,                                /* no parent window */
                    NULL,                          /* no menu in this window */
                    hInstance,            /* handle of this program instance */
                    NULL);                        /* no additional arguments */

    if (p_vout->p_sys->hwnd == NULL) {
        intf_WarnMsg( 3, "vout: DirectXCreateWindow create window FAILED" );
        return (1);
    }

    /* now display the window */
    ShowWindow(p_vout->p_sys->hwnd, SW_SHOW);

    return ( 0 );
}

/*****************************************************************************
 * DirectXInitDDraw: Takes care of all the DirectDraw initialisations
 *****************************************************************************
 * This function initialise and allocate resources for DirectDraw.
 *****************************************************************************/
static int DirectXInitDDraw( vout_thread_t *p_vout )
{
    HRESULT    dxresult;
    HRESULT    (WINAPI *OurDirectDrawCreate)(GUID *,LPDIRECTDRAW *,IUnknown *);
    LPDIRECTDRAW  p_ddobject;

    intf_WarnMsg( 3, "vout: DirectXInitDDraw" );

    /* load direct draw DLL */
    p_vout->p_sys->hddraw_dll = LoadLibrary("DDRAW.DLL");
    if( p_vout->p_sys->hddraw_dll == NULL )
    {
        intf_WarnMsg( 3, "vout: DirectXInitDDraw failed loading ddraw.dll" );
        return( 1 );
    }
      
    OurDirectDrawCreate = 
      (void *)GetProcAddress(p_vout->p_sys->hddraw_dll, "DirectDrawCreate");
    if ( OurDirectDrawCreate == NULL )
    {
        intf_ErrMsg( "vout error: DirectXInitDDraw failed GetProcAddress" );
        FreeLibrary( p_vout->p_sys->hddraw_dll );
        p_vout->p_sys->hddraw_dll = NULL;
        return( 1 );    
    }

    /* Initialize DirectDraw now */
    dxresult = OurDirectDrawCreate( NULL, &p_ddobject, NULL );
    if( dxresult != DD_OK )
    {
        intf_ErrMsg( "vout error: DirectXInitDDraw can't initialize DDraw" );
        p_vout->p_sys->p_ddobject = NULL;
        FreeLibrary( p_vout->p_sys->hddraw_dll );
        p_vout->p_sys->hddraw_dll = NULL;
        return( 1 );
    }

    /* Set DirectDraw Cooperative level, ie what control we want over Windows
     * display */
    dxresult = IDirectDraw_SetCooperativeLevel( p_ddobject,
                                           p_vout->p_sys->hwnd, DDSCL_NORMAL );
    if( dxresult != DD_OK )
    {
        intf_ErrMsg( "vout error: can't set direct draw cooperative level." );
        IDirectDraw_Release( p_ddobject );
        p_vout->p_sys->p_ddobject = NULL;
        FreeLibrary( p_vout->p_sys->hddraw_dll );
        p_vout->p_sys->hddraw_dll = NULL;
        return( 1 );
    }

    /* Get the IDirectDraw2 interface */
    dxresult = IDirectDraw_QueryInterface( p_ddobject, &IID_IDirectDraw2,
                                        (LPVOID *)&p_vout->p_sys->p_ddobject );
    if( dxresult != DD_OK )
    {
        intf_ErrMsg( "vout error: can't get IDirectDraw2 interface." );
        IDirectDraw_Release( p_ddobject );
        p_vout->p_sys->p_ddobject = NULL;
        FreeLibrary( p_vout->p_sys->hddraw_dll );
        p_vout->p_sys->hddraw_dll = NULL;
        return( 1 );
    }
    else
    {
        /* Release the unused interface */
        IDirectDraw_Release( p_ddobject );
    }

    return( 0 );
}

/*****************************************************************************
 * DirectXCreateDisplay: create the DirectDraw display.
 *****************************************************************************
 * Create and initialize display according to preferences specified in the vout
 * thread fields.
 *****************************************************************************/
static int DirectXCreateDisplay( vout_thread_t *p_vout )
{
    HRESULT              dxresult;
    DDSURFACEDESC        ddsd;
    LPDIRECTDRAWSURFACE  p_display;
    DDPIXELFORMAT        ddpfPixelFormat;

    intf_WarnMsg( 3, "vout: DirectXCreateDisplay" );

    /* Now create the primary surface. This surface is what you actually see
     * on your screen */
    memset( &ddsd, 0, sizeof( DDSURFACEDESC ));
    ddsd.dwSize = sizeof(DDSURFACEDESC);
    ddsd.dwFlags = DDSD_CAPS;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

    dxresult = IDirectDraw2_CreateSurface( p_vout->p_sys->p_ddobject,
                                           &ddsd,
                                           &p_display, NULL );
    if( dxresult != DD_OK )
    {
        intf_ErrMsg( "vout error: can't create direct draw primary surface." );
        p_vout->p_sys->p_display = NULL;
        return( 1 );
    }

    dxresult = IDirectDrawSurface_QueryInterface( p_display,
                                         &IID_IDirectDrawSurface3,
                                         (LPVOID *)&p_vout->p_sys->p_display );
    if ( dxresult != DD_OK )
    {
        intf_ErrMsg( "vout error: can't get IDirectDrawSurface3 interface." );
        IDirectDrawSurface_Release( p_display );
        p_vout->p_sys->p_display = NULL;
        return( 1 );
    }
    else
    {
        /* Release the old interface */
        IDirectDrawSurface_Release( p_display );
    }


    /* We need to fill in some information for the video output thread.
     * We do this here because it must be done before the video_output
     * thread enters its main loop - and DirectXCreateSurface can be called
     * after that ! */
    ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
    IDirectDrawSurface3_GetPixelFormat( p_vout->p_sys->p_display,
                                        &ddpfPixelFormat );
#ifdef NONAMELESSUNION
    p_vout->i_screen_depth =    ddpfPixelFormat.u1.dwRGBBitCount;
    p_vout->i_bytes_per_pixel = ddpfPixelFormat.u1.dwRGBBitCount/8;
    
    p_vout->i_red_mask =        ddpfPixelFormat.u2.dwRBitMask;
    p_vout->i_green_mask =      ddpfPixelFormat.u3.dwGBitMask;
    p_vout->i_blue_mask =       ddpfPixelFormat.u4.dwBBitMask;
#else
    p_vout->i_screen_depth =    ddpfPixelFormat.dwRGBBitCount;
    p_vout->i_bytes_per_pixel = ddpfPixelFormat.dwRGBBitCount/8;

    p_vout->i_red_mask =        ddpfPixelFormat.dwRBitMask;
    p_vout->i_green_mask =      ddpfPixelFormat.dwGBitMask;
    p_vout->i_blue_mask =       ddpfPixelFormat.dwBBitMask;
#endif /* NONAMELESSUNION */

    /* Create a video surface. This function will try to create an
     * YUV overlay first and if it can't it will create a simple RGB surface */
    if( DirectXCreateSurface( p_vout ) )
    {
        intf_ErrMsg( "vout error: can't create a video surface." );
        IDirectDrawSurface3_Release( p_vout->p_sys->p_display );
        p_vout->p_sys->p_display = NULL;
        return( 1 );
    }
      
    return( 0 );
}

/*****************************************************************************
 * DirectXCreateSurface: create an YUV overlay or RGB surface for the video.
 *****************************************************************************
 * The best method of display is with an YUV overlay because the YUV->RGB
 * conversion is done in hardware, so we'll try to create this surface first.
 * If we fail, we'll try to create a plain RGB surface.
 * ( Maybe we could also try an RGB overlay surface, which could have hardware
 * scaling and which would also be faster in window mode because you don't
 * need to do any blitting to the main display...)
 *****************************************************************************/
static int DirectXCreateSurface( vout_thread_t *p_vout )
{
    HRESULT dxresult;
    DDSURFACEDESC ddsd;
    LPDIRECTDRAWSURFACE p_surface;
    DDCAPS ddcaps;

    intf_WarnMsg( 3, "vout: DirectXCreateSurface" );

    /* Disable display */
    p_vout->p_sys->b_display_enabled = 0;

#if 1
    /* Probe the capabilities of the hardware */
    /* This is just an indication of whether or not we'll support overlay,
     * but with this test we don't know if we support YUV overlay */
    memset( &ddcaps, 0, sizeof( DDCAPS ));
    ddcaps.dwSize = sizeof(DDCAPS);
    dxresult = IDirectDraw2_GetCaps( p_vout->p_sys->p_ddobject,
                                     &ddcaps, NULL );
    if(dxresult != DD_OK )
    {
        intf_WarnMsg( 3,"vout error: can't get caps." );
    }
    else
    {
        BOOL bHasOverlay, bHasColorKey, bCanStretch;

        /* Determine if the hardware supports overlay surfaces */
        bHasOverlay = ((ddcaps.dwCaps & DDCAPS_OVERLAY) ==
                       DDCAPS_OVERLAY) ? TRUE : FALSE;
        /* Determine if the hardware supports colorkeying */
        bHasColorKey = ((ddcaps.dwCaps & DDCAPS_COLORKEY) ==
                        DDCAPS_COLORKEY) ? TRUE : FALSE;
        /* Determine if the hardware supports scaling of the overlay surface */
        bCanStretch = ((ddcaps.dwCaps & DDCAPS_OVERLAYSTRETCH) ==
                       DDCAPS_OVERLAYSTRETCH) ? TRUE : FALSE;
        intf_WarnMsg( 3, "vout: Dx Caps: overlay=%i colorkey=%i stretch=%i",
                         bHasOverlay, bHasColorKey, bCanStretch );

#if 0
        if( !bHasOverlay ) p_vout->b_need_render = 1;
#endif
    }
#endif


    /* Create the video surface */
    if( !p_vout->b_need_render )
    {
        /* Now try to create the YUV overlay surface.
         * This overlay will be displayed on top of the primary surface.
         * A color key is used to determine whether or not the overlay will be
         * displayed, ie the overlay will be displayed in place of the primary
         * surface wherever the primary surface will have this color.
         * The video window has been created with a background of this color so
         * the overlay will be only displayed on top of this window */

        memset( &ddsd, 0, sizeof( DDSURFACEDESC ));
        ddsd.dwSize = sizeof(DDSURFACEDESC);
        ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
        ddsd.ddpfPixelFormat.dwFlags = DDPF_FOURCC;
        ddsd.ddpfPixelFormat.dwFourCC = mmioFOURCC('Y','V','1','2');
#ifdef NONAMELESSUNION
        ddsd.ddpfPixelFormat.u1.dwYUVBitCount = 16;
#else
        ddsd.ddpfPixelFormat.dwYUVBitCount = 16;
#endif
        ddsd.dwFlags = DDSD_CAPS |
                       DDSD_HEIGHT |
                       DDSD_WIDTH |
                       DDSD_PIXELFORMAT;
        ddsd.ddsCaps.dwCaps = DDSCAPS_OVERLAY | DDSCAPS_VIDEOMEMORY;
        ddsd.dwHeight =  p_vout->p_sys->i_image_height;
        ddsd.dwWidth =  p_vout->p_sys->i_image_width;
        ddsd.dwBackBufferCount = 1;                       /* One back buffer */

        dxresult = IDirectDraw2_CreateSurface( p_vout->p_sys->p_ddobject,
                                               &ddsd, &p_surface, NULL );
        if( dxresult == DD_OK )
        {
            intf_WarnMsg( 3,"vout: DirectX YUV overlay created successfully" );
        }
        else
        {
            intf_ErrMsg( "vout error: can't create YUV overlay surface." );
            p_vout->b_need_render = 1;
        }
    }

    if( p_vout->b_need_render )
    {
        /* Now try to create a plain RGB surface. */
        memset( &ddsd, 0, sizeof( DDSURFACEDESC ));
        ddsd.dwSize = sizeof(DDSURFACEDESC);
        ddsd.dwFlags = DDSD_HEIGHT |
                       DDSD_WIDTH |
                       DDSD_CAPS;
        ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
        ddsd.dwHeight =  p_vout->p_sys->i_image_height;
        ddsd.dwWidth =  p_vout->p_sys->i_image_width;

        dxresult = IDirectDraw2_CreateSurface( p_vout->p_sys->p_ddobject,
                                               &ddsd, &p_surface, NULL );
        if( dxresult == DD_OK )
        {
            intf_WarnMsg( 3,"vout: DirectX RGB surface created successfully" );
        }
        else
        {
            intf_ErrMsg( "vout error: can't create RGB surface." );
            p_vout->p_sys->p_surface = NULL;
            return( 1 );
        }
    }
      
    /* Now that the surface is created, try to get a newer DirectX interface */
    dxresult = IDirectDrawSurface_QueryInterface( p_surface,
                                         &IID_IDirectDrawSurface3,
                                         (LPVOID *)&p_vout->p_sys->p_surface );
    if ( dxresult != DD_OK )
    {
        intf_ErrMsg( "vout error: can't get IDirectDrawSurface3 interface." );
        IDirectDrawSurface_Release( p_surface );
        p_vout->p_sys->p_surface = NULL;
        return( 1 );
    }
    else
    {
        /* Release the old interface */
        IDirectDrawSurface_Release( p_surface );
    }

    if( !p_vout->b_need_render )
    {
        /* Hide the overlay for now */
        IDirectDrawSurface3_UpdateOverlay(p_vout->p_sys->p_surface,
                                          NULL,
                                          p_vout->p_sys->p_display,
                                          NULL,
                                          DDOVER_HIDE,
                                          NULL);
    }
    else
    {
         DirectXCreateClipper( p_vout );
    }


    /* From now on, do some initialisation for video_output */

    /* if we want a valid pointer to the surface memory, we must lock
     * the surface */

    memset( &ddsd, 0, sizeof( DDSURFACEDESC ));
    ddsd.dwSize = sizeof(DDSURFACEDESC);
    dxresult = IDirectDrawSurface3_Lock( p_vout->p_sys->p_surface, NULL, &ddsd,
                                         DDLOCK_NOSYSLOCK | DDLOCK_WAIT, NULL);
    if ( dxresult == DDERR_SURFACELOST )
    {
        /* Your surface can be lost so be sure
         * to check this and restore it if needed */
        dxresult = IDirectDrawSurface3_Restore( p_vout->p_sys->p_surface );
        dxresult = IDirectDrawSurface3_Lock( p_vout->p_sys->p_surface,
                                             NULL, &ddsd, DDLOCK_NOSYSLOCK
                                             | DDLOCK_WAIT, NULL);
    }
    if( dxresult != DD_OK )
    {
        intf_ErrMsg( "vout: DirectXCreateDisplay could not lock the surface" );
        return( 1 );
    }

    /* Set the pointer to the surface memory */
    p_vout->p_sys->p_directx_buf[ 0 ] = ddsd.lpSurface;
    /* back buffer, none for now */
    p_vout->p_sys->p_directx_buf[ 1 ] = ddsd.lpSurface;

    /* Set thread information */
    p_vout->i_width =  ddsd.dwWidth;
    p_vout->i_height = ddsd.dwHeight;
#ifdef NONAMELESSUNION
    p_vout->i_bytes_per_line =  ddsd.u1.lPitch;
#else
    p_vout->i_bytes_per_line =  ddsd.lPitch;
#endif /* NONAMELESSUNION */


    if( p_vout->b_need_render )
    {
        /* For an RGB surface we need to fill in some more info */
#ifdef NONAMELESSUNION
        p_vout->i_screen_depth =    ddsd.ddpfPixelFormat.u1.dwRGBBitCount;
        p_vout->i_bytes_per_pixel = ddsd.ddpfPixelFormat.u1.dwRGBBitCount/8;

        p_vout->i_red_mask =        ddsd.ddpfPixelFormat.u2.dwRBitMask;
        p_vout->i_green_mask =      ddsd.ddpfPixelFormat.u3.dwGBitMask;
        p_vout->i_blue_mask =       ddsd.ddpfPixelFormat.u4.dwBBitMask;
#else
        p_vout->i_screen_depth =    ddsd.ddpfPixelFormat.dwRGBBitCount;
        p_vout->i_bytes_per_pixel = ddsd.ddpfPixelFormat.dwRGBBitCount/8;

        p_vout->i_red_mask =        ddsd.ddpfPixelFormat.dwRBitMask;
        p_vout->i_green_mask =      ddsd.ddpfPixelFormat.dwGBitMask;
        p_vout->i_blue_mask =       ddsd.ddpfPixelFormat.dwBBitMask;

#endif /* NONAMELESSUNION */
    }

    /* Unlock the Surface */
    dxresult = IDirectDrawSurface3_Unlock(p_vout->p_sys->p_surface,
                                          ddsd.lpSurface );

    /* Set and initialize buffers */
    p_vout->pf_setbuffers( p_vout, p_vout->p_sys->p_directx_buf[ 0 ],
                           p_vout->p_sys->p_directx_buf[ 1 ] );


    return ( 0 );
}


/*****************************************************************************
 * DirectXCreateClipper: Create a clipper that will be used when blitting the
 *                       RGB surface to the main display.
 *****************************************************************************
 * This clipper prevents us to modify by mistake anything on the screen
 * which doesn't belong to our window. For example when a part of our video
 * window is hidden by another window.
 *****************************************************************************/
static int DirectXCreateClipper( vout_thread_t *p_vout )
{
    HRESULT dxresult;

    intf_WarnMsg( 3, "vout: DirectXCreateClipper" );

    /* Create the clipper */
    dxresult = IDirectDraw2_CreateClipper( p_vout->p_sys->p_ddobject, 0,
                                           &p_vout->p_sys->p_clipper, NULL );
    if( dxresult != DD_OK )
    {
        intf_WarnMsg( 3, "vout: DirectXCreateClipper can't create clipper." );
        IDirectDrawSurface_Release( p_vout->p_sys->p_clipper );
        p_vout->p_sys->p_clipper = NULL;
        return( 1 );
    }
    
    /* associate the clipper to the window */
    dxresult = IDirectDrawClipper_SetHWnd(p_vout->p_sys->p_clipper, 0,
                                          p_vout->p_sys->hwnd);
    if( dxresult != DD_OK )
    {
        intf_WarnMsg( 3,
            "vout: DirectXCreateClipper can't attach clipper to window." );
        IDirectDrawSurface_Release( p_vout->p_sys->p_clipper );
        p_vout->p_sys->p_clipper = NULL;
        return( 1 );
    }
    
    /* associate the clipper with the surface */
    dxresult = IDirectDrawSurface_SetClipper(p_vout->p_sys->p_display,
                                             p_vout->p_sys->p_clipper);
    if( dxresult != DD_OK )
    {
        intf_WarnMsg( 3,
            "vout: DirectXCreateClipper can't attach clipper to surface." );
        IDirectDrawSurface_Release( p_vout->p_sys->p_clipper );
        p_vout->p_sys->p_clipper = NULL;
        return( 1 );
    }    
     
    return( 0 );
}


/*****************************************************************************
 * DirectXUpdateOverlay: Move or resize overlay surface on video display.
 *****************************************************************************
 * This function is used to move or resize an overlay surface on the screen.
 * Ususally the overlay is moved by the user and thus, by a move or resize
 * event (in vout_Manage).
 *****************************************************************************/
static int DirectXUpdateOverlay( vout_thread_t *p_vout )
{
    DDOVERLAYFX     ddofx;
    RECT            rect_window, rect_window_backup, rect_image;
    POINT           point_window;
    DWORD           dwFlags;
    HRESULT         dxresult;
    DWORD           dw_colorkey;
    DDPIXELFORMAT   pixel_format;
    DDSURFACEDESC   ddsd;

    if( p_vout->p_sys->p_surface == NULL || p_vout->b_need_render )
    {
        intf_WarnMsg( 3, "vout: DirectXUpdateOverlay no overlay !!" );
        return( 0 );
    }

    if( !p_vout->p_rendered_pic )
    {
        intf_WarnMsg( 3, "vout: DirectXUpdateOverlay p_rendered_pic=NULL !" );
        return( 1 );
    }

    if( !p_vout->p_sys->b_display_enabled )
    {
        return( 0 );
    }

    /* Now get the coordinates of the window. We don't actually want the
     * window coordinates but these of the usable surface inside the window.
     * By specification GetClientRect will always set rect_window.left and
     * rect_window.top to 0 because the Client area is always relative to the
     * container window */
    GetClientRect(p_vout->p_sys->hwnd, &rect_window);

    point_window.x = 0;
    point_window.y = 0;
    ClientToScreen(p_vout->p_sys->hwnd, &point_window);
    rect_window.left = point_window.x;
    rect_window.top = point_window.y;

    point_window.x = rect_window.right;
    point_window.y = rect_window.bottom;
    ClientToScreen(p_vout->p_sys->hwnd, &point_window);
    rect_window.right = point_window.x;
    rect_window.bottom = point_window.y;


    /* We want to keep the aspect ratio of the video */
    if( p_vout->b_scale )
    {
        DirectXKeepAspectRatio( p_vout, &rect_window );
    }

    /* It seems we can't feed the UpdateOverlay directdraw function with
     * negative values so we have to clip the computed rectangles */
    memset( &ddsd, 0, sizeof( DDSURFACEDESC ));
    ddsd.dwSize = sizeof(DDSURFACEDESC);
    ddsd.dwFlags = DDSD_HEIGHT | DDSD_WIDTH;
    IDirectDraw2_GetDisplayMode( p_vout->p_sys->p_ddobject, &ddsd );

    rect_window_backup = rect_window;

    /* Clip the destination window */
    rect_window.left = (rect_window.left < 0) ? 0 : rect_window.left;
    rect_window.right = (rect_window.right < 0) ? 0 : rect_window.right;
    rect_window.top = (rect_window.top < 0) ? 0 : rect_window.top;
    rect_window.bottom = (rect_window.bottom < 0) ? 0 : rect_window.bottom;

    rect_window.left = (rect_window.left > ddsd.dwWidth) ? ddsd.dwWidth
      : rect_window.left;
    rect_window.right = (rect_window.right > ddsd.dwWidth) ? ddsd.dwWidth
      : rect_window.right;
    rect_window.top = (rect_window.top > ddsd.dwHeight) ? ddsd.dwHeight
      : rect_window.top;
    rect_window.bottom = (rect_window.bottom > ddsd.dwHeight) ? ddsd.dwHeight
      : rect_window.bottom;

    intf_WarnMsg( 3, "vout: DirectXUpdateOverlay window coords: %i,%i,%i,%i",
                  rect_window.left, rect_window.top,
                  rect_window.right, rect_window.bottom);

    /* the 2 following lines are to fix a bug when click on Windows desktop */
    if( (rect_window.right-rect_window.left)==0 ||
        (rect_window.bottom-rect_window.top)==0 ) return 0;

    /* Clip the source image */
    rect_image.left = ( rect_window.left == rect_window_backup.left ) ? 0
      : labs(rect_window_backup.left - rect_window.left) *
      p_vout->p_rendered_pic->i_width /
      (rect_window_backup.right - rect_window_backup.left);
    rect_image.right = ( rect_window.right == rect_window_backup.right ) ?
      p_vout->p_rendered_pic->i_width
      : p_vout->p_rendered_pic->i_width -
      labs(rect_window_backup.right - rect_window.right) *
      p_vout->p_rendered_pic->i_width /
      (rect_window_backup.right - rect_window_backup.left);
    rect_image.top = ( rect_window.top == rect_window_backup.top ) ? 0
      : labs(rect_window_backup.top - rect_window.top) *
      p_vout->p_rendered_pic->i_height /
      (rect_window_backup.bottom - rect_window_backup.top);
    rect_image.bottom = ( rect_window.bottom == rect_window_backup.bottom ) ?
      p_vout->p_rendered_pic->i_height
      : p_vout->p_rendered_pic->i_height -
      labs(rect_window_backup.bottom - rect_window.bottom) *
      p_vout->p_rendered_pic->i_height /
      (rect_window_backup.bottom - rect_window_backup.top);

    intf_WarnMsg( 3, "vout: DirectXUpdateOverlay image coords: %i,%i,%i,%i",
                  rect_image.left, rect_image.top,
                  rect_image.right, rect_image.bottom);

    /* compute the colorkey pixel value from the RGB value we've got */
    memset( &pixel_format, 0, sizeof( DDPIXELFORMAT ));
    pixel_format.dwSize = sizeof( DDPIXELFORMAT );
    dxresult = IDirectDrawSurface3_GetPixelFormat( p_vout->p_sys->p_display,
                                                   &pixel_format );
    if( dxresult != DD_OK )
        intf_WarnMsg( 3, "vout: DirectXUpdateOverlay GetPixelFormat failed" );
    dw_colorkey = (DWORD)p_vout->p_sys->i_colorkey;
#ifdef NONAMELESSUNION
    dw_colorkey = (DWORD)((( dw_colorkey * pixel_format.u2.dwRBitMask) / 255)
                          & pixel_format.u2.dwRBitMask);
#else
    dw_colorkey = (DWORD)((( dw_colorkey * pixel_format.dwRBitMask) / 255)
                          & pixel_format.dwRBitMask);
#endif

    /* Position and show the overlay */
    memset(&ddofx, 0, sizeof(DDOVERLAYFX));
    ddofx.dwSize = sizeof(DDOVERLAYFX);
    ddofx.dckDestColorkey.dwColorSpaceLowValue = dw_colorkey;
    ddofx.dckDestColorkey.dwColorSpaceHighValue = dw_colorkey;

    dwFlags = DDOVER_KEYDESTOVERRIDE | DDOVER_SHOW;

    dxresult = IDirectDrawSurface3_UpdateOverlay(p_vout->p_sys->p_surface,
                                                 &rect_image,
                                                 p_vout->p_sys->p_display,
                                                 &rect_window,
                                                 dwFlags,
                                                 &ddofx);
    if(dxresult != DD_OK)
    {
        intf_WarnMsg( 3,
          "vout: DirectXUpdateOverlay can't move or resize overlay" );
    }

    return ( 0 );
}

/*****************************************************************************
 * DirectXCloseWindow: close the window created by DirectXCreateWindow
 *****************************************************************************
 * This function returns all resources allocated by DirectXCreateWindow.
 *****************************************************************************/
static void DirectXCloseWindow( vout_thread_t *p_vout )
{
    HINSTANCE hInstance;

    intf_WarnMsg( 3, "vout: DirectXCloseWindow" );
    if( p_vout->p_sys->hwnd != NULL )
    {
        DestroyWindow( p_vout->p_sys->hwnd);
        p_vout->p_sys->hwnd = NULL;
    }

    hInstance = GetModuleHandle(NULL);
    UnregisterClass( "VLC DirectX",                            /* class name */
                     hInstance );          /* handle to application instance */

    /* free window background brush */
    if( p_vout->p_sys->hwnd != NULL )
    {
        DeleteObject( p_vout->p_sys->hbrush );
        p_vout->p_sys->hbrush = NULL;
    }
}

/*****************************************************************************
 * DirectXCloseDDraw: Release the DDraw object allocated by DirectXInitDDraw
 *****************************************************************************
 * This function returns all resources allocated by DirectXInitDDraw.
 *****************************************************************************/
static void DirectXCloseDDraw( vout_thread_t *p_vout )
{
    intf_WarnMsg(3, "vout: DirectXCloseDDraw" );
    if( p_vout->p_sys->p_ddobject != NULL )
    {
        IDirectDraw2_Release(p_vout->p_sys->p_ddobject);
        p_vout->p_sys->p_ddobject = NULL;
    }

    if( p_vout->p_sys->hddraw_dll != NULL )
    {
        FreeLibrary( p_vout->p_sys->hddraw_dll );
        p_vout->p_sys->hddraw_dll = NULL;
    }
}

/*****************************************************************************
 * DirectXCloseDisplay: close and reset the DirectX display device
 *****************************************************************************
 * This function returns all resources allocated by DirectXCreateDisplay.
 *****************************************************************************/
static void DirectXCloseDisplay( vout_thread_t *p_vout )
{
    intf_WarnMsg( 3, "vout: DirectXCloseDisplay" );
    if( p_vout->p_sys->p_display != NULL )
    {
        DirectXCloseSurface( p_vout );

        intf_WarnMsg( 3, "vout: DirectXCloseDisplay display" );
        IDirectDraw2_Release( p_vout->p_sys->p_display );
        p_vout->p_sys->p_display = NULL;
    }
}

/*****************************************************************************
 * DirectXCloseSurface: close the YUV overlay or RGB surface.
 *****************************************************************************
 * This function returns all resources allocated by the surface.
 * We also call this function when the decoded picture change its dimensions
 * (in that case we close the overlay surface and reopen another with the
 * right dimensions).
 *****************************************************************************/
static void DirectXCloseSurface( vout_thread_t *p_vout )
{
    intf_WarnMsg( 3, "vout: DirectXCloseSurface" );
    if( p_vout->p_sys->p_surface != NULL )
    {
        intf_WarnMsg( 3, "vout: DirectXCloseSurface surface" );
        IDirectDraw2_Release( p_vout->p_sys->p_surface );
        p_vout->p_sys->p_surface = NULL;
    }

    if( p_vout->p_sys->p_clipper != NULL )
    {
        intf_WarnMsg( 3, "vout: DirectXCloseSurface clipper" );
        IDirectDraw2_Release( p_vout->p_sys->p_clipper );
        p_vout->p_sys->p_clipper = NULL;
    }

    /* Disable any display */
    p_vout->p_sys->b_display_enabled = 0;
}

/*****************************************************************************
 * DirectXKeepAspectRatio: 
 *****************************************************************************
 * This function adjusts the coordinates of the video rectangle to keep the
 * aspect/ratio of the video.
 *****************************************************************************/
static void DirectXKeepAspectRatio( vout_thread_t *p_vout, RECT *rect_window )
{

  if( !p_vout->p_rendered_pic ) return;

  switch( p_vout->p_rendered_pic->i_aspect_ratio )
  {
      case AR_16_9_PICTURE:
      if( ((rect_window->right-rect_window->left)*9)
          > ((rect_window->bottom-rect_window->top)*16) )
      {
        int temp;
        temp = (rect_window->bottom-rect_window->top)*16/9;
        temp = (rect_window->right-rect_window->left) - temp;
        rect_window->left += (temp/2);
        rect_window->right -= (temp/2);
      }
      else
        {
          int temp;
          temp = (rect_window->right-rect_window->left)*9/16;
          temp = (rect_window->bottom-rect_window->top) - temp;
          rect_window->top += (temp/2);
          rect_window->bottom -= (temp/2);
        }
      break;
      
  case AR_221_1_PICTURE:
    if( ((rect_window->right-rect_window->left)*100)
        > ((rect_window->bottom-rect_window->top)*221) )
      {
        int temp;
        temp = (rect_window->bottom-rect_window->top)*221/100;
        temp = (rect_window->right-rect_window->left) - temp;
        rect_window->left += (temp/2);
        rect_window->right -= (temp/2);
      }
    else
      {
        int temp;
        temp = (rect_window->right-rect_window->left)*100/221;
        temp = (rect_window->bottom-rect_window->top) - temp;
        rect_window->top += (temp/2);
        rect_window->bottom -= (temp/2);
      }
    break;
    
  case AR_3_4_PICTURE:
    if( ((rect_window->right-rect_window->left)*3)
        > ((rect_window->bottom-rect_window->top)*4) )
      {
        int temp;
        temp = (rect_window->bottom-rect_window->top)*4/3;
        temp = (rect_window->right-rect_window->left) - temp;
        rect_window->left += (temp/2);
        rect_window->right -= (temp/2);
      }
    else
      {
        int temp;
        temp = (rect_window->right-rect_window->left)*3/4;
        temp = (rect_window->bottom-rect_window->top) - temp;
        rect_window->top += (temp/2);
        rect_window->bottom -= (temp/2);
      }
    break;

  case AR_SQUARE_PICTURE:
  default:
    if( (rect_window->right-rect_window->left)
        > (rect_window->bottom-rect_window->top) )
      {
        int temp;
        temp = (rect_window->bottom-rect_window->top);
        temp = (rect_window->right-rect_window->left) - temp;
        rect_window->left += (temp/2);
        rect_window->right -= (temp/2);
      }
    else
      {
        int temp;
        temp = (rect_window->right-rect_window->left);
        temp = (rect_window->bottom-rect_window->top) - temp;
        rect_window->top += (temp/2);
        rect_window->bottom -= (temp/2);
      }
    break;
    
  }

}
