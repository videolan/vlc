/*****************************************************************************
 * intf_gnome.c: Gnome interface
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors:
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
#include "defs.h"

#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */
#include <sys/types.h>                        /* on BSD, uio.h needs types.h */
#include <sys/uio.h>                                          /* for input.h */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "plugins.h"

#include "input.h"
#include "video.h"
#include "video_output.h"

#include "audio_output.h" /* needed for mute */

#include "intf_msg.h"
#include "interface.h"

#include "main.h"

#include <stdio.h>

#include <gnome.h>

#include "intf_gnome_thread.h"
#include "intf_gnome.h"
#include "intf_gnome_interface.h"
#include "intf_gnome_support.h"

/*****************************************************************************
 * intf_GnomeCreate: initialize and create window
 *****************************************************************************/
int intf_GnomeCreate( intf_thread_t *p_intf )
{
    char       *psz_display;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        intf_ErrMsg("error: %s\n", strerror(ENOMEM));
        return( 1 );
    }

    p_intf->p_sys->p_gnome = malloc( sizeof( gnome_thread_t ) );
    if( p_intf->p_sys->p_gnome == NULL )
    {
        intf_ErrMsg("error: %s\n", strerror(ENOMEM));
        free( p_intf->p_sys );
        return( 1 );
    }

    /* Open display, unsing 'vlc_display' or DISPLAY environment variable */
    psz_display = XDisplayName( main_GetPszVariable( VOUT_DISPLAY_VAR, NULL ) );
    p_intf->p_sys->p_display = XOpenDisplay( psz_display );
    if( !p_intf->p_sys->p_display )                                 /* error */
    {
        intf_ErrMsg("error: can't open display %s\n", psz_display );
        free( p_intf->p_sys->p_gnome );
        free( p_intf->p_sys );
        return( 1 );
    }
    p_intf->p_sys->i_screen = DefaultScreen( p_intf->p_sys->p_display );

    /* Spawn base window - this window will include the video output window */
    if( GnomeCreateWindow( p_intf ) )
    {
        intf_ErrMsg( "error: can't create output window\n" );
        XCloseDisplay( p_intf->p_sys->p_display );
        free( p_intf->p_sys->p_gnome );
        free( p_intf->p_sys );
        return( 1 );
    }

    /* Spawn video output thread */
    if( p_main->b_video )
    {
        p_intf->p_vout = vout_CreateThread( psz_display, p_intf->p_sys->window,
                                            p_intf->p_sys->i_width,
                                            p_intf->p_sys->i_height, NULL, 0,
                                            (void *)&p_intf->p_sys->colormap );

        if( p_intf->p_vout == NULL )                                /* error */
        {
            intf_ErrMsg("error: can't create video output thread\n" );
            GnomeDestroyWindow( p_intf );
            XCloseDisplay( p_intf->p_sys->p_display );
            free( p_intf->p_sys->p_gnome );
            free( p_intf->p_sys );
            return( 1 );
        }
    }

    /* Spawn Gnome thread */
    p_intf->p_sys->p_gnome->b_die = 0;
    p_intf->p_sys->p_gnome->b_error = 0;
    
    p_intf->p_sys->p_gnome->b_popup_changed = 0;
    p_intf->p_sys->p_gnome->b_window_changed = 0;
    p_intf->p_sys->p_gnome->b_playlist_changed = 0;

    vlc_thread_create( &p_intf->p_sys->p_gnome->thread_id, "gnome",
                       (void *)GnomeThread, p_intf->p_sys->p_gnome );

    /* Disable screen saver and return */
    p_intf->p_sys->i_ss_count = 1;
    GnomeDisableScreenSaver( p_intf );
    return( 0 );
}

/*****************************************************************************
 * intf_GnomeDestroy: destroy interface window
 *****************************************************************************/
void intf_GnomeDestroy( intf_thread_t *p_intf )
{
    /* Enable screen saver */
    GnomeEnableScreenSaver( p_intf );

    /* Close input thread, if any (blocking) */
    if( p_intf->p_input )
    {
        input_DestroyThread( p_intf->p_input, NULL );
    }

    /* Close video output thread, if any (blocking) */
    if( p_intf->p_vout )
    {
        vout_DestroyThread( p_intf->p_vout, NULL );
    }

    /* Close gnome thread, if any (blocking) */
    if( p_intf->p_sys->p_gnome->thread_id )
    {
        p_intf->p_sys->p_gnome->b_die = 1;
        intf_Msg( "waiting for Gnome thread to terminate\n" );
        vlc_thread_join( p_intf->p_sys->p_gnome->thread_id );
        intf_Msg( "Gnome thread terminated\n" );
    }

    /* Close main window and display */
    GnomeDestroyWindow( p_intf );
    XCloseDisplay( p_intf->p_sys->p_display );

    /* Destroy structures */
    free( p_intf->p_sys->p_gnome );
    free( p_intf->p_sys );
}

/*****************************************************************************
 * intf_GnomeManage: event loop
 *****************************************************************************/
void intf_GnomeManage( intf_thread_t *p_intf )
{
    /* Manage main window */
    GnomeManageWindow( p_intf );

    /* Manage messages from the Gnome interface */
    GnomeManageInterface( p_intf );
}

/* following functions are local */

/*****************************************************************************
 * GnomeCreateWindow: open and set-up X11 main window
 *****************************************************************************/
static int GnomeCreateWindow( intf_thread_t *p_intf )
{
    XSizeHints              xsize_hints;
    XSetWindowAttributes    xwindow_attributes;
    XGCValues               xgcvalues;
    XEvent                  xevent;
    boolean_t               b_expose;
    boolean_t               b_configure_notify;
    boolean_t               b_map_notify;

    /* Set main window's size */
    p_intf->p_sys->i_width =  main_GetIntVariable( VOUT_WIDTH_VAR,
                                                   VOUT_WIDTH_DEFAULT );
    p_intf->p_sys->i_height = main_GetIntVariable( VOUT_HEIGHT_VAR,
                                                   VOUT_HEIGHT_DEFAULT );

    /* Prepare window manager hints and properties */
    xsize_hints.base_width =            p_intf->p_sys->i_width;
    xsize_hints.base_height =           p_intf->p_sys->i_height;
    xsize_hints.flags =                 PSize;
    p_intf->p_sys->wm_protocols =       XInternAtom( p_intf->p_sys->p_display,
                                                     "WM_PROTOCOLS", True );
    p_intf->p_sys->wm_delete_window =   XInternAtom( p_intf->p_sys->p_display,
                                                     "WM_DELETE_WINDOW", True );

    /* Prepare window attributes */
    xwindow_attributes.backing_store = Always;       /* save the hidden part */
    xwindow_attributes.background_pixel = WhitePixel( p_intf->p_sys->p_display,
                                                      p_intf->p_sys->i_screen );

    xwindow_attributes.event_mask = ExposureMask | StructureNotifyMask;

    /* Create the window and set hints - the window must receive ConfigureNotify
     * events, and, until it is displayed, Expose and MapNotify events. */
    p_intf->p_sys->window =
            XCreateWindow( p_intf->p_sys->p_display,
                           DefaultRootWindow( p_intf->p_sys->p_display ),
                           0, 0,
                           p_intf->p_sys->i_width, p_intf->p_sys->i_height, 1,
                           0, InputOutput, 0,
                           CWBackingStore | CWBackPixel | CWEventMask,
                           &xwindow_attributes );

    /* Set window manager hints and properties: size hints, command,
     * window's name, and accepted protocols */
    XSetWMNormalHints( p_intf->p_sys->p_display, p_intf->p_sys->window,
                       &xsize_hints );
    XSetCommand( p_intf->p_sys->p_display, p_intf->p_sys->window,
                 p_main->ppsz_argv, p_main->i_argc );
    XStoreName( p_intf->p_sys->p_display, p_intf->p_sys->window, VOUT_TITLE );
    if( (p_intf->p_sys->wm_protocols == None)        /* use WM_DELETE_WINDOW */
        || (p_intf->p_sys->wm_delete_window == None)
        || !XSetWMProtocols( p_intf->p_sys->p_display, p_intf->p_sys->window,
                             &p_intf->p_sys->wm_delete_window, 1 ) )
    {
        /* WM_DELETE_WINDOW is not supported by window manager */
        intf_Msg("error: missing or bad window manager - please exit program kindly.\n");
    }

    /* Creation of a graphic context that doesn't generate a GraphicsExpose
     * event when using functions like XCopyArea */
    xgcvalues.graphics_exposures = False;
    p_intf->p_sys->gc =  XCreateGC( p_intf->p_sys->p_display, p_intf->p_sys->window,
                                    GCGraphicsExposures, &xgcvalues);

    /* Send orders to server, and wait until window is displayed - three
     * events must be received: a MapNotify event, an Expose event allowing
     * drawing in the window, and a ConfigureNotify to get the window
     * dimensions. Once those events have been received, only ConfigureNotify
     * events need to be received. */
    b_expose = 0;
    b_configure_notify = 0;
    b_map_notify = 0;
    XMapWindow( p_intf->p_sys->p_display, p_intf->p_sys->window);
    do
    {
        XNextEvent( p_intf->p_sys->p_display, &xevent);
        if( (xevent.type == Expose)
            && (xevent.xexpose.window == p_intf->p_sys->window) )
        {
            b_expose = 1;
        }
        else if( (xevent.type == MapNotify)
                 && (xevent.xmap.window == p_intf->p_sys->window) )
        {
            b_map_notify = 1;
        }
        else if( (xevent.type == ConfigureNotify)
                 && (xevent.xconfigure.window == p_intf->p_sys->window) )
        {
            b_configure_notify = 1;
            p_intf->p_sys->i_width = xevent.xconfigure.width;
            p_intf->p_sys->i_height = xevent.xconfigure.height;
        }
    } while( !( b_expose && b_configure_notify && b_map_notify ) );

    XSelectInput( p_intf->p_sys->p_display, p_intf->p_sys->window,
                  StructureNotifyMask | KeyPressMask | ButtonPressMask );

    if( XDefaultDepth(p_intf->p_sys->p_display, p_intf->p_sys->i_screen) == 8 )
    {
        /* Allocate a new palette */
        p_intf->p_sys->colormap = XCreateColormap( p_intf->p_sys->p_display,
                              DefaultRootWindow( p_intf->p_sys->p_display ),
                              DefaultVisual( p_intf->p_sys->p_display,
                                             p_intf->p_sys->i_screen ),
                              AllocAll );

        xwindow_attributes.colormap = p_intf->p_sys->colormap;
        XChangeWindowAttributes( p_intf->p_sys->p_display,
                                 p_intf->p_sys->window,
                                 CWColormap, &xwindow_attributes );
    }

    /* At this stage, the window is open, displayed, and ready to receive data */
    return( 0 );
}

/*****************************************************************************
 * GnomeDestroyWindow: destroy X11 main window
 *****************************************************************************/
static void GnomeDestroyWindow( intf_thread_t *p_intf )
{
    XUnmapWindow( p_intf->p_sys->p_display, p_intf->p_sys->window );
    XFreeGC( p_intf->p_sys->p_display, p_intf->p_sys->gc );
    XDestroyWindow( p_intf->p_sys->p_display, p_intf->p_sys->window );
}

/*****************************************************************************
 * GnomeManageWindow: manage X11 main window
 *****************************************************************************/
static void GnomeManageWindow( intf_thread_t *p_intf )
{
    XEvent      xevent;                                         /* X11 event */
    boolean_t   b_resized;                        /* window has been resized */
    char        i_key;                                    /* ISO Latin-1 key */

    /* Handle X11 events: ConfigureNotify events are parsed to know if the
     * output window's size changed, MapNotify and UnmapNotify to know if the
     * window is mapped (and if the display is useful), and ClientMessages
     * to intercept window destruction requests */
    b_resized = 0;
    while( XCheckWindowEvent( p_intf->p_sys->p_display, p_intf->p_sys->window,
                              StructureNotifyMask | KeyPressMask |
                              ButtonPressMask, &xevent ) == True )
    {
        /* ConfigureNotify event: prepare  */
        if( (xevent.type == ConfigureNotify)
            && ((xevent.xconfigure.width != p_intf->p_sys->i_width)
                || (xevent.xconfigure.height != p_intf->p_sys->i_height)) )
        {
            /* Update dimensions */
            b_resized = 1;
            p_intf->p_sys->i_width = xevent.xconfigure.width;
            p_intf->p_sys->i_height = xevent.xconfigure.height;
        }
        /* MapNotify event: change window status and disable screen saver */
        else if( xevent.type == MapNotify)
        {
            if( (p_intf->p_vout != NULL) && !p_intf->p_vout->b_active )
            {
                GnomeDisableScreenSaver( p_intf );
                p_intf->p_vout->b_active = 1;
            }
        }
        /* UnmapNotify event: change window status and enable screen saver */
        else if( xevent.type == UnmapNotify )
        {
            if( (p_intf->p_vout != NULL) && p_intf->p_vout->b_active )
            {
                GnomeEnableScreenSaver( p_intf );
                p_intf->p_vout->b_active = 0;
            }
        }
        /* Keyboard event */
        else if( xevent.type == KeyPress )
        {
            if( XLookupString( &xevent.xkey, &i_key, 1, NULL, NULL ) )
            {
                if( intf_ProcessKey( p_intf, i_key ) )
                {
                    intf_DbgMsg( "unhandled key '%c' (%i)\n", (char) i_key, i_key );
                }
            }
        }
        /* Mouse click */
        else if( xevent.type == ButtonPress )
        {
            switch( ((XButtonEvent *)&xevent)->button )
            {
                case Button1:
                    /* in this part we will eventually manage
                     * clicks for DVD navigation for instance */
                    break;

                case Button2:
                    GnomeTogglePointer( p_intf );
                    break;

                case Button3:
                    /* toggle the menu display */
                    vlc_mutex_lock( &p_intf->p_sys->p_gnome->change_lock );
                    p_intf->p_sys->p_gnome->b_popup_changed = 1;
                    vlc_mutex_unlock( &p_intf->p_sys->p_gnome->change_lock );
                    break;
            }

        }
#ifdef DEBUG
        /* Other event */
        else
        {
            intf_DbgMsg( "%p -> unhandled event type %d received\n",
                         p_intf, xevent.type );
        }
#endif
    }

    /* ClientMessage event - only WM_PROTOCOLS with WM_DELETE_WINDOW data
     * are handled - according to the man pages, the format is always 32
     * in this case */
    while( XCheckTypedEvent( p_intf->p_sys->p_display,
                             ClientMessage, &xevent ) )
    {
        if( (xevent.xclient.message_type == p_intf->p_sys->wm_protocols)
            && (xevent.xclient.data.l[0] == p_intf->p_sys->wm_delete_window ) )
        {
            p_intf->b_die = 1;
        }
        else
        {
            intf_DbgMsg( "%p -> unhandled ClientMessage received\n", p_intf );
        }
    }

    /*
     * Handle vout or interface windows resizing
     */
    if( p_intf->p_vout != NULL )
    {
        if( b_resized )
        {
            /* If interface window has been resized, change vout size */
            intf_DbgMsg( "resizing output window\n" );
            vlc_mutex_lock( &p_intf->p_vout->change_lock );
            p_intf->p_vout->i_width =  p_intf->p_sys->i_width;
            p_intf->p_vout->i_height = p_intf->p_sys->i_height;
            p_intf->p_vout->i_changes |= VOUT_SIZE_CHANGE;
            vlc_mutex_unlock( &p_intf->p_vout->change_lock );
        }
        else if( (p_intf->p_vout->i_width  != p_intf->p_sys->i_width) ||
                 (p_intf->p_vout->i_height != p_intf->p_sys->i_height) )
        {
           /* If video output size has changed, change interface window size */
            intf_DbgMsg( "resizing output window\n" );
            p_intf->p_sys->i_width =    p_intf->p_vout->i_width;
            p_intf->p_sys->i_height =   p_intf->p_vout->i_height;
            XResizeWindow( p_intf->p_sys->p_display, p_intf->p_sys->window,
                           p_intf->p_sys->i_width, p_intf->p_sys->i_height );
        }
    }
}

/*****************************************************************************
 * GnomeEnableScreenSaver: enable screen saver
 *****************************************************************************
 * This function enable the screen saver on a display after it had been
 * disabled by XDisableScreenSaver. Both functions use a counter mechanism to
 * know wether the screen saver can be activated or not: if n successive calls
 * are made to XDisableScreenSaver, n successive calls to XEnableScreenSaver
 * will be required before the screen saver could effectively be activated.
 *****************************************************************************/
void GnomeEnableScreenSaver( intf_thread_t *p_intf )
{
    if( p_intf->p_sys->i_ss_count++ == 0 )
    {
        intf_Msg( "Enabling screen saver\n" );
        XSetScreenSaver( p_intf->p_sys->p_display, p_intf->p_sys->i_ss_timeout,
                         p_intf->p_sys->i_ss_interval, p_intf->p_sys->i_ss_blanking,
                         p_intf->p_sys->i_ss_exposure );
    }
}

/*****************************************************************************
 * GnomeDisableScreenSaver: disable screen saver
 *****************************************************************************
 * See XEnableScreenSaver
 *****************************************************************************/
void GnomeDisableScreenSaver( intf_thread_t *p_intf )
{
    if( --p_intf->p_sys->i_ss_count == 0 )
    {
        /* Save screen saver informations */
        XGetScreenSaver( p_intf->p_sys->p_display, &p_intf->p_sys->i_ss_timeout,
                         &p_intf->p_sys->i_ss_interval, &p_intf->p_sys->i_ss_blanking,
                         &p_intf->p_sys->i_ss_exposure );

        /* Disable screen saver */
        intf_Msg("Disabling screen saver\n");
        XSetScreenSaver( p_intf->p_sys->p_display, 0,
                         p_intf->p_sys->i_ss_interval, p_intf->p_sys->i_ss_blanking,
                         p_intf->p_sys->i_ss_exposure );
    }
}

/*****************************************************************************
 * GnomeTogglePointer: hide or show the mouse pointer
 *****************************************************************************
 * This function hides the X pointer if it is visible by putting it at
 * coordinates (32,32) and setting the pointer sprite to a blank one. To
 * show it again, we disable the sprite and restore the original coordinates.
 *****************************************************************************/
void GnomeTogglePointer( intf_thread_t *p_intf )
{
    static Cursor cursor;
    static boolean_t b_cursor = 0;

    if( p_intf->p_sys->b_mouse )
    {
        p_intf->p_sys->b_mouse = 0;

        if( !b_cursor )
        {
            XColor color;
            Pixmap blank = XCreatePixmap( p_intf->p_sys->p_display,
                               DefaultRootWindow(p_intf->p_sys->p_display),
                               1, 1, 1 );

            XParseColor( p_intf->p_sys->p_display,
                         XCreateColormap( p_intf->p_sys->p_display,
                                          DefaultRootWindow(
                                                  p_intf->p_sys->p_display ),
                                          DefaultVisual(
                                                  p_intf->p_sys->p_display,
                                                  p_intf->p_sys->i_screen ),
                                          AllocNone ),
                         "black", &color );

            cursor = XCreatePixmapCursor( p_intf->p_sys->p_display,
                           blank, blank, &color, &color, 1, 1 );

            b_cursor = 1;
        }
        XDefineCursor( p_intf->p_sys->p_display,
                       p_intf->p_sys->window, cursor );
    }
    else
    {
        p_intf->p_sys->b_mouse = 1;

        XUndefineCursor( p_intf->p_sys->p_display, p_intf->p_sys->window );
    }
}

/*****************************************************************************
 * GnomeManageInterface: manage messages from the Gnome interface

 *****************************************************************************
 * In this function, called approx. 10 times a second, we check what the
 * Gnome interface wanted to tell us.
 *****************************************************************************/
static void GnomeManageInterface( intf_thread_t *p_intf )
{
    gnome_thread_t *p_gnome = p_intf->p_sys->p_gnome;

    /* lock the change structure */
    vlc_mutex_lock( &p_gnome->change_lock );

    /* you killed my father, prepare to die */
    if( p_gnome->b_die )
    {
        p_intf->b_die = 1;
    }

    if( p_gnome->b_activity_changed )
    {
        vlc_mutex_lock( &p_intf->p_vout->picture_lock );
        p_intf->p_vout->b_active = p_gnome->b_activity;
        /* having to access p_main sucks */
        p_main->p_aout->b_active = p_gnome->b_activity;
        vlc_mutex_unlock( &p_intf->p_vout->picture_lock );

        p_gnome->b_activity_changed = 0;
    }

    /* unlock the change structure */
    vlc_mutex_unlock( &p_gnome->change_lock );
}

/*****************************************************************************
 * GnomeManageMain: manage main thread messages
 *****************************************************************************
 * In this function, called approx. 10 times a second, we check what the
 * main program wanted to tell us.
 *****************************************************************************/
static gint GnomeManageMain( gpointer p_data )
{
    gnome_thread_t *p_gnome = (void *)p_data;

    /* lock the change structure */
    vlc_mutex_lock( &p_gnome->change_lock );

    if( p_gnome->b_die )
    {
        /* unlock the change structure */
        vlc_mutex_unlock( &p_gnome->change_lock );

        /* prepare to die, young man */
        gtk_main_quit();
        return( FALSE );
    }

    /* if the "display popup" flag has changed */
    if( p_gnome->b_popup_changed )
    {
        gnome_popup_menu_do_popup( p_gnome->p_popup,
                                   NULL, NULL, NULL, NULL );
        p_gnome->b_popup_changed = 0;
    }

    /* unlock the change structure */
    vlc_mutex_unlock( &p_gnome->change_lock );

    return( TRUE );
}

/*****************************************************************************
 * GnomeThread: special Gnome thread
 *****************************************************************************
 * this part of the interface is in a separate thread so that we can call
 * gtk_main() from within it without annoying the rest of the program.
 * XXX: the approach may look kludgy, and probably is, but I could not find
 * a better way to dynamically load a Gnome interface at runtime.
 *****************************************************************************/
void GnomeThread( gnome_thread_t *p_gnome )
{
    /* gnome_init needs to know the command line. We don't care, so we
     * give it an empty one */
    char *p_args[] = { };

    /* Sleep to avoid using all CPU - since some interfaces needs to access
     * keyboard events, a 100ms delay is a good compromise */
    gtk_timeout_add( INTF_IDLE_SLEEP / 1000, GnomeManageMain, p_gnome );
 
    gnome_init( "vlc", VERSION, 1, p_args );

    /* create some useful widgets that will certainly be used */
    p_gnome->p_window = create_intf_window();
    p_gnome->p_popup = create_intf_popup( );

    /* we don't create these ones yet because we perhaps won't need them */
    p_gnome->p_about = NULL;
    p_gnome->p_playlist = NULL;

    /* store p_sys to keep an eye on it */
    gtk_object_set_data( GTK_OBJECT(p_gnome->p_window), "p_gnome", p_gnome );
    gtk_object_set_data( GTK_OBJECT(p_gnome->p_popup), "p_gnome", p_gnome );

    /* show the control window */
    //gtk_widget_show( p_gnome->p_window );

    /* enter gnome mode */
    gtk_main();
}

