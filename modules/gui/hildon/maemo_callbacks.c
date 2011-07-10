/*****************************************************************************
 * maemo_callbacks.c : Callbacks for the maemo plugin.
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

#include <vlc_common.h>

#include "maemo.h"
#include "maemo_callbacks.h"

#include <gdk/gdkkeysyms.h>
#include <vlc_keys.h>

#ifdef HAVE_HILDON_FM
# include <hildon/hildon-file-chooser-dialog.h>
#endif

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

void playlist_cb( GtkButton *button, gpointer user_data )
{
    (void)user_data;
    intf_thread_t *p_intf = get_intf_from_widget( GTK_WIDGET( button ) );
    if( GTK_WIDGET_VISIBLE(p_intf->p_sys->p_playlist_window) )
    {
      gtk_widget_show_all( p_intf->p_sys->p_video_window );
      gtk_widget_hide_all( p_intf->p_sys->p_playlist_window );
    }
    else
    {
      gtk_widget_hide_all( p_intf->p_sys->p_video_window );
      gtk_widget_show_all( p_intf->p_sys->p_playlist_window );
    }
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

    p_input = input_item_New( filename, NULL );
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

#ifdef HAVE_HILDON_FM
    dialog = hildon_file_chooser_dialog_new( GTK_WINDOW( p_intf->p_sys->p_main_window ),
                                             GTK_FILE_CHOOSER_ACTION_OPEN );
#else
    dialog = gtk_file_chooser_dialog_new( "Open File", GTK_WINDOW( p_intf->p_sys->p_main_window ),
                                          GTK_FILE_CHOOSER_ACTION_OPEN,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_OK, GTK_RESPONSE_OK, NULL );
#endif
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

    p_input = input_item_New( psz_filename, NULL );
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

    p_input = input_item_New( gtk_entry_get_text( GTK_ENTRY( entry ) ),
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

    p_input = input_item_New( "v4l2://", NULL );
    playlist_AddInput( p_intf->p_sys->p_playlist, p_input,
                       PLAYLIST_APPEND | PLAYLIST_GO,
                       PLAYLIST_END, true, false );
    vlc_gc_decref( p_input );
}

void snapshot_cb( GtkMenuItem *menuitem, gpointer user_data )
{
    intf_thread_t *p_intf = (intf_thread_t *)user_data;
    input_thread_t *p_input = p_intf->p_sys->p_input;
    vout_thread_t *p_vout = p_input ? input_GetVout( p_input ) : NULL;
    (void)menuitem;

    if( !p_vout )
    {
        hildon_banner_show_information(
                                GTK_WIDGET( p_intf->p_sys->p_main_window ),
                                "gtk-dialog-error", "There is no video" );
        return;
    }

    var_TriggerCallback( p_vout, "video-snapshot" );
    hildon_banner_show_information( GTK_WIDGET( p_intf->p_sys->p_main_window ),
                                    NULL, "Snapshot taken" );
}

void dropframe_cb( GtkMenuItem *menuitem, gpointer user_data )
{
    intf_thread_t *p_intf = (intf_thread_t *)user_data;

    if( gtk_check_menu_item_get_active( GTK_CHECK_MENU_ITEM( menuitem ) ) )
        config_PutInt( p_intf, "ffmpeg-skip-frame", 1 );
    else
        config_PutInt( p_intf, "ffmpeg-skip-frame", 0 );
}

static int keyModifiersToVLC( GdkEventKey *event )
{
    int i_keyModifiers = 0;
    if( event->state & GDK_SHIFT_MASK ) i_keyModifiers |= KEY_MODIFIER_SHIFT;
    if( event->state & GDK_MOD1_MASK ) i_keyModifiers |= KEY_MODIFIER_ALT;
    if( event->state & GDK_CONTROL_MASK ) i_keyModifiers |= KEY_MODIFIER_CTRL;
    if( event->state & GDK_META_MASK ) i_keyModifiers |= KEY_MODIFIER_META;
    return i_keyModifiers;
}

static int eventToVLCKey( GdkEventKey *event )
{
    int i_vlck = 0;

    switch( event->keyval )
    {
    case GDK_Left: i_vlck |= KEY_LEFT; break;
    case GDK_Right: i_vlck |= KEY_RIGHT; break;
    case GDK_Up: i_vlck |= KEY_UP; break;
    case GDK_Down: i_vlck |= KEY_DOWN; break;
    case GDK_Escape: i_vlck |= KEY_ESC; break;
    case GDK_Return: i_vlck |= KEY_ENTER; break;

    case GDK_F1: i_vlck |= KEY_F1; break;
    case GDK_F2: i_vlck |= KEY_F2; break;
    case GDK_F3: i_vlck |= KEY_F3; break;
    case GDK_F4: i_vlck |= KEY_F4; break;
    case GDK_F5: i_vlck |= KEY_F5; break;
    case GDK_F6: i_vlck |= KEY_F6; break;
    case GDK_F7: i_vlck |= KEY_F7; break;
    case GDK_F8: i_vlck |= KEY_F8; break;
    case GDK_F9: i_vlck |= KEY_F9; break;
    case GDK_F10: i_vlck |= KEY_F10; break;
    case GDK_F11: i_vlck |= KEY_F11; break;
    case GDK_F12: i_vlck |= KEY_F12; break;

    case GDK_Page_Up: i_vlck |= KEY_PAGEUP; break;
    case GDK_Page_Down: i_vlck |= KEY_PAGEDOWN; break;
    case GDK_Home: i_vlck |= KEY_HOME; break;
    case GDK_End: i_vlck |= KEY_END; break;
    case GDK_Insert: i_vlck |= KEY_INSERT; break;
    case GDK_Delete: i_vlck |= KEY_DELETE; break;

#ifndef HAVE_MAEMO
    case GDK_AudioLowerVolume: i_vlck |= KEY_VOLUME_DOWN; break;
    case GDK_AudioRaiseVolume: i_vlck |= KEY_VOLUME_UP; break;
    case GDK_AudioMute: i_vlck |= KEY_VOLUME_MUTE; break;
    case GDK_AudioPlay: i_vlck |= KEY_MEDIA_PLAY_PAUSE; break;
    case GDK_AudioStop: i_vlck |= KEY_MEDIA_STOP; break;
    case GDK_AudioNext: i_vlck |= KEY_MEDIA_NEXT_TRACK; break;
    case GDK_AudioPrev: i_vlck |= KEY_MEDIA_PREV_TRACK; break;
#endif
    }

    if( !i_vlck )
    {
        /* Force lowercase */
        if( event->keyval >= GDK_A && event->keyval <= GDK_Z )
            i_vlck = event->keyval + 32;
        /* Rest of the ascii range */
        else if( event->keyval >= GDK_space && event->keyval <= GDK_asciitilde )
            i_vlck = event->keyval;
    }

    /* Handle modifiers */
    i_vlck |= keyModifiersToVLC( event );

    return i_vlck;
}

gboolean key_cb(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
    intf_thread_t *p_intf = (intf_thread_t *)user_data;
    widget = widget; /* unused */

    int i_vlck = eventToVLCKey( event );
    if( i_vlck > 0 )
    {
        var_SetInteger( p_intf->p_libvlc, "key-pressed", i_vlck );
        return TRUE;
    }

    return FALSE;
}

gboolean fullscreen_cb( gpointer user_data )
{
    intf_thread_t *p_intf = (intf_thread_t *)user_data;

    if(p_intf->p_sys->b_fullscreen)
    {
        gtk_widget_hide_all( GTK_WIDGET( p_intf->p_sys->p_control_window ) );
        gtk_window_fullscreen( GTK_WINDOW(p_intf->p_sys->p_main_window) );
    }
    else
    {
        gtk_window_unfullscreen( GTK_WINDOW(p_intf->p_sys->p_main_window) );
        gtk_widget_show_all( GTK_WIDGET( p_intf->p_sys->p_control_window ) );
    }
    return FALSE;
}
