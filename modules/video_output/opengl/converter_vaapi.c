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

#include "internal.h"
#include "../../hw/vaapi/vlc_vaapi.h"
#include <vlc_vout_window.h>

#include <assert.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <va/va_drmcommon.h>

#ifdef HAVE_VA_WL
# include <va/va_wayland.h>
#endif

#ifdef HAVE_VA_X11
# include <va/va_x11.h>
# include <vlc_xlib.h>
#endif

#if defined(USE_OPENGL_ES2)
#   include <GLES2/gl2ext.h>
#endif

struct priv
{
    VADisplay vadpy;
    VASurfaceID *va_surface_ids;
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
#ifdef HAVE_VA_X11
    Display *x11dpy;
#endif

    unsigned fourcc;
    EGLint drm_fourccs[3];

    struct {
        picture_t *  pic;
        VAImage      va_image;
        VABufferInfo va_buffer_info;
        void *       egl_images[3];
    } last;
};

static void
vaegl_release_last_pic(const opengl_tex_converter_t *tc, struct priv *priv)
{
    vlc_object_t *o = VLC_OBJECT(tc->gl);
    vlc_gl_t *gl = tc->gl;

    for (unsigned i = 0; i < priv->last.va_image.num_planes; ++i)
        gl->egl.destroyImageKHR(gl, priv->last.egl_images[i]);

    vlc_vaapi_ReleaseBufferHandle(o, priv->vadpy, priv->last.va_image.buf);

    vlc_vaapi_DestroyImage(o, priv->vadpy, priv->last.va_image.image_id);

    picture_Release(priv->last.pic);
}

static int
vaegl_init_fourcc(const opengl_tex_converter_t *tc, struct priv *priv,
                  unsigned va_fourcc)
{
    (void) tc;
    switch (va_fourcc)
    {
        case VA_FOURCC_NV12:
            priv->drm_fourccs[0] = VLC_FOURCC('R', '8', ' ', ' ');
            priv->drm_fourccs[1] = VLC_FOURCC('G', 'R', '8', '8');
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
tc_vaegl_update(const opengl_tex_converter_t *tc, GLuint *textures,
                const GLsizei *tex_width, const GLsizei *tex_height,
                picture_t *pic, const size_t *plane_offset)
{
    (void) plane_offset;
    struct priv *priv = tc->priv;
    vlc_object_t *o = VLC_OBJECT(tc->gl);
    vlc_gl_t *gl = tc->gl;
    VAImage va_image;
    VABufferInfo va_buffer_info;
    EGLImageKHR egl_images[3] = { };
    bool release_image = false, release_buffer_info = false;

    if (pic == priv->last.pic)
    {
        va_image = priv->last.va_image;
        va_buffer_info = priv->last.va_buffer_info;
        for (unsigned i = 0; i < priv->last.va_image.num_planes; ++i)
            egl_images[i] = priv->last.egl_images[i];
    }
    else
    {
        if (vlc_vaapi_DeriveImage(o, priv->vadpy, vlc_vaapi_PicGetSurface(pic),
                                  &va_image))
            goto error;
        release_image = true;

        if (va_image.format.fourcc != priv->fourcc)
        {
            msg_Err(tc->gl, "fourcc changed");
            /* TODO: fetch new fourcc and reconfigure shader */
            goto error;
        }

        va_buffer_info = (VABufferInfo) {
            .mem_type = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME
        };
        if (vlc_vaapi_AcquireBufferHandle(o, priv->vadpy, va_image.buf,
                                          &va_buffer_info))
            goto error;
        release_buffer_info = true;
    }

    for (unsigned i = 0; i < va_image.num_planes; ++i)
    {
        EGLint attribs[] = {
            EGL_WIDTH, tex_width[i],
            EGL_HEIGHT, tex_height[i],
            EGL_LINUX_DRM_FOURCC_EXT, priv->drm_fourccs[i],
            EGL_DMA_BUF_PLANE0_FD_EXT, va_buffer_info.handle,
            EGL_DMA_BUF_PLANE0_OFFSET_EXT, va_image.offsets[i],
            EGL_DMA_BUF_PLANE0_PITCH_EXT, va_image.pitches[i],
            EGL_NONE
        };

        egl_images[i] = gl->egl.createImageKHR(gl, EGL_LINUX_DMA_BUF_EXT, NULL,
                                               attribs);
        if (egl_images[i] == NULL)
            goto error;

        glBindTexture(tc->tex_target, textures[i]);

        priv->glEGLImageTargetTexture2DOES(tc->tex_target, egl_images[i]);
    }

    if (pic != priv->last.pic)
    {
        if (priv->last.pic != NULL)
            vaegl_release_last_pic(tc, priv);
        priv->last.pic = picture_Hold(pic);
        priv->last.va_image = va_image;
        priv->last.va_buffer_info = va_buffer_info;
        for (unsigned i = 0; i < va_image.num_planes; ++i)
            priv->last.egl_images[i] = egl_images[i];
    }

    return VLC_SUCCESS;

error:
    if (release_image)
    {
        if (release_buffer_info)
            vlc_vaapi_ReleaseBufferHandle(o, priv->vadpy, va_image.buf);

        for (unsigned i = 0; i < 3 && egl_images[i] != NULL; ++i)
            gl->egl.destroyImageKHR(gl, egl_images[i]);

        vlc_vaapi_DestroyImage(o, priv->vadpy, va_image.image_id);
    }
    return VLC_EGENERIC;
}

static picture_pool_t *
tc_vaegl_get_pool(const opengl_tex_converter_t *tc, unsigned requested_count)
{
    vlc_object_t *o = VLC_OBJECT(tc->gl);
    struct priv *priv = tc->priv;

    picture_pool_t *pool =
        vlc_vaapi_PoolNew(VLC_OBJECT(tc->gl), priv->vadpy, requested_count,
                          &priv->va_surface_ids, &tc->fmt, VA_RT_FORMAT_YUV420,
                          VA_FOURCC_NV12);
    if (!pool)
        return NULL;

    /* Check if a surface from the pool can be derived */
    VAImage va_image;
    if (vlc_vaapi_DeriveImage(o, priv->vadpy, priv->va_surface_ids[0],
                              &va_image))
    {
        picture_pool_Release(pool);
        return NULL;
    }

    vlc_vaapi_DestroyImage(o, priv->vadpy, va_image.image_id);
    return pool;
}

static void
tc_vaegl_release(const opengl_tex_converter_t *tc)
{
    struct priv *priv = tc->priv;

    if (priv->last.pic != NULL)
        vaegl_release_last_pic(tc, priv);

    vlc_vaapi_ReleaseInstance(priv->vadpy);

#ifdef HAVE_VA_X11
    if (priv->x11dpy != NULL)
        XCloseDisplay(priv->x11dpy);
#endif
    free(tc->priv);
}

static int
tc_va_check_interop_blacklist(opengl_tex_converter_t *tc, VADisplay *vadpy)
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
    strncpy(vendor_prefix, vendor, BL_SIZE_MAX);
    vendor_prefix[BL_SIZE_MAX - 1] = '\0';

    const char *found = bsearch(vendor_prefix, blacklist_prefix,
                                ARRAY_SIZE(blacklist_prefix),
                                BL_SIZE_MAX, (void *) strcasecmp);
    if (found != NULL)
    {
        msg_Warn(tc->gl, "The '%s' driver is blacklisted: no interop", found);
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static int
tc_vaegl_init(opengl_tex_converter_t *tc, VADisplay *vadpy)
{
    if (vadpy == NULL)
        return VLC_EGENERIC;

    int ret = VLC_ENOMEM;
    struct priv *priv = tc->priv = calloc(1, sizeof(struct priv));
    if (unlikely(tc->priv == NULL))
        goto error;

    ret = VLC_EGENERIC;
    priv->vadpy = vadpy;
    priv->fourcc = 0;

    if (!HasExtension(tc->glexts, "GL_OES_EGL_image"))
        goto error;

    const char *eglexts = tc->gl->egl.queryString(tc->gl, EGL_EXTENSIONS);
    if (eglexts == NULL || !HasExtension(eglexts, "EGL_EXT_image_dma_buf_import"))
        goto error;

    if (vaegl_init_fourcc(tc, priv, VA_FOURCC_NV12))
        goto error;

    priv->glEGLImageTargetTexture2DOES =
        vlc_gl_GetProcAddress(tc->gl, "glEGLImageTargetTexture2DOES");
    if (priv->glEGLImageTargetTexture2DOES == NULL)
        goto error;

    tc->pf_update  = tc_vaegl_update;
    tc->pf_release = tc_vaegl_release;
    tc->pf_get_pool = tc_vaegl_get_pool;

    if (vlc_vaapi_Initialize(VLC_OBJECT(tc->gl), priv->vadpy))
        goto error;

    if (tc_va_check_interop_blacklist(tc, priv->vadpy))
        goto error;

    if (vlc_vaapi_SetInstance(priv->vadpy))
    {
        msg_Err(tc->gl, "VAAPI instance already in use");
        vadpy = NULL;
        goto error;
    }

    tc->fshader = opengl_fragment_shader_init(tc, GL_TEXTURE_2D, VLC_CODEC_NV12,
                                              tc->fmt.space);
    if (tc->fshader == 0)
    {
        vlc_vaapi_ReleaseInstance(priv->vadpy);
        vadpy = NULL;
        goto error;
    }
    return VLC_SUCCESS;

error:
    if (vadpy != NULL)
        vaTerminate(vadpy);
    free(tc->priv);
    return ret;
}

int
opengl_tex_converter_vaapi_init(opengl_tex_converter_t *tc)
{
    if (tc->fmt.i_chroma != VLC_CODEC_VAAPI_420 || tc->gl->ext != VLC_GL_EXT_EGL
     || tc->gl->egl.createImageKHR == NULL
     || tc->gl->egl.destroyImageKHR == NULL)
        return VLC_EGENERIC;

    switch (tc->gl->surface->type)
    {
#ifdef HAVE_VA_X11
        case VOUT_WINDOW_TYPE_XID:
        {
            if (!vlc_xlib_init(VLC_OBJECT(tc->gl)))
                return VLC_EGENERIC;
            Display *x11dpy = XOpenDisplay(tc->gl->surface->display.x11);
            if (x11dpy == NULL)
                return VLC_EGENERIC;

            if (tc_vaegl_init(tc, vaGetDisplay(x11dpy)))
            {
                XCloseDisplay(x11dpy);
                return VLC_EGENERIC;
            }
            struct priv *priv = tc->priv;
            priv->x11dpy = x11dpy;
            break;
        }
#endif
#ifdef HAVE_VA_WL
        case VOUT_WINDOW_TYPE_WAYLAND:
            if (tc_vaegl_init(tc, vaGetDisplayWl(tc->gl->surface->display.wl)))
                return VLC_EGENERIC;
            break;
#endif
        default:
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}
