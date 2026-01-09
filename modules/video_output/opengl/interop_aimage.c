/*****************************************************************************
 * interop_aimage.c: OpenGL AImage/EGL interop
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

#ifndef __ANDROID__
# error this file must be built from android
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include "interop.h"
#include "../android/utils.h"
#include "gl_api.h"
#include "gl_util.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>

/* AHardwareBuffer -> OpenGL/Vulkan image cache */
#define AHB_CACHE_SIZE 64
struct ahb_slot
{
    /* From AHardwareBuffer_getId */
    uint64_t ahb_id;
    bool valid;
    EGLImageKHR image;
};

struct ahb_cache {
    struct ahb_slot slots[AHB_CACHE_SIZE];
};
struct priv
{
    float mtx_2x3[2*3];

    struct aimage_reader_api *air_api;
    struct ahb_cache ahb_cache; /* EGLImage cache for AHardwareBuffers */

    struct {
        PFNGLACTIVETEXTUREPROC ActiveTexture;
        PFNGLBINDTEXTUREPROC BindTexture;
        PFNGLFLUSHPROC Flush;
        PFNGLEGLIMAGETARGETTEXTURE2DOESPROC EGLImageTargetTexture2DOES;
    } gl;

    struct {
        EGLDisplay display;
        PFNEGLGETCURRENTDISPLAYPROC GetCurrentDisplay;
        PFNEGLQUERYSTRINGPROC QueryString;
        PFNEGLCREATEIMAGEKHRPROC CreateImageKHR;
        PFNEGLDESTROYIMAGEKHRPROC DestroyImageKHR;
        PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC GetNativeClientBufferANDROID;
        PFNEGLCREATESYNCKHRPROC CreateSyncKHR;
        PFNEGLWAITSYNCKHRPROC WaitSyncKHR;
        PFNEGLDESTROYSYNCKHRPROC DestroySyncKHR;
        PFNEGLDUPNATIVEFENCEFDANDROIDPROC DupNativeFenceFDANDROID;
    } egl;
};


static void
ahb_cache_init(struct ahb_cache *pool)
{
    for (int i = 0; i < AHB_CACHE_SIZE; i++)
    {
        pool->slots[i].ahb_id = 0;
        pool->slots[i].valid = false;
        pool->slots[i].image = NULL;
    }
}

static struct ahb_slot *
ahb_cache_get(struct ahb_cache *pool, uint64_t ahb_id, bool *is_new)
{
    struct ahb_slot *free_slot = NULL;
    for (size_t i = 0; i < AHB_CACHE_SIZE; i++)
    {
        struct ahb_slot *slot = &pool->slots[i];

        if (slot->valid && slot->ahb_id == ahb_id)
        {
            *is_new = false;
            return slot;
        }

        if (free_slot == NULL && !slot->valid)
            free_slot = slot;
    }

    *is_new = true;
    if (free_slot == NULL)
        free_slot = &pool->slots[0];

    free_slot->ahb_id = ahb_id;
    free_slot->valid = true;
    return free_slot;
}

static int
ComputeAImageTransformMatrix(const struct vlc_gl_interop *interop, AImage *image)
{
    struct priv *priv = interop->priv;
    AImageCropRect crop;
    int32_t buf_width, buf_height;

    if (priv->air_api->AImage.getCropRect(image, &crop) != 0 ||
        priv->air_api->AImage.getWidth(image, &buf_width) != 0 ||
        priv->air_api->AImage.getHeight(image, &buf_height) != 0)
        return VLC_EGENERIC;

    if (buf_width <= 0 || buf_height <= 0 ||
        crop.right <= crop.left || crop.bottom <= crop.top)
        return VLC_EGENERIC;

    float crop_w = crop.right - crop.left;
    float crop_h = crop.bottom - crop.top;

    /* Scale + Y-flip: apply flip first (t' = 1-t), then crop */
    float sx = crop_w / buf_width;
    float sy = -crop_h / buf_height;  /* negative for Y-flip */

    float tx = (float)crop.left / buf_width;
    float ty = (float)crop.bottom / buf_height;  /* bottom, not top */

    priv->mtx_2x3[0] = sx;   priv->mtx_2x3[2] = 0.0f; priv->mtx_2x3[4] = tx;
    priv->mtx_2x3[1] = 0.0f; priv->mtx_2x3[3] = sy;   priv->mtx_2x3[5] = ty;

    return VLC_SUCCESS;
}

static int
tc_aimage_allocate_textures(const struct vlc_gl_interop *interop, uint32_t textures[],
                          const int32_t tex_width[], const int32_t tex_height[])
{
    (void) interop; (void) tex_width; (void) tex_height;
    assert(textures[0] != 0); (void) textures;

    return VLC_SUCCESS;
}

static EGLImageKHR
GetCachedEGLImage(const struct vlc_gl_interop *interop, AHardwareBuffer *ahb)
{
    struct priv *priv = interop->priv;
    vlc_object_t *o = VLC_OBJECT(interop->gl);

    uint64_t ahb_id;
    int32_t status = priv->air_api->AHardwareBuffer.getId(ahb, &ahb_id);

    bool is_new = false;
    struct ahb_slot *slot;
    if (status != 0)
    {
        /* If we can't identify buffers, always use slot 0 as "new" */
        is_new = true;
        slot = &priv->ahb_cache.slots[0];
    }
    else
        slot = ahb_cache_get(&priv->ahb_cache, ahb_id, &is_new);

    if (!is_new)
    {
        assert(slot->image != NULL);
        /* Reuse existing EGLImage */
        return slot->image;
    }
    /* else : cache miss or first init */

    EGLImageKHR egl_image;

    /* Destroy old EGLImage if present */
    if (slot->image != NULL)
    {
        msg_Dbg(o, "cache miss: creating a new EGLImageKHR");
        priv->egl.DestroyImageKHR(priv->egl.display, slot->image);
    }

    EGLClientBuffer client_buffer = priv->egl.GetNativeClientBufferANDROID(ahb);
    if (client_buffer == NULL)
    {
        msg_Err(o, "eglGetNativeClientBufferANDROID failed");
        slot->image = NULL;
        return NULL;
    }

    /* Create EGLImage from AHardwareBuffer */
    EGLint attrs[] = { EGL_NONE };
    egl_image = priv->egl.CreateImageKHR(priv->egl.display, EGL_NO_CONTEXT,
                                         EGL_NATIVE_BUFFER_ANDROID,
                                         client_buffer, attrs);

    if (egl_image == EGL_NO_IMAGE_KHR)
    {
        msg_Err(o, "eglCreateImageKHR failed: 0x%x", eglGetError());
        slot->image = NULL;
        return NULL;
    }

    slot->image = egl_image;
    return egl_image;
}

static int
tc_aimage_update(const struct vlc_gl_interop *interop, uint32_t textures[],
                 const int32_t tex_width[], const int32_t tex_height[],
                 picture_t *pic, const size_t plane_offset[])
{
    struct priv *priv = interop->priv;
    vlc_object_t *o = VLC_OBJECT(interop->gl);

    (void) tex_width; (void) tex_height; (void) plane_offset;
    assert(pic->context);
    assert(textures[0] != 0);

    struct android_picture_ctx *apctx =
        container_of(pic->context, struct android_picture_ctx, s);

    AHardwareBuffer *ahb = NULL;
    int32_t status = priv->air_api->AImage.getHardwareBuffer(apctx->image, &ahb);
    if (status != 0)
    {
        msg_Err(o, "AImage_getHardwareBuffer failed: %d", status);
        goto error;
    }
    assert(ahb != NULL);

    /* Get fence_fd and wait using EGL sync */
    int fence_fd = android_picture_ctx_get_fence_fd(apctx);
    if (fence_fd >= 0)
    {
        const EGLint sync_attribs[] = {
            EGL_SYNC_NATIVE_FENCE_FD_ANDROID, fence_fd,
            EGL_NONE
        };
        EGLSyncKHR sync = priv->egl.CreateSyncKHR(
            priv->egl.display, EGL_SYNC_NATIVE_FENCE_ANDROID, sync_attribs);

        if (sync != EGL_NO_SYNC_KHR)
        {
            priv->egl.WaitSyncKHR(priv->egl.display, sync, 0);
            priv->egl.DestroySyncKHR(priv->egl.display, sync);
        }
        else
        {
            msg_Warn(o, "eglCreateSyncKHR failed");
            close(fence_fd);
        }
    }

    EGLImageKHR egl_image = GetCachedEGLImage(interop, ahb);
    if (egl_image == NULL)
        goto error;

    priv->gl.ActiveTexture(GL_TEXTURE0);
    priv->gl.BindTexture(GL_TEXTURE_EXTERNAL_OES, textures[0]);
    priv->gl.EGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, egl_image);

    /* Create read fence for this picture */
    const EGLint attribs[] = {
        EGL_SYNC_NATIVE_FENCE_FD_ANDROID, EGL_NO_NATIVE_FENCE_FD_ANDROID,
        EGL_NONE
    };
    EGLSyncKHR sync = priv->egl.CreateSyncKHR(
        priv->egl.display, EGL_SYNC_NATIVE_FENCE_ANDROID, attribs);
    if (sync != EGL_NO_SYNC_KHR)
    {
        priv->gl.Flush();
        int read_fence_fd = priv->egl.DupNativeFenceFDANDROID(priv->egl.display,
                                                              sync);
        priv->egl.DestroySyncKHR(priv->egl.display, sync);
        if (read_fence_fd >= 0)
            android_picture_ctx_set_read_fence(apctx, read_fence_fd);
        else
            msg_Warn(o, "eglDupNativeFenceFDANDROID failed");
    }

    int ret = ComputeAImageTransformMatrix(interop, apctx->image);
    if (ret != VLC_SUCCESS)
    {
        msg_Warn(o, "Failed to get AImage dimensions/crop");
        goto error;
    }

    return VLC_SUCCESS;

error:
    return VLC_EGENERIC;
}

static const float *
tc_aimage_transform_matrix(const struct vlc_gl_interop *interop)
{
    struct priv *priv = interop->priv;
    return priv->mtx_2x3;
}

static void
Close(struct vlc_gl_interop *interop)
{
    struct priv *priv = interop->priv;

    for (int i = 0; i < AHB_CACHE_SIZE; i++)
    {
        struct ahb_slot *slot = &priv->ahb_cache.slots[i];
        if (slot->valid)
        {
            assert(slot->image != NULL);
            priv->egl.DestroyImageKHR(priv->egl.display, slot->image);
        }
    }

    free(priv);
}

static int
InitAImage(struct vlc_gl_interop *interop, android_video_context_t *avctx)
{
    struct priv *priv = interop->priv;

#define LOAD_GL_SYMBOL(name) \
    priv->gl.name = vlc_gl_GetProcAddress(interop->gl, "gl" # name); \
    if (priv->gl.name == NULL) return VLC_EGENERIC

#define LOAD_EGL_SYMBOL(name) \
    priv->egl.name = vlc_gl_GetProcAddress(interop->gl, "egl" # name); \
    if (priv->egl.name == NULL) return VLC_EGENERIC

    LOAD_GL_SYMBOL(ActiveTexture);
    LOAD_GL_SYMBOL(BindTexture);
    LOAD_GL_SYMBOL(Flush);
    LOAD_GL_SYMBOL(EGLImageTargetTexture2DOES);

    LOAD_EGL_SYMBOL(GetCurrentDisplay);
    LOAD_EGL_SYMBOL(QueryString);
    LOAD_EGL_SYMBOL(CreateImageKHR);
    LOAD_EGL_SYMBOL(DestroyImageKHR);
    LOAD_EGL_SYMBOL(GetNativeClientBufferANDROID);
    LOAD_EGL_SYMBOL(CreateSyncKHR);
    LOAD_EGL_SYMBOL(WaitSyncKHR);
    LOAD_EGL_SYMBOL(DestroySyncKHR);
    LOAD_EGL_SYMBOL(DupNativeFenceFDANDROID);

#undef LOAD_GL_SYMBOL
#undef LOAD_EGL_SYMBOL

    priv->egl.display = priv->egl.GetCurrentDisplay();
    if (priv->egl.display == NULL)
        return VLC_EGENERIC;

    const char *eglexts = priv->egl.QueryString(priv->egl.display,
                                                EGL_EXTENSIONS);
    if (eglexts == NULL)
        return VLC_EGENERIC;
    if (!vlc_gl_StrHasToken(eglexts, "EGL_ANDROID_get_native_client_buffer")
     || !vlc_gl_StrHasToken(eglexts, "EGL_ANDROID_image_native_buffer")
     || !vlc_gl_StrHasToken(eglexts, "EGL_ANDROID_native_fence_sync"))
        return VLC_EGENERIC;

    priv->air_api = avctx->air_api;
    ahb_cache_init(&priv->ahb_cache);
    return VLC_SUCCESS;
}

static int
Open(struct vlc_gl_interop *interop)
{
    if (interop->fmt_in.i_chroma != VLC_CODEC_ANDROID_OPAQUE
     || !interop->vctx)
        return VLC_EGENERIC;

    struct vlc_gl_extension_vt extension_vt;
    vlc_gl_LoadExtensionFunctions(interop->gl, &extension_vt);

    if (!vlc_gl_HasExtension(&extension_vt, "GL_OES_EGL_image_external"))
    {
        msg_Warn(&interop->obj, "GL_OES_EGL_image_external is not available,"
                " disabling android interop.");
        return VLC_EGENERIC;
    }

    android_video_context_t *avctx =
        vlc_video_context_GetPrivate(interop->vctx, VLC_VIDEO_CONTEXT_AWINDOW);
    assert(avctx != NULL);

    bool has_aimage = avctx->air != NULL && avctx->air_api != NULL;
    if (!has_aimage)
        return VLC_EGENERIC;

    interop->priv = malloc(sizeof(struct priv));
    if (unlikely(interop->priv == NULL))
        return VLC_ENOMEM;

    struct priv *priv = interop->priv;
    priv->air_api = NULL;

    /* Try to use AImage path if available */
    int ret = InitAImage(interop, avctx);
    if (ret != VLC_SUCCESS)
    {
        free(priv);
        return ret;
    }

    static const struct vlc_gl_interop_ops aimage_ops = {
        .allocate_textures = tc_aimage_allocate_textures,
        .update_textures = tc_aimage_update,
        .get_transform_matrix = tc_aimage_transform_matrix,
        .close = Close,
    };
    interop->ops = &aimage_ops;

    interop->tex_target = GL_TEXTURE_EXTERNAL_OES;
    if (vlc_gl_HasExtension(&extension_vt, "GL_EXT_YUV_target"))
    {
        msg_Warn(&interop->obj, "GL_EXT_YUV_target is available,"
                " using it.");
        /* We represent as Packed YUV 4:4:4 since there is a single
         * texture target available. */
        interop->fmt_out.i_chroma = VLC_CODEC_V308;
        interop->fmt_out.space = interop->fmt_in.space;
        interop->fmt_out.primaries = interop->fmt_in.primaries;
        interop->fmt_out.transfer = interop->fmt_in.transfer;
    }
    else
    {
        interop->fmt_out.i_chroma = VLC_CODEC_RGBA;
        interop->fmt_out.space = COLOR_SPACE_UNDEF;
    }

    interop->tex_count = 1;
    interop->texs[0] = (struct vlc_gl_tex_cfg) {
        .w = {1, 1},
        .h = {1, 1},
        .internal = GL_RGBA,
        .format = GL_RGBA,
        .type = GL_UNSIGNED_BYTE,
    };

    return VLC_SUCCESS;
}

vlc_module_begin ()
    set_description("Android OpenGL AImage converter")
    set_capability("glinterop", 2)
    set_callback(Open)
    set_subcategory(SUBCAT_VIDEO_VOUT)
vlc_module_end ()
