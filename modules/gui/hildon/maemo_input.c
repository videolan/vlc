/*****************************************************************************
* maemo_input.c : Input handling for the maemo plugin
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

#include <vlc_common.h>

#include "maemo.h"
#include "maemo_input.h"

static int input_event_cb( vlc_object_t *p_this, const char *psz_var,
                           vlc_value_t oldval, vlc_value_t newval, void *param );


gboolean process_events( gpointer data )
{
    intf_thread_t *p_intf = (intf_thread_t *)data;
    vlc_spin_lock( &p_intf->p_sys->event_lock );

    int i_event = p_intf->p_sys->i_event;
    p_intf->p_sys->i_event = 0;

    vlc_spin_unlock( &p_intf->p_sys->event_lock );
    if( i_event )
    {
        if( i_event & EVENT_PLAYLIST_CURRENT )
            item_changed_pl( p_intf );
        if( i_event & EVENT_ACTIVITY )
            item_changed_pl( p_intf );
        if( i_event & EVENT_ITEM_CHANGED )
            item_changed( p_intf );
        if( i_event & EVENT_INTF_CHANGED )
            update_position( p_intf );
    }

    return TRUE;
}

void set_input( intf_thread_t *p_intf, input_thread_t *p_input )
{
    if( p_input && !( p_input->b_die || p_input->b_dead ) )
    {
        p_intf->p_sys->p_input = p_input;
        vlc_object_hold( p_input );
        var_AddCallback( p_input, "intf-event", input_event_cb, p_intf );

        // "Activate" the seekbar
        gtk_widget_set_sensitive( GTK_WIDGET( p_intf->p_sys->p_seekbar ), TRUE );
    }
    else
        p_intf->p_sys->p_input = NULL;
}

void delete_input( intf_thread_t *p_intf )
{
    if( p_intf->p_sys->p_input )
    {
        var_DelCallback( p_intf->p_sys->p_input, "intf-event",
                         input_event_cb, p_intf );
        vlc_object_release( p_intf->p_sys->p_input );
        p_intf->p_sys->p_input = NULL;

        // Reset the seekbar
        hildon_seekbar_set_position( p_intf->p_sys->p_seekbar, 0 );
        gtk_widget_set_sensitive( GTK_WIDGET( p_intf->p_sys->p_seekbar ), FALSE );
    }
}

void item_changed_pl( intf_thread_t *p_intf )
{
    if( p_intf->p_sys->p_input &&
        ( p_intf->p_sys->p_input->b_dead || p_intf->p_sys->p_input->b_die ) )
    {
        delete_input( p_intf );
        return;
    }

    if( !p_intf->p_sys->p_input )
    {
        set_input( p_intf, playlist_CurrentInput( p_intf->p_sys->p_playlist ) );
    }
    return;
}

int playlist_current_cb( vlc_object_t *p_this, const char *psz_var,
                         vlc_value_t oldval, vlc_value_t newval, void *param )
{
    (void)p_this; (void)psz_var; (void)oldval; (void)newval;
    intf_thread_t *p_intf = (intf_thread_t *)param;
    vlc_spin_lock( &p_intf->p_sys->event_lock );

    p_intf->p_sys->i_event |= EVENT_PLAYLIST_CURRENT;

    vlc_spin_unlock( &p_intf->p_sys->event_lock );
    return VLC_SUCCESS;
}

int activity_cb( vlc_object_t *p_this, const char *psz_var,
                 vlc_value_t oldval, vlc_value_t newval, void *param )
{
    (void)p_this; (void)psz_var; (void)oldval; (void)newval;
    intf_thread_t *p_intf = (intf_thread_t *)param;
    vlc_spin_lock( &p_intf->p_sys->event_lock );

    p_intf->p_sys->i_event |= EVENT_ACTIVITY;

    vlc_spin_unlock( &p_intf->p_sys->event_lock );
    return VLC_SUCCESS;
}

void item_changed( intf_thread_t *p_intf )
{
    GtkButton *p_button = GTK_BUTTON( p_intf->p_sys->p_play_button );
    vlc_value_t state;

    if( !p_intf->p_sys->p_input )
        return;

    var_Get( p_intf->p_sys->p_input, "state", &state );

    // We change the "play" button
    if( state.i_int == PLAYING_S )
        gtk_button_set_image( p_button, gtk_image_new_from_stock( "vlc-pause",
                              GTK_ICON_SIZE_BUTTON ) );
    else
        gtk_button_set_image( p_button, gtk_image_new_from_stock( "vlc-play",
                              GTK_ICON_SIZE_BUTTON ) );
}

int item_changed_cb( vlc_object_t *p_this, const char *psz_var,
                     vlc_value_t oldval, vlc_value_t newval, void *param )
{
    (void)p_this; (void)psz_var; (void)oldval; (void)newval;
    intf_thread_t *p_intf = (intf_thread_t *)param;
    vlc_spin_lock( &p_intf->p_sys->event_lock );

    p_intf->p_sys->i_event |= EVENT_ITEM_CHANGED;

    vlc_spin_unlock( &p_intf->p_sys->event_lock );
    return VLC_SUCCESS;
}

void update_position( intf_thread_t *p_intf )
{
    if( p_intf->p_sys->p_input )
    {
        hildon_seekbar_set_total_time( p_intf->p_sys->p_seekbar,
                    var_GetTime( p_intf->p_sys->p_input, "length" )/1000000 );
        hildon_seekbar_set_position( p_intf->p_sys->p_seekbar,
                    var_GetTime( p_intf->p_sys->p_input, "time" )/1000000 );
    }
}

int interface_changed_cb( vlc_object_t *p_this, const char *psz_var,
                          vlc_value_t oldval, vlc_value_t newval, void *param )
{
    (void)p_this; (void)psz_var; (void)oldval; (void)newval;
    intf_thread_t *p_intf = (intf_thread_t *)param;
    vlc_spin_lock( &p_intf->p_sys->event_lock );

    p_intf->p_sys->i_event |= EVENT_INTF_CHANGED;

    vlc_spin_unlock( &p_intf->p_sys->event_lock );
    return VLC_SUCCESS;
}

static int input_event_cb( vlc_object_t *p_this, const char *psz_var,
                           vlc_value_t oldval, vlc_value_t newval, void *param )
{
    if( newval.i_int == INPUT_EVENT_STATE )
        return item_changed_cb( p_this, psz_var, oldval, newval, param );
    else
        return interface_changed_cb( p_this, psz_var, oldval, newval, param );
}

