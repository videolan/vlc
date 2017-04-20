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
    AWindow_SurfaceTexture,
    AWindow_Max,
};

/**
 * native_window_api_t. See android/native_window.h in NDK
 */
typedef struct
{
    int32_t (*winLock)(ANativeWindow*, ANativeWindow_Buffer*, ARect*);
    void (*unlockAndPost)(ANativeWindow*);
    int32_t (*setBuffersGeometry)(ANativeWindow*, int32_t, int32_t, int32_t); /* can be NULL */
} native_window_api_t;

/**
 * native_window_priv_api_t. See system/core/include/system/window.h in AOSP.
 */
typedef struct native_window_priv native_window_priv;
typedef struct
{
    native_window_priv *(*connect)(ANativeWindow *);
    int (*disconnect) (native_window_priv *);
    int (*setUsage) (native_window_priv *, bool, int );
    int (*setBuffersGeometry) (native_window_priv *, int, int, int );
    int (*getMinUndequeued) (native_window_priv *, unsigned int *);
    int (*getMaxBufferCount) (native_window_priv *, unsigned int *);
    int (*setBufferCount) (native_window_priv *, unsigned int );
    int (*setCrop) (native_window_priv *, int, int, int, int);
    int (*dequeue) (native_window_priv *, void **);
    int (*lock) (native_window_priv *, void *);
    int (*queue) (native_window_priv *, void *);
    int (*cancel) (native_window_priv *, void *);
    int (*lockData) (native_window_priv *, void **, ANativeWindow_Buffer *);
    int (*unlockData) (native_window_priv *, void *, bool b_render);
    int (*setOrientation) (native_window_priv *, int);
} native_window_priv_api_t;

struct awh_mouse_coords
{
    int i_action;
    int i_button;
    int i_x;
    int i_y;
};

typedef struct
{
    void (*on_new_window_size)(vout_window_t *wnd, unsigned i_width,
                               unsigned i_height);
    void (*on_new_mouse_coords)(vout_window_t *wnd,
                                const struct awh_mouse_coords *coords);
} awh_events_t;

/**
 * Load a private native window API
 *
 * This can be used to access the private ANativeWindow API.
 * \param api doesn't need to be released
 * \return 0 on success, -1 on error.
 */
int android_loadNativeWindowPrivApi(native_window_priv_api_t *api);

/**
 * Attach or get a JNIEnv*
 *
 * The returned JNIEnv* is created from the android JavaVM attached to the VLC
 * object var.
 * \return a valid JNIEnv * or NULL. It doesn't need to be released.
 */
JNIEnv *android_getEnv(vlc_object_t *p_obj, const char *psz_thread_name);

/**
 * Create new AWindowHandler
 *
 * This handle allow to access IAWindowNativeHandler jobject attached to the
 * VLC object var.
 * \return a valid AWindowHandler * or NULL. It must be released with
 * AWindowHandler_destroy.
 */
AWindowHandler *AWindowHandler_new(vout_window_t *wnd, awh_events_t *p_events);
void AWindowHandler_destroy(AWindowHandler *p_awh);

/**
 * Get the public native window API
 *
 * Used to access the public ANativeWindow API.
 * \return a valid native_window_api_t. It doesn't need to be released.
 */
native_window_api_t *AWindowHandler_getANativeWindowAPI(AWindowHandler *p_awh);

/**
 * Get the Video or the Subtitles Android Surface
 *
 * \return the surface in a jobject, or NULL. It should be released with
 * AWindowHandler_releaseANativeWindow() or AWindowHandler_destroy().
 */
jobject AWindowHandler_getSurface(AWindowHandler *p_awh, enum AWindow_ID id);

/**
 * Get the Video or the Subtitles ANativeWindow
 *
 * \return a valid ANativeWindow or NULL.It should be released with
 * AWindowHandler_releaseANativeWindow() or AWindowHandler_destroy.()
 */
ANativeWindow *AWindowHandler_getANativeWindow(AWindowHandler *p_awh,
                                               enum AWindow_ID id);

/**
 * Release the Video/Subtitles Surface/ANativeWindow
 */
void AWindowHandler_releaseANativeWindow(AWindowHandler *p_awh,
                                         enum AWindow_ID id);
/**
 * Pre-ICS hack of ANativeWindow_setBuffersGeometry
 *
 * This function is a fix up of ANativeWindow_setBuffersGeometry that doesn't
 * work before Android ICS. It configures the Surface from the Android
 * MainThread via a SurfaceHolder. It returns VLC_SUCCESS if the Surface was
 * configured (it returns VLC_EGENERIC after Android ICS).
 */
int AWindowHandler_setBuffersGeometry(AWindowHandler *p_awh, enum AWindow_ID id,
                                      int i_width, int i_height, int i_format);

/**
 * Returns true if the video layout can be changed
 */
bool AWindowHandler_canSetVideoLayout(AWindowHandler *p_awh);

/**
 * Set the video layout
 *
 * Should be called only if AWindowHandler_canSetVideoLayout() returned true
 */
int AWindowHandler_setVideoLayout(AWindowHandler *p_awh,
                                  int i_width, int i_height,
                                  int i_visible_width, int i_visible_height,
                                  int i_sar_num, int i_sar_den);

/**
 * Attach a SurfaceTexture to the OpenGL ES context that is current on the
 * calling thread.
 *
 * See SurfaceTexture Android documentation.
 * \return 0 on success, -1 on error.
 */
int
SurfaceTexture_attachToGLContext(AWindowHandler *p_awh, int tex_name);

/**
 * Detach a SurfaceTexture from the OpenGL ES context that owns the OpenGL ES
 * texture object.
 */
void
SurfaceTexture_detachFromGLContext(AWindowHandler *p_awh);

/**
 * Get a Java Surface from the attached SurfaceTexture
 *
 * This object can be used with mediacodec_jni.
 */
static inline jobject
SurfaceTexture_getSurface(AWindowHandler *p_awh)
{
    return AWindowHandler_getSurface(p_awh, AWindow_SurfaceTexture);
}

/**
 * Get a ANativeWindow from the attached SurfaceTexture
 *
 * This pointer can be used with mediacodec_ndk.
 */
static inline ANativeWindow *
SurfaceTexture_getANativeWindow(AWindowHandler *p_awh)
{
    return AWindowHandler_getANativeWindow(p_awh, AWindow_SurfaceTexture);
}

/**
 * Wait for a new frame and update it
 *
 * This function must be called from the OpenGL thread. This is an helper that
 * waits for a new frame via the Java SurfaceTexture.OnFrameAvailableListener
 * listener and update the frame via the SurfaceTexture.updateTexImage()
 * method.
 *
 * \param pp_transform_mtx the transform matrix fetched from
 * SurfaceTexture.getTransformMatrix() after the
 * SurfaceTexture.updateTexImage() call
 * \return VLC_SUCCESS or a VLC error
 */
int
SurfaceTexture_waitAndUpdateTexImage(AWindowHandler *p_awh,
                                     const float **pp_transform_mtx);
