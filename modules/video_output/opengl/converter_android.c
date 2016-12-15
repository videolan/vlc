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

#include <GLES2/gl2ext.h>
#include "internal.h"
#include "../android/display.h"
#include "../android/utils.h"

struct priv
{
    SurfaceTexture *stex;
    const float *transform_mtx;
};

static int
tc_anop_gen_textures(const opengl_tex_converter_t *tc,
                     const GLsizei *tex_width, const GLsizei *tex_height,
                     GLuint *textures)
{
    (void) tex_width; (void) tex_height;

    glActiveTexture(GL_TEXTURE0);
    glClientActiveTexture(GL_TEXTURE0);

    glGenTextures(1, textures);
    glBindTexture(tc->tex_target, textures[0]);

    glTexParameteri(tc->tex_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(tc->tex_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(tc->tex_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(tc->tex_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return VLC_SUCCESS;
}

static void
tc_anop_del_textures(const opengl_tex_converter_t *tc, const GLuint *textures)
{
    (void) tc;
    glDeleteTextures(1, textures);
}

static int
pool_lock_pic(picture_t *p_pic)
{
    picture_sys_t *p_picsys = p_pic->p_sys;

    p_picsys->b_locked = true;
    return 0;
}

static void
pool_unlock_pic(picture_t *p_pic)
{
    picture_sys_t *p_picsys = p_pic->p_sys;
    if (p_picsys->b_locked)
    {
        AndroidOpaquePicture_Release(p_picsys, false);
        p_picsys->b_locked  = false;
    }
}

static picture_pool_t *
tc_anop_get_pool(const opengl_tex_converter_t *tc, const video_format_t *fmt,
                 unsigned requested_count, const GLuint *textures)
{
    struct priv *priv = tc->priv;
    priv->stex = SurfaceTexture_create(tc->parent, textures[0]);
    if (priv->stex == NULL)
        return NULL;

#define FORCED_COUNT 31
    requested_count = FORCED_COUNT;
    picture_t *picture[FORCED_COUNT] = {NULL, };
    unsigned count;

    for (count = 0; count < requested_count; count++)
    {
        picture_sys_t *p_picsys = calloc(1, sizeof(*p_picsys));
        if (unlikely(p_picsys == NULL))
            break;
        picture_resource_t rsc = {
            .p_sys = p_picsys,
            .pf_destroy = AndroidOpaquePicture_DetachVout,
        };

        p_picsys->hw.b_vd_ref = true;
        p_picsys->hw.p_surface = SurfaceTexture_getANativeWindow(priv->stex);
        p_picsys->hw.p_jsurface = SurfaceTexture_getSurface(priv->stex);
        p_picsys->hw.i_index = -1;
        vlc_mutex_init(&p_picsys->hw.lock);

        picture[count] = picture_NewFromResource(fmt, &rsc);
        if (!picture[count])
        {
            free(p_picsys);
            break;
        }
    }
    if (count <= 0)
        goto error;

    /* Wrap the pictures into a pool */
    picture_pool_configuration_t pool_cfg = {
        .picture_count = requested_count,
        .picture       = picture,
        .lock          = pool_lock_pic,
        .unlock        = pool_unlock_pic,
    };
    picture_pool_t *pool = picture_pool_NewExtended(&pool_cfg);
    if (!pool)
        goto error;

    return pool;
error:
    SurfaceTexture_release(priv->stex);
    return NULL;
}

static int
tc_anop_update(const opengl_tex_converter_t *tc, const GLuint *textures,
               unsigned width, unsigned height,
               const picture_t *pic, const size_t *plane_offset)
{
    (void) width; (void) height; (void) plane_offset;

    if (plane_offset != NULL)
        return VLC_EGENERIC;

    if (!pic->p_sys->b_locked)
        return VLC_SUCCESS;

    struct priv *priv = tc->priv;

    AndroidOpaquePicture_Release(pic->p_sys, true);

    if (SurfaceTexture_waitAndUpdateTexImage(priv->stex, &priv->transform_mtx)
        != VLC_SUCCESS)
    {
        priv->transform_mtx = NULL;
        return VLC_EGENERIC;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(tc->tex_target, textures[0]);

    return VLC_SUCCESS;
}

static void
tc_anop_prepare_shader(const opengl_tex_converter_t *tc,
                       GLuint program, float alpha)
{
    (void) alpha;
    struct priv *priv = tc->priv;
    if (priv->transform_mtx != NULL)
    {
        GLint handle = tc->api->GetUniformLocation(program, "uSTMatrix");
        tc->api->UniformMatrix4fv(handle, 1, GL_FALSE, priv->transform_mtx);
    }
}

static void
tc_anop_release(const opengl_tex_converter_t *tc)
{
    tc->api->DeleteShader(tc->fragment_shader);

    struct priv *priv = tc->priv;
    if (priv->stex != NULL)
        SurfaceTexture_release(priv->stex);

    free(priv);
}

int
opengl_tex_converter_anop_init(const video_format_t *fmt,
                               opengl_tex_converter_t *tc)
{
    if (fmt->i_chroma != VLC_CODEC_ANDROID_OPAQUE)
        return VLC_EGENERIC;

    tc->priv = malloc(sizeof(struct priv));
    if (unlikely(tc->priv == NULL))
        return VLC_ENOMEM;

    struct priv *priv = tc->priv;
    priv->stex = NULL;
    priv->transform_mtx = NULL;

    tc->pf_gen_textures   = tc_anop_gen_textures;
    tc->pf_del_textures   = tc_anop_del_textures;
    tc->pf_get_pool       = tc_anop_get_pool;
    tc->pf_update         = tc_anop_update;
    tc->pf_prepare_shader = tc_anop_prepare_shader;
    tc->pf_release        = tc_anop_release;

    /* fake plane_count to 1 */
    static const vlc_chroma_description_t desc = {
        .plane_count = 1,
        .p[0].w.num = 1,
        .p[0].w.den = 1,
        .p[0].h.num = 1,
        .p[0].h.den = 1,
        .pixel_size = 0,
        .pixel_bits = 0,
    };

    tc->chroma       = VLC_CODEC_ANDROID_OPAQUE;
    tc->desc         = &desc;
    tc->tex_target   = GL_TEXTURE_EXTERNAL_OES;
    tc->orientation  = ORIENT_VFLIPPED;

    static const char *code =
        "#version " GLSL_VERSION "\n"
        "#extension GL_OES_EGL_image_external : require\n"
        PRECISION
        "varying vec4 TexCoord0;"
        "uniform samplerExternalOES sTexture;"
        "uniform mat4 uSTMatrix;"
        "void main()"
        "{ "
        "  gl_FragColor = texture2D(sTexture, (uSTMatrix * TexCoord0).xy);"
        "}";
    tc->fragment_shader = tc->api->CreateShader(GL_FRAGMENT_SHADER);
    tc->api->ShaderSource(tc->fragment_shader, 1, &code, NULL);
    tc->api->CompileShader(tc->fragment_shader);

    return VLC_SUCCESS;
}
