/*****************************************************************************
 * gtk_control.c : functions to handle stream control buttons.
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: gtk_control.c,v 1.5 2001/07/25 03:12:33 sam Exp $
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
#include "defs.h"
#include <sys/types.h>                                              /* off_t */
#include <stdlib.h>

#define gtk 12
#define gnome 42
#if ( MODULE_NAME == gtk )
#   include <gtk/gtk.h>
#elif ( MODULE_NAME == gnome )
#   include <gnome.h>
#endif
#undef gtk
#undef gnome

#include <string.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "stream_control.h"
#include "input_ext-intf.h"

#include "interface.h"
#include "intf_playlist.h"
#include "intf_msg.h"

#include "gtk_callbacks.h"
#include "gtk_interface.h"
#include "gtk_support.h"
#include "gtk_playlist.h"
#include "intf_gtk.h"

#include "main.h"

#include "modules_export.h"

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

    if( p_intf->p_input != NULL )
    {
        /* end playing item */
        p_intf->p_input->b_eof = 1;

        /* update playlist */
        vlc_mutex_lock( &p_main->p_playlist->change_lock );

        p_main->p_playlist->i_index--;
        p_main->p_playlist->b_stopped = 1;

        vlc_mutex_unlock( &p_main->p_playlist->change_lock );

    }

    return TRUE;
}


gboolean GtkControlPlay( GtkWidget       *widget,
                         GdkEventButton  *event,
                         gpointer         user_data )
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), (char*)user_data );

    if( p_intf->p_input != NULL )
    {
        input_SetStatus( p_intf->p_input, INPUT_STATUS_PLAY );
        p_main->p_playlist->b_stopped = 0;
    }
    else
    {
        vlc_mutex_lock( &p_main->p_playlist->change_lock );

        if( p_main->p_playlist->b_stopped )
        {
            if( p_main->p_playlist->i_size )
            {
                vlc_mutex_unlock( &p_main->p_playlist->change_lock );
                intf_PlaylistJumpto( p_main->p_playlist,
                                     p_main->p_playlist->i_index );
            }
            else
            {
                vlc_mutex_unlock( &p_main->p_playlist->change_lock );
                GtkFileOpenShow( widget, event, user_data );
            }
        }
        else
        {

            vlc_mutex_unlock( &p_main->p_playlist->change_lock );
        }

    }

    return TRUE;
}


gboolean GtkControlPause( GtkWidget       *widget,
                          GdkEventButton  *event,
                          gpointer         user_data )
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), (char*)user_data );

    if( p_intf->p_input != NULL )
    {
        input_SetStatus( p_intf->p_input, INPUT_STATUS_PAUSE );

        vlc_mutex_lock( &p_main->p_playlist->change_lock );
        p_main->p_playlist->b_stopped = 0;
        vlc_mutex_unlock( &p_main->p_playlist->change_lock );
    }

    return TRUE;
}


gboolean GtkControlSlow( GtkWidget       *widget,
                         GdkEventButton  *event,
                         gpointer         user_data )
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), (char*)user_data );

    if( p_intf->p_input != NULL )
    {
        input_SetStatus( p_intf->p_input, INPUT_STATUS_SLOWER );

        vlc_mutex_lock( &p_main->p_playlist->change_lock );
        p_main->p_playlist->b_stopped = 0;
        vlc_mutex_unlock( &p_main->p_playlist->change_lock );
    }

    return TRUE;
}


gboolean GtkControlFast( GtkWidget       *widget,
                         GdkEventButton  *event,
                         gpointer         user_data )
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), (char*)user_data );

    if( p_intf->p_input != NULL )
    {
        input_SetStatus( p_intf->p_input, INPUT_STATUS_FASTER );

        vlc_mutex_lock( &p_main->p_playlist->change_lock );
        p_main->p_playlist->b_stopped = 0;
        vlc_mutex_unlock( &p_main->p_playlist->change_lock );
    }

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


