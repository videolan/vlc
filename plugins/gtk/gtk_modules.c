/*****************************************************************************
 * gtk_modules.c : functions to build modules configuration boxes.
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: gtk_modules.c,v 1.5 2001/11/28 15:08:05 massiot Exp $
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
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"

#include "stream_control.h"
#include "input_ext-intf.h"

#include "interface.h"
#include "intf_playlist.h"

#include "gtk_callbacks.h"
#include "gtk_interface.h"
#include "gtk_support.h"
#include "gtk_playlist.h"
#include "intf_gtk.h"

#include "main.h"

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
