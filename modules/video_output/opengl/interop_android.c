/*****************************************************************************
 * converter_android.c: OpenGL Android opaque converter
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
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

struct priv
{
    float mtx_2x3[2*3];
    const float *transform_mtx;

    bool stex_attached;
    struct vlc_asurfacetexture *previous_texture;
    picture_t *current_picture;

    struct {
        PFNGLACTIVETEXTUREPROC ActiveTexture;
        PFNGLBINDTEXTUREPROC BindTexture;
        PFNGLGENTEXTURESPROC GenTextures;
    } gl;
};

static void
ReductMatrix(float *mtx_2x3, const float *mtx_4x4)
{
    /*
     * The transform matrix provided by Android is 4x4:
     * <https://developer.android.com/reference/android/graphics/SurfaceTexture#getTransformMatrix(float%5B%5D)>
     *
     * However, the third column is never used, since the input vector is in
     * the form (s, t, 0, 1). Similarly, the third row is never used either,
     * since only the two first coordinates of the output vector are kept.
     *
     *       mat_4x4        mat_2x3
     *
     *     / a b . c \
     *     | d e . f | --> / a b c \
     *     | . . . . |     \ d e f /
     *     \ . . . . /
     */

#define MTX4(ROW,COL) mtx_4x4[(COL)*4 + (ROW)]
#define MTX3(ROW,COL) mtx_2x3[(COL)*2 + (ROW)]
    MTX3(0,0) = MTX4(0,0); // a
    MTX3(0,1) = MTX4(0,1); // b
    MTX3(0,2) = MTX4(0,3); // c
    MTX3(1,0) = MTX4(1,0); // d
    MTX3(1,1) = MTX4(1,1); // e
    MTX3(1,2) = MTX4(1,3); // f
#undef MTX4
#undef MTX3
}

static int
tc_anop_allocate_textures(const struct vlc_gl_interop *interop, uint32_t textures[],
                          const int32_t tex_width[], const int32_t tex_height[])
{
    (void) tex_width; (void) tex_height;
    struct priv *priv = interop->priv;
    assert(textures[0] != 0);

    return VLC_SUCCESS;
}

static int
tc_anop_update(const struct vlc_gl_interop *interop, uint32_t textures[],
               const int32_t tex_width[], const int32_t tex_height[],
               picture_t *pic, const size_t plane_offset[])
{
    struct priv *priv = interop->priv;

    (void) tex_width; (void) tex_height; (void) plane_offset;
    assert(pic->context);
    assert(textures[0] != 0);

    picture_t *previous_picture = priv->current_picture;
    priv->current_picture = picture_Hold(pic);

    struct vlc_video_context *vctx = pic->context->vctx;
    android_video_context_t *avctx =
        vlc_video_context_GetPrivate(vctx, VLC_VIDEO_CONTEXT_AWINDOW);
    if (avctx == NULL)
        goto error;

    if (plane_offset != NULL)
        goto error;

    struct vlc_asurfacetexture *texture =
        avctx->get_texture(pic->context);

    struct vlc_asurfacetexture *previous_texture = priv->previous_texture;

    if (previous_texture != texture)
    {
        if (previous_texture != NULL)
        {
            SurfaceTexture_detachFromGLContext(previous_texture);
            /* SurfaceTexture_detachFromGLContext will destroy the previous
             * texture name, so we need to regenerate it. */
            priv->gl.GenTextures(1, &textures[0]);
        }

        if (SurfaceTexture_attachToGLContext(texture, textures[0]) != 0)
            goto error;

        priv->stex_attached = true;
        priv->previous_texture = texture;
    }

    if (avctx->render && !avctx->render(pic->context))
        goto success; /* already rendered */

    /* Release previous image */
    if (previous_texture && previous_texture != texture)
        SurfaceTexture_releaseTexImage(previous_texture);

    const float *mtx_4x4;
    if (SurfaceTexture_updateTexImage(texture, &mtx_4x4)
        != VLC_SUCCESS)
    {
        priv->transform_mtx = NULL;
        goto error;
    }

    ReductMatrix(priv->mtx_2x3, mtx_4x4);
    priv->transform_mtx = priv->mtx_2x3;

    priv->gl.ActiveTexture(GL_TEXTURE0);
    priv->gl.BindTexture(interop->tex_target, textures[0]);

success:
    if (previous_picture)
        picture_Release(previous_picture);
    return VLC_SUCCESS;

error:
    if (previous_picture)
        picture_Release(previous_picture);
    return VLC_EGENERIC;
}

static const float *
tc_get_transform_matrix(const struct vlc_gl_interop *interop)
{
    struct priv *priv = interop->priv;
    return priv->transform_mtx;
}

static void
Close(struct vlc_gl_interop *interop)
{
    struct priv *priv = interop->priv;

    if (priv->previous_texture)
        SurfaceTexture_detachFromGLContext(priv->previous_texture);

    if (priv->current_picture)
        picture_Release(priv->current_picture);

    free(priv);
}

static int
Open(vlc_object_t *obj)
{
    struct vlc_gl_interop *interop = (void *) obj;

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

    if (avctx == NULL || (avctx->texture == NULL && avctx->get_texture == NULL))
        return VLC_EGENERIC;

    interop->priv = malloc(sizeof(struct priv));
    if (unlikely(interop->priv == NULL))
        return VLC_ENOMEM;

    struct priv *priv = interop->priv;
    priv->transform_mtx = NULL;
    priv->current_picture = NULL;
    priv->previous_texture = NULL;
    priv->stex_attached = false;

#define LOAD_SYMBOL(name) \
    priv->gl.name = vlc_gl_GetProcAddress(interop->gl, "gl" # name); \
    assert(priv->gl.name != NULL);

    LOAD_SYMBOL(ActiveTexture);
    LOAD_SYMBOL(BindTexture);
    LOAD_SYMBOL(GenTextures);

    static const struct vlc_gl_interop_ops ops = {
        .allocate_textures = tc_anop_allocate_textures,
        .update_textures = tc_anop_update,
        .get_transform_matrix = tc_get_transform_matrix,
        .close = Close,
    };
    interop->ops = &ops;

    interop->tex_target = GL_TEXTURE_EXTERNAL_OES;
    interop->fmt_out.i_chroma = VLC_CODEC_RGB32;
    interop->fmt_out.space = COLOR_SPACE_UNDEF;

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
    set_description("Android OpenGL SurfaceTexture converter")
    set_capability("glinterop", 1)
    set_callback(Open)
    set_subcategory(SUBCAT_VIDEO_VOUT)
vlc_module_end ()
