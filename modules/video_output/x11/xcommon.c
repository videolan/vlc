/*****************************************************************************
 * xcommon.c: Functions common to the X11 and XVideo plugins
 *****************************************************************************
 * Copyright (C) 1998-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Sam Hocevar <sam@zoy.org>
 *          David Kennedy <dkennedy@tinytoad.com>
 *          Gildas Bazin <gbazin@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_playlist.h>
#include <vlc_vout.h>
#include <vlc_vout_window.h>
#include <vlc_keys.h>

#ifdef HAVE_MACHINE_PARAM_H
    /* BSD */
#   include <machine/param.h>
#   include <sys/types.h>                                  /* typedef ushort */
#   include <sys/ipc.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xmd.h>
#include <X11/Xutil.h>
#ifdef DPMSINFO_IN_DPMS_H
#   include <X11/extensions/dpms.h>
#endif

#ifdef MODULE_NAME_IS_glx
#   include <GL/glx.h>
#endif

#include "xcommon.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
int  Activate   ( vlc_object_t * );
void Deactivate ( vlc_object_t * );

static int  ManageVideo    ( vout_thread_t * );
static int  Control        ( vout_thread_t *, int, va_list );

static int  CreateWindow   ( vout_thread_t *, x11_window_t * );
static void DestroyWindow  ( vout_thread_t *, x11_window_t * );

static void ToggleFullScreen      ( vout_thread_t * );

static void EnableXScreenSaver    ( vout_thread_t * );
static void DisableXScreenSaver   ( vout_thread_t * );

static void CreateCursor   ( vout_thread_t * );
static void DestroyCursor  ( vout_thread_t * );
static void ToggleCursor   ( vout_thread_t * );

static int X11ErrorHandler( Display *, XErrorEvent * );

/*****************************************************************************
 * Activate: allocate X11 video thread output method
 *****************************************************************************
 * This function allocate and initialize a X11 vout method. It uses some of the
 * vout properties to choose the window size, and change them according to the
 * actual properties of the display.
 *****************************************************************************/
int Activate ( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;
    char *        psz_display;

    p_vout->pf_render = NULL;
    p_vout->pf_manage = ManageVideo;
    p_vout->pf_control = Control;

    /* Allocate structure */
    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
        return VLC_ENOMEM;

    /* Open display, using the "display" config variable or the DISPLAY
     * environment variable */
    psz_display = config_GetPsz( p_vout, MODULE_STRING "-display" );

    p_vout->p_sys->p_display = XOpenDisplay( psz_display );

    if( p_vout->p_sys->p_display == NULL )                         /* error */
    {
        msg_Err( p_vout, "cannot open display %s",
                         XDisplayName( psz_display ) );
        free( p_vout->p_sys );
        free( psz_display );
        return VLC_EGENERIC;
    }
    free( psz_display );

    /* Replace error handler so we can intercept some non-fatal errors */
    XSetErrorHandler( X11ErrorHandler );

    /* Get a screen ID matching the XOpenDisplay return value */
    p_vout->p_sys->i_screen = DefaultScreen( p_vout->p_sys->p_display );

#if defined(MODULE_NAME_IS_glx)
    {
        int i_opcode, i_evt, i_err = 0;
        int i_maj, i_min = 0;

        /* Check for GLX extension */
        if( !XQueryExtension( p_vout->p_sys->p_display, "GLX",
                              &i_opcode, &i_evt, &i_err ) )
        {
            msg_Err( p_this, "GLX extension not supported" );
            XCloseDisplay( p_vout->p_sys->p_display );
            free( p_vout->p_sys );
            return VLC_EGENERIC;
        }
        if( !glXQueryExtension( p_vout->p_sys->p_display, &i_err, &i_evt ) )
        {
            msg_Err( p_this, "glXQueryExtension failed" );
            XCloseDisplay( p_vout->p_sys->p_display );
            free( p_vout->p_sys );
            return VLC_EGENERIC;
        }

        /* Check GLX version */
        if (!glXQueryVersion( p_vout->p_sys->p_display, &i_maj, &i_min ) )
        {
            msg_Err( p_this, "glXQueryVersion failed" );
            XCloseDisplay( p_vout->p_sys->p_display );
            free( p_vout->p_sys );
            return VLC_EGENERIC;
        }
        if( i_maj <= 0 || ((i_maj == 1) && (i_min < 3)) )
        {
            p_vout->p_sys->b_glx13 = false;
            msg_Dbg( p_this, "using GLX 1.2 API" );
        }
        else
        {
            p_vout->p_sys->b_glx13 = true;
            msg_Dbg( p_this, "using GLX 1.3 API" );
        }
    }
#endif

    /* Create blank cursor (for mouse cursor autohiding) */
    p_vout->p_sys->i_time_mouse_last_moved = mdate();
    p_vout->p_sys->i_mouse_hide_timeout =
        var_GetInteger(p_vout, "mouse-hide-timeout") * 1000;
    p_vout->p_sys->b_mouse_pointer_visible = 1;
    CreateCursor( p_vout );

    /* Set main window's size */
    p_vout->p_sys->window.i_x      = 0;
    p_vout->p_sys->window.i_y      = 0;
    p_vout->p_sys->window.i_width  = p_vout->i_window_width;
    p_vout->p_sys->window.i_height = p_vout->i_window_height;
    var_Create( p_vout, "video-title", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    /* Spawn base window - this window will include the video output window,
     * but also command buttons, subtitles and other indicators */
    if( CreateWindow( p_vout, &p_vout->p_sys->window ) )
    {
        msg_Err( p_vout, "cannot create X11 window" );
        DestroyCursor( p_vout );
        XCloseDisplay( p_vout->p_sys->p_display );
        free( p_vout->p_sys );
        return VLC_EGENERIC;
    }

    /* Disable screen saver */
    DisableXScreenSaver( p_vout );

    /* Misc init */
    p_vout->p_sys->i_time_button_last_pressed = 0;

    /* Variable to indicate if the window should be on top of others */
    /* Trigger a callback right now */
    var_TriggerCallback( p_vout, "video-on-top" );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: destroy X11 video thread output method
 *****************************************************************************
 * Terminate an output method created by Open
 *****************************************************************************/
void Deactivate ( vlc_object_t *p_this )
{
    vout_thread_t *p_vout = (vout_thread_t *)p_this;

    /* Restore cursor if it was blanked */
    if( !p_vout->p_sys->b_mouse_pointer_visible )
    {
        ToggleCursor( p_vout );
    }

    DestroyCursor( p_vout );
    EnableXScreenSaver( p_vout );
    DestroyWindow( p_vout, &p_vout->p_sys->window );
    XCloseDisplay( p_vout->p_sys->p_display );

    /* Destroy structure */
    free( p_vout->p_sys );
}

/*****************************************************************************
 * ManageVideo: handle X11 events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * X11 events and allows window resizing. It returns a non null value on
 * error.
 *****************************************************************************/
static int ManageVideo( vout_thread_t *p_vout )
{
    XEvent      xevent;                                         /* X11 event */
    vlc_value_t val;

    /* Handle events from the owner window */
    while( XCheckWindowEvent( p_vout->p_sys->p_display,
                              p_vout->p_sys->window.owner_window->xid,
                              StructureNotifyMask, &xevent ) == True )
    {
        /* ConfigureNotify event: prepare  */
        if( xevent.type == ConfigureNotify )
            /* Update dimensions */
            XResizeWindow( p_vout->p_sys->p_display,
                           p_vout->p_sys->window.base_window,
                           xevent.xconfigure.width,
                           xevent.xconfigure.height );
    }

    /* Handle X11 events: ConfigureNotify events are parsed to know if the
     * output window's size changed, MapNotify and UnmapNotify to know if the
     * window is mapped (and if the display is useful), and ClientMessages
     * to intercept window destruction requests */

    while( XCheckWindowEvent( p_vout->p_sys->p_display,
                              p_vout->p_sys->window.base_window,
                              StructureNotifyMask |
                              ButtonPressMask | ButtonReleaseMask |
                              PointerMotionMask | Button1MotionMask , &xevent )
           == True )
    {
        /* ConfigureNotify event: prepare  */
        if( xevent.type == ConfigureNotify )
        {
            if( (unsigned int)xevent.xconfigure.width
                   != p_vout->p_sys->window.i_width
              || (unsigned int)xevent.xconfigure.height
                    != p_vout->p_sys->window.i_height )
            {
                /* Update dimensions */
                p_vout->i_changes |= VOUT_SIZE_CHANGE;
                p_vout->p_sys->window.i_width = xevent.xconfigure.width;
                p_vout->p_sys->window.i_height = xevent.xconfigure.height;
            }
        }
        /* Mouse click */
        else if( xevent.type == ButtonPress )
        {
            switch( ((XButtonEvent *)&xevent)->button )
            {
                case Button1:
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int |= 1;
                    var_Set( p_vout, "mouse-button-down", val );

                    var_SetBool( p_vout->p_libvlc, "intf-popupmenu", false );

                    /* detect double-clicks */
                    if( ( ((XButtonEvent *)&xevent)->time -
                          p_vout->p_sys->i_time_button_last_pressed ) < 300 )
                    {
                        p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
                    }

                    p_vout->p_sys->i_time_button_last_pressed =
                        ((XButtonEvent *)&xevent)->time;
                    break;
                case Button2:
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int |= 2;
                    var_Set( p_vout, "mouse-button-down", val );
                    break;

                case Button3:
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int |= 4;
                    var_Set( p_vout, "mouse-button-down", val );
                    var_SetBool( p_vout->p_libvlc, "intf-popupmenu", true );
                    break;

                case Button4:
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int |= 8;
                    var_Set( p_vout, "mouse-button-down", val );
                    break;

                case Button5:
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int |= 16;
                    var_Set( p_vout, "mouse-button-down", val );
                    break;
            }
        }
        /* Mouse release */
        else if( xevent.type == ButtonRelease )
        {
            switch( ((XButtonEvent *)&xevent)->button )
            {
                case Button1:
                    {
                        var_Get( p_vout, "mouse-button-down", &val );
                        val.i_int &= ~1;
                        var_Set( p_vout, "mouse-button-down", val );

                        var_SetBool( p_vout, "mouse-clicked", true );
                    }
                    break;

                case Button2:
                    {
                        var_Get( p_vout, "mouse-button-down", &val );
                        val.i_int &= ~2;
                        var_Set( p_vout, "mouse-button-down", val );

                        var_ToggleBool( p_vout->p_libvlc, "intf-show" );
                    }
                    break;

                case Button3:
                    {
                        var_Get( p_vout, "mouse-button-down", &val );
                        val.i_int &= ~4;
                        var_Set( p_vout, "mouse-button-down", val );

                    }
                    break;

                case Button4:
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int &= ~8;
                    var_Set( p_vout, "mouse-button-down", val );
                    break;

                case Button5:
                    var_Get( p_vout, "mouse-button-down", &val );
                    val.i_int &= ~16;
                    var_Set( p_vout, "mouse-button-down", val );
                    break;

            }
        }
        /* Mouse move */
        else if( xevent.type == MotionNotify )
        {
            unsigned int i_width, i_height, i_x, i_y;

            /* somewhat different use for vout_PlacePicture:
             * here the values are needed to give to mouse coordinates
             * in the original picture space */
            vout_PlacePicture( p_vout, p_vout->p_sys->window.i_width,
                               p_vout->p_sys->window.i_height,
                               &i_x, &i_y, &i_width, &i_height );

            /* Compute the x coordinate and check if the value is
               in [0,p_vout->fmt_in.i_visible_width] */
            val.i_int = ( xevent.xmotion.x - i_x ) *
                p_vout->fmt_in.i_visible_width / i_width +
                p_vout->fmt_in.i_x_offset;

            if( (int)(xevent.xmotion.x - i_x) < 0 )
                val.i_int = 0;
            else if( (unsigned int)val.i_int > p_vout->fmt_in.i_visible_width )
                val.i_int = p_vout->fmt_in.i_visible_width;

            var_Set( p_vout, "mouse-x", val );

            /* compute the y coordinate and check if the value is
               in [0,p_vout->fmt_in.i_visible_height] */
            val.i_int = ( xevent.xmotion.y - i_y ) *
                p_vout->fmt_in.i_visible_height / i_height +
                p_vout->fmt_in.i_y_offset;

            if( (int)(xevent.xmotion.y - i_y) < 0 )
                val.i_int = 0;
            else if( (unsigned int)val.i_int > p_vout->fmt_in.i_visible_height )
                val.i_int = p_vout->fmt_in.i_visible_height;

            var_Set( p_vout, "mouse-y", val );

            var_SetBool( p_vout, "mouse-moved", true );

            p_vout->p_sys->i_time_mouse_last_moved = mdate();
            if( ! p_vout->p_sys->b_mouse_pointer_visible )
            {
                ToggleCursor( p_vout );
            }
        }
        else if( xevent.type == ReparentNotify /* XXX: why do we get this? */
                  || xevent.type == MapNotify
                  || xevent.type == UnmapNotify )
        {
            /* Ignore these events */
        }
        else /* Other events */
        {
            msg_Warn( p_vout, "unhandled event %d received", xevent.type );
        }
    }

    /* Handle events for video output sub-window */
    while( XCheckWindowEvent( p_vout->p_sys->p_display,
                              p_vout->p_sys->window.video_window,
                              ExposureMask, &xevent ) == True );

    /* ClientMessage event - only WM_PROTOCOLS with WM_DELETE_WINDOW data
     * are handled - according to the man pages, the format is always 32
     * in this case */
    while( XCheckTypedEvent( p_vout->p_sys->p_display,
                             ClientMessage, &xevent ) )
    {
        if( (xevent.xclient.message_type == p_vout->p_sys->window.wm_protocols)
               && ((Atom)xevent.xclient.data.l[0]
                     == p_vout->p_sys->window.wm_delete_window ) )
        {
            /* the user wants to close the window */
            playlist_t * p_playlist = pl_Hold( p_vout );
            if( p_playlist != NULL )
            {
                playlist_Stop( p_playlist );
                pl_Release( p_vout );
            }
        }
    }

    /*
     * Fullscreen Change
     */
    if ( p_vout->i_changes & VOUT_FULLSCREEN_CHANGE )
    {
        /* Update the object variable and trigger callback */
        var_SetBool( p_vout, "fullscreen", !p_vout->b_fullscreen );

        ToggleFullScreen( p_vout );
        p_vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
    }

    /* autoscale toggle */
    if( p_vout->i_changes & VOUT_SCALE_CHANGE )
    {
        p_vout->i_changes &= ~VOUT_SCALE_CHANGE;

        p_vout->b_autoscale = var_GetBool( p_vout, "autoscale" );
        p_vout->i_zoom = ZOOM_FP_FACTOR;

        p_vout->i_changes |= VOUT_SIZE_CHANGE;
    }

    /* scaling factor */
    if( p_vout->i_changes & VOUT_ZOOM_CHANGE )
    {
        p_vout->i_changes &= ~VOUT_ZOOM_CHANGE;

        p_vout->b_autoscale = false;
        p_vout->i_zoom =
            (int)( ZOOM_FP_FACTOR * var_GetFloat( p_vout, "scale" ) );

        p_vout->i_changes |= VOUT_SIZE_CHANGE;
    }

    if( p_vout->i_changes & VOUT_CROP_CHANGE ||
        p_vout->i_changes & VOUT_ASPECT_CHANGE )
    {
        p_vout->i_changes &= ~VOUT_CROP_CHANGE;
        p_vout->i_changes &= ~VOUT_ASPECT_CHANGE;

        p_vout->fmt_out.i_x_offset = p_vout->fmt_in.i_x_offset;
        p_vout->fmt_out.i_y_offset = p_vout->fmt_in.i_y_offset;
        p_vout->fmt_out.i_visible_width = p_vout->fmt_in.i_visible_width;
        p_vout->fmt_out.i_visible_height = p_vout->fmt_in.i_visible_height;
        p_vout->fmt_out.i_aspect = p_vout->fmt_in.i_aspect;
        p_vout->fmt_out.i_sar_num = p_vout->fmt_in.i_sar_num;
        p_vout->fmt_out.i_sar_den = p_vout->fmt_in.i_sar_den;
        p_vout->output.i_aspect = p_vout->fmt_in.i_aspect;

        p_vout->i_changes |= VOUT_SIZE_CHANGE;
    }

    /*
     * Size change
     *
     * (Needs to be placed after VOUT_FULLSREEN_CHANGE because we can activate
     *  the size flag inside the fullscreen routine)
     */
    if( p_vout->i_changes & VOUT_SIZE_CHANGE )
    {
        unsigned int i_width, i_height, i_x, i_y;

        p_vout->i_changes &= ~VOUT_SIZE_CHANGE;

        vout_PlacePicture( p_vout, p_vout->p_sys->window.i_width,
                           p_vout->p_sys->window.i_height,
                           &i_x, &i_y, &i_width, &i_height );

        XMoveResizeWindow( p_vout->p_sys->p_display,
                           p_vout->p_sys->window.video_window,
                           i_x, i_y, i_width, i_height );
    }

    /* Autohide Cursour */
    if( mdate() - p_vout->p_sys->i_time_mouse_last_moved >
        p_vout->p_sys->i_mouse_hide_timeout )
    {
        /* Hide the mouse automatically */
        if( p_vout->p_sys->b_mouse_pointer_visible )
        {
            ToggleCursor( p_vout );
        }
    }

    return 0;
}

/* following functions are local */

/*****************************************************************************
 * CreateWindow: open and set-up X11 main window
 *****************************************************************************/
static int CreateWindow( vout_thread_t *p_vout, x11_window_t *p_win )
{
    XSizeHints              xsize_hints;
    XSetWindowAttributes    xwindow_attributes;
    XGCValues               xgcvalues;
    XEvent                  xevent;

    bool              b_map_notify = false;

    /* Prepare window manager hints and properties */
    p_win->wm_protocols =
             XInternAtom( p_vout->p_sys->p_display, "WM_PROTOCOLS", True );
    p_win->wm_delete_window =
             XInternAtom( p_vout->p_sys->p_display, "WM_DELETE_WINDOW", True );

    /* Never have a 0-pixel-wide window */
    xsize_hints.min_width = 2;
    xsize_hints.min_height = 1;

    /* Prepare window attributes */
    xwindow_attributes.backing_store = Always;       /* save the hidden part */
    xwindow_attributes.background_pixel = BlackPixel(p_vout->p_sys->p_display,
                                                     p_vout->p_sys->i_screen);
    xwindow_attributes.event_mask = ExposureMask | StructureNotifyMask;

    {
        vout_window_cfg_t wnd_cfg;
        memset( &wnd_cfg, 0, sizeof(wnd_cfg) );
        wnd_cfg.type   = VOUT_WINDOW_TYPE_XID;
        wnd_cfg.x      = p_win->i_x;
        wnd_cfg.y      = p_win->i_y;
        wnd_cfg.width  = p_win->i_width;
        wnd_cfg.height = p_win->i_height;

        p_win->owner_window = vout_window_New( VLC_OBJECT(p_vout), NULL, &wnd_cfg );
        if( !p_win->owner_window )
            return VLC_EGENERIC;
        xsize_hints.base_width  = xsize_hints.width = p_win->i_width;
        xsize_hints.base_height = xsize_hints.height = p_win->i_height;
        xsize_hints.flags       = PSize | PMinSize;

        if( p_win->i_x >=0 || p_win->i_y >= 0 )
        {
            xsize_hints.x = p_win->i_x;
            xsize_hints.y = p_win->i_y;
            xsize_hints.flags |= PPosition;
        }

        /* Select events we are interested in. */
        XSelectInput( p_vout->p_sys->p_display,
                      p_win->owner_window->xid, StructureNotifyMask );

        /* Get the parent window's geometry information */
        XGetGeometry( p_vout->p_sys->p_display,
                      p_win->owner_window->xid,
                      &(Window){ 0 }, &(int){ 0 }, &(int){ 0 },
                      &p_win->i_width,
                      &p_win->i_height,
                      &(unsigned){ 0 }, &(unsigned){ 0 } );

        /* From man XSelectInput: only one client at a time can select a
         * ButtonPress event, so we need to open a new window anyway. */
        p_win->base_window =
            XCreateWindow( p_vout->p_sys->p_display,
                           p_win->owner_window->xid,
                           0, 0,
                           p_win->i_width, p_win->i_height,
                           0,
                           0, CopyFromParent, 0,
                           CWBackingStore | CWBackPixel | CWEventMask,
                           &xwindow_attributes );
    }

    if( (p_win->wm_protocols == None)        /* use WM_DELETE_WINDOW */
        || (p_win->wm_delete_window == None)
        || !XSetWMProtocols( p_vout->p_sys->p_display, p_win->base_window,
                             &p_win->wm_delete_window, 1 ) )
    {
        /* WM_DELETE_WINDOW is not supported by window manager */
        msg_Warn( p_vout, "missing or bad window manager" );
    }

    /* Creation of a graphic context that doesn't generate a GraphicsExpose
     * event when using functions like XCopyArea */
    xgcvalues.graphics_exposures = False;
    p_win->gc = XCreateGC( p_vout->p_sys->p_display,
                           p_win->base_window,
                           GCGraphicsExposures, &xgcvalues );

    /* Wait till the window is mapped */
    XMapWindow( p_vout->p_sys->p_display, p_win->base_window );
    do
    {
        XWindowEvent( p_vout->p_sys->p_display, p_win->base_window,
                      SubstructureNotifyMask | StructureNotifyMask, &xevent);
        if( (xevent.type == MapNotify)
                 && (xevent.xmap.window == p_win->base_window) )
        {
            b_map_notify = true;
        }
        else if( (xevent.type == ConfigureNotify)
                 && (xevent.xconfigure.window == p_win->base_window) )
        {
            p_win->i_width = xevent.xconfigure.width;
            p_win->i_height = xevent.xconfigure.height;
        }
    } while( !b_map_notify );

    long mask = StructureNotifyMask | PointerMotionMask;
    if( var_CreateGetBool( p_vout, "mouse-events" ) )
        mask |= ButtonPressMask | ButtonReleaseMask;
    XSelectInput( p_vout->p_sys->p_display, p_win->base_window, mask );

    /* Create video output sub-window. */
    p_win->video_window =  XCreateSimpleWindow(
                                      p_vout->p_sys->p_display,
                                      p_win->base_window, 0, 0,
                                      p_win->i_width, p_win->i_height,
                                      0,
                                      BlackPixel( p_vout->p_sys->p_display,
                                                  p_vout->p_sys->i_screen ),
                                      WhitePixel( p_vout->p_sys->p_display,
                                                  p_vout->p_sys->i_screen ) );

    XSetWindowBackground( p_vout->p_sys->p_display, p_win->video_window,
                          BlackPixel( p_vout->p_sys->p_display,
                                      p_vout->p_sys->i_screen ) );

    XMapWindow( p_vout->p_sys->p_display, p_win->video_window );
    XSelectInput( p_vout->p_sys->p_display, p_win->video_window,
                  ExposureMask );

    /* make sure the video window will be centered in the next ManageVideo() */
    p_vout->i_changes |= VOUT_SIZE_CHANGE;

    /* If the cursor was formerly blank than blank it again */
    if( !p_vout->p_sys->b_mouse_pointer_visible )
    {
        ToggleCursor( p_vout );
        ToggleCursor( p_vout );
    }

    /* Do NOT use XFlush here ! */
    XSync( p_vout->p_sys->p_display, False );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DestroyWindow: destroy the window
 *****************************************************************************
 *
 *****************************************************************************/
static void DestroyWindow( vout_thread_t *p_vout, x11_window_t *p_win )
{
    /* Do NOT use XFlush here ! */
    XSync( p_vout->p_sys->p_display, False );

    if( p_win->video_window != None )
        XDestroyWindow( p_vout->p_sys->p_display, p_win->video_window );

    XFreeGC( p_vout->p_sys->p_display, p_win->gc );

    XUnmapWindow( p_vout->p_sys->p_display, p_win->base_window );
    XDestroyWindow( p_vout->p_sys->p_display, p_win->base_window );

    /* make sure base window is destroyed before proceeding further */
    bool b_destroy_notify = false;
    do
    {
        XEvent      xevent;
        XWindowEvent( p_vout->p_sys->p_display, p_win->base_window,
                      SubstructureNotifyMask | StructureNotifyMask, &xevent);
        if( (xevent.type == DestroyNotify)
                 && (xevent.xmap.window == p_win->base_window) )
        {
            b_destroy_notify = true;
        }
    } while( !b_destroy_notify );

    vout_window_Delete( p_win->owner_window );
}

/*****************************************************************************
 * ToggleFullScreen: Enable or disable full screen mode
 *****************************************************************************
 * This function will switch between fullscreen and window mode.
 *****************************************************************************/
static void ToggleFullScreen ( vout_thread_t *p_vout )
{
    p_vout->b_fullscreen = !p_vout->b_fullscreen;
    vout_window_SetFullScreen( p_vout->p_sys->window.owner_window,
                               p_vout->b_fullscreen );
}

/*****************************************************************************
 * EnableXScreenSaver: enable screen saver
 *****************************************************************************
 * This function enables the screen saver on a display after it has been
 * disabled by XDisableScreenSaver.
 * FIXME: what happens if multiple vlc sessions are running at the same
 *        time ???
 *****************************************************************************/
static void EnableXScreenSaver( vout_thread_t *p_vout )
{
#ifdef DPMSINFO_IN_DPMS_H
    int dummy;
#endif

    if( p_vout->p_sys->i_ss_timeout )
    {
        XSetScreenSaver( p_vout->p_sys->p_display, p_vout->p_sys->i_ss_timeout,
                         p_vout->p_sys->i_ss_interval,
                         p_vout->p_sys->i_ss_blanking,
                         p_vout->p_sys->i_ss_exposure );
    }

    /* Restore DPMS settings */
#ifdef DPMSINFO_IN_DPMS_H
    if( DPMSQueryExtension( p_vout->p_sys->p_display, &dummy, &dummy ) )
    {
        if( p_vout->p_sys->b_ss_dpms )
        {
            DPMSEnable( p_vout->p_sys->p_display );
        }
    }
#endif
}

/*****************************************************************************
 * DisableXScreenSaver: disable screen saver
 *****************************************************************************
 * See XEnableXScreenSaver
 *****************************************************************************/
static void DisableXScreenSaver( vout_thread_t *p_vout )
{
#ifdef DPMSINFO_IN_DPMS_H
    int dummy;
#endif

    /* Save screen saver information */
    XGetScreenSaver( p_vout->p_sys->p_display, &p_vout->p_sys->i_ss_timeout,
                     &p_vout->p_sys->i_ss_interval,
                     &p_vout->p_sys->i_ss_blanking,
                     &p_vout->p_sys->i_ss_exposure );

    /* Disable screen saver */
    if( p_vout->p_sys->i_ss_timeout )
    {
        XSetScreenSaver( p_vout->p_sys->p_display, 0,
                         p_vout->p_sys->i_ss_interval,
                         p_vout->p_sys->i_ss_blanking,
                         p_vout->p_sys->i_ss_exposure );
    }

    /* Disable DPMS */
#ifdef DPMSINFO_IN_DPMS_H
    if( DPMSQueryExtension( p_vout->p_sys->p_display, &dummy, &dummy ) )
    {
        CARD16 unused;
        /* Save DPMS current state */
        DPMSInfo( p_vout->p_sys->p_display, &unused,
                  &p_vout->p_sys->b_ss_dpms );
        DPMSDisable( p_vout->p_sys->p_display );
   }
#endif
}

/*****************************************************************************
 * CreateCursor: create a blank mouse pointer
 *****************************************************************************/
static void CreateCursor( vout_thread_t *p_vout )
{
    XColor cursor_color;

    p_vout->p_sys->cursor_pixmap =
        XCreatePixmap( p_vout->p_sys->p_display,
                       DefaultRootWindow( p_vout->p_sys->p_display ),
                       1, 1, 1 );

    XParseColor( p_vout->p_sys->p_display,
                 XCreateColormap( p_vout->p_sys->p_display,
                                  DefaultRootWindow(
                                                    p_vout->p_sys->p_display ),
                                  DefaultVisual(
                                                p_vout->p_sys->p_display,
                                                p_vout->p_sys->i_screen ),
                                  AllocNone ),
                 "black", &cursor_color );

    p_vout->p_sys->blank_cursor =
        XCreatePixmapCursor( p_vout->p_sys->p_display,
                             p_vout->p_sys->cursor_pixmap,
                             p_vout->p_sys->cursor_pixmap,
                             &cursor_color, &cursor_color, 1, 1 );
}

/*****************************************************************************
 * DestroyCursor: destroy the blank mouse pointer
 *****************************************************************************/
static void DestroyCursor( vout_thread_t *p_vout )
{
    XFreePixmap( p_vout->p_sys->p_display, p_vout->p_sys->cursor_pixmap );
}

/*****************************************************************************
 * ToggleCursor: hide or show the mouse pointer
 *****************************************************************************
 * This function hides the X pointer if it is visible by setting the pointer
 * sprite to a blank one. To show it again, we disable the sprite.
 *****************************************************************************/
static void ToggleCursor( vout_thread_t *p_vout )
{
    if( p_vout->p_sys->b_mouse_pointer_visible )
    {
        XDefineCursor( p_vout->p_sys->p_display,
                       p_vout->p_sys->window.base_window,
                       p_vout->p_sys->blank_cursor );
        p_vout->p_sys->b_mouse_pointer_visible = 0;
    }
    else
    {
        XUndefineCursor( p_vout->p_sys->p_display,
                         p_vout->p_sys->window.base_window );
        p_vout->p_sys->b_mouse_pointer_visible = 1;
    }
}

/*****************************************************************************
 * X11ErrorHandler: replace error handler so we can intercept some of them
 *****************************************************************************/
static int X11ErrorHandler( Display * display, XErrorEvent * event )
{
    char txt[1024];

    XGetErrorText( display, event->error_code, txt, sizeof( txt ) );
    fprintf( stderr,
             "[????????] x11 video output error: X11 request %u.%u failed "
              "with error code %u:\n %s\n",
             event->request_code, event->minor_code, event->error_code, txt );

    switch( event->request_code )
    {
    case X_SetInputFocus:
        /* Ignore errors on XSetInputFocus()
         * (they happen when a window is not yet mapped) */
        return 0;
    }

    XSetErrorHandler(NULL);
    return (XSetErrorHandler(X11ErrorHandler))( display, event );
}

/*****************************************************************************
 * Control: control facility for the vout
 *****************************************************************************/
static int Control( vout_thread_t *p_vout, int i_query, va_list args )
{
    unsigned int i_width, i_height;

    switch( i_query )
    {
        case VOUT_SET_SIZE:
            i_width  = va_arg( args, unsigned int );
            i_height = va_arg( args, unsigned int );
            if( !i_width ) i_width = p_vout->i_window_width;
            if( !i_height ) i_height = p_vout->i_window_height;

            return vout_window_SetSize( p_vout->p_sys->window.owner_window,
                                        i_width, i_height);

        case VOUT_SET_STAY_ON_TOP:
            return vout_window_SetOnTop( p_vout->p_sys->window.owner_window,
                                         va_arg( args, int ) );

       default:
            return VLC_EGENERIC;
    }
}
