/*****************************************************************************
 * gnome_callbacks.c : GNOME-specific callbacks.
 *****************************************************************************
 * Copyright (C) 2000, 2001, 2002 VideoLAN
 * $Id: gnome_callbacks.c,v 1.10 2002/06/01 12:31:59 sam Exp $
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
#include <vlc/vlc.h>
#include <vlc/intf.h>

#include "gnome_callbacks.h"
#include "gnome_interface.h"
#include "gnome_support.h"

#include <gnome.h>

/*
 * These wrappers are made necessary by a bug in glade that seems not
 * to put user_data in c source of menuitems.
 */

void
GnomeMenubarFileOpenActivate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkFileOpenShow( GTK_WIDGET( menuitem ), NULL, "intf_window" );
}


void
GnomeMenubarDiscOpenActivate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkDiscOpenShow( GTK_WIDGET( menuitem ), NULL, "intf_window" );
}


void
GnomeMenubarNetworkOpenActivate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkNetworkOpenShow( GTK_WIDGET( menuitem ), NULL, "intf_window" );
}

void
GnomeMenubarDiscEjectActivate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
     GtkDiscEject( GTK_WIDGET( menuitem ), NULL, "intf_window" );
}

void
GnomeMenubarExitActivate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkExit( GTK_WIDGET( menuitem ), NULL, "intf_window" );
}


void
GnomeMenubarWindowToggleActivate       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkWindowToggle( GTK_WIDGET( menuitem ), NULL, "intf_window" );
}


void
GnomeMenubarFullscreenActivate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkFullscreen( GTK_WIDGET( menuitem ), NULL, "intf_window" );
}


void
GnomeMenubarPlaylistActivate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkPlaylistShow( GTK_WIDGET( menuitem ), NULL, "intf_window" );
}


void
GnomeMenubarModulesActivate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkModulesShow( GTK_WIDGET( menuitem ), NULL, "intf_window" );
}


void
GnomeMenubarPreferencesActivate        (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkPreferencesActivate( menuitem, "intf_window" );
}


void
GnomeMenubarAboutActivate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkAboutShow( GTK_WIDGET( menuitem ), NULL, "intf_window" );
}


void
GnomePopupPlayActivate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkControlPlay( GTK_WIDGET( menuitem ), NULL, "intf_popup" );
}


void
GnomePopupPauseActivate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkControlPause( GTK_WIDGET( menuitem ), NULL, "intf_popup" );
}


void
GnomePopupStopActivate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkControlStop( GTK_WIDGET( menuitem ), NULL, "intf_popup" );
}


void
GnomePopupBackActivate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkControlBack( GTK_WIDGET( menuitem ), NULL, "intf_popup" );
}


void
GnomePopupSlowActivate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkControlSlow( GTK_WIDGET( menuitem ), NULL, "intf_popup" );
}


void
GnomePopupFastActivate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkControlFast( GTK_WIDGET( menuitem ), NULL, "intf_popup" );
}


void
GnomePopupWindowToggleActivate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkWindowToggle( GTK_WIDGET( menuitem ), NULL, "intf_popup" );
}


void
GnomePopupFullscreenActivate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkFullscreen( GTK_WIDGET( menuitem ), NULL, "intf_popup" );
}


void
GnomePopupNextActivate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkPlaylistNext( GTK_WIDGET( menuitem ), NULL, "intf_popup" );
}


void
GnomePopupPrevActivate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkPlaylistPrev( GTK_WIDGET( menuitem ), NULL, "intf_popup" );
}


void
GnomePopupFileOpenActivate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkFileOpenShow( GTK_WIDGET( menuitem ), NULL, "intf_popup" );
}


void
GnomePopupDiscOpenActivate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkDiscOpenShow( GTK_WIDGET( menuitem ), NULL, "intf_popup" );
}


void
GnomePopupNetworkOpenActivate          (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkNetworkOpenShow( GTK_WIDGET( menuitem ), NULL, "intf_popup" );
}


void
GnomePopupAboutActivate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkAboutShow( GTK_WIDGET( menuitem ), NULL, "intf_popup" );
}


void
GnomePopupPlaylistActivate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkPlaylistShow( GTK_WIDGET( menuitem ), NULL, "intf_popup" );
}


void
GnomePopupPreferencesActivate          (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkPreferencesActivate( menuitem, "intf_window" );
}


void
GnomePopupExitActivate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkExit( GTK_WIDGET( menuitem ), NULL, "intf_popup" );
}


void
GnomePlaylistDiscOpenActivate          (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkDiscOpenShow( GTK_WIDGET( menuitem ), NULL, "intf_playlist" );
}


void
GnomePlaylistFileOpenActivate          (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkFileOpenShow( GTK_WIDGET( menuitem ), NULL, "intf_playlist" );
}


void
GnomePlaylistNetworkOpenActivate       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkNetworkOpenShow( GTK_WIDGET( menuitem ), NULL, "intf_playlist" );
}


void
GnomePopupJumpActivate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkJumpShow( GTK_WIDGET( menuitem ), NULL, "intf_popup" );
}


void
GnomeMenubarMessagesActivate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
    GtkMessagesShow( GTK_WIDGET( menuitem ), NULL, "intf_window" );
}

