/*****************************************************************************
 * gtk_control.c : functions to handle stream control buttons.
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: gtk_control.c,v 1.11 2002/06/02 09:03:54 sam Exp $
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
#include "gtk_playlist.h"
#include "gtk_common.h"

/****************************************************************************
 * Control functions: this is where the functions are defined
 ****************************************************************************
 * These functions are button-items callbacks, and are used
 * by other callbacks
 ****************************************************************************/
gboolean GtkControlBack( GtkWidget       *widget,
                         GdkEventButton  *event,
                         gpointer         user_data )
{
    return FALSE;
}


gboolean GtkControlStop( GtkWidget       *widget,
                         GdkEventButton  *event,
                         gpointer         user_data )
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), (char*)user_data );
    playlist_t *p_playlist;

    p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

    if( p_playlist )
    {
        playlist_Stop( p_playlist );
        vlc_object_release( p_playlist );
    }

    return TRUE;
}


gboolean GtkControlPlay( GtkWidget       *widget,
                         GdkEventButton  *event,
                         gpointer         user_data )
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), (char*)user_data );
    playlist_t *p_playlist;

    p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

    if( p_playlist )
    {
        playlist_Play( p_playlist );
        vlc_object_release( p_playlist );
    }

#if 0 /* FIXME: deal with this */
    {
        vlc_mutex_unlock( &p_intf->p_vlc->p_playlist->change_lock );
        GtkFileOpenShow( widget, event, user_data );
    }
#endif

    return TRUE;
}


gboolean GtkControlPause( GtkWidget       *widget,
                          GdkEventButton  *event,
                          gpointer         user_data )
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), (char*)user_data );
    playlist_t *p_playlist;

    p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

    if( p_playlist )
    {
        playlist_Pause( p_playlist );
        vlc_object_release( p_playlist );
    }

    return TRUE;
}


gboolean GtkControlSlow( GtkWidget       *widget,
                         GdkEventButton  *event,
                         gpointer         user_data )
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), (char*)user_data );
    playlist_t *p_playlist;

    p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

#if 0
    if( p_playlist )
    {
        playlist_Slow( p_playlist );
        vlc_object_release( p_playlist );
    }
#endif

    return TRUE;
}


gboolean GtkControlFast( GtkWidget       *widget,
                         GdkEventButton  *event,
                         gpointer         user_data )
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), (char*)user_data );
    playlist_t *p_playlist;

    p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

#if 0
    if( p_playlist )
    {
        playlist_Fast( p_playlist );
        vlc_object_release( p_playlist );
    }
#endif

    return TRUE;
}


/****************************************************************************
 * Control callbacks for menuitems
 ****************************************************************************
 * We have different callaback for menuitem since we must use the
 * activate signal toi popdown the menu automatically
 ****************************************************************************/
void GtkPlayActivate( GtkMenuItem * menuitem, gpointer user_data )
{
    GtkControlPlay( GTK_WIDGET( menuitem ), NULL, user_data );
}


void GtkPauseActivate( GtkMenuItem * menuitem, gpointer user_data )
{
    GtkControlPause( GTK_WIDGET( menuitem ), NULL, user_data );

}


void
GtKStopActivate                        (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkControlStop( GTK_WIDGET( menuitem ), NULL, user_data );

}


void
GtkBackActivate                        (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkControlBack( GTK_WIDGET( menuitem ), NULL, user_data );

}


void
GtkSlowActivate                        (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkControlSlow( GTK_WIDGET( menuitem ), NULL, user_data );

}


void
GtkFastActivate                        (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkControlFast( GTK_WIDGET( menuitem ), NULL, user_data );
}


