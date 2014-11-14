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
typedef int32_t (*ptr_ANativeWindow_setBuffersGeometry)(ANativeWindow*, int32_t, int32_t, int32_t);

typedef struct
{
    ptr_ANativeWindow_fromSurface winFromSurface;
    ptr_ANativeWindow_release winRelease;
    ptr_ANativeWindow_lock winLock;
    ptr_ANativeWindow_unlockAndPost unlockAndPost;
    ptr_ANativeWindow_setBuffersGeometry setBuffersGeometry;
} native_window_api_t;

/* Fill the structure passed as parameter and return a library handle
   that should be destroyed with dlclose. */
void *LoadNativeWindowAPI(native_window_api_t *native);
void Manage(vout_display_t *);

typedef int (*ptr_ANativeWindowPriv_connect) (void *);
typedef int (*ptr_ANativeWindowPriv_disconnect) (void *);
typedef int (*ptr_ANativeWindowPriv_setup) (void *, int, int, int, int );
typedef int (*ptr_ANativeWindowPriv_getMinUndequeued) (void *, unsigned int *);
typedef int (*ptr_ANativeWindowPriv_setBufferCount) (void *, unsigned int );
typedef int (*ptr_ANativeWindowPriv_setCrop) (void *, int, int, int, int);
typedef int (*ptr_ANativeWindowPriv_dequeue) (void *, void **);
typedef int (*ptr_ANativeWindowPriv_lock) (void *, void *);
typedef int (*ptr_ANativeWindowPriv_queue) (void *, void *);
typedef int (*ptr_ANativeWindowPriv_cancel) (void *, void *);
typedef int (*ptr_ANativeWindowPriv_setOrientation) (void *, int);

typedef struct
{
    ptr_ANativeWindowPriv_connect connect;
    ptr_ANativeWindowPriv_disconnect disconnect;
    ptr_ANativeWindowPriv_setup setup;
    ptr_ANativeWindowPriv_getMinUndequeued getMinUndequeued;
    ptr_ANativeWindowPriv_setBufferCount setBufferCount;
    ptr_ANativeWindowPriv_setCrop setCrop;
    ptr_ANativeWindowPriv_dequeue dequeue;
    ptr_ANativeWindowPriv_lock lock;
    ptr_ANativeWindowPriv_queue queue;
    ptr_ANativeWindowPriv_cancel cancel;
    ptr_ANativeWindowPriv_setOrientation setOrientation;
} native_window_priv_api_t;

/* Fill the structure passed as parameter and return 0 if all symbols are
   found. Don't need to call dlclose, the lib is already loaded. */
int LoadNativeWindowPrivAPI(native_window_priv_api_t *native);
