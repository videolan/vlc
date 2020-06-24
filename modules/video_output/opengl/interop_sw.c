/*****************************************************************************
 * converter_sw.c: OpenGL converters for software video formats
 *****************************************************************************
 * Copyright (C) 2016,2017 VLC authors and VideoLAN
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
#include <limits.h>
#include <stdlib.h>

#include <vlc_common.h>
#include "gl_api.h"
#include "internal.h"

#define PBO_DISPLAY_COUNT 2 /* Double buffering */
typedef struct
{
    PFNGLDELETEBUFFERSPROC DeleteBuffers;
    GLuint      buffers[PICTURE_PLANE_MAX];
    size_t      bytes[PICTURE_PLANE_MAX];
} picture_sys_t;

struct priv
{
    bool   has_unpack_subimage;
    void * texture_temp_buf;
    size_t texture_temp_buf_size;
    struct {
        picture_t *display_pics[PBO_DISPLAY_COUNT];
        size_t display_idx;
    } pbo;
};

static void
pbo_picture_destroy(picture_t *pic)
{
    picture_sys_t *picsys = pic->p_sys;

    picsys->DeleteBuffers(pic->i_planes, picsys->buffers);

    free(picsys);
}

static picture_t *
pbo_picture_create(const struct vlc_gl_interop *interop)
{
    picture_sys_t *picsys = calloc(1, sizeof(*picsys));
    if (unlikely(picsys == NULL))
        return NULL;

    picture_resource_t rsc = {
        .p_sys = picsys,
        .pf_destroy = pbo_picture_destroy,
    };
    picture_t *pic = picture_NewFromResource(&interop->fmt_out, &rsc);
    if (pic == NULL)
    {
        free(picsys);
        return NULL;
    }

    interop->vt->GenBuffers(pic->i_planes, picsys->buffers);
    picsys->DeleteBuffers = interop->vt->DeleteBuffers;

    /* XXX: needed since picture_NewFromResource override pic planes */
    if (picture_Setup(pic, &interop->fmt_out))
    {
        picture_Release(pic);
        return NULL;
    }

    assert(pic->i_planes > 0
        && (unsigned) pic->i_planes == interop->tex_count);

    for (int i = 0; i < pic->i_planes; ++i)
    {
        const plane_t *p = &pic->p[i];

        if( p->i_pitch < 0 || p->i_lines <= 0 ||
            (size_t)p->i_pitch > SIZE_MAX/p->i_lines )
        {
            picture_Release(pic);
            return NULL;
        }
        picsys->bytes[i] = p->i_pitch * p->i_lines;
    }
    return pic;
}

static int
pbo_data_alloc(const struct vlc_gl_interop *interop, picture_t *pic)
{
    picture_sys_t *picsys = pic->p_sys;

    interop->vt->GetError();

    for (int i = 0; i < pic->i_planes; ++i)
    {
        interop->vt->BindBuffer(GL_PIXEL_UNPACK_BUFFER, picsys->buffers[i]);
        interop->vt->BufferData(GL_PIXEL_UNPACK_BUFFER, picsys->bytes[i], NULL,
                                GL_DYNAMIC_DRAW);

        if (interop->vt->GetError() != GL_NO_ERROR)
        {
            msg_Err(interop->gl, "could not alloc PBO buffers");
            interop->vt->DeleteBuffers(i, picsys->buffers);
            return VLC_EGENERIC;
        }
    }
    return VLC_SUCCESS;
}

static int
pbo_pics_alloc(const struct vlc_gl_interop *interop)
{
    struct priv *priv = interop->priv;
    for (size_t i = 0; i < PBO_DISPLAY_COUNT; ++i)
    {
        picture_t *pic = priv->pbo.display_pics[i] =
            pbo_picture_create(interop);
        if (pic == NULL)
            goto error;

        if (pbo_data_alloc(interop, pic) != VLC_SUCCESS)
            goto error;
    }

    /* turn off pbo */
    interop->vt->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    return VLC_SUCCESS;
error:
    for (size_t i = 0; i < PBO_DISPLAY_COUNT && priv->pbo.display_pics[i]; ++i)
        picture_Release(priv->pbo.display_pics[i]);
    return VLC_EGENERIC;
}

static int
tc_pbo_update(const struct vlc_gl_interop *interop, GLuint *textures,
              const GLsizei *tex_width, const GLsizei *tex_height,
              picture_t *pic, const size_t *plane_offset)
{
    (void) plane_offset; assert(plane_offset == NULL);
    struct priv *priv = interop->priv;

    picture_t *display_pic = priv->pbo.display_pics[priv->pbo.display_idx];
    picture_sys_t *p_sys = display_pic->p_sys;
    priv->pbo.display_idx = (priv->pbo.display_idx + 1) % PBO_DISPLAY_COUNT;

    for (int i = 0; i < pic->i_planes; i++)
    {
        GLsizeiptr size = pic->p[i].i_lines * pic->p[i].i_pitch;
        const GLvoid *data = pic->p[i].p_pixels;
        interop->vt->BindBuffer(GL_PIXEL_UNPACK_BUFFER,
                           p_sys->buffers[i]);
        interop->vt->BufferSubData(GL_PIXEL_UNPACK_BUFFER, 0, size, data);

        interop->vt->ActiveTexture(GL_TEXTURE0 + i);
        interop->vt->BindTexture(interop->tex_target, textures[i]);

        interop->vt->PixelStorei(GL_UNPACK_ROW_LENGTH, pic->p[i].i_pitch
            * tex_width[i] / (pic->p[i].i_visible_pitch ? pic->p[i].i_visible_pitch : 1));

        interop->vt->TexSubImage2D(interop->tex_target, 0, 0, 0, tex_width[i], tex_height[i],
                                   interop->texs[i].format, interop->texs[i].type, NULL);
        interop->vt->PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }

    /* turn off pbo */
    interop->vt->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    return VLC_SUCCESS;
}

static int
tc_common_allocate_textures(const struct vlc_gl_interop *interop, GLuint *textures,
                            const GLsizei *tex_width, const GLsizei *tex_height)
{
    for (unsigned i = 0; i < interop->tex_count; i++)
    {
        interop->vt->BindTexture(interop->tex_target, textures[i]);
        interop->vt->TexImage2D(interop->tex_target, 0, interop->texs[i].internal,
                                tex_width[i], tex_height[i], 0, interop->texs[i].format,
                                interop->texs[i].type, NULL);
    }
    return VLC_SUCCESS;
}

static int
upload_plane(const struct vlc_gl_interop *interop, unsigned tex_idx,
             GLsizei width, GLsizei height,
             unsigned pitch, unsigned visible_pitch, const void *pixels)
{
    struct priv *priv = interop->priv;
    GLenum tex_format = interop->texs[tex_idx].format;
    GLenum tex_type = interop->texs[tex_idx].type;

    /* This unpack alignment is the default, but setting it just in case. */
    interop->vt->PixelStorei(GL_UNPACK_ALIGNMENT, 4);

    if (!priv->has_unpack_subimage)
    {
        if (pitch != visible_pitch)
        {
            visible_pitch = vlc_align(visible_pitch, 4);
            size_t buf_size = visible_pitch * height;
            const uint8_t *source = pixels;
            uint8_t *destination;
            if (priv->texture_temp_buf_size < buf_size)
            {
                priv->texture_temp_buf =
                    realloc_or_free(priv->texture_temp_buf, buf_size);
                if (priv->texture_temp_buf == NULL)
                {
                    priv->texture_temp_buf_size = 0;
                    return VLC_ENOMEM;
                }
                priv->texture_temp_buf_size = buf_size;
            }
            destination = priv->texture_temp_buf;

            for (GLsizei h = 0; h < height ; h++)
            {
                memcpy(destination, source, visible_pitch);
                source += pitch;
                destination += visible_pitch;
            }
            interop->vt->TexSubImage2D(interop->tex_target, 0, 0, 0, width, height,
                                       tex_format, tex_type, priv->texture_temp_buf);
        }
        else
        {
            interop->vt->TexSubImage2D(interop->tex_target, 0, 0, 0, width, height,
                                       tex_format, tex_type, pixels);
        }
    }
    else
    {
        interop->vt->PixelStorei(GL_UNPACK_ROW_LENGTH, pitch * width / (visible_pitch ? visible_pitch : 1));
        interop->vt->TexSubImage2D(interop->tex_target, 0, 0, 0, width, height,
                                   tex_format, tex_type, pixels);
        interop->vt->PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }
    return VLC_SUCCESS;
}

static int
tc_common_update(const struct vlc_gl_interop *interop, GLuint *textures,
                 const GLsizei *tex_width, const GLsizei *tex_height,
                 picture_t *pic, const size_t *plane_offset)
{
    int ret = VLC_SUCCESS;
    for (unsigned i = 0; i < interop->tex_count && ret == VLC_SUCCESS; i++)
    {
        assert(textures[i] != 0);
        interop->vt->ActiveTexture(GL_TEXTURE0 + i);
        interop->vt->BindTexture(interop->tex_target, textures[i]);
        const void *pixels = plane_offset != NULL ?
                             &pic->p[i].p_pixels[plane_offset[i]] :
                             pic->p[i].p_pixels;

        ret = upload_plane(interop, i, tex_width[i], tex_height[i],
                           pic->p[i].i_pitch, pic->p[i].i_visible_pitch, pixels);
    }
    return ret;
}

void
opengl_interop_generic_deinit(struct vlc_gl_interop *interop)
{
    struct priv *priv = interop->priv;
    for (size_t i = 0; i < PBO_DISPLAY_COUNT && priv->pbo.display_pics[i]; ++i)
        picture_Release(priv->pbo.display_pics[i]);
    free(priv->texture_temp_buf);
    free(priv);
}

int
opengl_interop_generic_init(struct vlc_gl_interop *interop, bool allow_dr)
{
    video_color_space_t space;
    const vlc_fourcc_t *list;

    if (vlc_fourcc_IsYUV(interop->fmt_in.i_chroma))
    {
        GLint max_texture_units = 0;
        interop->vt->GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_units);
        if (max_texture_units < 3)
            return VLC_EGENERIC;

        list = vlc_fourcc_GetYUVFallback(interop->fmt_in.i_chroma);
        space = interop->fmt_in.space;
    }
    else if (interop->fmt_in.i_chroma == VLC_CODEC_XYZ12)
    {
        static const vlc_fourcc_t xyz12_list[] = { VLC_CODEC_XYZ12, 0 };
        list = xyz12_list;
        space = COLOR_SPACE_UNDEF;
    }
    else
    {
        list = vlc_fourcc_GetRGBFallback(interop->fmt_in.i_chroma);
        space = COLOR_SPACE_UNDEF;
    }

    /* The pictures are uploaded upside-down */
    video_format_TransformBy(&interop->fmt_out, TRANSFORM_VFLIP);

    int ret = VLC_EGENERIC;
    while (*list)
    {
        ret = opengl_interop_init(interop, GL_TEXTURE_2D, *list, space);
        if (ret == VLC_SUCCESS)
        {
            interop->fmt_out.i_chroma = *list;

            if (interop->fmt_out.i_chroma == VLC_CODEC_RGB32)
            {
#if defined(WORDS_BIGENDIAN)
                interop->fmt_out.i_rmask  = 0xff000000;
                interop->fmt_out.i_gmask  = 0x00ff0000;
                interop->fmt_out.i_bmask  = 0x0000ff00;
#else
                interop->fmt_out.i_rmask  = 0x000000ff;
                interop->fmt_out.i_gmask  = 0x0000ff00;
                interop->fmt_out.i_bmask  = 0x00ff0000;
#endif
                video_format_FixRgb(&interop->fmt_out);
            }
            break;
        }
        list++;
    }
    if (ret != VLC_SUCCESS)
        return ret;

    struct priv *priv = interop->priv = calloc(1, sizeof(struct priv));
    if (unlikely(priv == NULL))
        return VLC_ENOMEM;

    static const struct vlc_gl_interop_ops ops = {
        .allocate_textures = tc_common_allocate_textures,
        .update_textures = tc_common_update,
        .close = opengl_interop_generic_deinit,
    };
    interop->ops = &ops;

    /* OpenGL or OpenGL ES2 with GL_EXT_unpack_subimage ext */
    priv->has_unpack_subimage =
        !interop->api->is_gles || vlc_gl_StrHasToken(interop->api->extensions, "GL_EXT_unpack_subimage");

    if (allow_dr && priv->has_unpack_subimage)
    {
        /* Ensure we do direct rendering / PBO with OpenGL 3.0 or higher. */
        const unsigned char *ogl_version = interop->vt->GetString(GL_VERSION);
        const bool glver_ok = strverscmp((const char *)ogl_version, "3.0") >= 0;

        const bool has_pbo = glver_ok &&
            (vlc_gl_StrHasToken(interop->api->extensions, "GL_ARB_pixel_buffer_object") ||
             vlc_gl_StrHasToken(interop->api->extensions, "GL_EXT_pixel_buffer_object"));

        const bool supports_pbo = has_pbo && interop->vt->BufferData
            && interop->vt->BufferSubData;
        if (supports_pbo && pbo_pics_alloc(interop) == VLC_SUCCESS)
        {
            static const struct vlc_gl_interop_ops pbo_ops = {
                .allocate_textures = tc_common_allocate_textures,
                .update_textures = tc_pbo_update,
                .close = opengl_interop_generic_deinit,
            };
            interop->ops = &pbo_ops;
            msg_Dbg(interop->gl, "PBO support enabled");
        }
    }

    return VLC_SUCCESS;
}
