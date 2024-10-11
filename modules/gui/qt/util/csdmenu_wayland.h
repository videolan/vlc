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
#ifndef CSDMENU_WAYLAND_HPP
#define CSDMENU_WAYLAND_HPP

#include "csdmenu_module.h"

int WaylandCSDMenuOpen(qt_csd_menu_t* obj, qt_csd_menu_info* info);
void WaylandCSDMenuClose(vlc_object_t* obj);

#endif // CSDMENU_WAYLAND_HPP
