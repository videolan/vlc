/*****************************************************************************
* maemo.c : Maemo plugin for VLC
*****************************************************************************
* Copyright (C) 2008 the VideoLAN team
* $Id$
*
* Authors: Antoine Lejeune <phytos@videolan.org>
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

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_window.h>

#include <hildon/hildon-program.h>
#include <hildon/hildon-banner.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <inttypes.h>

#include "maemo.h"
#include "maemo_callbacks.h"
#include "maemo_input.h"
#include "maemo_interface.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int      Open               ( vlc_object_t * );
static void     Close              ( vlc_object_t * );
static void     Run                ( intf_thread_t * );
static gboolean should_die         ( gpointer );
static int      OpenWindow         ( vlc_object_t * );
static void     CloseWindow        ( vlc_object_t * );
static int      ControlWindow      ( vout_window_t *, int, va_list );
static void*    request_video      ( intf_thread_t *, vout_thread_t * );
static void     release_video      ( intf_thread_t * );
static gboolean video_widget_ready ( gpointer data );

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
        set_capability( "xwindow", 50 );
        set_callbacks( OpenWindow, CloseWindow );
vlc_module_end();

/*****************************************************************************
 * Module callbacks
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        msg_Err( p_intf, "out of memory" );
        return VLC_ENOMEM;
    }

    p_intf->pf_run = Run;

    p_intf->p_sys->p_playlist = pl_Hold( p_intf );
    p_intf->p_sys->p_input = NULL;
    p_intf->p_sys->p_vout = NULL;

    p_intf->p_sys->p_main_window = NULL;
    p_intf->p_sys->p_video_window = NULL;

    vlc_spin_init( &p_intf->p_sys->event_lock );
    vlc_mutex_init( &p_intf->p_sys->p_video_mutex );
    vlc_cond_init( &p_intf->p_sys->p_video_cond );

    return VLC_SUCCESS;
}

static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    vlc_object_release( p_intf->p_sys->p_playlist );

    vlc_spin_destroy( &p_intf->p_sys->event_lock );

    /* Destroy structure */
    free( p_intf->p_sys );
}

/*****************************************************************************
* Initialize and launch the interface
*****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    char  *p_args[] = { "", NULL };
    char **pp_args  = p_args;
    int    i_args   = 1;

    HildonProgram *program;
    HildonWindow *window;
    GtkWidget *main_vbox;

    GtkWidget *tabs;
    GtkWidget *video;
    GtkWidget *bottom_hbox;
    GtkWidget *play_button;
    GtkWidget *prev_button;
    GtkWidget *next_button;
    GtkWidget *stop_button;
    GtkWidget *seekbar;

    gtk_init( &i_args, &pp_args );

    program = HILDON_PROGRAM( hildon_program_get_instance() );
    g_set_application_name( "VLC Media Player" );

    window = HILDON_WINDOW( hildon_window_new() );
    hildon_program_add_window( program, window );
    gtk_object_set_data( GTK_OBJECT( window ),
                         "p_intf", p_intf );
    p_intf->p_sys->p_main_window = window;

    // A little theming
    char *psz_rc_file = NULL;
    if( asprintf( &psz_rc_file, "%s/maemo/vlc_intf.rc",
                  config_GetDataDir() ) != -1 )
    {
        gtk_rc_parse( psz_rc_file );
        free( psz_rc_file );
    }

    // We create the main vertical box
    main_vbox = gtk_vbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER( window ), main_vbox );

    tabs = gtk_notebook_new();
    p_intf->p_sys->p_tabs = tabs;
    gtk_notebook_set_tab_pos( GTK_NOTEBOOK( tabs ), GTK_POS_LEFT );
    gtk_notebook_set_show_border( GTK_NOTEBOOK( tabs ), FALSE );
    gtk_box_pack_start( GTK_BOX( main_vbox ), tabs, TRUE, TRUE, 0 );

    // We put first the embedded video
    video = gtk_event_box_new();
    gtk_notebook_append_page( GTK_NOTEBOOK( tabs ),
                video,
                gtk_image_new_from_stock( "vlc",
                                          GTK_ICON_SIZE_DIALOG ) );
    gtk_notebook_set_tab_label_packing( GTK_NOTEBOOK( tabs ),
                                        video,
                                        FALSE, FALSE, 0 );
    create_playlist( p_intf );

    // We put the horizontal box which contains all the buttons
    bottom_hbox = gtk_hbox_new( FALSE, 0 );

    // We create the buttons
    play_button = gtk_button_new();
    gtk_button_set_image( GTK_BUTTON( play_button ),
                   gtk_image_new_from_stock( "vlc-play", GTK_ICON_SIZE_BUTTON ) );
    gtk_widget_set_size_request( play_button, 60, 60);
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
    seekbar = hildon_seekbar_new();
    p_intf->p_sys->p_seekbar = HILDON_SEEKBAR( seekbar );

    // We add them to the hbox
    gtk_box_pack_start( GTK_BOX( bottom_hbox ), play_button, FALSE, FALSE, 5 );
    gtk_box_pack_start( GTK_BOX( bottom_hbox ), stop_button, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( bottom_hbox ), prev_button, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( bottom_hbox ), next_button, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( bottom_hbox ), seekbar    , TRUE , TRUE , 5 );
    // We add the hbox to the main vbox
    gtk_box_pack_start( GTK_BOX( main_vbox ), bottom_hbox, FALSE, FALSE, 0 );

    g_signal_connect( window, "delete_event",
                      G_CALLBACK( delete_event_cb ), NULL );
    g_signal_connect( play_button, "clicked", G_CALLBACK( play_cb ), NULL );
    g_signal_connect( stop_button, "clicked", G_CALLBACK( stop_cb ), NULL );
    g_signal_connect( prev_button, "clicked", G_CALLBACK( prev_cb ), NULL );
    g_signal_connect( next_button, "clicked", G_CALLBACK( next_cb ), NULL );
    g_signal_connect( seekbar, "change-value",
                      G_CALLBACK( seekbar_changed_cb ), NULL );

    gtk_widget_show_all( GTK_WIDGET( window ) );

    create_menu( p_intf );

    // Set callback with the vlc core
    g_timeout_add( INTF_IDLE_SLEEP / 1000, process_events, p_intf );
    g_timeout_add( 150 /* miliseconds */, should_die, p_intf );
    var_AddCallback( p_intf->p_sys->p_playlist, "item-change",
                     item_changed_cb, p_intf );
    var_AddCallback( p_intf->p_sys->p_playlist, "item-current",
                     playlist_current_cb, p_intf );
    var_AddCallback( p_intf->p_sys->p_playlist, "activity",
                     activity_cb, p_intf );

    // Look if the playlist is already started
    item_changed_pl( p_intf );

    // The embedded video is only ready after gtk_main and windows are shown
    g_idle_add( video_widget_ready, video );

    gtk_main();

    delete_input( p_intf );
    var_DelCallback( p_intf->p_sys->p_playlist, "item-change",
                     item_changed_cb, p_intf );
    var_DelCallback( p_intf->p_sys->p_playlist, "item-current",
                     playlist_current_cb, p_intf );
    var_DelCallback( p_intf->p_sys->p_playlist, "activity",
                     activity_cb, p_intf );

    assert( !p_intf->p_sys->p_vout ); /* too late */
    gtk_object_destroy( GTK_OBJECT( window ) );
}

static gboolean should_die( gpointer data )
{
    intf_thread_t *p_intf = (intf_thread_t *)data;
    if( !vlc_object_alive( p_intf ) )
        gtk_main_quit();
    return TRUE;
}

/**
* Video output window provider
*/
static int OpenWindow (vlc_object_t *obj)
{
    vout_window_t *wnd = (vout_window_t *)obj;

    /* TODO: should probably be in the libvlc core instead: */
    if (!config_GetInt (obj, "embedded-video"))
        return VLC_EGENERIC;

    intf_thread_t *intf = (intf_thread_t *)
    vlc_object_find_name (obj, "maemo", FIND_ANYWHERE);
    if (intf == NULL)
    {
        msg_Err( obj, "Maemo interface not found" );
        return VLC_EGENERIC; /* Maemo not in use */
    }

    wnd->handle.xid = request_video( intf, wnd->vout );
    msg_Dbg( intf, "Using handle %"PRIu32, wnd->handle.xid );

    wnd->control = ControlWindow;
    wnd->p_private = intf;

    // Signaling that the window is not at the requested sizeof
    int i_width, i_height, i_x_top, i_y_top, i_x, i_y;
    gdk_drawable_get_size( GDK_DRAWABLE( intf->p_sys->p_video_window->window ),
                           &i_width, &i_height );
    gdk_window_get_position( GTK_WIDGET(intf->p_sys->p_main_window)->window,
                             &i_x_top, &i_y_top );
    gdk_window_get_position( intf->p_sys->p_video_window->window, &i_x, &i_y );

    wnd->width = i_width;
    wnd->height = i_height;
    wnd->pos_x = i_x_top + i_x;
    wnd->pos_y = i_y_top + i_y;

    return VLC_SUCCESS;
}

static int ControlWindow (vout_window_t *wnd, int query, va_list args)
{
    (void)wnd; (void)query; (void)args;
    return VLC_EGENERIC;
}

static void CloseWindow (vlc_object_t *obj)
{
    intf_thread_t *intf = (intf_thread_t *)obj->p_private;

    release_video( intf );
    vlc_object_release (intf);
}

static void *request_video( intf_thread_t *p_intf, vout_thread_t *p_nvout )
{
    if( p_intf->p_sys->p_vout )
    {
        msg_Dbg( p_intf, "Embedded video already in use" );
        return NULL;
    }

    vlc_mutex_lock( &p_intf->p_sys->p_video_mutex );
    mutex_cleanup_push( &p_intf->p_sys->p_video_mutex );

    // We wait until the p_video_window is set
    while( p_intf->p_sys->p_video_window == NULL )
        vlc_cond_wait( &p_intf->p_sys->p_video_cond,
                       &p_intf->p_sys->p_video_mutex );

    vlc_cleanup_run();

    p_intf->p_sys->p_vout = p_nvout;
    return ( void * )GDK_WINDOW_XID( p_intf->p_sys->p_video_window->window );
}

static void release_video( intf_thread_t *p_intf )
{
    msg_Dbg( p_intf, "Releasing embedded video" );
    p_intf->p_sys->p_vout = NULL;
}

static gboolean video_widget_ready( gpointer data )
{
    intf_thread_t *p_intf = NULL;
    GtkWidget *top_window = NULL;
    GtkWidget *video = (GtkWidget *)data;

    top_window = gtk_widget_get_toplevel( GTK_WIDGET( video ) );
    p_intf = (intf_thread_t *)gtk_object_get_data( GTK_OBJECT( top_window ),
                                                   "p_intf" );
    p_intf->p_sys->p_video_window = video;
    gtk_widget_grab_focus( video );

    vlc_cond_signal( &p_intf->p_sys->p_video_cond );

    // We rewind the input
    if( p_intf->p_sys->p_input )
    {
        input_Control( p_intf->p_sys->p_input, INPUT_SET_POSITION, 0.0 );
    }

    // We want it to be executed only one time
    return FALSE;
}
