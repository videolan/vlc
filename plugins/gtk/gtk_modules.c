/*****************************************************************************
 * gtk_modules.c : functions to build modules configuration boxes.
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: gtk_modules.c,v 1.7 2001/12/30 07:09:55 sam Exp $
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

#include <videolan/vlc.h>

#ifdef MODULE_NAME_IS_gnome
#   include <gnome.h>
#else
#   include <gtk/gtk.h>
#endif

#include <string.h>

#include "stream_control.h"
#include "input_ext-intf.h"

#include "interface.h"
#include "intf_playlist.h"

#include "gtk_callbacks.h"
#include "gtk_interface.h"
#include "gtk_support.h"
#include "gtk_playlist.h"
#include "gtk_common.h"

gboolean GtkModulesShow( GtkWidget       *widget,
                         GdkEventButton  *event,
                         gpointer         user_data )
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(widget), "intf_window" );

    if( !GTK_IS_WIDGET( p_intf->p_sys->p_modules ) )
    {
//        p_intf->p_sys->p_modules = create_intf_modules();
        gtk_object_set_data( GTK_OBJECT( p_intf->p_sys->p_modules ),
                             "p_intf", p_intf );
    }
    gtk_widget_show( p_intf->p_sys->p_modules );
    gdk_window_raise( p_intf->p_sys->p_modules->window );

    return FALSE;
}

void GtkModulesCancel( GtkButton * button, gpointer user_data )
{
    intf_thread_t *p_intf = GetIntf( GTK_WIDGET(button), "intf_modules" );

    gtk_widget_hide( p_intf->p_sys->p_modules );
}

/****************************************************************************
 * Callbacks for menuitems
 ****************************************************************************/
void GtkModulesActivate( GtkMenuItem * menuitem, gpointer user_data )
{
    GtkModulesShow( GTK_WIDGET( menuitem ), NULL, user_data );
}
