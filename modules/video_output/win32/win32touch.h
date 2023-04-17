/*****************************************************************************
 * win32touch.c: touch gestures recognition
 *****************************************************************************
 * Copyright © 2013-2014 VideoLAN
 *
 * Authors: Ludovic Fauvet <etix@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_GESTURE_H_
#define VLC_GESTURE_H_

# include <windows.h>
# include <winuser.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#ifndef WM_GESTURE
# define WM_GESTURE 0x0119
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct win32_gesture_sys_t;

struct win32_gesture_sys_t *InitGestures( HWND hwnd, bool b_isProjected );

bool DecodeGesture( vlc_object_t *, struct win32_gesture_sys_t *, HGESTUREINFO );

void CloseGestures( struct win32_gesture_sys_t * );

#ifdef __cplusplus
}
#endif

#endif
