/*****************************************************************************
 * converter_vaapi.c: OpenGL VAAPI opaque converter
 *****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
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

#include <assert.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <va/va_drmcommon.h>

#include <vlc_common.h>
#include <vlc_window.h>
#include <vlc_codec.h>
#include <vlc_plugin.h>

#include "gl_util.h"
#include "interop.h"
#include "../../hw/vaapi/vlc_vaapi.h"

/* From https://www.khronos.org/registry/OpenGL/extensions/OES/OES_EGL_image.txt
 * The extension is an OpenGL ES extension but can (and usually is) available on
 * OpenGL implementations. */
#ifndef GL_OES_EGL_image
#define GL_OES_EGL_image 1
typedef void *GLeglImageOES;
typedef void (*PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)(GLenum target, GLeglImageOES image);
#endif

#define DRM_FORMAT_MOD_VENDOR_NONE    0
#define DRM_FORMAT_RESERVED           ((1ULL << 56) - 1)

#define fourcc_mod_code(vendor, val) \
        ((((EGLuint64KHR)DRM_FORMAT_MOD_VENDOR_## vendor) << 56) | ((val) & 0x00ffffffffffffffULL))

#define DRM_FORMAT_MOD_INVALID  fourcc_mod_code(NONE, DRM_FORMAT_RESERVED)


struct priv
{
    VADisplay vadpy;
    VASurfaceID *va_surface_ids;
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;

    struct
    {
        EGLDisplay display;
        EGLDisplay (*getCurrentDisplay)(void);
        const char *(*queryString)(EGLDisplay, EGLint);
        EGLImage (*createImageKHR)(EGLDisplay, EGLContext, EGLenum target, EGLClientBuffer buffer,
                const EGLint *attrib_list);
        void (*destroyImageKHR)(EGLDisplay, EGLImage image);
    } egl;

    struct
    {
        PFNGLBINDTEXTUREPROC BindTexture;
    } gl;

    unsigned fourcc;
    EGLint drm_fourccs[3];

    struct {
        picture_t *                 pic;
#if VA_CHECK_VERSION(1, 1, 0)
        /* VADRMPRIMESurfaceDescriptor carries modifier information
         * (GPU tiling, compression, etc...) */
        VADRMPRIMESurfaceDescriptor va_surface_descriptor;
#else
        VAImage                     va_image;
        VABufferInfo                va_buffer_info;
#endif
        unsigned                    num_planes;
        void *                      egl_images[3];
    } last;
};

static EGLImageKHR
vaegl_image_create(const struct vlc_gl_interop *interop, EGLint w, EGLint h,
                   EGLint fourcc, EGLint fd, EGLint offset, EGLint pitch,
                   EGLuint64KHR modifier)
{
    struct priv *priv = interop->priv;
    const EGLint attribs[] = {
        EGL_WIDTH, w,
        EGL_HEIGHT, h,
        EGL_LINUX_DRM_FOURCC_EXT, fourcc,
        EGL_DMA_BUF_PLANE0_FD_EXT, fd,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, offset,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, pitch,
        EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, modifier & 0xffffffff,
        EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, modifier >> 32,
        EGL_NONE
    };

    return priv->egl.createImageKHR(priv->egl.display,
            EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attribs);
}

static void
vaegl_image_destroy(const struct vlc_gl_interop *interop, EGLImageKHR image)
{
    struct priv *priv = interop->priv;
    priv->egl.destroyImageKHR(priv->egl.display, image);
}

static void
vaegl_release_last_pic(const struct vlc_gl_interop *interop, struct priv *priv)
{
    vlc_object_t *o = VLC_OBJECT(interop->gl);

    for (unsigned i = 0; i < priv->last.num_planes; ++i)
        vaegl_image_destroy(interop, priv->last.egl_images[i]);

#if VA_CHECK_VERSION(1, 1, 0)
    for (unsigned i = 0; i < priv->last.va_surface_descriptor.num_objects; ++i)
        close(priv->last.va_surface_descriptor.objects[i].fd);
#else
    vlc_vaapi_ReleaseBufferHandle(o, priv->vadpy, priv->last.va_image.buf);
    vlc_vaapi_DestroyImage(o, priv->vadpy, priv->last.va_image.image_id);
#endif

    picture_Release(priv->last.pic);
}

static int
vaegl_init_fourcc(struct priv *priv, unsigned va_fourcc)
{
    switch (va_fourcc)
    {
        case VA_FOURCC_NV12:
            priv->drm_fourccs[0] = VLC_FOURCC('R', '8', ' ', ' ');
            priv->drm_fourccs[1] = VLC_FOURCC('G', 'R', '8', '8');
            break;
        case VA_FOURCC_P010:
            priv->drm_fourccs[0] = VLC_FOURCC('R', '1', '6', ' ');
            priv->drm_fourccs[1] = VLC_FOURCC('G', 'R', '3', '2');
            break;
#if 0
        /* TODO: the following fourcc are not handled for now */
        case VA_FOURCC_RGBA:
            priv->drm_fourccs[0] = VLC_FOURCC('G', 'R', '3', '2');
            break;
        case VA_FOURCC_BGRA:
            priv->drm_fourccs[0] = VLC_FOURCC('G', 'R', '3', '2');
            break;
        case VA_FOURCC_YV12:
            priv->drm_fourccs[0] = VLC_FOURCC('R', '8', ' ', ' ');
            priv->drm_fourccs[1] = VLC_FOURCC('R', '8', ' ', ' ');
            priv->drm_fourccs[2] = VLC_FOURCC('R', '8', ' ', ' ');
            break;
        case VA_FOURCC_422H:
            priv->drm_fourccs[0] = VLC_FOURCC('R', '8', ' ', ' ');
            priv->drm_fourccs[1] = VLC_FOURCC('R', '8', ' ', ' ');
            priv->drm_fourccs[2] = VLC_FOURCC('R', '8', ' ', ' ');
            break;
        case VA_FOURCC_UYVY:
            priv->drm_fourccs[0] = VLC_FOURCC('R', '1', '6', ' ');
            break;
        case VA_FOURCC_444P:
            priv->drm_fourccs[0] = VLC_FOURCC('R', '1', '6', ' ');
            priv->drm_fourccs[1] = VLC_FOURCC('R', '1', '6', ' ');
            priv->drm_fourccs[2] = VLC_FOURCC('R', '1', '6', ' ');
            break;
#endif
        default: return VLC_EGENERIC;
    }
    priv->fourcc = va_fourcc;
    return VLC_SUCCESS;
}

static int
tc_vaegl_update(const struct vlc_gl_interop *interop, uint32_t textures[],
                const int32_t tex_width[], const int32_t tex_height[],
                picture_t *pic, const size_t *plane_offset)
{
    (void) plane_offset;
    struct priv *priv = interop->priv;
    vlc_object_t *o = VLC_OBJECT(interop->gl);
#if VA_CHECK_VERSION(1, 1, 0)
    VADRMPRIMESurfaceDescriptor va_surface_descriptor;
#else
    VAImage va_image;
    VABufferInfo va_buffer_info;
#endif
    EGLImageKHR egl_images[3] = { };
    bool release_image = false, release_buffer_info = false;
    unsigned num_planes = 0;

    if (pic == priv->last.pic)
    {
#if VA_CHECK_VERSION(1, 1, 0)
        va_surface_descriptor = priv->last.va_surface_descriptor;
#else
        va_image = priv->last.va_image;
#endif
        for (unsigned i = 0; i < priv->last.num_planes; ++i)
            egl_images[i] = priv->last.egl_images[i];
    }
    else
    {
#if VA_CHECK_VERSION(1, 1, 0)
        VA_CALL(o, vaSyncSurface, priv->vadpy, vlc_vaapi_PicGetSurface(pic));
        if (vlc_vaapi_ExportSurfaceHandle(o, priv->vadpy, vlc_vaapi_PicGetSurface(pic),
                                          VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                          VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS,
                                          &va_surface_descriptor))
            goto error;
        release_image = true;
#else
        if (vlc_vaapi_DeriveImage(o, priv->vadpy, vlc_vaapi_PicGetSurface(pic),
                                  &va_image))
            goto error;
        release_image = true;

        assert(va_image.format.fourcc == priv->fourcc);

        va_buffer_info = (VABufferInfo) {
            .mem_type = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME
        };
        if (vlc_vaapi_AcquireBufferHandle(o, priv->vadpy, va_image.buf,
                                          &va_buffer_info))
            goto error;
#endif
        release_buffer_info = true;
    }

#if VA_CHECK_VERSION(1, 1, 0)
    num_planes = va_surface_descriptor.num_layers;
    for (unsigned i = 0; i < num_planes; ++i)
    {
        unsigned obj_idx = va_surface_descriptor.layers[i].object_index[0];

        egl_images[i] =
            vaegl_image_create(interop, tex_width[i], tex_height[i],
                               priv->drm_fourccs[i],
                               va_surface_descriptor.objects[obj_idx].fd,
                               va_surface_descriptor.layers[i].offset[0],
                               va_surface_descriptor.layers[i].pitch[0],
                               va_surface_descriptor.objects[obj_idx].drm_format_modifier);
        if (egl_images[i] == NULL)
            goto error;

        priv->gl.BindTexture(interop->tex_target, textures[i]);

        priv->glEGLImageTargetTexture2DOES(interop->tex_target, egl_images[i]);
    }
#else
    num_planes = va_image.num_planes;
    for (unsigned i = 0; i < num_planes; ++i)
    {
        egl_images[i] =
            vaegl_image_create(interop, tex_width[i], tex_height[i],
                               priv->drm_fourccs[i], va_buffer_info.handle,
                               va_image.offsets[i], va_image.pitches[i],
                               DRM_FORMAT_MOD_INVALID);
        if (egl_images[i] == NULL)
            goto error;

        priv->gl.BindTexture(interop->tex_target, textures[i]);

        priv->glEGLImageTargetTexture2DOES(interop->tex_target, egl_images[i]);
    }
#endif

    if (pic != priv->last.pic)
    {
        if (priv->last.pic != NULL)
            vaegl_release_last_pic(interop, priv);
        priv->last.pic = picture_Hold(pic);
#if VA_CHECK_VERSION(1, 1, 0)
        priv->last.va_surface_descriptor = va_surface_descriptor;
#else
        priv->last.va_image = va_image;
        priv->last.va_buffer_info = va_buffer_info;
#endif
        priv->last.num_planes = num_planes;

        for (unsigned i = 0; i < num_planes; ++i)
            priv->last.egl_images[i] = egl_images[i];
    }

    return VLC_SUCCESS;

error:
    if (release_image)
    {
        if (release_buffer_info)
        {
#if VA_CHECK_VERSION(1, 1, 0)
            for (unsigned i = 0; i < va_surface_descriptor.num_objects; ++i)
                close(va_surface_descriptor.objects[i].fd);
#else
            vlc_vaapi_ReleaseBufferHandle(o, priv->vadpy, va_image.buf);
#endif
        }

        for (unsigned i = 0; i < 3 && egl_images[i] != NULL; ++i)
            vaegl_image_destroy(interop, egl_images[i]);

#if !VA_CHECK_VERSION(1, 1, 0)
        vlc_vaapi_DestroyImage(o, priv->vadpy, va_image.image_id);
#endif
    }
    return VLC_EGENERIC;
}

static void
Close(struct vlc_gl_interop *interop)
{
    struct priv *priv = interop->priv;

    if (priv->last.pic != NULL)
        vaegl_release_last_pic(interop, priv);

    free(priv);
}

static int strcasecmp_void(const void *a, const void *b)
{
    return strcasecmp(a, b);
}

static int
tc_va_check_interop_blacklist(const struct vlc_gl_interop *interop, VADisplay *vadpy)
{
    const char *vendor = vaQueryVendorString(vadpy);
    if (vendor == NULL)
        return VLC_SUCCESS;

#define BL_SIZE_MAX 19
    static const char blacklist_prefix[][BL_SIZE_MAX] = {
        /* XXX: case insensitive and alphabetical order */
        "mesa gallium vaapi",
    };

    char vendor_prefix[BL_SIZE_MAX];
    strncpy(vendor_prefix, vendor, sizeof(vendor_prefix) - 1);
    vendor_prefix[sizeof(vendor_prefix) - 1] = '\0';

    const char *found = bsearch(vendor_prefix, blacklist_prefix,
                                ARRAY_SIZE(blacklist_prefix),
                                BL_SIZE_MAX, strcasecmp_void);
    if (found != NULL)
    {
        msg_Warn(interop->gl, "The '%s' driver is blacklisted: no interop", found);
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static int
tc_va_check_derive_image(const struct vlc_gl_interop *interop)
{
    vlc_object_t *o = VLC_OBJECT(interop->gl);
    struct priv *priv = interop->priv;
    VASurfaceID *va_surface_ids;

    picture_pool_t *pool = vlc_vaapi_PoolNew(o, interop->vctx, priv->vadpy, 1,
                                             &va_surface_ids,
                                             &interop->fmt_out);
    if (!pool)
        return VLC_EGENERIC;

    VAImage va_image = { .image_id = VA_INVALID_ID };
    int ret = vlc_vaapi_DeriveImage(o, priv->vadpy, va_surface_ids[0],
                                    &va_image);
    if (ret != VLC_SUCCESS)
        goto done;
    assert(va_image.format.fourcc == priv->fourcc);

    const vlc_chroma_description_t *image_desc =
        vlc_fourcc_GetChromaDescription(va_image.format.fourcc);
    assert(image_desc->plane_count == va_image.num_planes);

    VABufferInfo va_buffer_info = (VABufferInfo) {
        .mem_type = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME
    };
    ret = vlc_vaapi_AcquireBufferHandle(o ,priv->vadpy, va_image.buf,
                                        &va_buffer_info);
    if (ret != VLC_SUCCESS)
        goto done;

    for (unsigned i = 0; i < image_desc->plane_count; ++i)
    {
        unsigned w_num = image_desc->p[i].w.num;
        if (image_desc->plane_count == 2 && i == 1)
            // for NV12/P010 the second plane uses GL_RG which has a double pitch
            w_num /= 2;
        EGLint w = (va_image.width * w_num) / image_desc->p[i].w.den;
        EGLint h = (va_image.height * image_desc->p[i].h.num) / image_desc->p[i].h.den;
        EGLImageKHR egl_image =
            vaegl_image_create(interop, w, h, priv->drm_fourccs[i], va_buffer_info.handle,
                               va_image.offsets[i], va_image.pitches[i],
                               DRM_FORMAT_MOD_INVALID);
        if (egl_image == NULL)
        {
            msg_Warn(o, "Can't create Image KHR: kernel too old ?");
            ret = VLC_EGENERIC;
            goto done;
        }
        vaegl_image_destroy(interop, egl_image);
    }

done:
    if (va_image.image_id != VA_INVALID_ID)
    {
        if (va_image.buf != VA_INVALID_ID)
            vlc_vaapi_ReleaseBufferHandle(o, priv->vadpy, va_image.buf);
        vlc_vaapi_DestroyImage(o, priv->vadpy, va_image.image_id);
    }

    picture_pool_Release(pool);

    return ret;
}

static int
Open(struct vlc_gl_interop *interop)
{
    struct priv *priv = NULL;

    if (interop->vctx == NULL)
        return VLC_EGENERIC;
    vlc_decoder_device *dec_device = vlc_video_context_HoldDevice(interop->vctx);
    if (dec_device->type != VLC_DECODER_DEVICE_VAAPI
     || !vlc_vaapi_IsChromaOpaque(interop->fmt_in.i_chroma))
    {
        goto error;
    }

    struct vlc_gl_extension_vt extension_vt;
    vlc_gl_LoadExtensionFunctions(interop->gl, &extension_vt);

    if (!vlc_gl_HasExtension(&extension_vt, "GL_OES_EGL_image"))
        goto error;

    priv = interop->priv = calloc(1, sizeof(struct priv));
    if (unlikely(priv == NULL))
        goto error;
    priv->fourcc = 0;

    int va_fourcc;
    int vlc_sw_chroma;
    switch (interop->fmt_in.i_chroma)
    {
        case VLC_CODEC_VAAPI_420:
            va_fourcc = VA_FOURCC_NV12;
            vlc_sw_chroma = VLC_CODEC_NV12;

            interop->tex_count = 2;
            interop->texs[0] = (struct vlc_gl_tex_cfg) {
                .w = {1, 1},
                .h = {1, 1},
                .internal = GL_RED,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE,
            };
            interop->texs[1] = (struct vlc_gl_tex_cfg) {
                .w = {1, 2},
                .h = {1, 2},
                .internal = GL_RG,
                .format = GL_RG,
                .type = GL_UNSIGNED_BYTE,
            };

            break;
        case VLC_CODEC_VAAPI_420_10BPP:
            va_fourcc = VA_FOURCC_P010;
            vlc_sw_chroma = VLC_CODEC_P010;

            if (vlc_gl_interop_GetTexFormatSize(interop, GL_TEXTURE_2D, GL_RG,
                                                GL_RG16, GL_UNSIGNED_SHORT) != 16)
                goto error;

            interop->tex_count = 2;
            interop->texs[0] = (struct vlc_gl_tex_cfg) {
                .w = {1, 1},
                .h = {1, 1},
                .internal = GL_R16,
                .format = GL_RED,
                .type = GL_UNSIGNED_BYTE,
            };
            interop->texs[1] = (struct vlc_gl_tex_cfg) {
                .w = {1, 2},
                .h = {1, 2},
                .internal = GL_RG16,
                .format = GL_RG,
                .type = GL_UNSIGNED_BYTE,
            };

            break;
        default:
            vlc_assert_unreachable();
    }

    if (vaegl_init_fourcc(priv, va_fourcc))
        goto error;

    priv->egl.getCurrentDisplay = vlc_gl_GetProcAddress(interop->gl, "eglGetCurrentDisplay");
    if (priv->egl.getCurrentDisplay == EGL_NO_DISPLAY)
        goto error;

    priv->egl.display = priv->egl.getCurrentDisplay();
    if (priv->egl.display == EGL_NO_DISPLAY)
        goto error;

    priv->egl.queryString = vlc_gl_GetProcAddress(interop->gl, "eglQueryString");
    if (priv->egl.queryString == NULL)
        goto error;

    /* EGL_EXT_image_dma_buf_import implies EGL_KHR_image_base */
    const char *eglexts = priv->egl.queryString(priv->egl.display, EGL_EXTENSIONS);
    if (eglexts == NULL || !vlc_gl_StrHasToken(eglexts, "EGL_EXT_image_dma_buf_import"))
        goto error;

    priv->egl.createImageKHR =
        vlc_gl_GetProcAddress(interop->gl, "eglCreateImageKHR");
    if (priv->egl.createImageKHR == NULL)
        goto error;

    priv->egl.destroyImageKHR =
        vlc_gl_GetProcAddress(interop->gl, "eglDestroyImageKHR");
    if (priv->egl.destroyImageKHR == NULL)
        goto error;

    priv->glEGLImageTargetTexture2DOES =
        vlc_gl_GetProcAddress(interop->gl, "glEGLImageTargetTexture2DOES");
    if (priv->glEGLImageTargetTexture2DOES == NULL)
        goto error;

    priv->gl.BindTexture =
        vlc_gl_GetProcAddress(interop->gl, "glBindTexture");
    if (priv->gl.BindTexture == NULL)
        goto error;

    priv->vadpy = dec_device->opaque;
    assert(priv->vadpy != NULL);

    if (tc_va_check_interop_blacklist(interop, priv->vadpy))
        goto error;

    if (tc_va_check_derive_image(interop))
        goto error;

    /* The pictures are uploaded upside-down */
    video_format_TransformBy(&interop->fmt_out, TRANSFORM_VFLIP);

    interop->tex_target = GL_TEXTURE_2D;
    interop->fmt_out.i_chroma = vlc_sw_chroma;
    interop->fmt_out.space = interop->fmt_in.space;

    static const struct vlc_gl_interop_ops ops = {
        .update_textures = tc_vaegl_update,
        .close = Close,
    };
    interop->ops = &ops;

    vlc_decoder_device_Release(dec_device);

    return VLC_SUCCESS;
error:
    vlc_decoder_device_Release(dec_device);
    free(priv);
    return VLC_EGENERIC;
}

vlc_module_begin ()
    set_description("VA-API OpenGL surface converter")
    set_capability("glinterop", 1)
    set_callback(Open)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    add_shortcut("vaapi")
vlc_module_end ()
