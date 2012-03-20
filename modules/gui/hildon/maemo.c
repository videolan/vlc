/****************************************************************************
 * maemo.c : Maemo plugin for VLC
 *****************************************************************************
 * Copyright (C) 2008 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Lejeune <phytos@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_vout_window.h>
#include <vlc_xlib.h>

#include <hildon/hildon-program.h>
#include <hildon/hildon-banner.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <stdio.h>
#include <inttypes.h>

#include "maemo.h"
#include "maemo_callbacks.h"
#include "maemo_input.h"
#include "maemo_interface.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int      Open            ( vlc_object_t * );
static void     Close           ( vlc_object_t * );
static void     *Thread         ( void * );
static int      OpenWindow      ( vout_window_t *, const vout_window_cfg_t * );
static void     CloseWindow     ( vout_window_t * );
static int      ControlWindow   ( vout_window_t *, int, va_list );
static gboolean interface_ready ( gpointer );

/*****************************************************************************
* Module descriptor
*****************************************************************************/
vlc_module_begin();
    set_shortname( "Maemo" );
    set_description( N_("Maemo hildon interface") );
    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_MAIN );
    set_capability( "interface", 70 );
    set_callbacks( Open, Close );
    add_shortcut( "maemo" );

    add_submodule();
        set_capability( "vout window xid", 50 );
        set_callbacks( OpenWindow, CloseWindow );
vlc_module_end();

/*****************************************************************************
 * Module callbacks
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys;
    vlc_value_t val;

    if( !vlc_xlib_init( p_this ) )
        return VLC_EGENERIC;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
        return VLC_ENOMEM;

    p_sys->p_playlist = pl_Get( p_intf );
    p_sys->p_input = NULL;

    p_sys->p_main_window = NULL;
    p_sys->p_video_window = NULL;
    p_sys->p_control_window = NULL;
    p_sys->b_fullscreen = false;
    p_sys->i_event = 0;

    vlc_spin_init( &p_sys->event_lock );

    /* Create separate thread for main interface */
    vlc_sem_init (&p_sys->ready, 0);
    if( vlc_clone( &p_sys->thread, Thread, p_intf, VLC_THREAD_PRIORITY_LOW ) )
    {
        free (p_sys);
        return VLC_ENOMEM;
    }

    /* Wait for interface thread to be fully initialised */
    vlc_sem_wait (&p_sys->ready);
    vlc_sem_destroy (&p_sys->ready);

    var_Create (p_this->p_libvlc, "hildon-iface", VLC_VAR_ADDRESS);
    val.p_address = p_this;
    var_Set (p_this->p_libvlc, "hildon-iface", val);

    return VLC_SUCCESS;
}

static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    var_Destroy (p_this->p_libvlc, "hildon-iface");

    gtk_main_quit();
    vlc_join (p_intf->p_sys->thread, NULL);
    vlc_spin_destroy( &p_intf->p_sys->event_lock );
    free( p_intf->p_sys );
}

static gint quit_event( GtkWidget *widget, GdkEvent *event, gpointer data )
{
    intf_thread_t *p_intf = (intf_thread_t *)data;
    (void)widget; (void)event;
    libvlc_Quit( p_intf->p_libvlc );
    return TRUE;
}

/*****************************************************************************
* Initialize and launch the interface
*****************************************************************************/
static void *Thread( void *obj )
{
    intf_thread_t *p_intf = (intf_thread_t *)obj;
    const char *p_args[] = { "vlc" };
    int i_args = sizeof(p_args)/sizeof(char *);
    char **pp_args  = (char **)p_args;

    HildonProgram *program;
    HildonWindow *window;
    GtkWidget *main_vbox, *bottom_hbox;
    GtkWidget *video, *seekbar;
    GtkWidget *play_button, *prev_button, *next_button;
    GtkWidget *stop_button, *playlist_button;

    gtk_init( &i_args, &pp_args );

    program = HILDON_PROGRAM( hildon_program_get_instance() );
    g_set_application_name( "VLC Media Player" );

    window = HILDON_WINDOW( hildon_window_new() );
    hildon_program_add_window( program, window );
    gtk_object_set_data( GTK_OBJECT( window ), "p_intf", p_intf );
    p_intf->p_sys->p_main_window = window;

    g_signal_connect( GTK_WIDGET(window), "key-press-event",
                      G_CALLBACK( key_cb ), p_intf );
    g_signal_connect (GTK_WIDGET(window), "delete_event",
                      GTK_SIGNAL_FUNC( quit_event), p_intf );

    // A little theming
    char *psz_rc_file = NULL;
    char *psz_data = config_GetDataDir();
    if( asprintf( &psz_rc_file, "%s/maemo/vlc_intf.rc", psz_data ) != -1 )
    {
        gtk_rc_parse( psz_rc_file );
        free( psz_rc_file );
    }
    free( psz_data );

    // We create the main vertical box
    main_vbox = gtk_vbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( window ), main_vbox );

    // Menubar
    GtkWidget *main_menu = create_menu( p_intf );
#ifdef HAVE_MAEMO
    hildon_window_set_menu( HILDON_WINDOW( p_intf->p_sys->p_main_window ),
                            GTK_MENU( main_menu ) );
#else
    GtkWidget *menu_bar = gtk_menu_bar_new ();
    GtkWidget *item = gtk_menu_item_new_with_label ("Menu");
    gtk_menu_bar_append(menu_bar, item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), main_menu);
    gtk_widget_show_all (menu_bar);
    gtk_box_pack_start(GTK_BOX(main_vbox), menu_bar, FALSE, FALSE, 0);
#endif

    // We put first the embedded video
    video = gtk_event_box_new();
    GdkColor black = {0,0,0,0};
    gtk_widget_modify_bg(video, GTK_STATE_NORMAL, &black);
    p_intf->p_sys->p_video_window = video;
    gtk_box_pack_start( GTK_BOX( main_vbox ), video, TRUE, TRUE, 0 );

    create_playlist( p_intf );
    gtk_box_pack_start( GTK_BOX( main_vbox ), p_intf->p_sys->p_playlist_window, TRUE, TRUE, 0 );

    // We put the horizontal box which contains all the buttons
    p_intf->p_sys->p_control_window = bottom_hbox = gtk_hbox_new( FALSE, 0 );

    // We create the buttons
    play_button = gtk_button_new();
    gtk_button_set_image( GTK_BUTTON( play_button ),
                   gtk_image_new_from_stock( "vlc-play", GTK_ICON_SIZE_BUTTON ) );
    p_intf->p_sys->p_play_button = play_button;
    stop_button = gtk_button_new();
    gtk_button_set_image( GTK_BUTTON( stop_button ),
                          gtk_image_new_from_stock( "vlc-stop", GTK_ICON_SIZE_BUTTON ) );
    prev_button = gtk_button_new();
    gtk_button_set_image( GTK_BUTTON( prev_button ),
                      gtk_image_new_from_stock( "vlc-previous", GTK_ICON_SIZE_BUTTON ) );
    next_button = gtk_button_new();
    gtk_button_set_image( GTK_BUTTON( next_button ),
                      gtk_image_new_from_stock( "vlc-next", GTK_ICON_SIZE_BUTTON ) );
    playlist_button = gtk_button_new();
    gtk_button_set_image( GTK_BUTTON( playlist_button ),
                          gtk_image_new_from_stock( "vlc-playlist", GTK_ICON_SIZE_BUTTON ) );
    seekbar = hildon_seekbar_new();
    p_intf->p_sys->p_seekbar = HILDON_SEEKBAR( seekbar );

    // We add them to the hbox
    gtk_box_pack_start( GTK_BOX( bottom_hbox ), play_button, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( bottom_hbox ), stop_button, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( bottom_hbox ), prev_button, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( bottom_hbox ), next_button, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( bottom_hbox ), playlist_button, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( bottom_hbox ), seekbar    , TRUE , TRUE , 5 );
    // We add the hbox to the main vbox
    gtk_box_pack_start( GTK_BOX( main_vbox ), bottom_hbox, FALSE, FALSE, 0 );

    g_signal_connect( play_button, "clicked", G_CALLBACK( play_cb ), NULL );
    g_signal_connect( stop_button, "clicked", G_CALLBACK( stop_cb ), NULL );
    g_signal_connect( prev_button, "clicked", G_CALLBACK( prev_cb ), NULL );
    g_signal_connect( next_button, "clicked", G_CALLBACK( next_cb ), NULL );
    g_signal_connect( playlist_button, "clicked", G_CALLBACK( playlist_cb ), NULL );
    g_signal_connect( seekbar, "change-value",
                      G_CALLBACK( seekbar_changed_cb ), NULL );

    gtk_widget_show_all( GTK_WIDGET( window ) );
    gtk_widget_hide_all( p_intf->p_sys->p_playlist_window );

#if 1
    /* HACK: Only one X11 client can subscribe to mouse button press events.
     * VLC currently handles those in the video display.
     * Force GTK to unsubscribe from mouse press and release events. */
    Display *dpy = GDK_WINDOW_XDISPLAY( gtk_widget_get_window(p_intf->p_sys->p_video_window) );
    Window w = GDK_WINDOW_XID( gtk_widget_get_window(p_intf->p_sys->p_video_window) );
    XWindowAttributes attr;

    XGetWindowAttributes( dpy, w, &attr );
    attr.your_event_mask &= ~(ButtonPressMask|ButtonReleaseMask);
    XSelectInput( dpy, w, attr.your_event_mask );
#endif

    // The embedded video is only ready after gtk_main and windows are shown
    g_idle_add( interface_ready, p_intf );

    gtk_main();

    delete_input( p_intf );
    delete_playlist( p_intf );

    gtk_object_destroy( GTK_OBJECT( main_menu ) );
    gtk_object_destroy( GTK_OBJECT( window ) );

    return NULL;
}

/**
* Video output window provider
*/
static int OpenWindow (vout_window_t *p_wnd, const vout_window_cfg_t *cfg)
{
    intf_thread_t *p_intf;
    vlc_value_t val;

    if (cfg->is_standalone)
        return VLC_EGENERIC;

    if( var_Get( p_obj->p_libvlc, "hildon-iface", &val ) )
        val.p_address = NULL;

    p_intf = (intf_thread_t *)val.p_address;
    if( !p_intf )
    {   /* If another interface is used, this plugin cannot work */
        msg_Dbg( p_obj, "Hildon interface not found" );
        return VLC_EGENERIC;
    }

    p_wnd->handle.xid = p_intf->p_sys->xid;

    if (!p_wnd->handle.xid)
        return VLC_EGENERIC;

    p_wnd->control = ControlWindow;
    p_wnd->sys = (vout_window_sys_t*)p_intf;

    return VLC_SUCCESS;
}

static int ControlWindow (vout_window_t *p_wnd, int query, va_list args)
{
    intf_thread_t *p_intf = (intf_thread_t *)p_wnd->sys;

    switch( query )
    {
    case VOUT_WINDOW_SET_SIZE:
    {
        int i_width  = (int)va_arg( args, int );
        int i_height = (int)va_arg( args, int );

        int i_current_w, i_current_h;
        gdk_drawable_get_size( GDK_DRAWABLE( p_intf->p_sys->p_video_window ),
                               &i_current_w, &i_current_h );
        if( i_width != i_current_w || i_height != i_current_h )
            return VLC_EGENERIC;
        return VLC_SUCCESS;
    }
    case VOUT_WINDOW_SET_FULLSCREEN:
    {
        bool b_fs = va_arg( args, int );
        p_intf->p_sys->b_fullscreen = b_fs;
        g_idle_add( fullscreen_cb, p_intf );
        return VLC_SUCCESS;
    }
    default:
        return VLC_EGENERIC;
    }
}

static void CloseWindow (vout_window_t *p_wnd)
{
    intf_thread_t *p_intf = (intf_thread_t *)p_wnd->sys;

    if( p_intf->p_sys->b_fullscreen )
    {
        p_intf->p_sys->b_fullscreen = false;
        g_idle_add( fullscreen_cb, p_intf );
    }
}

static gboolean interface_ready( gpointer data )
{
    intf_thread_t *p_intf = (intf_thread_t *)data;

    p_intf->p_sys->xid =
        GDK_WINDOW_XID( gtk_widget_get_window(p_intf->p_sys->p_video_window) );

    // Refresh playlist
    post_event( p_intf, EVENT_PLAYLIST_CURRENT );

    // Everything is initialised
    vlc_sem_post (&p_intf->p_sys->ready);

    // We want it to be executed only one time
    return FALSE;
}
