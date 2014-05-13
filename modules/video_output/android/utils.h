/*****************************************************************************
 * utils.h: shared code between Android vout modules.
 *****************************************************************************
 * Copyright (C) 2014 VLC authors and VideoLAN
 *
 * Authors: Felix Abecassis <felix.abecassis@gmail.com>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <android/native_window.h>
#include <jni.h>
#include <android/native_window_jni.h>
#include <android/input.h>

#include <vlc_vout_display.h>

typedef ANativeWindow* (*ptr_ANativeWindow_fromSurface)(JNIEnv*, jobject);
typedef void (*ptr_ANativeWindow_release)(ANativeWindow*);
typedef int32_t (*ptr_ANativeWindow_lock)(ANativeWindow*, ANativeWindow_Buffer*, ARect*);
typedef void (*ptr_ANativeWindow_unlockAndPost)(ANativeWindow*);

typedef struct
{
    ptr_ANativeWindow_fromSurface winFromSurface;
    ptr_ANativeWindow_release winRelease;
    ptr_ANativeWindow_lock winLock;
    ptr_ANativeWindow_unlockAndPost unlockAndPost;
} native_window_api_t;

/* Fill the structure passed as parameter and return a library handle
   that should be destroyed with dlclose. */
void *LoadNativeWindowAPI(native_window_api_t *native);
void Manage(vout_display_t *);
