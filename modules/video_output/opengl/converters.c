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

#include <vlc_common.h>
#include <vlc_memory.h>
#include <vlc_memstream.h>
#include "internal.h"

#ifndef GL_RED
#define GL_RED 0
#endif
#ifndef GL_R16
#define GL_R16 0
#endif

#ifndef GL_LUMINANCE16
#define GL_LUMINANCE16 0
#endif

#ifndef GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#define NEED_GL_EXT_unpack_subimage
#endif

#ifdef VLCGL_HAS_PBO
#define PBO_DISPLAY_COUNT 2 /* Double buffering */
struct picture_sys_t
{
    const opengl_tex_converter_t *tc;
    GLuint      buffers[PICTURE_PLANE_MAX];
    size_t      bytes[PICTURE_PLANE_MAX];
#ifdef VLCGL_HAS_MAP_PERSISTENT
    GLsync      fence;
    unsigned    index;
#endif
};
#endif

struct priv
{
    bool   has_unpack_subimage;
    void * texture_temp_buf;
    size_t texture_temp_buf_size;
#ifdef VLCGL_HAS_PBO
    struct {
        picture_t *display_pics[PBO_DISPLAY_COUNT];
        size_t display_idx;
    } pbo;
#ifdef VLCGL_HAS_MAP_PERSISTENT
    struct {
        picture_t *pics[VLCGL_PICTURE_MAX];
        unsigned long long list;
    } persistent;
#endif
#endif
};

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
tc_yuv_base_init(opengl_tex_converter_t *tc, GLenum tex_target,
                 vlc_fourcc_t chroma, video_color_space_t yuv_space,
                 bool *swap_uv, const char *swizzle_per_tex[])
{
    const vlc_chroma_description_t *desc = vlc_fourcc_GetChromaDescription(chroma);
    if (desc == NULL)
        return VLC_EGENERIC;

    GLint oneplane_texfmt, oneplane16_texfmt, twoplanes_texfmt;

#if !defined(USE_OPENGL_ES2)
    if (HasExtension(tc->glexts, "GL_ARB_texture_rg"))
    {
        oneplane_texfmt = GL_RED;
        oneplane16_texfmt = GL_R16;
        twoplanes_texfmt = GL_RG;
    }
    else
#endif
    {
        oneplane_texfmt = GL_LUMINANCE;
        oneplane16_texfmt = GL_LUMINANCE16;
        twoplanes_texfmt = GL_LUMINANCE_ALPHA;
    }

    float yuv_range_correction = 1.0;
    if (desc->plane_count == 3)
    {
        GLint internal = 0;
        GLenum type = 0;

        if (desc->pixel_size == 1)
        {
            internal = oneplane_texfmt;
            type = GL_UNSIGNED_BYTE;
        }
#if !defined(USE_OPENGL_ES2)
        else if (desc->pixel_size == 2)
        {
            if (oneplane16_texfmt == 0
             || GetTexFormatSize(tex_target, oneplane_texfmt, oneplane16_texfmt,
                                 GL_UNSIGNED_SHORT) != 16)
                return VLC_EGENERIC;

            internal = oneplane16_texfmt;
            type = GL_UNSIGNED_SHORT;
            yuv_range_correction = (float)((1 << 16) - 1)
                                 / ((1 << desc->pixel_bits) - 1);
        }
#endif
        else
            return VLC_EGENERIC;

        assert(internal != 0 && type != 0);

        tc->tex_count = 3;
        for (unsigned i = 0; i < tc->tex_count; ++i )
        {
            tc->texs[i] = (struct opengl_tex_cfg) {
                { desc->p[i].w.num, desc->p[i].w.den },
                { desc->p[i].h.num, desc->p[i].h.den },
                internal, oneplane_texfmt, type
            };
        }

        if (oneplane_texfmt == GL_RED)
            swizzle_per_tex[0] = swizzle_per_tex[1] = swizzle_per_tex[2] = "r";
    }
    else if (desc->plane_count == 2)
    {
        if (desc->pixel_size != 1)
            return VLC_EGENERIC;

        tc->tex_count = 2;
        tc->texs[0] = (struct opengl_tex_cfg) {
            { 1, 1 }, { 1, 1 }, oneplane_texfmt, oneplane_texfmt, GL_UNSIGNED_BYTE
        };
        tc->texs[1] = (struct opengl_tex_cfg) {
            { 1, 2 }, { 1, 2 }, twoplanes_texfmt, twoplanes_texfmt, GL_UNSIGNED_BYTE
        };

        if (oneplane_texfmt == GL_RED)
        {
            swizzle_per_tex[0] = "r";
            swizzle_per_tex[1] = "rg";
        }
        else
        {
            swizzle_per_tex[0] = NULL;
            swizzle_per_tex[1] = "xa";
        }
    }
    else if (desc->plane_count == 1)
    {
        tc->tex_count = 1;
        tc->texs[0] = (struct opengl_tex_cfg) {
            { 1, 1 }, { 1, 1 }, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE
        };
        switch (chroma)
        {
            case VLC_CODEC_UYVY:
                swizzle_per_tex[0] = "gbr";
                break;
            case VLC_CODEC_YUYV:
                swizzle_per_tex[0] = "rgb";
                break;
            case VLC_CODEC_VYUY:
                swizzle_per_tex[0] = "bgr";
                break;
            case VLC_CODEC_YVYU:
                swizzle_per_tex[0] = "rbg";
                break;
            default:
                assert(!"missing chroma");
                return VLC_EGENERIC;
        }
    }
    else
        return VLC_EGENERIC;

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
    switch (yuv_space)
    {
        case COLOR_SPACE_BT601:
            matrix = matrix_bt601_tv2full;
            break;
        default:
            matrix = matrix_bt709_tv2full;
    };

    for (int i = 0; i < 4; i++) {
        float correction = i < 3 ? yuv_range_correction : 1.f;
        /* We place coefficient values for coefficient[4] in one array from
         * matrix values. Notice that we fill values from top down instead
         * of left to right.*/
        for (int j = 0; j < 4; j++)
            tc->yuv_coefficients[i*4+j] = j < 3 ? correction * matrix[j*4+i] : 0.f;
    }

    tc->chroma = chroma;
    tc->yuv_color = true;

    *swap_uv = chroma == VLC_CODEC_YV12 || chroma == VLC_CODEC_YV9 ||
               chroma == VLC_CODEC_NV21;
    return VLC_SUCCESS;
}

static int
tc_rgba_base_init(opengl_tex_converter_t *tc, GLenum tex_target,
                  vlc_fourcc_t chroma)
{
    (void) tex_target;

    if (chroma != VLC_CODEC_RGBA && chroma != VLC_CODEC_RGB32)
        return VLC_EGENERIC;

    tc->chroma = VLC_CODEC_RGBA;
    tc->tex_count = 1;
    tc->texs[0] = (struct opengl_tex_cfg) {
        { 1, 1 }, { 1, 1 }, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE
    };
    return VLC_SUCCESS;
}

static int
tc_base_fetch_locations(opengl_tex_converter_t *tc, GLuint program)
{
    if (tc->yuv_color)
    {
        tc->uloc.Coefficients = tc->api->GetUniformLocation(program,
                                                            "Coefficients");
        if (tc->uloc.Coefficients == -1)
            return VLC_EGENERIC;
    }

    for (unsigned int i = 0; i < tc->tex_count; ++i)
    {
        char name[sizeof("TextureX")];
        snprintf(name, sizeof(name), "Texture%1u", i);
        tc->uloc.Texture[i] = tc->api->GetUniformLocation(program, name);
        if (tc->uloc.Texture[i] == -1)
            return VLC_EGENERIC;
#ifdef GL_TEXTURE_RECTANGLE
        if (tc->tex_target == GL_TEXTURE_RECTANGLE)
        {
            snprintf(name, sizeof(name), "TexSize%1u", i);
            tc->uloc.TexSize[i] = tc->api->GetUniformLocation(program, name);
            if (tc->uloc.TexSize[i] == -1)
                return VLC_EGENERIC;
        }
#endif
    }

    tc->uloc.FillColor = tc->api->GetUniformLocation(program, "FillColor");
    if (tc->uloc.FillColor == -1)
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

static void
tc_base_prepare_shader(const opengl_tex_converter_t *tc,
                       const GLsizei *tex_width, const GLsizei *tex_height,
                       float alpha)
{
    (void) tex_width; (void) tex_height;

    if (tc->yuv_color)
        tc->api->Uniform4fv(tc->uloc.Coefficients, 4, tc->yuv_coefficients);

    for (unsigned i = 0; i < tc->tex_count; ++i)
        tc->api->Uniform1i(tc->uloc.Texture[i], i);

    tc->api->Uniform4f(tc->uloc.FillColor, 1.0f, 1.0f, 1.0f, alpha);

#ifdef GL_TEXTURE_RECTANGLE
    if (tc->tex_target == GL_TEXTURE_RECTANGLE)
    {
        for (unsigned i = 0; i < tc->tex_count; ++i)
            tc->api->Uniform2f(tc->uloc.TexSize[i], tex_width[i],
                               tex_height[i]);
    }
#endif
}

GLuint
opengl_fragment_shader_init(opengl_tex_converter_t *tc, GLenum tex_target,
                            vlc_fourcc_t chroma, video_color_space_t yuv_space)
{
    const char *swizzle_per_tex[PICTURE_PLANE_MAX] = { NULL, };
    const bool is_yuv = vlc_fourcc_IsYUV(chroma);
    bool yuv_swap_uv = false;
    int ret;
    if (is_yuv)
        ret = tc_yuv_base_init(tc, tex_target, chroma, yuv_space,
                               &yuv_swap_uv, swizzle_per_tex);
    else
        ret = tc_rgba_base_init(tc, tex_target, chroma);

    if (ret != VLC_SUCCESS)
        return 0;

    const char *sampler, *lookup, *coord_name;
    switch (tex_target)
    {
        case GL_TEXTURE_2D:
            sampler = "sampler2D";
            lookup  = "texture2D";
            coord_name = "TexCoord";
            break;
#ifdef GL_TEXTURE_RECTANGLE
        case GL_TEXTURE_RECTANGLE:
            sampler = "sampler2DRect";
            lookup  = "texture2DRect";
            coord_name = "TexCoordRect";
            break;
#endif
        default:
            vlc_assert_unreachable();
    }

    struct vlc_memstream ms;
    if (vlc_memstream_open(&ms) != 0)
        return 0;

#define ADD(x) vlc_memstream_puts(&ms, x)
#define ADDF(x, ...) vlc_memstream_printf(&ms, x, ##__VA_ARGS__)

    ADD("#version " GLSL_VERSION "\n" PRECISION);

    for (unsigned i = 0; i < tc->tex_count; ++i)
        ADDF("uniform %s Texture%u;"
             "varying vec2 TexCoord%u;", sampler, i, i);

#ifdef GL_TEXTURE_RECTANGLE
    if (tex_target == GL_TEXTURE_RECTANGLE)
    {
        for (unsigned i = 0; i < tc->tex_count; ++i)
            ADDF("uniform vec2 TexSize%u;", i);
    }
#endif

    if (is_yuv)
        ADD("uniform vec4 Coefficients[4];");

    ADD("uniform vec4 FillColor;"
        "void main(void) {"
        "float val;vec4 colors;");

#ifdef GL_TEXTURE_RECTANGLE
    if (tex_target == GL_TEXTURE_RECTANGLE)
    {
        for (unsigned i = 0; i < tc->tex_count; ++i)
            ADDF("vec2 TexCoordRect%u = vec2(TexCoord%u.x * TexSize%u.x, "
                 "TexCoord%u.y * TexSize%u.y);", i, i, i, i, i);
    }
#endif

    unsigned color_idx = 0;
    for (unsigned i = 0; i < tc->tex_count; ++i)
    {
        const char *swizzle = swizzle_per_tex[i];
        if (swizzle)
        {
            size_t swizzle_count = strlen(swizzle);
            ADDF("colors = %s(Texture%u, %s%u);", lookup, i, coord_name, i);
            for (unsigned j = 0; j < swizzle_count; ++j)
            {
                ADDF("val = colors.%c;"
                     "vec4 color%u = vec4(val, val, val, 1);",
                     swizzle[j], color_idx);
                color_idx++;
                assert(color_idx <= PICTURE_PLANE_MAX);
            }
        }
        else
        {
            ADDF("vec4 color%u = %s(Texture%u, %s%u);",
                 color_idx, lookup, i, coord_name, i);
            color_idx++;
            assert(color_idx <= PICTURE_PLANE_MAX);
        }
    }
    unsigned color_count = color_idx;
    assert(yuv_space == COLOR_SPACE_UNDEF || color_count == 3);

    if (is_yuv)
        ADD("vec4 result = (color0 * Coefficients[0]) + Coefficients[3];");
    else
        ADD("vec4 result = color0;");

    for (unsigned i = 1; i < color_count; ++i)
    {
        unsigned color_idx;
        if (yuv_swap_uv)
        {
            assert(color_count == 3);
            color_idx = (i % 2) + 1;
        }
        else
            color_idx = i;

        if (is_yuv)
            ADDF("result = (color%u * Coefficients[%u]) + result;", color_idx, i);
        else
            ADDF("result = color%u + result;", color_idx);
    }

    ADD("gl_FragColor = result * FillColor;"
        "}");

#undef ADD
#undef ADDF

    if (vlc_memstream_close(&ms) != 0)
        return 0;

    GLuint fragment_shader = tc->api->CreateShader(GL_FRAGMENT_SHADER);
    if (fragment_shader == 0)
    {
        free(ms.ptr);
        return 0;
    }
    GLint length = ms.length;
    tc->api->ShaderSource(fragment_shader, 1, (const char **)&ms.ptr, &length);
    tc->api->CompileShader(fragment_shader);
    free(ms.ptr);

    tc->tex_target = tex_target;

    tc->pf_fetch_locations = tc_base_fetch_locations;
    tc->pf_prepare_shader = tc_base_prepare_shader;

    return fragment_shader;
}

#ifdef VLCGL_HAS_PBO
# ifndef GL_PIXEL_UNPACK_BUFFER
#  define GL_PIXEL_UNPACK_BUFFER 0x88EC
# endif
# ifndef GL_DYNAMIC_DRAW
#  define GL_DYNAMIC_DRAW 0x88E8
# endif

static picture_t *
pbo_picture_create(const opengl_tex_converter_t *tc, const video_format_t *fmt,
                   void (*pf_destroy)(picture_t *))
{
    picture_sys_t *picsys = calloc(1, sizeof(*picsys));
    if (unlikely(picsys == NULL))
        return NULL;
    picsys->tc = tc;
    picture_resource_t rsc = {
        .p_sys = picsys,
        .pf_destroy = pf_destroy,
    };

    picture_t *pic = picture_NewFromResource(fmt, &rsc);
    if (pic == NULL)
    {
        free(picsys);
        return NULL;
    }
    if (picture_Setup(pic, fmt))
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

    glGetError();
    tc->api->GenBuffers(pic->i_planes, picsys->buffers);

    for (int i = 0; i < pic->i_planes; ++i)
    {
        tc->api->BindBuffer(GL_PIXEL_UNPACK_BUFFER, picsys->buffers[i]);
        tc->api->BufferData(GL_PIXEL_UNPACK_BUFFER, picsys->bytes[i], NULL,
                            GL_DYNAMIC_DRAW);

        if (glGetError() != GL_NO_ERROR)
        {
            msg_Err(tc->gl, "could not alloc PBO buffers");
            tc->api->DeleteBuffers(i, picsys->buffers);
            return VLC_EGENERIC;
        }
    }
    return VLC_SUCCESS;
}

static void
picture_pbo_destroy_cb(picture_t *pic)
{
    picture_sys_t *picsys = pic->p_sys;
    const opengl_tex_converter_t *tc = picsys->tc;

    if (picsys->buffers[0] != 0)
        tc->api->DeleteBuffers(pic->i_planes, picsys->buffers);
    free(picsys);
    free(pic);
}

static int
pbo_pics_alloc(const opengl_tex_converter_t *tc, const video_format_t *fmt)
{
    struct priv *priv = tc->priv;
    for (size_t i = 0; i < PBO_DISPLAY_COUNT; ++i)
    {
        picture_t *pic = priv->pbo.display_pics[i] =
            pbo_picture_create(tc, fmt, picture_pbo_destroy_cb);
        if (pic == NULL)
            goto error;

        if (pbo_data_alloc(tc, pic) != VLC_SUCCESS)
            goto error;
    }

    /* turn off pbo */
    tc->api->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

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
        GLsizeiptr size = pic->p[i].i_visible_lines * pic->p[i].i_visible_pitch;
        const GLvoid *data = pic->p[i].p_pixels;
        tc->api->BindBuffer(GL_PIXEL_UNPACK_BUFFER,
                            display_pic->p_sys->buffers[i]);
        tc->api->BufferSubData(GL_PIXEL_UNPACK_BUFFER, 0, size, data);

        glActiveTexture(GL_TEXTURE0 + i);
        glClientActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(tc->tex_target, textures[i]);

        glPixelStorei(GL_UNPACK_ROW_LENGTH,
                      pic->p[i].i_pitch / pic->p[i].i_pixel_pitch);

        glTexSubImage2D(tc->tex_target, 0, 0, 0, tex_width[i], tex_height[i],
                        tc->texs[i].format, tc->texs[i].type, NULL);
    }

    /* turn off pbo */
    tc->api->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    return VLC_SUCCESS;
}

static void
tc_pbo_release(const opengl_tex_converter_t *tc)
{
    struct priv *priv = tc->priv;
    for (size_t i = 0; i < PBO_DISPLAY_COUNT && priv->pbo.display_pics[i]; ++i)
        picture_Release(priv->pbo.display_pics[i]);
    free(tc->priv);
}

#ifdef VLCGL_HAS_MAP_PERSISTENT
static int
persistent_map(const opengl_tex_converter_t *tc, picture_t *pic)
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
            msg_Err(tc->gl, "could not map PBO buffers");
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
                            : tc->api->ClientWaitSync(picsys->fence, 0, 0);

        if (wait == GL_ALREADY_SIGNALED || wait == GL_CONDITION_SATISFIED)
        {
            tc->api->DeleteSync(picsys->fence);
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
        tc->api->BindBuffer(GL_PIXEL_UNPACK_BUFFER, picsys->buffers[i]);
        if (picsys->fence == NULL)
            tc->api->FlushMappedBufferRange(GL_PIXEL_UNPACK_BUFFER, 0,
                                            picsys->bytes[i]);
        glActiveTexture(GL_TEXTURE0 + i);
        glClientActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(tc->tex_target, textures[i]);

        glPixelStorei(GL_UNPACK_ROW_LENGTH,
                      pic->p[i].i_pitch / pic->p[i].i_pixel_pitch);

        glTexSubImage2D(tc->tex_target, 0, 0, 0, tex_width[i], tex_height[i],
                        tc->texs[i].format, tc->texs[i].type, NULL);
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
    tc->api->BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    return VLC_SUCCESS;
}

static void
tc_persistent_release(const opengl_tex_converter_t *tc)
{
    persistent_release_gpupics(tc, true);
    free(tc->priv);
}

static void
picture_persistent_destroy_cb(picture_t *pic)
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
tc_persistent_get_pool(const opengl_tex_converter_t *tc, const video_format_t *fmt,
                       unsigned requested_count)
{
    struct priv *priv = tc->priv;
    picture_t *pictures[VLCGL_PICTURE_MAX];
    unsigned count;

    priv->persistent.list = 0;

    for (count = 0; count < requested_count; count++)
    {
        picture_t *pic = pictures[count] =
            pbo_picture_create(tc, fmt, picture_persistent_destroy_cb);
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
#endif /* VLCGL_HAS_MAP_PERSISTENT */
#endif /* VLCGL_HAS_PBO */

static int
tc_common_allocate_textures(const opengl_tex_converter_t *tc, GLuint *textures,
                            const GLsizei *tex_width, const GLsizei *tex_height)
{
    for (unsigned i = 0; i < tc->tex_count; i++)
    {
        glBindTexture(tc->tex_target, textures[i]);
        glTexImage2D(tc->tex_target, 0, tc->texs[i].internal,
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

            for (GLsizei h = 0; h < height ; h++)
            {
                memcpy(destination, source, width * pixel_pitch);
                source += pitch;
                destination += dst_pitch;
            }
            glTexSubImage2D(tc->tex_target, 0, 0, 0, width, height,
                            tex_format, tex_type, priv->texture_temp_buf);
        }
        else
        {
            glTexSubImage2D(tc->tex_target, 0, 0, 0, width, height,
                            tex_format, tex_type, pixels);
        }
#undef ALIGN
    }
    else
    {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / pixel_pitch);
        glTexSubImage2D(tc->tex_target, 0, 0, 0, width, height,
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
        glActiveTexture(GL_TEXTURE0 + i);
        glClientActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(tc->tex_target, textures[i]);
        const void *pixels = plane_offset != NULL ?
                             &pic->p[i].p_pixels[plane_offset[i]] :
                             pic->p[i].p_pixels;

        ret = upload_plane(tc, i, tex_width[i], tex_height[i],
                           pic->p[i].i_pitch, pic->p[i].i_pixel_pitch, pixels);
    }
    return ret;
}

static void
tc_common_release(const opengl_tex_converter_t *tc)
{
    struct priv *priv = tc->priv;
    free(priv->texture_temp_buf);
    free(tc->priv);
}

static int
tc_xyz12_fetch_locations(opengl_tex_converter_t *tc, GLuint program)
{
    tc->uloc.Texture[0] = tc->api->GetUniformLocation(program, "Texture0");
    return tc->uloc.Texture[0] != -1 ? VLC_SUCCESS : VLC_EGENERIC;
}

static void
tc_xyz12_prepare_shader(const opengl_tex_converter_t *tc,
                        const GLsizei *tex_width, const GLsizei *tex_height,
                        float alpha)
{
    (void) tex_width; (void) tex_height; (void) alpha;
    tc->api->Uniform1i(tc->uloc.Texture[0], 0);
}

static GLuint
xyz12_shader_init(opengl_tex_converter_t *tc)
{
    tc->chroma  = VLC_CODEC_XYZ12;
    tc->tex_count = 1;
    tc->tex_target = GL_TEXTURE_2D;
    tc->texs[0] = (struct opengl_tex_cfg) {
        { 1, 1 }, { 1, 1 }, GL_RGB, GL_RGB, GL_UNSIGNED_SHORT
    };

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

        "varying vec2 TexCoord0;"
        "void main()"
        "{ "
        " vec4 v_in, v_out;"
        " v_in  = texture2D(Texture0, TexCoord0);"
        " v_in = pow(v_in, xyz_gamma);"
        " v_out = matrix_xyz_rgb * v_in ;"
        " v_out = pow(v_out, rgb_gamma) ;"
        " v_out = clamp(v_out, 0.0, 1.0) ;"
        " gl_FragColor = v_out;"
        "}";

    GLuint fragment_shader = tc->api->CreateShader(GL_FRAGMENT_SHADER);
    if (fragment_shader == 0)
        return 0;
    tc->api->ShaderSource(fragment_shader, 1, &code, NULL);
    tc->api->CompileShader(fragment_shader);
    return fragment_shader;
}

static GLuint
generic_init(const video_format_t *fmt, opengl_tex_converter_t *tc,
             bool allow_dr)
{
    const vlc_chroma_description_t *desc =
        vlc_fourcc_GetChromaDescription(fmt->i_chroma);
    if (!desc || desc->plane_count == 0)
        return 0;

    GLuint fragment_shader = 0;
    if (fmt->i_chroma == VLC_CODEC_XYZ12)
        fragment_shader = xyz12_shader_init(tc);
    else
    {
        video_color_space_t space;
        const vlc_fourcc_t *(*get_fallback)(vlc_fourcc_t i_fourcc);

        if (vlc_fourcc_IsYUV(fmt->i_chroma))
        {
            GLint max_texture_units = 0;
            glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_units);
            if (max_texture_units < 3)
                return 0;

            get_fallback = vlc_fourcc_GetYUVFallback;
            space = fmt->space;
        }
        else
        {
            get_fallback = vlc_fourcc_GetRGBFallback;
            space = COLOR_SPACE_UNDEF;
        }

        const vlc_fourcc_t *list = get_fallback(fmt->i_chroma);
        while (*list && fragment_shader == 0)
        {
            fragment_shader =
                opengl_fragment_shader_init(tc, GL_TEXTURE_2D, *list, space);
            list++;
        }
    }
    if (fragment_shader == 0)
        return 0;

    struct priv *priv = tc->priv = calloc(1, sizeof(struct priv));
    if (unlikely(priv == NULL))
        goto error;

    tc->pf_update            = tc_common_update;
    tc->pf_release           = tc_common_release;
    tc->pf_allocate_textures = tc_common_allocate_textures;

    if (allow_dr)
    {
        bool supports_map_persistent = false;

        const bool has_pbo =
            HasExtension(tc->glexts, "GL_ARB_pixel_buffer_object") ||
            HasExtension(tc->glexts, "GL_EXT_pixel_buffer_object");

#ifdef VLCGL_HAS_MAP_PERSISTENT
        const bool has_bs =
            HasExtension(tc->glexts, "GL_ARB_buffer_storage") ||
            HasExtension(tc->glexts, "GL_EXT_buffer_storage");
        supports_map_persistent = has_pbo && has_bs && tc->api->BufferStorage
            && tc->api->MapBufferRange && tc->api->FlushMappedBufferRange
            && tc->api->UnmapBuffer && tc->api->FenceSync && tc->api->DeleteSync
            && tc->api->ClientWaitSync;
        if (supports_map_persistent)
        {
            tc->pf_get_pool = tc_persistent_get_pool;
            tc->pf_update   = tc_persistent_update;
            tc->pf_release  = tc_persistent_release;
            msg_Dbg(tc->gl, "MAP_PERSISTENT support (direct rendering) enabled");
        }
#endif
#ifdef VLCGL_HAS_PBO
        if (!supports_map_persistent)
        {
            const bool supports_pbo = has_pbo && tc->api->BufferData
                && tc->api->BufferSubData;
            if (supports_pbo && pbo_pics_alloc(tc, fmt) == VLC_SUCCESS)
            {
                tc->pf_update  = tc_pbo_update;
                tc->pf_release = tc_pbo_release;
                msg_Err(tc->gl, "PBO support enabled");
            }
        }
#endif
    }

#ifdef NEED_GL_EXT_unpack_subimage
    priv->has_unpack_subimage = HasExtension(tc->glexts,
                                             "GL_EXT_unpack_subimage");
#else
    priv->has_unpack_subimage = true;
#endif

    return fragment_shader;
error:
    tc->api->DeleteShader(fragment_shader);
    return 0;
}

GLuint
opengl_tex_converter_subpictures_init(const video_format_t *fmt,
                                      opengl_tex_converter_t *tc)
{
    return generic_init(fmt, tc, false);
}

GLuint
opengl_tex_converter_generic_init(const video_format_t *fmt,
                                  opengl_tex_converter_t *tc)
{
    return generic_init(fmt, tc, true);
}
