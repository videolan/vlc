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

struct priv
{
    const float *transform_mtx;
    bool stex_attached;
    struct vlc_asurfacetexture *previous_texture;
    picture_t *current_picture;
};

static int
tc_anop_allocate_textures(const struct vlc_gl_interop *interop, GLuint *textures,
                          const GLsizei *tex_width, const GLsizei *tex_height)
{
    (void) tex_width; (void) tex_height;
    struct priv *priv = interop->priv;
    assert(textures[0] != 0);

    return VLC_SUCCESS;
}

static int
tc_anop_update(struct vlc_gl_interop *interop, GLuint *textures,
               const GLsizei *tex_width, const GLsizei *tex_height,
               picture_t *pic, const size_t *plane_offset)
{
    struct priv *priv = interop->priv;

    (void) tex_width; (void) tex_height; (void) plane_offset;
    assert(pic->context);
    assert(textures[0] != 0);

    if (priv->current_picture)
        picture_Release(priv->current_picture);
    priv->current_picture = picture_Hold(pic);

    struct vlc_video_context *vctx = pic->context->vctx;
    android_video_context_t *avctx =
        vlc_video_context_GetPrivate(vctx, VLC_VIDEO_CONTEXT_AWINDOW);
    if (avctx == NULL)
        return VLC_EGENERIC;

    if (plane_offset != NULL)
        return VLC_EGENERIC;

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
            interop->api->vt.GenTextures(1, &textures[0]);
        }

        if (SurfaceTexture_attachToGLContext(texture, textures[0]) != 0)
            return VLC_EGENERIC;

        priv->stex_attached = true;
        priv->previous_texture = texture;
    }

    if (!avctx->render(pic->context))
        return VLC_SUCCESS; /* already rendered */

    /* Release previous image */
    if (previous_texture && previous_texture != texture)
        SurfaceTexture_releaseTexImage(previous_texture);

    if (SurfaceTexture_updateTexImage(texture, &priv->transform_mtx)
        != VLC_SUCCESS)
    {
        priv->transform_mtx = NULL;
        return VLC_EGENERIC;
    }

    interop->vt->ActiveTexture(GL_TEXTURE0);
    interop->vt->BindTexture(interop->tex_target, textures[0]);

    return VLC_SUCCESS;
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

    const char *extensions = interop->api->vt.GetString(GL_EXTENSIONS);
    if (!vlc_gl_StrHasToken(extensions, "GL_OES_EGL_image_external"))
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

    static const struct vlc_gl_interop_ops ops = {
        .allocate_textures = tc_anop_allocate_textures,
        .update_textures = tc_anop_update,
        .get_transform_matrix = tc_get_transform_matrix,
        .close = Close,
    };
    interop->ops = &ops;

    int ret = opengl_interop_init(interop, GL_TEXTURE_EXTERNAL_OES,
                                  VLC_CODEC_RGB32,
                                  COLOR_SPACE_UNDEF);

    if (ret != VLC_SUCCESS)
    {
        free(priv);
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

vlc_module_begin ()
    set_description("Android OpenGL SurfaceTexture converter")
    set_capability("glinterop", 1)
    set_callback(Open)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
vlc_module_end ()
