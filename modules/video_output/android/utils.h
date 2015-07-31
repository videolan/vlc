/*****************************************************************************
 * utils.h: shared code between Android vout modules.
 *****************************************************************************
 * Copyright (C) 2014-2015 VLC authors and VideoLAN
 *
 * Authors: Felix Abecassis <felix.abecassis@gmail.com>
 *          Thomas Guillem <thomas@gllm.fr>
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

#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/input.h>

#include <vlc_vout_display.h>
#include <vlc_common.h>

typedef struct AWindowHandler AWindowHandler;

enum AWindow_ID {
    AWindow_Video,
    AWindow_Subtitles,
    AWindow_Max,
};

/**
 * native_window_api_t. See android/native_window.h in NDK
 */

typedef int32_t (*ptr_ANativeWindow_lock)(ANativeWindow*, ANativeWindow_Buffer*, ARect*);
typedef void (*ptr_ANativeWindow_unlockAndPost)(ANativeWindow*);
typedef int32_t (*ptr_ANativeWindow_setBuffersGeometry)(ANativeWindow*, int32_t, int32_t, int32_t);

typedef struct
{
    ptr_ANativeWindow_lock winLock;
    ptr_ANativeWindow_unlockAndPost unlockAndPost;
    ptr_ANativeWindow_setBuffersGeometry setBuffersGeometry; /* can be NULL */
} native_window_api_t;

/**
 * native_window_priv_api_t. See system/core/include/system/window.h in AOSP.
 */

typedef struct native_window_priv native_window_priv;
typedef native_window_priv *(*ptr_ANativeWindowPriv_connect) (ANativeWindow *);
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

/**
 * This function returns a JNIEnv* created from the android JavaVM attached to
 * the VLC object var. it doesn't need to be released.
 */
JNIEnv *android_getEnv(vlc_object_t *p_obj, const char *psz_thread_name);

/**
 * This function return a new AWindowHandler created from a
 * IAWindowNativeHandler jobject attached to the VLC object var. It must be
 * released with AWindowHandler_destroy.
 */
AWindowHandler *AWindowHandler_new(vlc_object_t *p_obj);
void AWindowHandler_destroy(AWindowHandler *p_awh);

/**
 * This functions returns a native_window_api_t that can be used to access the
 * public ANativeWindow API. It can't be NULL and shouldn't be released
 */
native_window_api_t *AWindowHandler_getANativeWindowAPI(AWindowHandler *p_awh);

/**
 * This function returns a native_window_priv_api_t that can be used to access
 * the private ANativeWindow API. It can be NULL and shouldn't be released
 */
native_window_priv_api_t *AWindowHandler_getANativeWindowPrivAPI(AWindowHandler *p_awh);

/**
 * This function retrieves the mouse coordinates sent by the Android
 * MediaPlayer. It returns true if the coordinates are valid.
 */
bool AWindowHandler_getMouseCoordinates(AWindowHandler *p_awh,
                                        int *p_action, int *p_button,
                                        int *p_x, int *p_y);

/**
 * This function retrieves the window size sent by the Android MediaPlayer.  It
 * returns true if the size is valid.
 */
bool AWindowHandler_getWindowSize(AWindowHandler *p_awh,
                                  int *p_width, int *p_height);

/**
 * This function returns the Video or the Subtitles Android Surface attached to
 * the MediaPlayer. It can be released with AWindowHandler_releaseSurface or by
 * AWindowHandler_destroy.
 */
jobject AWindowHandler_getSurface(AWindowHandler *p_awh, enum AWindow_ID id);
void AWindowHandler_releaseSurface(AWindowHandler *p_awh, enum AWindow_ID id);

/**
 * This function returns the Video or the Subtitles ANativeWindow attached to
 * the Android Surface. It can be released with
 * AWindowHandler_releaseANativeWindow, AWindowHandler_releaseSurface or by
 * AWindowHandler_destroy.
 */
ANativeWindow *AWindowHandler_getANativeWindow(AWindowHandler *p_awh,
                                               enum AWindow_ID id);
void AWindowHandler_releaseANativeWindow(AWindowHandler *p_awh,
                                         enum AWindow_ID id);
/**
 * This function is a fix up of ANativeWindow_setBuffersGeometry that doesn't
 * work before Android ICS. It configures the Surface from the Android
 * MainThread via a SurfaceHolder. It returns VLC_SUCCESS if the Surface was
 * configured (it returns VLC_EGENERIC after Android ICS).
 */
int AWindowHandler_setBuffersGeometry(AWindowHandler *p_awh, enum AWindow_ID id,
                                      int i_width, int i_height, int i_format);

/**
 * This function set the window layout.
 */
int AWindowHandler_setWindowLayout(AWindowHandler *p_awh,
                                   int i_width, int i_height,
                                   int i_visible_width, int i_visible_height,
                                   int i_sar_num, int i_sar_den);
