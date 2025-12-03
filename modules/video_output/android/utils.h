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

#include <unistd.h>
#include <assert.h>

#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/hardware_buffer.h>
#include <android/input.h>
#include <android/surface_control.h>
#include <android/hdr_metadata.h>
#include <android/data_space.h>
#include <media/NdkImageReader.h>

#include <vlc_vout_display.h>
#include <vlc_common.h>

/* AWH using a VideoLAN AWindow helpers */
#define AWH_CAPS_SET_VIDEO_LAYOUT 0x1
/* AWH backed by a Android SurfaceView */
#define AWH_CAPS_SURFACE_VIEW 0x2

/*
 * AImageReader function pointers
 */
typedef int32_t (*pfn_AImageReader_newWithUsage)(
    int32_t width, int32_t height, int32_t format, uint64_t usage,
    int32_t maxImages, AImageReader **reader);
typedef void (*pfn_AImageReader_delete)(AImageReader *reader);
typedef int32_t (*pfn_AImageReader_getWindow)(
    AImageReader *reader, ANativeWindow **window);
typedef int32_t (*pfn_AImageReader_acquireNextImageAsync)(
    AImageReader *reader, AImage **image, int *acquireFenceFd);
typedef void (*pfn_AImage_deleteAsync)(AImage *image, int releaseFenceFd);
typedef int32_t (*pfn_AImage_getHardwareBuffer)(
    const AImage *image, struct AHardwareBuffer **buffer);
typedef int32_t (*pfn_AImage_getTimestamp)(
    const AImage *image, int64_t *timestampNs);
typedef int32_t (*pfn_AImage_getCropRect)(
    const AImage *image, AImageCropRect *rect);
typedef int32_t (*pfn_AImage_getWidth)(
    const AImage *image, int32_t *width);
typedef int32_t (*pfn_AImage_getHeight)(
    const AImage *image, int32_t *height);
typedef int32_t (*pfn_AImageReader_setImageListener)(
    AImageReader *reader, AImageReader_ImageListener *listener);
typedef int (*pfn_sync_merge)(const char *name, int fd1, int fd2);

typedef int32_t (*pfn_AHardwareBuffer_getId)(
    const struct AHardwareBuffer *buffer, uint64_t *outId);
typedef void (*pfn_AHardwareBuffer_describe)(
    const struct AHardwareBuffer *buffer, AHardwareBuffer_Desc *outDesc);

/*
* ASurfaceControl function pointers
*/
typedef ASurfaceControl* (*pfn_ASurfaceControl_createFromWindow)(
    ANativeWindow *parent, const char *debug_name);
typedef void (*pfn_ASurfaceControl_release)(ASurfaceControl *surface_control);
typedef ASurfaceTransaction* (*pfn_ASurfaceTransaction_create)(void);
typedef void (*pfn_ASurfaceTransaction_delete)(ASurfaceTransaction *transaction);
typedef void (*pfn_ASurfaceTransaction_apply)(ASurfaceTransaction *transaction);
typedef void (*pfn_ASurfaceTransaction_setBuffer)(
    ASurfaceTransaction *transaction, ASurfaceControl *surface_control,
    struct AHardwareBuffer *buffer, int acquire_fence_fd);
typedef void (*pfn_ASurfaceTransaction_setVisibility)(
    ASurfaceTransaction *transaction, ASurfaceControl *surface_control,
    int8_t visibility);
typedef void (*pfn_ASurfaceTransaction_setBufferTransparency)(
    ASurfaceTransaction *transaction, ASurfaceControl *surface_control,
    int8_t transparency);
typedef void (*pfn_ASurfaceTransaction_setBufferDataSpace)(
    ASurfaceTransaction *transaction, ASurfaceControl *surface_control,
    int32_t data_space);
typedef void (*pfn_ASurfaceTransaction_setHdrMetadata_smpte2086)(
    ASurfaceTransaction *transaction, ASurfaceControl *surface_control,
    struct AHdrMetadata_smpte2086 *metadata);
typedef void (*pfn_ASurfaceTransaction_setHdrMetadata_cta861_3)(
    ASurfaceTransaction *transaction, ASurfaceControl *surface_control,
    struct AHdrMetadata_cta861_3 *metadata);
typedef void (*pfn_ASurfaceTransaction_setOnComplete)(
    ASurfaceTransaction *transaction, void *context,
    void (*func)(void *context, ASurfaceTransactionStats *stats));
typedef void (*pfn_ASurfaceTransaction_setCrop)(
    ASurfaceTransaction *transaction, ASurfaceControl *surface_control,
    const ARect *crop);
typedef void (*pfn_ASurfaceTransaction_setPosition)(
    ASurfaceTransaction *transaction, ASurfaceControl *surface_control,
    int32_t x, int32_t y);
typedef void (*pfn_ASurfaceTransaction_setBufferTransform)(
    ASurfaceTransaction *transaction, ASurfaceControl *surface_control,
    int32_t transform);
typedef void (*pfn_ASurfaceTransaction_setScale)(
    ASurfaceTransaction *transaction, ASurfaceControl *surface_control,
    float xScale, float yScale);
typedef void (*pfn_ASurfaceTransaction_setDesiredPresentTime)(
    ASurfaceTransaction* transaction,
    int64_t desiredPresentTime);
typedef int (*pfn_ASurfaceTransactionStats_getPreviousReleaseFenceFd)(
    ASurfaceTransactionStats *stats, ASurfaceControl *surface_control);

struct aimage_reader_api
{
    /* AImageReader, API 31+ (because AHardwareBuffer) */
    struct {
        pfn_AImageReader_newWithUsage newWithUsage;
        pfn_AImageReader_delete delete;
        pfn_AImageReader_getWindow getWindow;
        pfn_AImageReader_acquireNextImageAsync acquireNextImageAsync;
        pfn_AImageReader_setImageListener setImageListener;
    } AImageReader;
    struct {
        pfn_AImage_deleteAsync deleteAsync;
        pfn_AImage_getHardwareBuffer getHardwareBuffer;
        pfn_AImage_getTimestamp getTimestamp;
        pfn_AImage_getCropRect getCropRect;
        pfn_AImage_getWidth getWidth;
        pfn_AImage_getHeight getHeight;
    } AImage;
    pfn_sync_merge sync_merge;
    struct {
        pfn_AHardwareBuffer_getId getId;
        pfn_AHardwareBuffer_describe describe;
    } AHardwareBuffer;
};

struct asurface_control_api
{
    struct {
        pfn_ASurfaceControl_createFromWindow createFromWindow;
        pfn_ASurfaceControl_release release;
    } ASurfaceControl;
    struct {
        pfn_ASurfaceTransaction_create create;
        pfn_ASurfaceTransaction_delete delete;
        pfn_ASurfaceTransaction_apply apply;
        pfn_ASurfaceTransaction_setBuffer setBuffer;
        pfn_ASurfaceTransaction_setVisibility setVisibility;
        pfn_ASurfaceTransaction_setBufferTransparency setBufferTransparency;
        pfn_ASurfaceTransaction_setBufferDataSpace setBufferDataSpace;
        pfn_ASurfaceTransaction_setHdrMetadata_smpte2086 setHdrMetadata_smpte2086;
        pfn_ASurfaceTransaction_setHdrMetadata_cta861_3 setHdrMetadata_cta861_3;
        pfn_ASurfaceTransaction_setOnComplete setOnComplete;
        pfn_ASurfaceTransaction_setCrop setCrop;
        pfn_ASurfaceTransaction_setPosition setPosition;
        pfn_ASurfaceTransaction_setBufferTransform setBufferTransform;
        pfn_ASurfaceTransaction_setScale setScale;
        pfn_ASurfaceTransaction_setDesiredPresentTime setDesiredPresentTime;
    } ASurfaceTransaction;
    struct {
        pfn_ASurfaceTransactionStats_getPreviousReleaseFenceFd getPreviousReleaseFenceFd;
    } ASurfaceTransactionStats;
};

typedef struct AWindowHandler AWindowHandler;
typedef struct ASurfaceTexture ASurfaceTexture;

enum AWindow_ID {
    AWindow_Video,
    AWindow_Subtitles,
    AWindow_Max,
};

struct awh_mouse_coords
{
    int i_action;
    int i_button;
    int i_x;
    int i_y;
};

typedef struct
{
    void (*on_new_window_size)(vlc_window_t *wnd, unsigned i_width,
                               unsigned i_height);
    void (*on_new_mouse_coords)(vlc_window_t *wnd,
                                const struct awh_mouse_coords *coords);
} awh_events_t;

typedef struct android_video_context_t android_video_context_t;

struct android_video_context_t
{
    struct aimage_reader_api *air_api;
    struct asurface_control_api *asc_api;
    AImageReader *air;

    struct vlc_asurfacetexture *texture;
    void *dec_opaque;
    bool (*render)(struct picture_context_t *ctx);
    bool (*render_ts)(struct picture_context_t *ctx, vlc_tick_t ts);

    struct vlc_asurfacetexture *
        (*get_texture)(struct picture_context_t *ctx);
};

struct android_picture_ctx
{
    picture_context_t s;
    AImage *image;
    int fence_fd;
    int read_fence_fd;
    vlc_atomic_rc_t rc;
    ASurfaceControl *sc;
};

static inline int
android_picture_ctx_get_fence_fd(struct android_picture_ctx *apctx)
{
    return (apctx->fence_fd >= 0) ? dup(apctx->fence_fd) : -1;
}

static inline void
android_picture_ctx_set_read_fence(struct android_picture_ctx *apctx, int fd)
{
    assert(fd >= 0);
    if (apctx->read_fence_fd < 0)
        apctx->read_fence_fd = fd;
    else
    {
        android_video_context_t *avctx =
            vlc_video_context_GetPrivate(apctx->s.vctx, VLC_VIDEO_CONTEXT_AWINDOW);

        int merged = avctx->air_api->sync_merge("vlc_read_fence", apctx->read_fence_fd, fd);
        close(apctx->read_fence_fd);
        close(fd);
        apctx->read_fence_fd = merged;
    }
}

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

    void (*release_tex_image)(
            struct vlc_asurfacetexture *st);

    void (*destroy)(
            struct vlc_asurfacetexture *surface);
};

/**
 * Create new AWindowHandler
 *
 * This handle allow to access IAWindowNativeHandler jobject attached to the
 * VLC object var.
 * \return a valid AWindowHandler * or NULL. It must be released with
 * AWindowHandler_destroy.
 */
AWindowHandler *AWindowHandler_new(vlc_object_t *obj, vlc_window_t *wnd, awh_events_t *p_events);
void AWindowHandler_destroy(AWindowHandler *p_awh);

AWindowHandler *
AWindowHandler_newFromANWs(vlc_object_t *obj, ANativeWindow *video,
                           ANativeWindow *subtitle);

struct aimage_reader_api *
AWindowHandler_getAImageReaderApi(AWindowHandler *p_awh);

struct asurface_control_api *
AWindowHandler_getASurfaceControlApi(AWindowHandler *p_awh);

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

int AWindowHandler_getCapabilities(AWindowHandler *p_awh);

/**
 * Set the video layout
 *
 * Should be called only if AWindowHandler_getCapabilities() has AWH_CAPS_SET_VIDEO_LAYOUT
 */
int AWindowHandler_setVideoLayout(AWindowHandler *p_awh,
                                  int i_display_width, int i_display_height,
                                  const vout_display_place_t *);

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
vlc_asurfacetexture_New(AWindowHandler *p_awh, bool single_buffer);

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

void
SurfaceTexture_releaseTexImage(struct vlc_asurfacetexture *st);
