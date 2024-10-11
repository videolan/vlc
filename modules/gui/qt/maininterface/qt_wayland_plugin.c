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

#include "compositor_wayland_module.h"

#ifdef QT_HAS_WAYLAND_PROTOCOLS
#include "util/csdmenu_wayland.h"
#endif

vlc_module_begin()
    set_shortname(N_("QtWayland"))
    set_description(N_(" calls for compositing with Qt"))
    set_capability("qtwayland", 10)
    set_callback(OpenCompositor)
#ifdef QT_HAS_WAYLAND_PROTOCOLS
add_submodule()
    set_capability("qtcsdmenu", 10)
    set_callbacks(WaylandCSDMenuOpen, WaylandCSDMenuClose)
#endif
vlc_module_end()
