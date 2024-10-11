/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include "csdmenu_x11.h"
#include <cassert>
#include "maininterface/compositor_x11_utils.hpp"

typedef struct {
    xcb_connection_t* connection;
    xcb_atom_t gtkShowWindowMenuAtom;
    xcb_window_t rootWindow;
} csd_menu_priv_t;

extern "C" {

static bool CSDMenuPopup(qt_csd_menu_t* p_this, qt_csd_menu_event* event)
{
    csd_menu_priv_t* sys = static_cast<csd_menu_priv_t*>(p_this->p_sys);

    assert(event->platform == QT_CSD_PLATFORM_X11);

    xcb_client_message_event_t x11event;
    x11event.response_type = XCB_CLIENT_MESSAGE;
    x11event.type = sys->gtkShowWindowMenuAtom;
    x11event.window = event->data.x11.window;
    x11event.sequence = 0;
    x11event.format = 32;
    x11event.data.data32[0] = 0; //device id ignored
    x11event.data.data32[1] = event->x;
    x11event.data.data32[2] = event->y;
    x11event.data.data32[3] = 0;
    x11event.data.data32[4] = 0;

    // ungrab pointer to allow the window manager to grab them
    xcb_ungrab_pointer(sys->connection, XCB_CURRENT_TIME);
    xcb_send_event(sys->connection, false, sys->rootWindow, XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY, reinterpret_cast<const char*>(&x11event));
    return true;
}

int X11CSDMenuOpen(qt_csd_menu_t* p_this, qt_csd_menu_info* info)
{
    if (info->platform != QT_CSD_PLATFORM_X11) {
        return VLC_EGENERIC;
    }

    if (info->data.x11.connection == NULL || info->data.x11.rootwindow == 0) {
        msg_Warn(p_this, "X11 connection or root window missing");
        return VLC_EGENERIC;
    }

    csd_menu_priv_t* sys = (csd_menu_priv_t*)vlc_obj_calloc(p_this, 1, sizeof(csd_menu_priv_t));
    if (!sys)
        return VLC_ENOMEM;

    sys->connection = info->data.x11.connection;
    sys->rootWindow = info->data.x11.rootwindow;

    sys->gtkShowWindowMenuAtom = vlc::getInternAtom(sys->connection, "_GTK_SHOW_WINDOW_MENU");

    if (sys->gtkShowWindowMenuAtom == 0)
        goto error;

    if (!vlc::wmNetSupport(sys->connection, info->data.x11.rootwindow, sys->gtkShowWindowMenuAtom))
        goto error;


    //module functions
    p_this->popup = CSDMenuPopup;
    p_this->p_sys = sys;

    return VLC_SUCCESS;

error:
    p_this->p_sys = NULL;
    vlc_obj_free(p_this, sys);
    return VLC_EGENERIC;
}

}
