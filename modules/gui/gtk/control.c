/*****************************************************************************
 * control.c : functions to handle stream control buttons.
 *****************************************************************************
 * Copyright (C) 2000, 2001 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Stéphane Borel <stef@via.ecp.fr>
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
#include <sys/types.h>                                              /* off_t */
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/intf.h>

#ifdef MODULE_NAME_IS_gnome
#   include <gnome.h>
#else
#   include <gtk/gtk.h>
#endif

#include <string.h>

#include "gtk_callbacks.h"
#include "gtk_interface.h"
#include "gtk_support.h"

#include "playlist.h"
#include "common.h"

/****************************************************************************
 * Control functions: this is where the functions are defined
 ****************************************************************************
 * These functions are button-items callbacks, and are used
 * by other callbacks
 ****************************************************************************/
gboolean GtkControlBack( GtkWidget       *widget,
                         gpointer         user_data )
{
    return FALSE;
}


gboolean GtkControlStop( GtkWidget       *widget,
                         gpointer         user_data )
{
    intf_thread_t *  p_intf = GtkGetIntf( widget );
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        return FALSE;
    }

    playlist_Stop( p_playlist );
    vlc_object_release( p_playlist );

    return TRUE;
}


gboolean GtkControlPlay( GtkWidget       *widget,
                         gpointer         user_data )
{
    intf_thread_t *  p_intf = GtkGetIntf( widget );
    playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                                       FIND_ANYWHERE );
    if( p_playlist == NULL )
    {
        GtkFileOpenShow( widget, user_data );
        return TRUE;
    }

    /* If the playlist is empty, open a file requester instead */
    vlc_mutex_lock( &p_playlist->object_lock );
    if( p_playlist->i_size )
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        playlist_Play( p_playlist );
        vlc_object_release( p_playlist );
    }
    else
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        vlc_object_release( p_playlist );
        GtkFileOpenShow( widget, user_data );
    }

    return TRUE;
}


gboolean GtkControlPause( GtkWidget       *widget,
                          gpointer         user_data )
{
    intf_thread_t *  p_intf = GtkGetIntf( widget );

    if( p_intf->p_sys->p_input == NULL )
    {
        return FALSE;
    }

    var_SetInteger( p_intf->p_sys->p_input, "state", PAUSE_S );

    return TRUE;
}


gboolean GtkControlSlow( GtkWidget       *widget,
                         gpointer         user_data )
{
    intf_thread_t *  p_intf = GtkGetIntf( widget );

    if( p_intf->p_sys->p_input == NULL )
    {
        return FALSE;
    }

    var_SetVoid( p_intf->p_sys->p_input, "rate-slower" );

    return TRUE;
}


gboolean GtkControlFast( GtkWidget       *widget,
                         gpointer         user_data )
{
    intf_thread_t *  p_intf = GtkGetIntf( widget );

    if( p_intf->p_sys->p_input == NULL )
    {
        return FALSE;
    }

    var_SetVoid( p_intf->p_sys->p_input, "rate-faster" );

    return TRUE;
}

