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
#include "converter.h"
#include "../android/display.h"
#include "../android/utils.h"

struct priv
{
    AWindowHandler *awh;
    const float *transform_mtx;
    bool stex_attached;

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

static int
tc_anop_allocate_textures(const opengl_tex_converter_t *tc, GLuint *textures,
                          const GLsizei *tex_width, const GLsizei *tex_height)
{
    (void) tex_width; (void) tex_height;
    struct priv *priv = tc->priv;
    assert(textures[0] != 0);
    if (SurfaceTexture_attachToGLContext(priv->awh, textures[0]) != 0)
    {
        msg_Err(tc->gl, "SurfaceTexture_attachToGLContext failed");
        return VLC_EGENERIC;
    }
    priv->stex_attached = true;
    return VLC_SUCCESS;
}

static picture_pool_t *
tc_anop_get_pool(const opengl_tex_converter_t *tc, unsigned requested_count)
{
    struct priv *priv = tc->priv;
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
        p_picsys->hw.p_surface = SurfaceTexture_getANativeWindow(priv->awh);
        p_picsys->hw.p_jsurface = SurfaceTexture_getSurface(priv->awh);
        p_picsys->hw.i_index = -1;
        vlc_mutex_init(&p_picsys->hw.lock);

        picture[count] = picture_NewFromResource(&tc->fmt, &rsc);
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

    if (SurfaceTexture_waitAndUpdateTexImage(priv->awh, &priv->transform_mtx)
        != VLC_SUCCESS)
    {
        priv->transform_mtx = NULL;
        return VLC_EGENERIC;
    }

    tc->vt->ActiveTexture(GL_TEXTURE0);
    tc->vt->BindTexture(tc->tex_target, textures[0]);

    return VLC_SUCCESS;
}

static int
tc_anop_fetch_locations(opengl_tex_converter_t *tc, GLuint program)
{
    struct priv *priv = tc->priv;
    priv->uloc.uSTMatrix = tc->vt->GetUniformLocation(program, "uSTMatrix");
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
        tc->vt->UniformMatrix4fv(priv->uloc.uSTMatrix, 1, GL_FALSE,
                                  priv->transform_mtx);
}

static void
Close(vlc_object_t *obj)
{
    opengl_tex_converter_t *tc = (void *)obj;
    struct priv *priv = tc->priv;

    if (priv->stex_attached)
        SurfaceTexture_detachFromGLContext(priv->awh);

    free(priv);
}

static int
Open(vlc_object_t *obj)
{
    opengl_tex_converter_t *tc = (void *) obj;

    if (tc->fmt.i_chroma != VLC_CODEC_ANDROID_OPAQUE
     || !tc->gl->surface->handle.anativewindow)
        return VLC_EGENERIC;

    tc->priv = malloc(sizeof(struct priv));
    if (unlikely(tc->priv == NULL))
        return VLC_ENOMEM;

    struct priv *priv = tc->priv;
    priv->awh = tc->gl->surface->handle.anativewindow;
    priv->transform_mtx = NULL;
    priv->stex_attached = false;

    tc->pf_allocate_textures = tc_anop_allocate_textures;
    tc->pf_get_pool       = tc_anop_get_pool;
    tc->pf_update         = tc_anop_update;
    tc->pf_fetch_locations = tc_anop_fetch_locations;
    tc->pf_prepare_shader = tc_anop_prepare_shader;

    tc->tex_count = 1;
    tc->texs[0] = (struct opengl_tex_cfg) { { 1, 1 }, { 1, 1 }, 0, 0, 0 };

    tc->tex_target   = GL_TEXTURE_EXTERNAL_OES;

    /* The transform Matrix (uSTMatrix) given by the SurfaceTexture is not
     * using the same origin than us. Ask the caller to rotate textures
     * coordinates, via the vertex shader, by forcing an orientation. */
    switch (tc->fmt.orientation)
    {
        case ORIENT_TOP_LEFT:
            tc->fmt.orientation = ORIENT_BOTTOM_LEFT;
            break;
        case ORIENT_TOP_RIGHT:
            tc->fmt.orientation = ORIENT_BOTTOM_RIGHT;
            break;
        case ORIENT_BOTTOM_LEFT:
            tc->fmt.orientation = ORIENT_TOP_LEFT;
            break;
        case ORIENT_BOTTOM_RIGHT:
            tc->fmt.orientation = ORIENT_TOP_RIGHT;
            break;
        case ORIENT_LEFT_TOP:
            tc->fmt.orientation = ORIENT_RIGHT_TOP;
            break;
        case ORIENT_LEFT_BOTTOM:
            tc->fmt.orientation = ORIENT_RIGHT_BOTTOM;
            break;
        case ORIENT_RIGHT_TOP:
            tc->fmt.orientation = ORIENT_LEFT_TOP;
            break;
        case ORIENT_RIGHT_BOTTOM:
            tc->fmt.orientation = ORIENT_LEFT_BOTTOM;
            break;
    }

    static const char *template =
        "#version %u\n"
        "#extension GL_OES_EGL_image_external : require\n"
        "%s" /* precision */
        "varying vec2 TexCoord0;"
        "uniform samplerExternalOES sTexture;"
        "uniform mat4 uSTMatrix;"
        "void main()"
        "{ "
        "  gl_FragColor = texture2D(sTexture, (uSTMatrix * vec4(TexCoord0, 1, 1)).xy);"
        "}";

    char *code;
    if (asprintf(&code, template, tc->glsl_version, tc->glsl_precision_header) < 0)
        return 0;
    GLuint fragment_shader = tc->vt->CreateShader(GL_FRAGMENT_SHADER);
    tc->vt->ShaderSource(fragment_shader, 1, (const char **) &code, NULL);
    tc->vt->CompileShader(fragment_shader);
    tc->fshader = fragment_shader;
    free(code);

    return VLC_SUCCESS;
}

vlc_module_begin ()
    set_description("Android OpenGL SurfaceTexture converter")
    set_capability("glconv", 1)
    set_callbacks(Open, Close)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
vlc_module_end ()
