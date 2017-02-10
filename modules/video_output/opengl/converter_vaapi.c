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
/* TODO ugly way to get the X11 Display via EGL. */
struct vlc_gl_sys_t
{
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    Display *x11;
};
#endif

struct priv
{
    VADisplay vadpy;
    VASurfaceID *va_surface_ids;
    PFNEGLCREATEIMAGEKHRPROC            eglCreateImageKHR;
    PFNEGLDESTROYIMAGEKHRPROC           eglDestroyImageKHR;
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
    EGLDisplay egldpy;

    video_color_space_t yuv_space;
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
vaegl_release_last_pic(vlc_object_t *o, struct priv *priv)
{
    for (unsigned i = 0; i < priv->last.va_image.num_planes; ++i)
        priv->eglDestroyImageKHR(priv->egldpy, priv->last.egl_images[i]);

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

        egl_images[i] = priv->eglCreateImageKHR(priv->egldpy, EGL_NO_CONTEXT,
                                                EGL_LINUX_DMA_BUF_EXT, NULL,
                                                attribs);
        if (egl_images[i] == NULL)
            goto error;

        glBindTexture(tc->tex_target, textures[i]);

        priv->glEGLImageTargetTexture2DOES(tc->tex_target, egl_images[i]);
    }

    if (pic != priv->last.pic)
    {
        if (priv->last.pic != NULL)
            vaegl_release_last_pic(o, priv);
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
            priv->eglDestroyImageKHR(priv->egldpy, egl_images[i]);

        vlc_vaapi_DestroyImage(o, priv->vadpy, va_image.image_id);
    }
    return VLC_EGENERIC;
}

static void
tc_vaegl_release(const opengl_tex_converter_t *tc)
{
    struct priv *priv = tc->priv;

    if (priv->last.pic != NULL)
        vaegl_release_last_pic(VLC_OBJECT(tc->gl), priv);

    vlc_vaapi_ReleaseInstance(priv->vadpy);

    free(tc->priv);
}

static GLuint
tc_vaegl_init(video_format_t *fmt, opengl_tex_converter_t *tc,
              struct priv *priv, VADisplay *vadpy)
{
#define GETPROC(x) do { \
    if ((priv->x = vlc_gl_GetProcAddress(tc->gl, #x)) == NULL) return -1; \
} while(0)

    if (vadpy == NULL)
        return 0;
    priv->vadpy = vadpy;
    priv->fourcc = 0;
    priv->yuv_space = fmt->space;

    if (!HasExtension(tc->glexts, "GL_OES_EGL_image"))
        return 0;

    void *(*func)() = vlc_gl_GetProcAddress(tc->gl, "eglGetCurrentDisplay");
    priv->egldpy = func ? func() : NULL;
    if (priv->egldpy == NULL)
        return 0;

    func = vlc_gl_GetProcAddress(tc->gl, "eglQueryString");
    const char *eglexts = func ? func(priv->egldpy, EGL_EXTENSIONS) : "";
    if (!HasExtension(eglexts, "EGL_EXT_image_dma_buf_import"))
        return 0;

    if (vaegl_init_fourcc(tc, priv, VA_FOURCC_NV12))
        return 0;

    GETPROC(eglCreateImageKHR);
    GETPROC(eglDestroyImageKHR);
    GETPROC(glEGLImageTargetTexture2DOES);

    tc->pf_update  = tc_vaegl_update;
    tc->pf_release = tc_vaegl_release;

    if (vlc_vaapi_Initialize(VLC_OBJECT(tc->gl), priv->vadpy))
        return 0;

    if (vlc_vaapi_SetInstance(priv->vadpy))
    {
        msg_Err(tc->gl, "VAAPI instance already in use");
        return 0;
    }

    return opengl_fragment_shader_init(tc, GL_TEXTURE_2D, VLC_CODEC_NV12,
                                       fmt->space);
#undef GETPROC
}

static picture_pool_t *
tc_va_get_pool(const opengl_tex_converter_t *tc, const video_format_t *fmt,
               unsigned requested_count)
{
    struct priv *priv = tc->priv;

    picture_pool_t *pool =
        vlc_vaapi_PoolNew(VLC_OBJECT(tc->gl), priv->vadpy, requested_count,
                          &priv->va_surface_ids, fmt, VA_RT_FORMAT_YUV420,
                          VA_FOURCC_NV12);
    if (!pool)
        return NULL;

    return pool;
}

GLuint
opengl_tex_converter_vaapi_init(video_format_t *fmt, opengl_tex_converter_t *tc)
{
    if (fmt->i_chroma != VLC_CODEC_VAAPI_420)
        return 0;

    struct priv *priv = tc->priv = calloc(1, sizeof(struct priv));
    if (unlikely(priv == NULL))
        return VLC_ENOMEM;

    GLuint fshader = 0;
    switch (tc->gl->surface->type)
    {
#ifdef HAVE_VA_X11
        case VOUT_WINDOW_TYPE_XID:
        {
            struct vlc_gl_sys_t *glsys = tc->gl->sys;
            fshader = tc_vaegl_init(fmt, tc, priv, vaGetDisplay(glsys->x11));
            break;
        }
#endif
#ifdef HAVE_VA_WL
        case VOUT_WINDOW_TYPE_WAYLAND:
            fshader = tc_vaegl_init(fmt, tc, priv,
                                    vaGetDisplayWl(tc->gl->surface->display.wl));
            break;
#endif
        default:
            goto error;
    }
    if (fshader == 0)
        goto error;

    tc->priv              = priv;
    tc->pf_get_pool       = tc_va_get_pool;

    return fshader;

error:
    free(priv);
    return 0;
}
