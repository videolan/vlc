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

#define PRIV_WINDOW_FORMAT_YV12 0x32315659

static inline int ChromaToAndroidHal(vlc_fourcc_t i_chroma)
{
    switch (i_chroma) {
        case VLC_CODEC_YV12:
        case VLC_CODEC_I420:
            return PRIV_WINDOW_FORMAT_YV12;
        case VLC_CODEC_RGB16:
            return WINDOW_FORMAT_RGB_565;
        case VLC_CODEC_RGB32:
            return WINDOW_FORMAT_RGBX_8888;
        case VLC_CODEC_RGBA:
            return WINDOW_FORMAT_RGBA_8888;
        default:
            return -1;
    }
}

typedef struct native_window_priv native_window_priv;
typedef native_window_priv *(*ptr_ANativeWindowPriv_connect) (void *);
typedef int (*ptr_ANativeWindowPriv_disconnect) (native_window_priv *);
typedef int (*ptr_ANativeWindowPriv_setUsage) (native_window_priv *, bool, int );
typedef int (*ptr_ANativeWindowPriv_setBuffersGeometry) (native_window_priv *, int, int, int );
typedef int (*ptr_ANativeWindowPriv_getMinUndequeued) (native_window_priv *, unsigned int *);
typedef int (*ptr_ANativeWindowPriv_getMaxBufferCount) (native_window_priv *, unsigned int *);
typedef int (*ptr_ANativeWindowPriv_setBufferCount) (native_window_priv *, unsigned int );
typedef int (*ptr_ANativeWindowPriv_setCrop) (native_window_priv *, int, int, int, int);
typedef int (*ptr_ANativeWindowPriv_dequeue) (native_window_priv *, void **);
typedef int (*ptr_ANativeWindowPriv_lock) (native_window_priv *, void *);
typedef int (*ptr_ANativeWindowPriv_queue) (native_window_priv *, void *);
typedef int (*ptr_ANativeWindowPriv_cancel) (native_window_priv *, void *);
typedef int (*ptr_ANativeWindowPriv_lockData) (native_window_priv *, void **, ANativeWindow_Buffer *);
typedef int (*ptr_ANativeWindowPriv_unlockData) (native_window_priv *, void *, bool b_render);
typedef int (*ptr_ANativeWindowPriv_setOrientation) (native_window_priv *, int);

typedef struct
{
    ptr_ANativeWindowPriv_connect connect;
    ptr_ANativeWindowPriv_disconnect disconnect;
    ptr_ANativeWindowPriv_setUsage setUsage;
    ptr_ANativeWindowPriv_setBuffersGeometry setBuffersGeometry;
    ptr_ANativeWindowPriv_getMinUndequeued getMinUndequeued;
    ptr_ANativeWindowPriv_getMaxBufferCount getMaxBufferCount;
    ptr_ANativeWindowPriv_setBufferCount setBufferCount;
    ptr_ANativeWindowPriv_setCrop setCrop;
    ptr_ANativeWindowPriv_dequeue dequeue;
    ptr_ANativeWindowPriv_lock lock;
    ptr_ANativeWindowPriv_lockData lockData;
    ptr_ANativeWindowPriv_unlockData unlockData;
    ptr_ANativeWindowPriv_queue queue;
    ptr_ANativeWindowPriv_cancel cancel;
    ptr_ANativeWindowPriv_setOrientation setOrientation;
} native_window_priv_api_t;

/* Fill the structure passed as parameter and return 0 if all symbols are
   found. Don't need to call dlclose, the lib is already loaded. */
int LoadNativeWindowPrivAPI(native_window_priv_api_t *native);
