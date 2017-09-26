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
#include <vlc_memory.h>
#include "internal.h"

#ifndef GL_UNPACK_ROW_LENGTH
# define GL_UNPACK_ROW_LENGTH 0x0CF2
#endif

#ifndef GL_PIXEL_UNPACK_BUFFER
# define GL_PIXEL_UNPACK_BUFFER 0x88EC
#endif
#ifndef GL_DYNAMIC_DRAW
# define GL_DYNAMIC_DRAW 0x88E8
#endif

#ifndef GL_MAP_READ_BIT
# define GL_MAP_READ_BIT 0x0001
#endif
#ifndef GL_MAP_WRITE_BIT
# define GL_MAP_WRITE_BIT 0x0002
#endif
#ifndef GL_MAP_FLUSH_EXPLICIT_BIT
#define GL_MAP_FLUSH_EXPLICIT_BIT 0x0010
#endif
#ifndef GL_MAP_PERSISTENT_BIT
# define GL_MAP_PERSISTENT_BIT 0x0040
#endif

#ifndef GL_CLIENT_STORAGE_BIT
# define GL_CLIENT_STORAGE_BIT 0x0200
#endif

#ifndef GL_ALREADY_SIGNALED
# define GL_ALREADY_SIGNALED 0x911A
#endif
#ifndef GL_CONDITION_SATISFIED
# define GL_CONDITION_SATISFIED 0x911C
#endif
#ifndef GL_SYNC_GPU_COMMANDS_COMPLETE
# define GL_SYNC_GPU_COMMANDS_COMPLETE 0x9117
#endif

#define PBO_DISPLAY_COUNT 2 /* Double buffering */
struct picture_sys_t
{
    vlc_gl_t    *gl;
    PFNGLDELETEBUFFERSPROC DeleteBuffers;
    GLuint      buffers[PICTURE_PLANE_MAX];
    size_t      bytes[PICTURE_PLANE_MAX];
    GLsync      fence;
    unsigned    index;
};

struct priv
{
    bool   has_unpack_subimage;
    void * texture_temp_buf;
    size_t texture_temp_buf_size;
    struct {
        picture_t *display_pics[PBO_DISPLAY_COUNT];
        size_t display_idx;
    } pbo;
    struct {
        picture_t *pics[VLCGL_PICTURE_MAX];
        unsigned long long list;
    } persistent;
};

static void
pbo_picture_destroy(picture_t *pic)
{
    picture_sys_t *picsys = pic->p_sys;

    if (picsys->gl)
    {
        /* Don't call glDeleteBuffers() here, since a picture can be destroyed
         * from any threads after the vout is destroyed. Instead, release the
         * reference to the GL context. All buffers will be destroyed when it
         * reaches 0. */
        vlc_gl_Release(picsys->gl);
    }
    else
        picsys->DeleteBuffers(pic->i_planes, picsys->buffers);

    free(picsys);
    free(pic);
}

static picture_t *
pbo_picture_create(const opengl_tex_converter_t *tc, bool direct_rendering)
{
    picture_sys_t *picsys = calloc(1, sizeof(*picsys));
    if (unlikely(picsys == NULL))
        return NULL;

    picture_resource_t rsc = {
        .p_sys = picsys,
        .pf_destroy = pbo_picture_destroy,
    };
    picture_t *pic = picture_NewFromResource(&tc->fmt, &rsc);
    if (pic == NULL)
    {
        free(picsys);
        return NULL;
    }

    tc->vt->GenBuffers(pic->i_planes, picsys->buffers);
    picsys->DeleteBuffers = tc->vt->DeleteBuffers;

    if (direct_rendering)
    {
        picsys->gl = tc->gl;
        vlc_gl_Hold(picsys->gl);
    }
    if (picture_Setup(pic, &tc->fmt))
    {
        picture_Release(pic);
        return NULL;
    }

    assert(pic->i_planes > 0
        && (unsigned) pic->i_planes == tc->tex_count);

    for (int i = 0; i < pic->i_planes; ++i)
    {
        const plane_t *p = &pic->p[i];

        if( p->i_pitch < 0 || p->i_lines <= 0 ||
            (size_t)p->i_pitch > SIZE_MAX/p->i_lines )
            return NULL;
        picsys->bytes[i] = (p->i_pitch * p->i_lines) + 15 / 16 * 16;
    }
    return pic;
}

static int
pbo_data_alloc(const opengl_tex_converter_t *tc, picture_t *pic)
{
    picture_sys_t *picsys = pic->p_sys;

    tc->vt->GetError();

    for (int i = 0; i < pic->i_planes; ++i)
    {
        tc->vt->BindBuffer(GL_PIXEL_UNPACK_BUFFER, picsys->buffers[i]);
        tc->vt->BufferData(GL_PIXEL_UNPACK_BUFFER, picsys->bytes[i], NULL,
                            GL_DYNAMIC_DRAW);

        if (tc->vt->GetError() != GL_NO_ERROR)
        {
            msg_Err(tc->gl, "could not alloc PBO buffers");
            tc->vt->DeleteBuffers(i, picsys->buffers);
            return VLC_EGENERIC;
        }
    }
    return VLC_SUCCESS;
}

static int
pbo_pics_alloc(const opengl_tex_converter_t *tc)
{
    struct priv *priv = tc->priv;
    for (size_t i = 0; i < PBO_DISPLAY_COUNT; ++i)
    {
        picture_t *pic = priv->pbo.display_pics[i] = pbo_picture_create(tc, false);
        if (pic == NULL)
            goto error;

        if (pbo_data_alloc(tc, pic) != VLC_SUCCESS)
            goto error;
    }

    /* turn off pbo */
    tc->vt->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    return VLC_SUCCESS;
error:
    for (size_t i = 0; i < PBO_DISPLAY_COUNT && priv->pbo.display_pics[i]; ++i)
        picture_Release(priv->pbo.display_pics[i]);
    return VLC_EGENERIC;
}

static int
tc_pbo_update(const opengl_tex_converter_t *tc, GLuint *textures,
              const GLsizei *tex_width, const GLsizei *tex_height,
              picture_t *pic, const size_t *plane_offset)
{
    (void) plane_offset; assert(plane_offset == NULL);
    struct priv *priv = tc->priv;

    picture_t *display_pic = priv->pbo.display_pics[priv->pbo.display_idx];
    priv->pbo.display_idx = (priv->pbo.display_idx + 1) % PBO_DISPLAY_COUNT;

    for (int i = 0; i < pic->i_planes; i++)
    {
        GLsizeiptr size = pic->p[i].i_lines * pic->p[i].i_pitch;
        const GLvoid *data = pic->p[i].p_pixels;
        tc->vt->BindBuffer(GL_PIXEL_UNPACK_BUFFER,
                            display_pic->p_sys->buffers[i]);
        tc->vt->BufferSubData(GL_PIXEL_UNPACK_BUFFER, 0, size, data);

        tc->vt->ActiveTexture(GL_TEXTURE0 + i);
        tc->vt->BindTexture(tc->tex_target, textures[i]);

        tc->vt->PixelStorei(GL_UNPACK_ROW_LENGTH,
                            pic->p[i].i_pitch / pic->p[i].i_pixel_pitch);

        tc->vt->TexSubImage2D(tc->tex_target, 0, 0, 0, tex_width[i], tex_height[i],
                              tc->texs[i].format, tc->texs[i].type, NULL);
    }

    /* turn off pbo */
    tc->vt->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    return VLC_SUCCESS;
}

static int
persistent_map(const opengl_tex_converter_t *tc, picture_t *pic)
{
    picture_sys_t *picsys = pic->p_sys;

    const GLbitfield access = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT |
                              GL_MAP_PERSISTENT_BIT;
    for (int i = 0; i < pic->i_planes; ++i)
    {
        tc->vt->BindBuffer(GL_PIXEL_UNPACK_BUFFER, picsys->buffers[i]);
        tc->vt->BufferStorage(GL_PIXEL_UNPACK_BUFFER, picsys->bytes[i], NULL,
                               access | GL_CLIENT_STORAGE_BIT);

        pic->p[i].p_pixels =
            tc->vt->MapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, picsys->bytes[i],
                                    access | GL_MAP_FLUSH_EXPLICIT_BIT);

        if (pic->p[i].p_pixels == NULL)
        {
            msg_Err(tc->gl, "could not map PBO buffers");
            for (i = i - 1; i >= 0; --i)
            {
                tc->vt->BindBuffer(GL_PIXEL_UNPACK_BUFFER,
                                    picsys->buffers[i]);
                tc->vt->UnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
            }
            tc->vt->DeleteBuffers(pic->i_planes, picsys->buffers);
            memset(picsys->buffers, 0, PICTURE_PLANE_MAX * sizeof(GLuint));
            return VLC_EGENERIC;
        }
    }
    return VLC_SUCCESS;
}

/** Find next (bit) set */
static int fnsll(unsigned long long x, unsigned i)
{
    if (i >= CHAR_BIT * sizeof (x))
        return 0;
    return ffsll(x & ~((1ULL << i) - 1));
}

static void
persistent_release_gpupics(const opengl_tex_converter_t *tc, bool force)
{
    struct priv *priv = tc->priv;

    /* Release all pictures that are not used by the GPU anymore */
    for (unsigned i = ffsll(priv->persistent.list); i;
         i = fnsll(priv->persistent.list, i))
    {
        assert(priv->persistent.pics[i - 1] != NULL);

        picture_t *pic = priv->persistent.pics[i - 1];
        picture_sys_t *picsys = pic->p_sys;

        assert(picsys->fence != NULL);
        GLenum wait = force ? GL_ALREADY_SIGNALED
                            : tc->vt->ClientWaitSync(picsys->fence, 0, 0);

        if (wait == GL_ALREADY_SIGNALED || wait == GL_CONDITION_SATISFIED)
        {
            tc->vt->DeleteSync(picsys->fence);
            picsys->fence = NULL;

            priv->persistent.list &= ~(1ULL << (i - 1));
            priv->persistent.pics[i - 1] = NULL;
            picture_Release(pic);
        }
    }
}

static int
tc_persistent_update(const opengl_tex_converter_t *tc, GLuint *textures,
                     const GLsizei *tex_width, const GLsizei *tex_height,
                     picture_t *pic, const size_t *plane_offset)
{
    (void) plane_offset; assert(plane_offset == NULL);
    struct priv *priv = tc->priv;
    picture_sys_t *picsys = pic->p_sys;

    for (int i = 0; i < pic->i_planes; i++)
    {
        tc->vt->BindBuffer(GL_PIXEL_UNPACK_BUFFER, picsys->buffers[i]);
        if (picsys->fence == NULL)
            tc->vt->FlushMappedBufferRange(GL_PIXEL_UNPACK_BUFFER, 0,
                                            picsys->bytes[i]);
        tc->vt->ActiveTexture(GL_TEXTURE0 + i);
        tc->vt->BindTexture(tc->tex_target, textures[i]);

        tc->vt->PixelStorei(GL_UNPACK_ROW_LENGTH,
                            pic->p[i].i_pitch / pic->p[i].i_pixel_pitch);

        tc->vt->TexSubImage2D(tc->tex_target, 0, 0, 0, tex_width[i], tex_height[i],
                              tc->texs[i].format, tc->texs[i].type, NULL);
    }

    bool hold;
    if (picsys->fence == NULL)
        hold = true;
    else
    {
        /* The picture is already held */
        hold = false;
        tc->vt->DeleteSync(picsys->fence);
    }

    picsys->fence = tc->vt->FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    if (pic->p_sys->fence == NULL)
    {
        /* Error (corner case): don't hold the picture */
        hold = false;
    }

    persistent_release_gpupics(tc, false);

    if (hold)
    {
        /* Hold the picture while it's used by the GPU */
        unsigned index = pic->p_sys->index;

        priv->persistent.list |= 1ULL << index;
        assert(priv->persistent.pics[index] == NULL);
        priv->persistent.pics[index] = pic;
        picture_Hold(pic);
    }

    /* turn off pbo */
    tc->vt->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    return VLC_SUCCESS;
}

static picture_pool_t *
tc_persistent_get_pool(const opengl_tex_converter_t *tc, unsigned requested_count)
{
    struct priv *priv = tc->priv;
    picture_t *pictures[VLCGL_PICTURE_MAX];
    unsigned count;

    priv->persistent.list = 0;
    requested_count++;

    for (count = 0; count < requested_count; count++)
    {
        picture_t *pic = pictures[count] = pbo_picture_create(tc, true);
        if (pic == NULL)
            break;
#ifndef NDEBUG
        for (int i = 0; i < pic->i_planes; ++i)
            assert(pic->p_sys->bytes[i] == pictures[0]->p_sys->bytes[i]);
#endif
        pic->p_sys->index = count;

        if (persistent_map(tc, pic) != VLC_SUCCESS)
        {
            picture_Release(pic);
            break;
        }
    }

    /* We need minumum 2 pbo buffers */
    if (count <= 1)
        goto error;

    /* turn off pbo */
    tc->vt->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    /* Wrap the pictures into a pool */
    picture_pool_t *pool = picture_pool_New(count, pictures);
    if (!pool)
        goto error;
    return pool;

error:
    for (unsigned i = 0; i < count; i++)
        picture_Release(pictures[i]);

    return NULL;
}

static int
tc_common_allocate_textures(const opengl_tex_converter_t *tc, GLuint *textures,
                            const GLsizei *tex_width, const GLsizei *tex_height)
{
    for (unsigned i = 0; i < tc->tex_count; i++)
    {
        tc->vt->BindTexture(tc->tex_target, textures[i]);
        tc->vt->TexImage2D(tc->tex_target, 0, tc->texs[i].internal,
                           tex_width[i], tex_height[i], 0, tc->texs[i].format,
                           tc->texs[i].type, NULL);
    }
    return VLC_SUCCESS;
}

static int
upload_plane(const opengl_tex_converter_t *tc, unsigned tex_idx,
             GLsizei width, GLsizei height,
             unsigned pitch, unsigned pixel_pitch, const void *pixels)
{
    struct priv *priv = tc->priv;
    GLenum tex_format = tc->texs[tex_idx].format;
    GLenum tex_type = tc->texs[tex_idx].type;

    /* This unpack alignment is the default, but setting it just in case. */
    tc->vt->PixelStorei(GL_UNPACK_ALIGNMENT, 4);

    if (!priv->has_unpack_subimage)
    {
#define ALIGN(x, y) (((x) + ((y) - 1)) & ~((y) - 1))
        unsigned dst_width = width;
        unsigned dst_pitch = ALIGN(dst_width * pixel_pitch, 4);
        if (pitch != dst_pitch)
        {
            size_t buf_size = dst_pitch * height * pixel_pitch;
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
                memcpy(destination, source, width * pixel_pitch);
                source += pitch;
                destination += dst_pitch;
            }
            tc->vt->TexSubImage2D(tc->tex_target, 0, 0, 0, width, height,
                                  tex_format, tex_type, priv->texture_temp_buf);
        }
        else
        {
            tc->vt->TexSubImage2D(tc->tex_target, 0, 0, 0, width, height,
                                  tex_format, tex_type, pixels);
        }
#undef ALIGN
    }
    else
    {
        tc->vt->PixelStorei(GL_UNPACK_ROW_LENGTH, pitch / pixel_pitch);
        tc->vt->TexSubImage2D(tc->tex_target, 0, 0, 0, width, height,
                              tex_format, tex_type, pixels);
    }
    return VLC_SUCCESS;
}

static int
tc_common_update(const opengl_tex_converter_t *tc, GLuint *textures,
                 const GLsizei *tex_width, const GLsizei *tex_height,
                 picture_t *pic, const size_t *plane_offset)
{
    assert(pic->p_sys == NULL);
    int ret = VLC_SUCCESS;
    for (unsigned i = 0; i < tc->tex_count && ret == VLC_SUCCESS; i++)
    {
        assert(textures[i] != 0);
        tc->vt->ActiveTexture(GL_TEXTURE0 + i);
        tc->vt->BindTexture(tc->tex_target, textures[i]);
        const void *pixels = plane_offset != NULL ?
                             &pic->p[i].p_pixels[plane_offset[i]] :
                             pic->p[i].p_pixels;

        ret = upload_plane(tc, i, tex_width[i], tex_height[i],
                           pic->p[i].i_pitch, pic->p[i].i_pixel_pitch, pixels);
    }
    return ret;
}

int
opengl_tex_converter_generic_init(opengl_tex_converter_t *tc, bool allow_dr)
{
    GLuint fragment_shader = 0;
    video_color_space_t space;
    const vlc_fourcc_t *list;

    if (vlc_fourcc_IsYUV(tc->fmt.i_chroma))
    {
        GLint max_texture_units = 0;
        tc->vt->GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_units);
        if (max_texture_units < 3)
            return VLC_EGENERIC;

        list = vlc_fourcc_GetYUVFallback(tc->fmt.i_chroma);
        space = tc->fmt.space;
    }
    else if (tc->fmt.i_chroma == VLC_CODEC_XYZ12)
    {
        static const vlc_fourcc_t xyz12_list[] = { VLC_CODEC_XYZ12, 0 };
        list = xyz12_list;
        space = COLOR_SPACE_UNDEF;
    }
    else
    {
        list = vlc_fourcc_GetRGBFallback(tc->fmt.i_chroma);
        space = COLOR_SPACE_UNDEF;
    }

    while (*list)
    {
        fragment_shader =
            opengl_fragment_shader_init(tc, GL_TEXTURE_2D, *list, space);
        if (fragment_shader != 0)
        {
            tc->fmt.i_chroma = *list;

            if (tc->fmt.i_chroma == VLC_CODEC_RGB32)
            {
#if defined(WORDS_BIGENDIAN)
                tc->fmt.i_rmask  = 0xff000000;
                tc->fmt.i_gmask  = 0x00ff0000;
                tc->fmt.i_bmask  = 0x0000ff00;
#else
                tc->fmt.i_rmask  = 0x000000ff;
                tc->fmt.i_gmask  = 0x0000ff00;
                tc->fmt.i_bmask  = 0x00ff0000;
#endif
                video_format_FixRgb(&tc->fmt);
            }
            break;
        }
        list++;
    }
    if (fragment_shader == 0)
        return VLC_EGENERIC;

    struct priv *priv = tc->priv = calloc(1, sizeof(struct priv));
    if (unlikely(priv == NULL))
    {
        tc->vt->DeleteShader(fragment_shader);
        return VLC_ENOMEM;
    }

    tc->pf_update            = tc_common_update;
    tc->pf_allocate_textures = tc_common_allocate_textures;

    if (allow_dr)
    {
        bool supports_map_persistent = false;

        const bool has_pbo =
            HasExtension(tc->glexts, "GL_ARB_pixel_buffer_object") ||
            HasExtension(tc->glexts, "GL_EXT_pixel_buffer_object");

        const bool has_bs =
            HasExtension(tc->glexts, "GL_ARB_buffer_storage") ||
            HasExtension(tc->glexts, "GL_EXT_buffer_storage");

        /* Ensure we do direct rendering with OpenGL 3.0 or higher. Indeed,
         * persistent mapped buffers seems to be slow with OpenGL 2.1 drivers
         * and bellow. This may be caused by OpenGL compatibility layer. */
        const unsigned char *ogl_version = tc->vt->GetString(GL_VERSION);
        const bool glver_ok = strverscmp((const char *)ogl_version, "3.0") >= 0;

        supports_map_persistent = glver_ok && has_pbo && has_bs && tc->gl->module
            && tc->vt->BufferStorage && tc->vt->MapBufferRange && tc->vt->FlushMappedBufferRange
            && tc->vt->UnmapBuffer && tc->vt->FenceSync && tc->vt->DeleteSync
            && tc->vt->ClientWaitSync;
        if (supports_map_persistent)
        {
            tc->pf_get_pool = tc_persistent_get_pool;
            tc->pf_update   = tc_persistent_update;
            msg_Dbg(tc->gl, "MAP_PERSISTENT support (direct rendering) enabled");
        }
        if (!supports_map_persistent)
        {
            const bool supports_pbo = has_pbo && tc->vt->BufferData
                && tc->vt->BufferSubData;
            if (supports_pbo && pbo_pics_alloc(tc) == VLC_SUCCESS)
            {
                tc->pf_update  = tc_pbo_update;
                msg_Dbg(tc->gl, "PBO support enabled");
            }
        }
    }

    /* OpenGL or OpenGL ES2 with GL_EXT_unpack_subimage ext */
    priv->has_unpack_subimage =
        !tc->is_gles || HasExtension(tc->glexts, "GL_EXT_unpack_subimage");
    tc->fshader = fragment_shader;

    return VLC_SUCCESS;
}

void
opengl_tex_converter_generic_deinit(opengl_tex_converter_t *tc)
{
    struct priv *priv = tc->priv;
    for (size_t i = 0; i < PBO_DISPLAY_COUNT && priv->pbo.display_pics[i]; ++i)
        picture_Release(priv->pbo.display_pics[i]);
    persistent_release_gpupics(tc, true);
    free(priv->texture_temp_buf);
    free(tc->priv);
}
