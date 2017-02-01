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

    struct {
        GLint uSTMatrix;
    } uloc;
};

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
                 unsigned requested_count, GLuint *textures)
{
    struct priv *priv = tc->priv;
    assert(textures[0] != 0);
    priv->stex = SurfaceTexture_create(tc->parent, textures[0]);
    if (priv->stex == NULL)
    {
        msg_Err(tc->parent, "tc_anop_get_pool: SurfaceTexture_create failed");
        return NULL;
    }

#define FORCED_COUNT 31
    requested_count = FORCED_COUNT;
    picture_t *picture[FORCED_COUNT] = {NULL, };
    unsigned count;

    for (count = 0; count < requested_count; count++)
    {
        picture_sys_t *p_picsys = calloc(1, sizeof(*p_picsys));
        if (unlikely(p_picsys == NULL))
            goto error;
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
            goto error;
        }
    }

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
    for (unsigned i = 0; i < count; i++)
        picture_Release(picture[i]);
    SurfaceTexture_release(priv->stex);
    return NULL;
}

static int
tc_anop_update(const opengl_tex_converter_t *tc, GLuint *textures,
               const GLsizei *tex_width, const GLsizei *tex_height,
               picture_t *pic, const size_t *plane_offset)
{
    (void) tex_width; (void) tex_height; (void) plane_offset;
    assert(textures[0] != 0);

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

static int
tc_anop_fetch_locations(opengl_tex_converter_t *tc, GLuint program)
{
    struct priv *priv = tc->priv;
    priv->uloc.uSTMatrix = tc->api->GetUniformLocation(program, "uSTMatrix");
    return priv->uloc.uSTMatrix != -1 ? VLC_SUCCESS : VLC_EGENERIC;
}

static void
tc_anop_prepare_shader(const opengl_tex_converter_t *tc,
                       const GLsizei *tex_width, const GLsizei *tex_height,
                       float alpha)
{
    (void) tex_width; (void) tex_height; (void) alpha;
    struct priv *priv = tc->priv;
    if (priv->transform_mtx != NULL)
        tc->api->UniformMatrix4fv(priv->uloc.uSTMatrix, 1, GL_FALSE,
                                  priv->transform_mtx);
}

static void
tc_anop_release(const opengl_tex_converter_t *tc)
{
    struct priv *priv = tc->priv;
    if (priv->stex != NULL)
        SurfaceTexture_release(priv->stex);

    free(priv);
}

GLuint
opengl_tex_converter_anop_init(const video_format_t *fmt,
                               opengl_tex_converter_t *tc)
{
    if (fmt->i_chroma != VLC_CODEC_ANDROID_OPAQUE)
        return 0;

    tc->priv = malloc(sizeof(struct priv));
    if (unlikely(tc->priv == NULL))
        return 0;

    struct priv *priv = tc->priv;
    priv->stex = NULL;
    priv->transform_mtx = NULL;

    tc->pf_get_pool       = tc_anop_get_pool;
    tc->pf_update         = tc_anop_update;
    tc->pf_fetch_locations = tc_anop_fetch_locations;
    tc->pf_prepare_shader = tc_anop_prepare_shader;
    tc->pf_release        = tc_anop_release;

    tc->tex_count = 1;
    tc->texs[0] = (struct opengl_tex_cfg) { { 1, 1 }, { 1, 1 } };

    tc->chroma       = VLC_CODEC_ANDROID_OPAQUE;
    tc->tex_target   = GL_TEXTURE_EXTERNAL_OES;

    /* The transform Matrix (uSTMatrix) given by the SurfaceTexture is not
     * using the same origin than us. Ask the caller to rotate textures
     * coordinates, via the vertex shader, by forcing an orientation. */
    switch (tc->orientation)
    {
        case ORIENT_TOP_LEFT:
            tc->orientation = ORIENT_BOTTOM_LEFT;
            break;
        case ORIENT_TOP_RIGHT:
            tc->orientation = ORIENT_BOTTOM_RIGHT;
            break;
        case ORIENT_BOTTOM_LEFT:
            tc->orientation = ORIENT_TOP_LEFT;
            break;
        case ORIENT_BOTTOM_RIGHT:
            tc->orientation = ORIENT_TOP_RIGHT;
            break;
        case ORIENT_LEFT_TOP:
            tc->orientation = ORIENT_RIGHT_TOP;
            break;
        case ORIENT_LEFT_BOTTOM:
            tc->orientation = ORIENT_RIGHT_BOTTOM;
            break;
        case ORIENT_RIGHT_TOP:
            tc->orientation = ORIENT_LEFT_TOP;
            break;
        case ORIENT_RIGHT_BOTTOM:
            tc->orientation = ORIENT_LEFT_BOTTOM;
            break;
    }

    static const char *code =
        "#version " GLSL_VERSION "\n"
        "#extension GL_OES_EGL_image_external : require\n"
        PRECISION
        "varying vec2 TexCoord0;"
        "uniform samplerExternalOES sTexture;"
        "uniform mat4 uSTMatrix;"
        "void main()"
        "{ "
        "  gl_FragColor = texture2D(sTexture, (uSTMatrix * vec4(TexCoord0, 1, 1)).xy);"
        "}";
    GLuint fragment_shader = tc->api->CreateShader(GL_FRAGMENT_SHADER);
    tc->api->ShaderSource(fragment_shader, 1, &code, NULL);
    tc->api->CompileShader(fragment_shader);

    return fragment_shader;
}
