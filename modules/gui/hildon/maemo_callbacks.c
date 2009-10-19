/*****************************************************************************
* maemo_callbacks.c : Callbacks for the maemo plugin.
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

#include <vlc_common.h>

#include "maemo.h"
#include "maemo_callbacks.h"

/*
 * Function used to retrieve an intf_thread_t object from a GtkWidget
 */
static intf_thread_t *get_intf_from_widget( GtkWidget *widget )
{
    if( GTK_IS_MENU_ITEM( widget ) )
    {
        /* Look for a GTK_MENU */
        while( widget->parent && !GTK_IS_MENU( widget ) )
        {
            widget = widget->parent;
        }

        widget = gtk_menu_get_attach_widget( GTK_MENU( widget ) );
    }
    widget = gtk_widget_get_toplevel( GTK_WIDGET( widget ) );
    return (intf_thread_t *)gtk_object_get_data( GTK_OBJECT( widget ),
                                                 "p_intf" );
}

gboolean delete_event_cb( GtkWidget *widget,
                          GdkEvent *event,
                          gpointer user_data )
{
    (void)event; (void)user_data;
    intf_thread_t *p_intf = get_intf_from_widget( widget );

    libvlc_Quit( p_intf->p_libvlc );
    gtk_main_quit();

    return TRUE;
}

void play_cb( GtkButton *button, gpointer user_data )
{
    (void)user_data;
    intf_thread_t *p_intf = get_intf_from_widget( GTK_WIDGET( button ) );

    // If there is no input, we ask the playlist to play
    if( p_intf->p_sys->p_input == NULL )
    {
        playlist_Play( p_intf->p_sys->p_playlist );
        return;
    }

    // If there is an input, we toggle its state
    vlc_value_t state;
    var_Get( p_intf->p_sys->p_input, "state", &state );
    state.i_int = ( state.i_int != PLAYING_S ) ? PLAYING_S : PAUSE_S;
    var_Set( p_intf->p_sys->p_input, "state", state );
}

void stop_cb( GtkButton *button, gpointer user_data )
{
    (void)user_data;
    intf_thread_t *p_intf = get_intf_from_widget( GTK_WIDGET( button ) );
    playlist_Stop( p_intf->p_sys->p_playlist );
}

void prev_cb( GtkButton *button, gpointer user_data )
{
    (void)user_data;
    intf_thread_t *p_intf = get_intf_from_widget( GTK_WIDGET( button ) );
    playlist_Prev( p_intf->p_sys->p_playlist );
}

void next_cb( GtkButton *button, gpointer user_data )
{
    (void)user_data;
    intf_thread_t *p_intf = get_intf_from_widget( GTK_WIDGET( button ) );
    playlist_Next( p_intf->p_sys->p_playlist );
}

void seekbar_changed_cb( GtkRange *range, GtkScrollType scroll,
                         gdouble value, gpointer data )
{
    (void)scroll; (void)data;
    intf_thread_t *p_intf = get_intf_from_widget( GTK_WIDGET( range ) );
    if( p_intf->p_sys->p_input )
    {
        int i_length = hildon_seekbar_get_total_time( p_intf->p_sys->p_seekbar );
        var_SetFloat( p_intf->p_sys->p_input, "position", (float)(value/i_length) );
    }
}

void pl_row_activated_cb( GtkTreeView *tree_view , GtkTreePath *path,
                          GtkTreeViewColumn *column, gpointer user_data )
{
    (void)column; (void)user_data;
    intf_thread_t *p_intf = get_intf_from_widget( GTK_WIDGET( tree_view ) );
    input_item_t *p_input;
    GtkTreeModel *model = gtk_tree_view_get_model( tree_view );
    GtkTreeIter iter;
    gchar *filename = NULL;

    gtk_tree_model_get_iter( model, &iter, path );
    gtk_tree_model_get( model, &iter, 0, &filename, -1 );

    gtk_notebook_set_current_page( GTK_NOTEBOOK( p_intf->p_sys->p_tabs ), 0 );

    p_input = input_item_New( p_intf, filename, NULL );
    playlist_AddInput( p_intf->p_sys->p_playlist, p_input,
                       PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END, true, false );
    vlc_gc_decref( p_input );
}

void open_cb( GtkMenuItem *menuitem, gpointer user_data )
{
    (void)menuitem;
    intf_thread_t *p_intf = (intf_thread_t *)user_data;
    input_item_t *p_input;
    GtkWidget *dialog;
    char *psz_filename = NULL;

    dialog = hildon_file_chooser_dialog_new( GTK_WINDOW( p_intf->p_sys->p_main_window ),
                                             GTK_FILE_CHOOSER_ACTION_OPEN );
    gtk_widget_show_all( GTK_WIDGET( dialog ) );

    if( gtk_dialog_run( GTK_DIALOG( dialog ) ) == GTK_RESPONSE_OK )
    {
        psz_filename = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER( dialog ) );
    }
    else
    {
        gtk_widget_destroy( dialog );
        return;
    }

    gtk_widget_destroy( dialog );

    p_input = input_item_New( p_intf, psz_filename, NULL );
    playlist_AddInput( p_intf->p_sys->p_playlist, p_input,
                       PLAYLIST_APPEND | PLAYLIST_GO,
                       PLAYLIST_END, true, false );
    vlc_gc_decref( p_input );
}

void open_address_cb( GtkMenuItem *menuitem, gpointer user_data )
{
    (void)menuitem;
    intf_thread_t *p_intf = (intf_thread_t *)user_data;
    input_item_t *p_input;
    GtkWidget *dialog, *hbox, *label, *entry;

    dialog = gtk_dialog_new_with_buttons( "Open Address",
                GTK_WINDOW( p_intf->p_sys->p_main_window ),
                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_STOCK_OK, GTK_RESPONSE_OK,
                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                NULL );
    label = gtk_label_new( "Address :" );
    entry = gtk_entry_new();
    gtk_entry_set_width_chars( GTK_ENTRY( entry ), 30 );
    hbox = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( hbox ), label, FALSE, FALSE, 0 );
    gtk_box_pack_start( GTK_BOX( hbox ), entry, TRUE, TRUE, 0 );
    gtk_container_add( GTK_CONTAINER( GTK_DIALOG( dialog )->vbox ), hbox );

    gtk_widget_show_all( dialog );
    if( gtk_dialog_run( GTK_DIALOG( dialog ) ) == GTK_RESPONSE_CANCEL )
    {
        gtk_widget_destroy( dialog );
        return;
    }

    p_input = input_item_New( p_intf,
                              gtk_entry_get_text( GTK_ENTRY( entry ) ),
                              NULL );
    playlist_AddInput( p_intf->p_sys->p_playlist, p_input,
                       PLAYLIST_APPEND | PLAYLIST_GO,
                       PLAYLIST_END, true, false );
    vlc_gc_decref( p_input );

    gtk_widget_destroy( dialog );
}

void open_webcam_cb( GtkMenuItem *menuitem, gpointer user_data )
{
    (void)menuitem;
    intf_thread_t *p_intf = (intf_thread_t *)user_data;
    input_item_t *p_input;

    p_input = input_item_New( p_intf, "v4l2://", NULL );
    playlist_AddInput( p_intf->p_sys->p_playlist, p_input,
                       PLAYLIST_APPEND | PLAYLIST_GO,
                       PLAYLIST_END, true, false );
    vlc_gc_decref( p_input );
}

void snapshot_cb( GtkMenuItem *menuitem, gpointer user_data )
{
    (void)menuitem;
    intf_thread_t *p_intf = (intf_thread_t *)user_data;

    if( !p_intf->p_sys->p_vout )
    {
        hildon_banner_show_information(
                                GTK_WIDGET( p_intf->p_sys->p_main_window ),
                                "gtk-dialog-error",
                                "There is no video" );
        return;
    }

    var_TriggerCallback( p_intf->p_sys->p_vout, "video-snapshot" );
    hildon_banner_show_information( GTK_WIDGET( p_intf->p_sys->p_main_window ),
                                    NULL,
                                    "Snapshot taken" );
}

void dropframe_cb( GtkMenuItem *menuitem, gpointer user_data )
{
    intf_thread_t *p_intf = (intf_thread_t *)user_data;

    if( gtk_check_menu_item_get_active( GTK_CHECK_MENU_ITEM( menuitem ) ) )
        config_PutInt( p_intf, "ffmpeg-skip-frame", 1 );
    else
        config_PutInt( p_intf, "ffmpeg-skip-frame", 0 );
}
