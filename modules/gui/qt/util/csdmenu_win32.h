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
#ifndef CSDMENU_WIN32_H
#define CSDMENU_WIN32_H

#include "csdmenu_module.h"

#ifdef __cplusplus
extern "C" {
#endif

int QtWin32CSDMenuOpen(qt_csd_menu_t* obj, qt_csd_menu_info* info);

#ifdef __cplusplus
}
#endif

#endif // CSDMENU_WIN32_H
