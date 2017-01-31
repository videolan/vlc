/*****************************************************************************
 * converters.c: OpenGL converters for common video formats
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

#include <assert.h>
#include <limits.h>
#include <stdlib.h>

#include <vlc_memory.h>
#include "internal.h"

#ifndef GL_RED
#define GL_RED 0
#endif
#ifndef GL_R16
#define GL_R16 0
#endif

#ifndef GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#define NEED_GL_EXT_unpack_subimage
#endif

#ifdef VLCGL_HAS_PBO
struct picture_sys_t
{
    const opengl_tex_converter_t *tc;
    GLuint      buffers[PICTURE_PLANE_MAX];
    size_t      bytes[PICTURE_PLANE_MAX];
    GLsync      fence;
    unsigned    index;
};
#endif

struct priv
{
    GLint  tex_internal;
    GLenum tex_format;
    GLenum tex_type;

    struct {
        GLint Texture0;
        GLint Texture1;
        GLint Texture2;
        GLint Coefficient;
        GLint FillColor;
    } uloc;

    bool   has_unpack_subimage;
    void * texture_temp_buf;
    size_t texture_temp_buf_size;
#ifdef VLCGL_HAS_PBO
    struct {
        picture_t *pics[VLCGL_PICTURE_MAX];
        unsigned long long list;
    } ongpu;
#endif
};

struct yuv_priv
{
    struct priv priv;
    GLfloat local_value[16];
};

#ifdef VLCGL_HAS_PBO
static int
pbo_map(const opengl_tex_converter_t *tc, picture_t *pic)
{
    picture_sys_t *picsys = pic->p_sys;

    tc->api->GenBuffers(pic->i_planes, picsys->buffers);

    const GLbitfield access = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT |
                              GL_MAP_PERSISTENT_BIT;
    for (int i = 0; i < pic->i_planes; ++i)
    {
        tc->api->BindBuffer(GL_PIXEL_UNPACK_BUFFER, picsys->buffers[i]);
        tc->api->BufferStorage(GL_PIXEL_UNPACK_BUFFER, picsys->bytes[i], NULL,
                               access);

        pic->p[i].p_pixels =
            tc->api->MapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, picsys->bytes[i],
                                    access);

        if (pic->p[i].p_pixels == NULL)
        {
            msg_Err(tc->parent, "could not map PBO buffers");
            for (i = i - 1; i >= 0; --i)
            {
                tc->api->BindBuffer(GL_PIXEL_UNPACK_BUFFER,
                                    picsys->buffers[i]);
                tc->api->UnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
            }
            tc->api->DeleteBuffers(pic->i_planes, picsys->buffers);
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
pbo_release_gpupics(const opengl_tex_converter_t *tc, bool force)
{
    struct priv *priv = tc->priv;

    /* Release all pictures that are not used by the GPU anymore */
    for (unsigned i = ffsll(priv->ongpu.list); i;
         i = fnsll(priv->ongpu.list, i))
    {
        assert(priv->ongpu.pics[i - 1] != NULL);

        picture_t *pic = priv->ongpu.pics[i - 1];
        picture_sys_t *picsys = pic->p_sys;

        assert(picsys->fence != NULL);
        GLenum wait = force ? GL_ALREADY_SIGNALED
                            : tc->api->ClientWaitSync(picsys->fence, 0, 0);

        if (wait == GL_ALREADY_SIGNALED || wait == GL_CONDITION_SATISFIED)
        {
            tc->api->DeleteSync(picsys->fence);
            picsys->fence = NULL;

            priv->ongpu.list &= ~(1ULL << (i - 1));
            priv->ongpu.pics[i - 1] = NULL;
            picture_Release(pic);
        }
    }
}

static int
pbo_common_update(const opengl_tex_converter_t *tc, const GLuint *textures,
                  unsigned width, unsigned height, picture_t *pic)
{
    struct priv *priv = tc->priv;
    picture_sys_t *picsys = pic->p_sys;

    for (int i = 0; i < pic->i_planes; i++)
    {
        tc->api->BindBuffer(GL_PIXEL_UNPACK_BUFFER, picsys->buffers[i]);
        if (picsys->fence == NULL)
            tc->api->FlushMappedBufferRange(GL_PIXEL_UNPACK_BUFFER, 0,
                                            picsys->bytes[i]);
        glActiveTexture(GL_TEXTURE0 + i);
        glClientActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(tc->tex_target, textures[i]);

        glPixelStorei(GL_UNPACK_ROW_LENGTH,
                      pic->p[i].i_pitch / pic->p[i].i_pixel_pitch);

        glTexSubImage2D(tc->tex_target, 0, 0, 0,
                        width * tc->desc->p[i].w.num / tc->desc->p[i].w.den,
                        height * tc->desc->p[i].h.num / tc->desc->p[i].h.den,
                        priv->tex_format, priv->tex_type, NULL);
    }

    bool hold;
    if (picsys->fence == NULL)
        hold = true;
    else
    {
        /* The picture is already held */
        hold = false;
        tc->api->DeleteSync(picsys->fence);
    }

    picsys->fence = tc->api->FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    if (pic->p_sys->fence == NULL)
    {
        /* Error (corner case): don't hold the picture */
        hold = false;
    }

    pbo_release_gpupics(tc, false);

    if (hold)
    {
        /* Hold the picture while it's used by the GPU */
        unsigned index = pic->p_sys->index;

        priv->ongpu.list |= 1ULL << index;
        assert(priv->ongpu.pics[index] == NULL);
        priv->ongpu.pics[index] = pic;
        picture_Hold(pic);
    }

    /* turn off pbo */
    tc->api->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    return VLC_SUCCESS;
}

static void
picture_destroy_cb(picture_t *pic)
{
    picture_sys_t *picsys = pic->p_sys;
    const opengl_tex_converter_t *tc = picsys->tc;

    if (picsys->buffers[0] != 0)
    {
        for (int i = 0; i < pic->i_planes; ++i)
        {
            tc->api->BindBuffer(GL_PIXEL_UNPACK_BUFFER, picsys->buffers[i]);
            tc->api->UnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        }
        tc->api->DeleteBuffers(pic->i_planes, picsys->buffers);
    }
    free(picsys);
    free(pic);
}

static picture_pool_t *
tc_common_get_pool(const opengl_tex_converter_t *tc, const video_format_t *fmt,
                   unsigned requested_count, GLuint *textures)
{
    struct priv *priv = tc->priv;
    picture_t *pictures[VLCGL_PICTURE_MAX];
    unsigned count;
    (void) textures;

    priv->ongpu.list = 0;

    for (count = 0; count < requested_count; count++)
    {
        picture_sys_t *picsys = calloc(1, sizeof(*picsys));
        if (unlikely(picsys == NULL))
            break;
        picsys->tc = tc;
        picsys->index = count;
        picture_resource_t rsc = {
            .p_sys = picsys,
            .pf_destroy = picture_destroy_cb,
        };

        picture_t *pic = pictures[count] = picture_NewFromResource(fmt, &rsc);
        if (pic == NULL)
        {
            free(picsys);
            break;
        }
        if (picture_Setup(pic, fmt))
        {
            picture_Release(pic);
            break;
        }

        assert(pic->i_planes > 0
            && (unsigned) pic->i_planes == tc->desc->plane_count);

        for (int i = 0; i < pic->i_planes; ++i)
        {
            const plane_t *p = &pic->p[i];

            if( p->i_pitch < 0 || p->i_lines <= 0 ||
                (size_t)p->i_pitch > SIZE_MAX/p->i_lines )
                goto error;
            picsys->bytes[i] = (p->i_pitch * p->i_lines) + 15 / 16 * 16;
            assert(picsys->bytes[i] == pictures[0]->p_sys->bytes[i]);
        }

        if (pbo_map(tc, pic) != VLC_SUCCESS)
        {
            picture_Release(pic);
            break;
        }
    }

    /* We need minumum 2 pbo buffers */
    if (count <= 1)
        goto error;

    /* turn off pbo */
    tc->api->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

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
#endif /* VLCGL_HAS_PBO */

static int
tc_common_allocate_texture(const opengl_tex_converter_t *tc, GLuint texture,
                           unsigned tex_idx, const GLsizei tex_width,
                           const GLsizei tex_height)
{
    (void) texture; (void) tex_idx;
    struct priv *priv = tc->priv;

    glTexImage2D(tc->tex_target, 0, priv->tex_internal, tex_width, tex_height,
                 0, priv->tex_format, priv->tex_type, NULL);
    return VLC_SUCCESS;
}

static int
upload_plane(const opengl_tex_converter_t *tc,
             unsigned width, unsigned height,
             unsigned pitch, unsigned pixel_pitch, const void *pixels)
{
    struct priv *priv = tc->priv;
    int tex_target = tc->tex_target, tex_format = priv->tex_format,
        tex_type = priv->tex_type;

    /* This unpack alignment is the default, but setting it just in case. */
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

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

            for (unsigned h = 0; h < height ; h++)
            {
                memcpy(destination, source, width * pixel_pitch);
                source += pitch;
                destination += dst_pitch;
            }
            glTexSubImage2D(tex_target, 0, 0, 0, width, height,
                            tex_format, tex_type, priv->texture_temp_buf);
        }
        else
        {
            glTexSubImage2D(tex_target, 0, 0, 0, width, height,
                            tex_format, tex_type, pixels);
        }
#undef ALIGN
    }
    else
    {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / pixel_pitch);
        glTexSubImage2D(tex_target, 0, 0, 0, width, height,
                        tex_format, tex_type, pixels);
    }
    return VLC_SUCCESS;
}

static int
tc_common_update(const opengl_tex_converter_t *tc, GLuint *textures,
                 unsigned width, unsigned height,
                 picture_t *pic, const size_t *plane_offset)
{
#ifdef VLCGL_HAS_PBO
    if (pic->p_sys != NULL)
        return pbo_common_update(tc, textures, width, height, pic);
#endif

    int ret = VLC_SUCCESS;
    for (unsigned i = 0; i < tc->desc->plane_count && ret == VLC_SUCCESS; i++)
    {
        assert(textures[i] != 0);
        glActiveTexture(GL_TEXTURE0 + i);
        glClientActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(tc->tex_target, textures[i]);
        const void *pixels = plane_offset != NULL ?
                             &pic->p[i].p_pixels[plane_offset[i]] :
                             pic->p[i].p_pixels;

        ret = upload_plane(tc,
                           width * tc->desc->p[i].w.num / tc->desc->p[i].w.den,
                           height * tc->desc->p[i].h.num / tc->desc->p[i].h.den,
                           pic->p[i].i_pitch, pic->p[i].i_pixel_pitch, pixels);
    }
    return ret;
}

static void
tc_common_release(const opengl_tex_converter_t *tc)
{
    if (tc->fragment_shader != 0)
        tc->api->DeleteShader(tc->fragment_shader);

    struct priv *priv = tc->priv;
    free(priv->texture_temp_buf);

#ifdef VLCGL_HAS_PBO
    pbo_release_gpupics(tc, true);
#endif

    free(tc->priv);
}

static int
common_init(opengl_tex_converter_t *tc, size_t priv_size, vlc_fourcc_t chroma,
            GLint tex_internal, GLenum tex_format, GLenum tex_type)
{
    struct priv *priv = tc->priv = calloc(1, priv_size);
    if (unlikely(priv == NULL))
        return VLC_ENOMEM;

    tc->chroma  = chroma;
    tc->desc    = vlc_fourcc_GetChromaDescription(chroma);
    assert(tc->desc != NULL);

    tc->pf_update       = tc_common_update;
    tc->pf_release      = tc_common_release;
    tc->pf_allocate_texture = tc_common_allocate_texture;

#ifdef VLCGL_HAS_PBO
    const bool supports_pbo = tc->api->BufferStorage
        && tc->api->MapBufferRange && tc->api->FlushMappedBufferRange
        && tc->api->UnmapBuffer && tc->api->FenceSync && tc->api->DeleteSync
        && tc->api->ClientWaitSync
        && HasExtension(tc->glexts, "GL_ARB_pixel_buffer_object")
        && HasExtension(tc->glexts, "GL_ARB_buffer_storage");
    if (supports_pbo)
        tc->pf_get_pool = tc_common_get_pool;
    msg_Dbg(tc->parent, "PBO support for %4.4s (direct rendering): %s",
            (const char *) &chroma, supports_pbo ? "On" : "Off");
#endif

    tc->tex_target      = GL_TEXTURE_2D;
    priv->tex_internal  = tex_internal;
    priv->tex_format    = tex_format;
    priv->tex_type      = tex_type;

#ifdef NEED_GL_EXT_unpack_subimage
    priv->has_unpack_subimage = HasExtension(tc->glexts,
                                             "GL_EXT_unpack_subimage");
#else
    priv->has_unpack_subimage = true;
#endif

    return VLC_SUCCESS;
}

static int
tc_rgba_fetch_locations(const opengl_tex_converter_t *tc, GLuint program)
{
    struct priv *priv = tc->priv;
    priv->uloc.Texture0 = tc->api->GetUniformLocation(program, "Texture0");
    priv->uloc.FillColor = tc->api->GetUniformLocation(program, "FillColor");
    return priv->uloc.Texture0 != -1 && priv->uloc.FillColor != -1 ?
           VLC_SUCCESS : VLC_EGENERIC;
}

static void
tc_rgba_prepare_shader(const opengl_tex_converter_t *tc,
                       const GLsizei *tex_width, const GLsizei *tex_height,
                       float alpha)
{
    (void) tex_width; (void) tex_height;
    struct priv *priv = tc->priv;
    tc->api->Uniform1i(priv->uloc.Texture0, 0);
    tc->api->Uniform4f(priv->uloc.FillColor, 1.0f, 1.0f, 1.0f, alpha);
}

int
opengl_tex_converter_rgba_init(const video_format_t *fmt,
                               opengl_tex_converter_t *tc)
{
    if (fmt->i_chroma != VLC_CODEC_RGBA && fmt->i_chroma != VLC_CODEC_RGB32)
        return VLC_EGENERIC;

    if (common_init(tc, sizeof(struct priv), VLC_CODEC_RGBA,
                    GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE) != VLC_SUCCESS)
        return VLC_ENOMEM;

    tc->pf_fetch_locations = tc_rgba_fetch_locations;
    tc->pf_prepare_shader = tc_rgba_prepare_shader;

#if 0
    /* Simple shader for RGB */
    static const char *code =
        "#version " GLSL_VERSION "\n"
        PRECISION
        "uniform sampler2D Texture[3];"
        "varying vec4 TexCoord0,TexCoord1,TexCoord2;"
        "void main()"
        "{ "
        "  gl_FragColor = texture2D(Texture[0], TexCoord0.st);"
        "}";
#endif

    /* Simple shader for RGBA */
    static const char *code =
        "#version " GLSL_VERSION "\n"
        PRECISION
        "uniform sampler2D Texture0;"
        "uniform vec4 FillColor;"
        "varying vec4 TexCoord0;"
        "void main()"
        "{ "
        "  gl_FragColor = texture2D(Texture0, TexCoord0.st) * FillColor;"
        "}";

    tc->fragment_shader = tc->api->CreateShader(GL_FRAGMENT_SHADER);
    if (tc->fragment_shader == 0)
    {
        tc_common_release(tc);
        return VLC_EGENERIC;
    }
    tc->api->ShaderSource(tc->fragment_shader, 1, &code, NULL);
    tc->api->CompileShader(tc->fragment_shader);
    return VLC_SUCCESS;
}

#if !defined(USE_OPENGL_ES2)
static int GetTexFormatSize(int target, int tex_format, int tex_internal,
                            int tex_type)
{
    GLint tex_param_size;
    switch (tex_format)
    {
        case GL_RED:
            tex_param_size = GL_TEXTURE_RED_SIZE;
            break;
        case GL_LUMINANCE:
            tex_param_size = GL_TEXTURE_LUMINANCE_SIZE;
            break;
        default:
            return -1;
    }
    GLuint texture;

    glGenTextures(1, &texture);
    glBindTexture(target, texture);
    glTexImage2D(target, 0, tex_internal, 64, 64, 0, tex_format, tex_type, NULL);
    GLint size = 0;
    glGetTexLevelParameteriv(target, 0, tex_param_size, &size);

    glDeleteTextures(1, &texture);
    return size;
}
#endif

static int
tc_yuv_fetch_locations(const opengl_tex_converter_t *tc, GLuint program)
{
    struct priv *priv = tc->priv;
    priv->uloc.Coefficient = tc->api->GetUniformLocation(program, "Coefficient");
    priv->uloc.Texture0 = tc->api->GetUniformLocation(program, "Texture0");
    priv->uloc.Texture1 = tc->api->GetUniformLocation(program, "Texture1");
    priv->uloc.Texture2 = tc->api->GetUniformLocation(program, "Texture2");
    return priv->uloc.Coefficient != -1 && priv->uloc.Texture0 != -1
        && priv->uloc.Texture1 != -1 && priv->uloc.Texture2 != -1 ?
        VLC_SUCCESS : VLC_EGENERIC;
}

static void
tc_yuv_prepare_shader(const opengl_tex_converter_t *tc,
                      const GLsizei *tex_width, const GLsizei *tex_height,
                      float alpha)
{
    (void) tex_width; (void) tex_height; (void) alpha;
    struct priv *priv = tc->priv;
    tc->api->Uniform4fv(priv->uloc.Coefficient, 4,
                        ((struct yuv_priv *)priv)->local_value);
    tc->api->Uniform1i(priv->uloc.Texture0, 0);
    tc->api->Uniform1i(priv->uloc.Texture1, 1);
    tc->api->Uniform1i(priv->uloc.Texture2, 2);
}

int
opengl_tex_converter_yuv_init(const video_format_t *fmt,
                              opengl_tex_converter_t *tc)
{
    if (!vlc_fourcc_IsYUV(fmt->i_chroma))
        return VLC_EGENERIC;

    GLint max_texture_units = 0;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_units);

    if (max_texture_units < 3)
        return VLC_EGENERIC;

#if !defined(USE_OPENGL_ES2)
    const unsigned char *ogl_version = glGetString(GL_VERSION);
    const bool oglv3 = strverscmp((const char *)ogl_version, "3.0") >= 0;
    const int yuv_plane_texformat = oglv3 ? GL_RED : GL_LUMINANCE;
    const int yuv_plane_texformat_16 = oglv3 ? GL_R16 : GL_LUMINANCE16;
#else
    const int yuv_plane_texformat = GL_LUMINANCE;
#endif

    float yuv_range_correction = 1.0;
    const vlc_fourcc_t *list = vlc_fourcc_GetYUVFallback(fmt->i_chroma);
    while (*list)
    {
        const vlc_chroma_description_t *dsc =
            vlc_fourcc_GetChromaDescription(*list);
        if (dsc && dsc->plane_count == 3 && dsc->pixel_size == 1)
        {
            if (common_init(tc, sizeof(struct yuv_priv), *list,
                            yuv_plane_texformat, yuv_plane_texformat,
                            GL_UNSIGNED_BYTE) != VLC_SUCCESS)
                return VLC_ENOMEM;

            yuv_range_correction = 1.0;
            break;
#if !defined(USE_OPENGL_ES2)
        } else if (dsc && dsc->plane_count == 3 && dsc->pixel_size == 2 &&
                   GetTexFormatSize(GL_TEXTURE_2D,
                                    yuv_plane_texformat,
                                    yuv_plane_texformat_16,
                                    GL_UNSIGNED_SHORT) == 16)
        {
            if (common_init(tc, sizeof(struct yuv_priv), *list,
                            yuv_plane_texformat_16, yuv_plane_texformat,
                            GL_UNSIGNED_SHORT) != VLC_SUCCESS)
                return VLC_ENOMEM;

            yuv_range_correction = (float)((1 << 16) - 1)
                                 / ((1 << dsc->pixel_bits) - 1);
            break;
#endif
        }
        list++;
    }
    if (!*list)
        return VLC_EGENERIC;

    tc->pf_fetch_locations = tc_yuv_fetch_locations;
    tc->pf_prepare_shader = tc_yuv_prepare_shader;

    GLfloat *local_value = ((struct yuv_priv*) tc->priv)->local_value;

    /* [R/G/B][Y U V O] from TV range to full range
     * XXX we could also do hue/brightness/constrast/gamma
     * by simply changing the coefficients
     */
    static const float matrix_bt601_tv2full[12] = {
        1.164383561643836,  0.0000,             1.596026785714286, -0.874202217873451 ,
        1.164383561643836, -0.391762290094914, -0.812967647237771,  0.531667823499146 ,
        1.164383561643836,  2.017232142857142,  0.0000,            -1.085630789302022 ,
    };
    static const float matrix_bt709_tv2full[12] = {
        1.164383561643836,  0.0000,             1.792741071428571, -0.972945075016308 ,
        1.164383561643836, -0.21324861427373,  -0.532909328559444,  0.301482665475862 ,
        1.164383561643836,  2.112401785714286,  0.0000,            -1.133402217873451 ,
    };
    const float *matrix;
    switch( fmt->space )
    {
        case COLOR_SPACE_BT601:
            matrix = matrix_bt601_tv2full;
            break;
        default:
            matrix = matrix_bt709_tv2full;
    };

    /* Basic linear YUV -> RGB conversion using bilinear interpolation */
    static const char *template_glsl_yuv =
        "#version " GLSL_VERSION "\n"
        PRECISION
        "uniform sampler2D Texture0;"
        "uniform sampler2D Texture1;"
        "uniform sampler2D Texture2;"
        "uniform vec4      Coefficient[4];"
        "varying vec4      TexCoord0,TexCoord1,TexCoord2;"

        "void main(void) {"
        " vec4 x,y,z,result;"

        /* The texture format can be GL_RED: vec4(R,0,0,1) or GL_LUMINANCE:
         * vec4(L,L,L,1). The following transform a vec4(x, y, z, w) into a
         * vec4(x, x, x, 1) (we may want to use texture swizzling starting
         * OpenGL 3.3). */
        " float val0 = texture2D(Texture0, TexCoord0.st).x;"
        " float val1 = texture2D(Texture1, TexCoord1.st).x;"
        " float val2 = texture2D(Texture2, TexCoord2.st).x;"
        " x  = vec4(val0, val0, val0, 1);"
        " %c = vec4(val1, val1, val1, 1);"
        " %c = vec4(val2, val2, val2, 1);"

        " result = x * Coefficient[0] + Coefficient[3];"
        " result = (y * Coefficient[1]) + result;"
        " result = (z * Coefficient[2]) + result;"
        " gl_FragColor = result;"
        "}";
    bool swap_uv = fmt->i_chroma == VLC_CODEC_YV12 ||
                   fmt->i_chroma == VLC_CODEC_YV9;

    char *code;
    if (asprintf(&code, template_glsl_yuv,
                 swap_uv ? 'z' : 'y',
                 swap_uv ? 'y' : 'z') < 0)
    {
        tc_common_release(tc);
        return VLC_ENOMEM;
    }

    for (int i = 0; i < 4; i++) {
        float correction = i < 3 ? yuv_range_correction : 1.f;
        /* We place coefficient values for coefficient[4] in one array from
         * matrix values. Notice that we fill values from top down instead of
         * left to right.*/
        for (int j = 0; j < 4; j++)
            local_value[i*4+j] = j < 3 ? correction * matrix[j*4+i] : 0.f;
    }

    tc->fragment_shader = tc->api->CreateShader(GL_FRAGMENT_SHADER);
    if (tc->fragment_shader == 0)
    {
        tc_common_release(tc);
        free(code);
        return VLC_EGENERIC;
    }
    tc->api->ShaderSource(tc->fragment_shader, 1, (const char **)&code, NULL);
    tc->api->CompileShader(tc->fragment_shader);
    free(code);

    return VLC_SUCCESS;
}

static int
tc_xyz12_fetch_locations(const opengl_tex_converter_t *tc, GLuint program)
{
    struct priv *priv = tc->priv;
    priv->uloc.Texture0 = tc->api->GetUniformLocation(program, "Texture0");
    return priv->uloc.Texture0 != -1 ? VLC_SUCCESS : VLC_EGENERIC;
}

static void
tc_xyz12_prepare_shader(const opengl_tex_converter_t *tc,
                        const GLsizei *tex_width, const GLsizei *tex_height,
                        float alpha)
{
    (void) tex_width; (void) tex_height; (void) alpha;
    struct priv *priv = tc->priv;
    tc->api->Uniform1i(priv->uloc.Texture0, 0);
}

int
opengl_tex_converter_xyz12_init(const video_format_t *fmt,
                                opengl_tex_converter_t *tc)
{
    if (fmt->i_chroma != VLC_CODEC_XYZ12)
        return VLC_EGENERIC;

    if (common_init(tc, sizeof(struct priv), VLC_CODEC_XYZ12,
                    GL_RGB, GL_RGB, GL_UNSIGNED_SHORT) != VLC_SUCCESS)
        return VLC_ENOMEM;

    tc->pf_fetch_locations = tc_xyz12_fetch_locations;
    tc->pf_prepare_shader = tc_xyz12_prepare_shader;

    /* Shader for XYZ to RGB correction
     * 3 steps :
     *  - XYZ gamma correction
     *  - XYZ to RGB matrix conversion
     *  - reverse RGB gamma correction
     */
    static const char *code =
        "#version " GLSL_VERSION "\n"
        PRECISION
        "uniform sampler2D Texture0;"
        "uniform vec4 xyz_gamma = vec4(2.6);"
        "uniform vec4 rgb_gamma = vec4(1.0/2.2);"
        /* WARN: matrix Is filled column by column (not row !) */
        "uniform mat4 matrix_xyz_rgb = mat4("
        "    3.240454 , -0.9692660, 0.0556434, 0.0,"
        "   -1.5371385,  1.8760108, -0.2040259, 0.0,"
        "    -0.4985314, 0.0415560, 1.0572252,  0.0,"
        "    0.0,      0.0,         0.0,        1.0 "
        " );"

        "varying vec4 TexCoord0;"
        "void main()"
        "{ "
        " vec4 v_in, v_out;"
        " v_in  = texture2D(Texture0, TexCoord0.st);"
        " v_in = pow(v_in, xyz_gamma);"
        " v_out = matrix_xyz_rgb * v_in ;"
        " v_out = pow(v_out, rgb_gamma) ;"
        " v_out = clamp(v_out, 0.0, 1.0) ;"
        " gl_FragColor = v_out;"
        "}";

    tc->fragment_shader = tc->api->CreateShader(GL_FRAGMENT_SHADER);
    if (tc->fragment_shader == 0)
    {
        tc_common_release(tc);
        return VLC_EGENERIC;
    }
    tc->api->ShaderSource(tc->fragment_shader, 1, &code, NULL);
    tc->api->CompileShader(tc->fragment_shader);
    return VLC_SUCCESS;
}
