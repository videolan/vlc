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
typedef struct ASurfaceTexture ASurfaceTexture;

enum AWindow_ID {
    AWindow_Video,
    AWindow_Subtitles,
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

typedef struct android_video_context_t android_video_context_t;

struct android_video_context_t
{
    struct vlc_asurfacetexture *texture;
    void *dec_opaque;
    bool (*render)(struct picture_context_t *ctx);
    bool (*render_ts)(struct picture_context_t *ctx, vlc_tick_t ts);
};

struct vlc_asurfacetexture
{
    struct ANativeWindow *window;
    jobject              *jsurface;

    const struct vlc_asurfacetexture_operations *ops;
};

/**
 * Wrapper structure for Android SurfaceTexture object.
 *
 * It can use either the NDK API or JNI API.
 */
struct vlc_asurfacetexture_operations
{
    int (*attach_to_gl_context)(
            struct vlc_asurfacetexture *surface,
            uint32_t tex_name);

    void (*detach_from_gl_context)(
            struct vlc_asurfacetexture *surface);

    int (*update_tex_image)(
            struct vlc_asurfacetexture *surface,
            const float **pp_transform_mtx);

    void (*destroy)(
            struct vlc_asurfacetexture *surface);
};

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
SurfaceTexture_attachToGLContext(struct vlc_asurfacetexture *st, uint32_t tex_name);

/**
 * Detach a SurfaceTexture from the OpenGL ES context that owns the OpenGL ES
 * texture object.
 */
void
SurfaceTexture_detachFromGLContext(struct vlc_asurfacetexture *st);

/**
 * Create a new SurfaceTexture object.
 *
 * See Android SurfaceTexture
 */
struct vlc_asurfacetexture *
vlc_asurfacetexture_New(AWindowHandler *p_awh);

/**
 * Delete a SurfaceTexture object created with SurfaceTexture_New.
 */
static inline void
vlc_asurfacetexture_Delete(struct vlc_asurfacetexture *st)
{
    if (st->ops->destroy)
        st->ops->destroy(st);
}

/**
 * Update the SurfaceTexture to the most recent frame.
 *
 * \param pp_transform_mtx the transform matrix fetched from
 * SurfaceTexture.getTransformMatrix() after the
 * SurfaceTexture.updateTexImage() call
 * \return VLC_SUCCESS or a VLC error
 */
int
SurfaceTexture_updateTexImage(struct vlc_asurfacetexture *st, const float **pp_transform_mtx);
