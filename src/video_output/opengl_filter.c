/*****************************************************************************
 * opengl_filter.c:
 *****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
 * Copyright (C) 2019-2020 Videolabs
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
#include <vlc_common.h>
#include <vlc_picture.h>
#include <vlc_modules.h>
#include <vlc_memstream.h>

#include <vlc_opengl.h>
#include <vlc_opengl_filter.h>
#include <vlc_opengl_platform.h>

#include "internal.h"
#include "sampler_priv.h"

int
vlc_gl_api_Init(struct vlc_gl_api *api, vlc_gl_t *gl)
{
#if defined(HAVE_GL_CORE_SYMBOLS)
#define GET_PROC_ADDR_CORE(name) api->vt.name = gl##name
#else
#define GET_PROC_ADDR_CORE(name) GET_PROC_ADDR_EXT(name, true)
#endif

#define GET_PROC_ADDR_EXT(name, critical) do { \
    api->vt.name = vlc_gl_GetProcAddress(gl, "gl"#name); \
    if (api->vt.name == NULL && critical) { \
        msg_Err(gl, "gl"#name" symbol not found, bailing out"); \
        return VLC_EGENERIC; \
    } \
} while(0)

#define GET_PROC_ADDR(name) \
    if (gl->api_type == VLC_OPENGL_ES2) \
        GET_PROC_ADDR_CORE(name); \
    else \
        GET_PROC_ADDR_EXT(name, true)

#define GET_PROC_ADDR_CORE_GL(name) \
    if (gl->api_type == VLC_OPENGL_ES2) \
        GET_PROC_ADDR_EXT(name, false); /* optional for GLES */ \
    else \
        GET_PROC_ADDR_CORE(name)

#define GET_PROC_ADDR_OPTIONAL(name) GET_PROC_ADDR_EXT(name, false) /* GL 3 or more */

    GET_PROC_ADDR_CORE(BindTexture);
    GET_PROC_ADDR_CORE(BlendFunc);
    GET_PROC_ADDR_CORE(Clear);
    GET_PROC_ADDR_CORE(ClearColor);
    GET_PROC_ADDR_CORE(DeleteTextures);
    GET_PROC_ADDR_CORE(DepthMask);
    GET_PROC_ADDR_CORE(Disable);
    GET_PROC_ADDR_CORE(DrawArrays);
    GET_PROC_ADDR_CORE(DrawElements);
    GET_PROC_ADDR_CORE(Enable);
    GET_PROC_ADDR_CORE(Finish);
    GET_PROC_ADDR_CORE(Flush);
    GET_PROC_ADDR_CORE(GenTextures);
    GET_PROC_ADDR_CORE(GetError);
    GET_PROC_ADDR_CORE(GetIntegerv);
    GET_PROC_ADDR_CORE(GetString);
    GET_PROC_ADDR_CORE(PixelStorei);
    GET_PROC_ADDR_CORE(TexImage2D);
    GET_PROC_ADDR_CORE(TexParameterf);
    GET_PROC_ADDR_CORE(TexParameteri);
    GET_PROC_ADDR_CORE(TexSubImage2D);
    GET_PROC_ADDR_CORE(Viewport);

    GET_PROC_ADDR_CORE_GL(GetTexLevelParameteriv);
    GET_PROC_ADDR_CORE_GL(TexEnvf);

    GET_PROC_ADDR(CreateShader);
    GET_PROC_ADDR(ShaderSource);
    GET_PROC_ADDR(CompileShader);
    GET_PROC_ADDR(AttachShader);
    GET_PROC_ADDR(DeleteShader);

    GET_PROC_ADDR(GetProgramiv);
    GET_PROC_ADDR(GetShaderiv);
    GET_PROC_ADDR(GetProgramInfoLog);
    GET_PROC_ADDR(GetShaderInfoLog);
    GET_PROC_ADDR(GetShaderSource);

    GET_PROC_ADDR(GetUniformLocation);
    GET_PROC_ADDR(GetAttribLocation);
    GET_PROC_ADDR(VertexAttribPointer);
    GET_PROC_ADDR(EnableVertexAttribArray);
    GET_PROC_ADDR(UniformMatrix4fv);
    GET_PROC_ADDR(UniformMatrix3fv);
    GET_PROC_ADDR(UniformMatrix2fv);
    GET_PROC_ADDR(Uniform4fv);
    GET_PROC_ADDR(Uniform4f);
    GET_PROC_ADDR(Uniform3f);
    GET_PROC_ADDR(Uniform2f);
    GET_PROC_ADDR(Uniform1f);
    GET_PROC_ADDR(Uniform1i);

    GET_PROC_ADDR(CreateProgram);
    GET_PROC_ADDR(LinkProgram);
    GET_PROC_ADDR(UseProgram);
    GET_PROC_ADDR(DeleteProgram);

    GET_PROC_ADDR(ActiveTexture);

    GET_PROC_ADDR(GenBuffers);
    GET_PROC_ADDR(BindBuffer);
    GET_PROC_ADDR(BufferData);
    GET_PROC_ADDR(DeleteBuffers);

    GET_PROC_ADDR_OPTIONAL(GetFramebufferAttachmentParameteriv);
    GET_PROC_ADDR_OPTIONAL(GenFramebuffers);
    GET_PROC_ADDR_OPTIONAL(DeleteFramebuffers);
    GET_PROC_ADDR_OPTIONAL(BindFramebuffer);
    GET_PROC_ADDR_OPTIONAL(FramebufferTexture2D);
    GET_PROC_ADDR_OPTIONAL(CheckFramebufferStatus);
    GET_PROC_ADDR_OPTIONAL(GenRenderbuffers);
    GET_PROC_ADDR_OPTIONAL(DeleteRenderbuffers);
    GET_PROC_ADDR_OPTIONAL(BindRenderbuffer);
    GET_PROC_ADDR_OPTIONAL(RenderbufferStorageMultisample);
    GET_PROC_ADDR_OPTIONAL(FramebufferRenderbuffer);
    GET_PROC_ADDR_OPTIONAL(BlitFramebuffer);

    GET_PROC_ADDR_OPTIONAL(ReadPixels);

    GET_PROC_ADDR_OPTIONAL(BufferSubData);
    GET_PROC_ADDR_OPTIONAL(BufferStorage);
    GET_PROC_ADDR_OPTIONAL(MapBufferRange);
    GET_PROC_ADDR_OPTIONAL(FlushMappedBufferRange);
    GET_PROC_ADDR_OPTIONAL(MapBuffer);
    GET_PROC_ADDR_OPTIONAL(UnmapBuffer);
    GET_PROC_ADDR_OPTIONAL(FenceSync);
    GET_PROC_ADDR_OPTIONAL(DeleteSync);
    GET_PROC_ADDR_OPTIONAL(ClientWaitSync);
#undef GET_PROC_ADDR

    GL_ASSERT_NOERROR(&api->vt);

    api->extensions = (const char *) api->vt.GetString(GL_EXTENSIONS);
    assert(api->extensions);
    if (!api->extensions)
    {
        msg_Err(gl, "glGetString returned NULL");
        return VLC_EGENERIC;
    }

    GL_ASSERT_NOERROR(&api->vt);
    GLint version;
    api->vt.GetIntegerv(GL_MAJOR_VERSION, &version);
    GLenum error = api->vt.GetError();

    /* OpenGL >= 3.0:
     *     https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glRenderbufferStorageMultisample.xhtml
     * OpenGL ES >= 3.0:
     *     https://www.khronos.org/registry/OpenGL-Refpages/es3.1/html/glRenderbufferStorageMultisample.xhtml
     */
    api->supports_multisample = version >= 3 && error == GL_NO_ERROR;

    /* Drain the errors before continuing. */
    while (error != GL_NO_ERROR)
        error = api->vt.GetError();

    if (gl->api_type == VLC_OPENGL_ES2)
    {
        api->is_gles = true;
        /* OpenGL ES 2 includes support for non-power of 2 textures by specification
         * so checks for extensions are bound to fail. Check for OpenGL ES version instead. */
        api->supports_npot = true;
    }
    else
    {
        api->is_gles = false;
        api->supports_npot = vlc_gl_StrHasToken(api->extensions, "GL_ARB_texture_non_power_of_two") ||
                             vlc_gl_StrHasToken(api->extensions, "GL_APPLE_texture_2D_limited_npot");
    }

    return VLC_SUCCESS;
}

static void
LogShaderErrors(vlc_object_t *obj, const opengl_vtable_t *vt, GLuint id)
{
    GLint info_len;
    vt->GetShaderiv(id, GL_INFO_LOG_LENGTH, &info_len);
    if (info_len > 0)
    {
        char *info_log = malloc(info_len);
        if (info_log)
        {
            GLsizei written;
            vt->GetShaderInfoLog(id, info_len, &written, info_log);
            msg_Err(obj, "shader: %s", info_log);
            free(info_log);

        }
    }

    GLint source_len;
    vt->GetShaderiv(id, GL_SHADER_SOURCE_LENGTH, &source_len);

    if (source_len > 0)
    {
        char *source_log = malloc(source_len);
        if (source_log)
        {
            GLsizei written;
            vt->GetShaderSource(id, source_len, &written, source_log);
            msg_Err(obj, "Shader:\n%s", source_log);
            free(source_log);
        }
    }

}

static void
LogProgramErrors(vlc_object_t *obj, const opengl_vtable_t *vt, GLuint id)
{
    GLint info_len;
    vt->GetProgramiv(id, GL_INFO_LOG_LENGTH, &info_len);
    if (info_len > 0)
    {
        char *info_log = malloc(info_len);
        if (info_log)
        {
            GLsizei written;
            vt->GetProgramInfoLog(id, info_len, &written, info_log);
            msg_Err(obj, "program: %s", info_log);
            free(info_log);
        }
    }
}

static GLuint
CreateShader(vlc_object_t *obj, const opengl_vtable_t *vt, GLenum type,
             GLsizei count, const GLchar **src)
{
    GLuint shader = vt->CreateShader(type);
    if (!shader)
        return 0;

    vt->ShaderSource(shader, count, src, NULL);
    vt->CompileShader(shader);

    LogShaderErrors(obj, vt, shader);

    GLint compiled;
    vt->GetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        msg_Err(obj, "Failed to compile shader");
        vt->DeleteShader(shader);
        return 0;
    }

    return shader;
}


unsigned
vlc_gl_BuildProgram(vlc_object_t *obj, const struct opengl_vtable_t *vt,
                    size_t vstring_count, const char **vstrings,
                    size_t fstring_count, const char **fstrings)
{
    GLuint program = 0;

    GLuint vertex_shader = CreateShader(obj, vt, GL_VERTEX_SHADER,
                                        vstring_count, vstrings);
    if (!vertex_shader)
        return 0;

    GLuint fragment_shader = CreateShader(obj, vt, GL_FRAGMENT_SHADER,
                                          fstring_count, fstrings);
    if (!fragment_shader)
        goto finally_1;

    program = vt->CreateProgram();
    if (!program)
        goto finally_2;

    vt->AttachShader(program, vertex_shader);
    vt->AttachShader(program, fragment_shader);

    vt->LinkProgram(program);

    LogProgramErrors(obj, vt, program);

    GLint linked;
    vt->GetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        msg_Err(obj, "Failed to link program");
        vt->DeleteProgram(program);
        program = 0;
    }

finally_2:
    vt->DeleteShader(fragment_shader);
finally_1:
    vt->DeleteShader(vertex_shader);

    return program;
}

struct vlc_gl_interop *
vlc_gl_interop_New(struct vlc_gl_t *gl, const struct vlc_gl_api *api,
                   vlc_video_context *context, const video_format_t *fmt)
{
    struct vlc_gl_interop *interop = vlc_object_create(gl, sizeof(*interop));
    if (!interop)
        return NULL;

    interop->init = opengl_interop_init_impl;
    interop->ops = NULL;
    interop->fmt_out = interop->fmt_in = *fmt;
    /* this is the only allocated field, and we don't need it */
    interop->fmt_out.p_palette = interop->fmt_in.p_palette = NULL;

    interop->gl = gl;
    interop->api = api;
    interop->vt = &api->vt;

    const vlc_chroma_description_t *desc =
        vlc_fourcc_GetChromaDescription(fmt->i_chroma);

    if (desc == NULL)
    {
        vlc_object_delete(interop);
        return NULL;
    }

    interop->vctx = context;
    interop->module = module_need_var(interop, "glinterop", "glinterop");
    if (interop->module == NULL)
        goto error;

    return interop;

error:
    vlc_object_delete(interop);
    return NULL;
}

struct vlc_gl_interop *
vlc_gl_interop_NewForSubpictures(struct vlc_gl_t *gl,
                                 const struct vlc_gl_api *api)
{
    struct vlc_gl_interop *interop = vlc_object_create(gl, sizeof(*interop));
    if (!interop)
        return NULL;

    interop->init = opengl_interop_init_impl;
    interop->ops = NULL;
    interop->gl = gl;
    interop->api = api;
    interop->vt = &api->vt;

    video_format_Init(&interop->fmt_in, VLC_CODEC_RGB32);
    interop->fmt_out = interop->fmt_in;

    interop->vctx = NULL;
    interop->module = module_need_var(interop, "glinterop", "glinterop");
    if (interop->module == NULL)
        goto error;

    return interop;
error:
    vlc_object_delete(interop);
    return NULL;
}

void
vlc_gl_interop_Delete(struct vlc_gl_interop *interop)
{
    if (interop->ops && interop->ops->close)
        interop->ops->close(interop);
    if (interop->module)
        module_unneed(interop, interop->module);
    vlc_object_delete(interop);
}

int
vlc_gl_interop_GenerateTextures(const struct vlc_gl_interop *interop,
                                const size_t *tex_width,
                                const size_t *tex_height, unsigned *textures)
{
    interop->vt->GenTextures(interop->tex_count, textures);

    for (unsigned i = 0; i < interop->tex_count; i++)
    {
        interop->vt->BindTexture(interop->tex_target, textures[i]);

#if !defined(USE_OPENGL_ES2)
        /* Set the texture parameters */
        interop->vt->TexParameterf(interop->tex_target, GL_TEXTURE_PRIORITY, 1.0);
        interop->vt->TexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
#endif

        interop->vt->TexParameteri(interop->tex_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        interop->vt->TexParameteri(interop->tex_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        interop->vt->TexParameteri(interop->tex_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        interop->vt->TexParameteri(interop->tex_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    if (interop->ops->allocate_textures != NULL)
    {
        int ret = interop->ops->allocate_textures(interop, textures, tex_width, tex_height);
        if (ret != VLC_SUCCESS)
        {
            interop->vt->DeleteTextures(interop->tex_count, textures);
            memset(textures, 0, interop->tex_count * sizeof(GLuint));
            return ret;
        }
    }
    return VLC_SUCCESS;
}

void
vlc_gl_interop_DeleteTextures(const struct vlc_gl_interop *interop,
                              unsigned *textures)
{
    interop->vt->DeleteTextures(interop->tex_count, textures);
    memset(textures, 0, interop->tex_count * sizeof(GLuint));
}

static int GetTexFormatSize(const opengl_vtable_t *vt, int target,
                            int tex_format, int tex_internal, int tex_type)
{
    GL_ASSERT_NOERROR(vt);
    if (!vt->GetTexLevelParameteriv)
        return -1;

    GLint tex_param_size;
    int mul = 1;
    switch (tex_format)
    {
        case GL_BGRA:
            mul = 4;
            /* fall through */
        case GL_RED:
        case GL_RG:
            tex_param_size = GL_TEXTURE_RED_SIZE;
            break;
        case GL_LUMINANCE:
            tex_param_size = GL_TEXTURE_LUMINANCE_SIZE;
            break;
        default:
            return -1;
    }
    GLuint texture;

    vt->GenTextures(1, &texture);
    vt->BindTexture(target, texture);
    vt->TexImage2D(target, 0, tex_internal, 64, 64, 0, tex_format, tex_type, NULL);
    GLint size = 0;
    vt->GetTexLevelParameteriv(target, 0, tex_param_size, &size);

    vt->DeleteTextures(1, &texture);

    bool has_error = false;
    while (vt->GetError() != GL_NO_ERROR)
        has_error = true;

    if (has_error)
        return -1;

    return size > 0 ? size * mul : size;
}

static inline void
DivideRationalByTwo(vlc_rational_t *r) {
    if (r->num % 2 == 0)
        r->num /= 2;
    else
        r->den *= 2;
}

static int
interop_yuv_base_init(struct vlc_gl_interop *interop, GLenum tex_target,
                      vlc_fourcc_t chroma,
                      const vlc_chroma_description_t *desc)
{
    (void) chroma;

    GLint oneplane_texfmt, oneplane16_texfmt,
          twoplanes_texfmt, twoplanes16_texfmt;

    if (vlc_gl_StrHasToken(interop->api->extensions, "GL_ARB_texture_rg"))
    {
        oneplane_texfmt = GL_RED;
        oneplane16_texfmt = GL_R16;
        twoplanes_texfmt = GL_RG;
        twoplanes16_texfmt = GL_RG16;
    }
    else
    {
        oneplane_texfmt = GL_LUMINANCE;
        oneplane16_texfmt = GL_LUMINANCE16;
        twoplanes_texfmt = GL_LUMINANCE_ALPHA;
        twoplanes16_texfmt = 0;
    }

    if (desc->pixel_size == 2)
    {
        if (GetTexFormatSize(interop->vt, tex_target, oneplane_texfmt,
                             oneplane16_texfmt, GL_UNSIGNED_SHORT) != 16)
            return VLC_EGENERIC;
    }

    if (desc->plane_count == 3)
    {
        GLint internal = 0;
        GLenum type = 0;

        if (desc->pixel_size == 1)
        {
            internal = oneplane_texfmt;
            type = GL_UNSIGNED_BYTE;
        }
        else if (desc->pixel_size == 2)
        {
            internal = oneplane16_texfmt;
            type = GL_UNSIGNED_SHORT;
        }
        else
            return VLC_EGENERIC;

        assert(internal != 0 && type != 0);

        interop->tex_count = 3;
        for (unsigned i = 0; i < interop->tex_count; ++i )
        {
            interop->texs[i] = (struct vlc_gl_tex_cfg) {
                { desc->p[i].w.num, desc->p[i].w.den },
                { desc->p[i].h.num, desc->p[i].h.den },
                internal, oneplane_texfmt, type
            };
        }
    }
    else if (desc->plane_count == 2)
    {
        interop->tex_count = 2;

        if (desc->pixel_size == 1)
        {
            interop->texs[0] = (struct vlc_gl_tex_cfg) {
                { desc->p[0].w.num, desc->p[0].w.den },
                { desc->p[0].h.num, desc->p[0].h.den },
                oneplane_texfmt, oneplane_texfmt, GL_UNSIGNED_BYTE
            };
            interop->texs[1] = (struct vlc_gl_tex_cfg) {
                { desc->p[1].w.num, desc->p[1].w.den },
                { desc->p[1].h.num, desc->p[1].h.den },
                twoplanes_texfmt, twoplanes_texfmt, GL_UNSIGNED_BYTE
            };
        }
        else if (desc->pixel_size == 2)
        {
            if (twoplanes16_texfmt == 0
             || GetTexFormatSize(interop->vt, tex_target, twoplanes_texfmt,
                                 twoplanes16_texfmt, GL_UNSIGNED_SHORT) != 16)
                return VLC_EGENERIC;
            interop->texs[0] = (struct vlc_gl_tex_cfg) {
                { desc->p[0].w.num, desc->p[0].w.den },
                { desc->p[0].h.num, desc->p[0].h.den },
                oneplane16_texfmt, oneplane_texfmt, GL_UNSIGNED_SHORT
            };
            interop->texs[1] = (struct vlc_gl_tex_cfg) {
                { desc->p[1].w.num, desc->p[1].w.den },
                { desc->p[1].h.num, desc->p[1].h.den },
                twoplanes16_texfmt, twoplanes_texfmt, GL_UNSIGNED_SHORT
            };
        }
        else
            return VLC_EGENERIC;

        /*
         * If plane_count == 2, then the chroma is semiplanar: the U and V
         * planes are packed in the second plane. As a consequence, the
         * horizontal scaling, as reported in the vlc_chroma_description_t, is
         * doubled.
         *
         * But once imported as an OpenGL texture, both components are stored
         * in a single texel (the two first components of the vec4).
         * Therefore, from OpenGL, the width is not doubled, so the horizontal
         * scaling must be divided by 2 to compensate.
         */
         DivideRationalByTwo(&interop->texs[1].w);
    }
    else if (desc->plane_count == 1)
    {
        /* Only YUV 4:2:2 formats */
        /* Y1 U Y2 V fits in R G B A */
        interop->tex_count = 1;
        interop->texs[0] = (struct vlc_gl_tex_cfg) {
            { desc->p[0].w.num, desc->p[0].w.den },
            { desc->p[0].h.num, desc->p[0].h.den },
            GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE
        };

        /*
         * Currently, Y2 is ignored, so the texture is stored at chroma
         * resolution. In other words, half the horizontal resolution is lost,
         * so we must adapt the horizontal scaling.
         */
        DivideRationalByTwo(&interop->texs[0].w);
    }
    else
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

static int
interop_rgb_base_init(struct vlc_gl_interop *interop, GLenum tex_target,
                      vlc_fourcc_t chroma)
{
    (void) tex_target;

    switch (chroma)
    {
        case VLC_CODEC_RGB24:
            interop->texs[0] = (struct vlc_gl_tex_cfg) {
                { 1, 1 }, { 1, 1 }, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE
            };
            break;

        case VLC_CODEC_RGB32:
        case VLC_CODEC_RGBA:
            interop->texs[0] = (struct vlc_gl_tex_cfg) {
                { 1, 1 }, { 1, 1 }, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE
            };
            break;
        case VLC_CODEC_BGRA: {
            if (GetTexFormatSize(interop->vt, tex_target, GL_BGRA, GL_RGBA,
                                 GL_UNSIGNED_BYTE) != 32)
                return VLC_EGENERIC;
            interop->texs[0] = (struct vlc_gl_tex_cfg) {
                { 1, 1 }, { 1, 1 }, GL_RGBA, GL_BGRA, GL_UNSIGNED_BYTE
            };
            break;
        }
        default:
            return VLC_EGENERIC;
    }
    interop->tex_count = 1;
    return VLC_SUCCESS;
}

static void
interop_xyz12_init(struct vlc_gl_interop *interop)
{
    interop->tex_count = 1;
    interop->tex_target = GL_TEXTURE_2D;
    interop->texs[0] = (struct vlc_gl_tex_cfg) {
        { 1, 1 }, { 1, 1 }, GL_RGB, GL_RGB, GL_UNSIGNED_SHORT
    };
}

int
opengl_interop_init_impl(struct vlc_gl_interop *interop, GLenum tex_target,
                         vlc_fourcc_t chroma, video_color_space_t yuv_space)
{
    bool is_yuv = vlc_fourcc_IsYUV(chroma);
    const vlc_chroma_description_t *desc =
        vlc_fourcc_GetChromaDescription(chroma);
    if (!desc)
        return VLC_EGENERIC;

    assert(!interop->fmt_out.p_palette);
    interop->fmt_out.i_chroma = chroma;
    interop->fmt_out.space = yuv_space;
    interop->tex_target = tex_target;

    if (chroma == VLC_CODEC_XYZ12)
    {
        interop_xyz12_init(interop);
        return VLC_SUCCESS;
    }

    if (is_yuv)
        return interop_yuv_base_init(interop, tex_target, chroma, desc);

    return interop_rgb_base_init(interop, tex_target, chroma);
}

/*****************************************************************************
 * sampler.c
 *****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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


#ifdef HAVE_LIBPLACEBO
#include <libplacebo/shaders.h>
#include <libplacebo/shaders/colorspace.h>
#include "../libplacebo/utils.h"
#endif

#include "sampler_priv.h"
#include "internal.h"

struct vlc_gl_sampler_priv {
    struct vlc_gl_sampler sampler;

    struct vlc_gl_t *gl;
    const struct vlc_gl_api *api;
    const opengl_vtable_t *vt; /* for convenience, same as &api->vt */

    struct {
        GLfloat OrientationMatrix[4*4];
        GLfloat TexCoordsMaps[PICTURE_PLANE_MAX][3*3];
    } var;
    struct {
        GLint Textures[PICTURE_PLANE_MAX];
        GLint TexSizes[PICTURE_PLANE_MAX]; /* for GL_TEXTURE_RECTANGLE */
        GLint ConvMatrix;
        GLint *pl_vars; /* for pl_sh_res */

        GLint TransformMatrix;
        GLint OrientationMatrix;
        GLint TexCoordsMaps[PICTURE_PLANE_MAX];
    } uloc;

    bool yuv_color;
    GLfloat conv_matrix[4*4];

    /* libplacebo context */
    struct pl_context *pl_ctx;
    struct pl_shader *pl_sh;
    const struct pl_shader_res *pl_sh_res;

    GLsizei tex_widths[PICTURE_PLANE_MAX];
    GLsizei tex_heights[PICTURE_PLANE_MAX];

    GLsizei visible_widths[PICTURE_PLANE_MAX];
    GLsizei visible_heights[PICTURE_PLANE_MAX];

    GLuint textures[PICTURE_PLANE_MAX];

    GLenum tex_target;

    struct {
        unsigned int i_x_offset;
        unsigned int i_y_offset;
        unsigned int i_visible_width;
        unsigned int i_visible_height;
    } last_source;

    /* A sampler supports 2 kinds of input.
     *  - created with _NewFromInterop(), it receives input pictures from VLC
     *    (picture_t) via _UpdatePicture();
     *  - created with _NewFromTexture2D() (interop is NULL), it receives
     *    directly OpenGL textures via _UpdateTextures().
     */
    struct vlc_gl_interop *interop;

    /* Only used for "direct" sampler (when interop == NULL) */
    video_format_t direct_fmt;

    /* If set, vlc_texture() exposes a single plane (without chroma
     * conversion), selected by vlc_gl_sampler_SetCurrentPlane(). */
    bool expose_planes;
    unsigned plane;
};

#define PRIV(sampler) container_of(sampler, struct vlc_gl_sampler_priv, sampler)

static const float MATRIX_COLOR_RANGE_LIMITED[4*3] = {
    255.0/219,         0,         0, -255.0/219 *  16.0/255,
            0, 255.0/224,         0, -255.0/224 * 128.0/255,
            0,         0, 255.0/224, -255.0/224 * 128.0/255,
};

static const float MATRIX_COLOR_RANGE_FULL[4*3] = {
    1, 0, 0,          0,
    0, 1, 0, -128.0/255,
    0, 0, 1, -128.0/255,
};

/*
 * Construct the transformation matrix from the luma weight of the red and blue
 * component (the green component is deduced).
 */
#define MATRIX_YUV_TO_RGB(KR, KB) \
    MATRIX_YUV_TO_RGB_(KR, (1-(KR)-(KB)), KB)

/*
 * Construct the transformation matrix from the luma weight of the RGB
 * components.
 *
 * KR: luma weight of the red component
 * KG: luma weight of the green component
 * KB: luma weight of the blue component
 *
 * By definition, KR + KG + KB == 1.
 *
 * Ref: <https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.601_conversion>
 * Ref: libplacebo: src/colorspace.c:luma_coeffs()
 * */
#define MATRIX_YUV_TO_RGB_(KR, KG, KB) { \
    1,                         0,              2*(1.0-(KR)), \
    1, -2*(1.0-(KB))*((KB)/(KG)), -2*(1.0-(KR))*((KR)/(KG)), \
    1,              2*(1.0-(KB)),                         0, \
}

static const float MATRIX_BT601[3*3] = MATRIX_YUV_TO_RGB(0.299, 0.114);
static const float MATRIX_BT709[3*3] = MATRIX_YUV_TO_RGB(0.2126, 0.0722);
static const float MATRIX_BT2020[3*3] = MATRIX_YUV_TO_RGB(0.2627, 0.0593);

void
vlc_sampler_yuv2rgb_matrix(float conv_matrix_out[],
                           video_color_space_t color_space,
                           video_color_range_t color_range)
{
    const float *space_matrix;
    switch (color_space) {
        case COLOR_SPACE_BT601:
            space_matrix = MATRIX_BT601;
            break;
        case COLOR_SPACE_BT2020:
            space_matrix = MATRIX_BT2020;
            break;
        default:
            space_matrix = MATRIX_BT709;
    }

    /* Init the conversion matrix in column-major order (OpenGL expects
     * column-major order by default, and OpenGL ES does not support row-major
     * order at all). */

    const float *range_matrix = color_range == COLOR_RANGE_FULL
                              ? MATRIX_COLOR_RANGE_FULL
                              : MATRIX_COLOR_RANGE_LIMITED;
    /* Multiply the matrices on CPU once for all */
    for (int x = 0; x < 4; ++x)
    {
        for (int y = 0; y < 3; ++y)
        {
            /* Perform intermediate computation in double precision even if the
             * result is in single-precision, to avoid unnecessary errors. */
            double sum = 0;
            for (int k = 0; k < 3; ++k)
                sum += space_matrix[y * 3 + k] * range_matrix[k * 4 + x];
            /* Notice the reversed indices: x is now the row, y is the
             * column. */
            conv_matrix_out[x * 4 + y] = sum;
        }
    }

    /* Add a row to fill a 4x4 matrix (remember it's in column-major order).
     * (non-square matrices are not supported on old OpenGL ES versions) */
    conv_matrix_out[3] = 0;
    conv_matrix_out[7] = 0;
    conv_matrix_out[11] = 0;
    conv_matrix_out[15] = 1;
}

static int
sampler_yuv_base_init(struct vlc_gl_sampler *sampler, vlc_fourcc_t chroma,
                      const vlc_chroma_description_t *desc,
                      video_color_space_t yuv_space)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);

    /* The current implementation always converts from limited to full range. */
    const video_color_range_t range = COLOR_RANGE_LIMITED;
    float *matrix = priv->conv_matrix;
    vlc_sampler_yuv2rgb_matrix(matrix, yuv_space, range);

    if (desc->pixel_size == 2)
    {
        if (chroma != VLC_CODEC_P010 && chroma != VLC_CODEC_P016) {
            /* Do a bit shift if samples are stored on LSB. */
            float yuv_range_correction = (float)((1 << 16) - 1)
                                         / ((1 << desc->pixel_bits) - 1);
            /* We want to transform the input color (y, u, v, 1) to
             * (r*y, r*u, r*v, 1), where r = yuv_range_correction.
             *
             * This can be done by left-multiplying the color vector by a
             * matrix R:
             *
             *                 R
             *  / r*y \   / r 0 0 0 \   / y \
             *  | r*u | = | 0 r 0 0 | * | u |
             *  | r*v |   | 0 0 r 0 |   | v |
             *  \  1  /   \ 0 0 0 1 /   \ 1 /
             *
             * Combine this transformation with the color conversion matrix:
             *
             *     matrix := matrix * R
             *
             * This is equivalent to multipying the 3 first rows by r
             * (yuv_range_conversion).
             */
            for (int i = 0; i < 4*3; ++i)
                matrix[i] *= yuv_range_correction;
        }
    }

    priv->yuv_color = true;

    /* Some formats require to swap the U and V components.
     *
     * This can be done by left-multiplying the color vector by a matrix S:
     *
     *               S
     *  / y \   / 1 0 0 0 \   / y \
     *  | v | = | 0 0 1 0 | * | u |
     *  | u |   | 0 1 0 0 |   | v |
     *  \ 1 /   \ 0 0 0 1 /   \ 1 /
     *
     * Combine this transformation with the color conversion matrix:
     *
     *     matrix := matrix * S
     *
     * This is equivalent to swap columns 1 and 2.
     */
    bool swap_uv = chroma == VLC_CODEC_YV12 || chroma == VLC_CODEC_YV9 ||
                   chroma == VLC_CODEC_NV21;
    if (swap_uv)
    {
        /* Remember, the matrix in column-major order */
        float tmp[4];
        /* tmp <- column1 */
        memcpy(tmp, matrix + 4, sizeof(tmp));
        /* column1 <- column2 */
        memcpy(matrix + 4, matrix + 8, sizeof(tmp));
        /* column2 <- tmp */
        memcpy(matrix + 8, tmp, sizeof(tmp));
    }
    return VLC_SUCCESS;
}

static void
sampler_base_fetch_locations(struct vlc_gl_sampler *sampler, GLuint program)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);

    const opengl_vtable_t *vt = priv->vt;

    if (priv->yuv_color)
    {
        priv->uloc.ConvMatrix = vt->GetUniformLocation(program, "ConvMatrix");
        assert(priv->uloc.ConvMatrix != -1);
    }

    priv->uloc.TransformMatrix =
        vt->GetUniformLocation(program, "TransformMatrix");
    assert(priv->uloc.TransformMatrix != -1);

    priv->uloc.OrientationMatrix =
        vt->GetUniformLocation(program, "OrientationMatrix");
    assert(priv->uloc.OrientationMatrix != -1);

    assert(sampler->tex_count < 10); /* to guarantee variable names length */
    for (unsigned int i = 0; i < sampler->tex_count; ++i)
    {
        char name[sizeof("TexCoordsMaps[X]")];

        snprintf(name, sizeof(name), "Textures[%1u]", i);
        priv->uloc.Textures[i] = vt->GetUniformLocation(program, name);
        assert(priv->uloc.Textures[i] != -1);

        snprintf(name, sizeof(name), "TexCoordsMaps[%1u]", i);
        priv->uloc.TexCoordsMaps[i] = vt->GetUniformLocation(program, name);
        assert(priv->uloc.TexCoordsMaps[i] != -1);

        if (priv->tex_target == GL_TEXTURE_RECTANGLE)
        {
            snprintf(name, sizeof(name), "TexSizes[%1u]", i);
            priv->uloc.TexSizes[i] = vt->GetUniformLocation(program, name);
            assert(priv->uloc.TexSizes[i] != -1);
        }
    }

#ifdef HAVE_LIBPLACEBO
    const struct pl_shader_res *res = priv->pl_sh_res;
    for (int i = 0; res && i < res->num_variables; i++) {
        struct pl_shader_var sv = res->variables[i];
        priv->uloc.pl_vars[i] = vt->GetUniformLocation(program, sv.var.name);
    }
#endif
}

static const GLfloat *
GetTransformMatrix(const struct vlc_gl_interop *interop)
{
    const GLfloat *tm = NULL;
    if (interop && interop->ops && interop->ops->get_transform_matrix)
        tm = interop->ops->get_transform_matrix(interop);
    if (!tm)
        tm = MATRIX4_IDENTITY;
    return tm;
}

static void
sampler_base_load(const struct vlc_gl_sampler *sampler)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);

    const opengl_vtable_t *vt = priv->vt;

    if (priv->yuv_color)
        vt->UniformMatrix4fv(priv->uloc.ConvMatrix, 1, GL_FALSE,
                             priv->conv_matrix);

    for (unsigned i = 0; i < sampler->tex_count; ++i)
    {
        vt->Uniform1i(priv->uloc.Textures[i], i);

        assert(priv->textures[i] != 0);
        vt->ActiveTexture(GL_TEXTURE0 + i);
        vt->BindTexture(priv->tex_target, priv->textures[i]);

        vt->UniformMatrix3fv(priv->uloc.TexCoordsMaps[i], 1, GL_FALSE,
                             priv->var.TexCoordsMaps[i]);
    }

    /* Return the expected transform matrix if interop == NULL */
    const GLfloat *tm = GetTransformMatrix(priv->interop);
    vt->UniformMatrix4fv(priv->uloc.TransformMatrix, 1, GL_FALSE, tm);

    vt->UniformMatrix4fv(priv->uloc.OrientationMatrix, 1, GL_FALSE,
                         priv->var.OrientationMatrix);

    if (priv->tex_target == GL_TEXTURE_RECTANGLE)
    {
        for (unsigned i = 0; i < sampler->tex_count; ++i)
            vt->Uniform2f(priv->uloc.TexSizes[i], priv->tex_widths[i],
                          priv->tex_heights[i]);
    }

#ifdef HAVE_LIBPLACEBO
    const struct pl_shader_res *res = priv->pl_sh_res;
    for (int i = 0; res && i < res->num_variables; i++) {
        GLint loc = priv->uloc.pl_vars[i];
        if (loc == -1) // uniform optimized out
            continue;

        struct pl_shader_var sv = res->variables[i];
        struct pl_var var = sv.var;
        // libplacebo doesn't need anything else anyway
        if (var.type != PL_VAR_FLOAT)
            continue;
        if (var.dim_m > 1 && var.dim_m != var.dim_v)
            continue;

        const float *f = sv.data;
        switch (var.dim_m) {
        case 4: vt->UniformMatrix4fv(loc, 1, GL_FALSE, f); break;
        case 3: vt->UniformMatrix3fv(loc, 1, GL_FALSE, f); break;
        case 2: vt->UniformMatrix2fv(loc, 1, GL_FALSE, f); break;

        case 1:
            switch (var.dim_v) {
            case 1: vt->Uniform1f(loc, f[0]); break;
            case 2: vt->Uniform2f(loc, f[0], f[1]); break;
            case 3: vt->Uniform3f(loc, f[0], f[1], f[2]); break;
            case 4: vt->Uniform4f(loc, f[0], f[1], f[2], f[3]); break;
            }
            break;
        }
    }
#endif
}

static void
sampler_xyz12_fetch_locations(struct vlc_gl_sampler *sampler, GLuint program)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);
    const opengl_vtable_t *vt = priv->vt;

    priv->uloc.Textures[0] = vt->GetUniformLocation(program, "Textures[0]");
    assert(priv->uloc.Textures[0] != -1);

    priv->uloc.TransformMatrix =
        vt->GetUniformLocation(program, "TransformMatrix");
    assert(priv->uloc.TransformMatrix != -1);

    priv->uloc.OrientationMatrix =
        vt->GetUniformLocation(program, "OrientationMatrix");
    assert(priv->uloc.OrientationMatrix != -1);

    priv->uloc.TexCoordsMaps[0] =
        vt->GetUniformLocation(program, "TexCoordsMaps[0]");
    assert(priv->uloc.TexCoordsMaps[0] != -1);
}

static void
sampler_xyz12_load(const struct vlc_gl_sampler *sampler)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);
    const opengl_vtable_t *vt = priv->vt;

    vt->Uniform1i(priv->uloc.Textures[0], 0);

    assert(priv->textures[0] != 0);
    vt->ActiveTexture(GL_TEXTURE0);
    vt->BindTexture(priv->tex_target, priv->textures[0]);

    vt->UniformMatrix3fv(priv->uloc.TexCoordsMaps[0], 1, GL_FALSE,
                         priv->var.TexCoordsMaps[0]);

    /* Return the expected transform matrix if interop == NULL */
    const GLfloat *tm = GetTransformMatrix(priv->interop);
    vt->UniformMatrix4fv(priv->uloc.TransformMatrix, 1, GL_FALSE, tm);

    vt->UniformMatrix4fv(priv->uloc.OrientationMatrix, 1, GL_FALSE,
                         priv->var.OrientationMatrix);
}

static int
xyz12_shader_init(struct vlc_gl_sampler *sampler)
{
    static const struct vlc_gl_sampler_ops ops = {
        .fetch_locations = sampler_xyz12_fetch_locations,
        .load = sampler_xyz12_load,
    };
    sampler->ops = &ops;

    /* Shader for XYZ to RGB correction
     * 3 steps :
     *  - XYZ gamma correction
     *  - XYZ to RGB matrix conversion
     *  - reverse RGB gamma correction
     */
    static const char *template =
        "uniform sampler2D Textures[1];"
        "uniform vec4 xyz_gamma = vec4(2.6);"
        "uniform vec4 rgb_gamma = vec4(1.0/2.2);"
        /* WARN: matrix Is filled column by column (not row !) */
        "uniform mat4 matrix_xyz_rgb = mat4("
        "    3.240454 , -0.9692660, 0.0556434, 0.0,"
        "   -1.5371385,  1.8760108, -0.2040259, 0.0,"
        "    -0.4985314, 0.0415560, 1.0572252,  0.0,"
        "    0.0,      0.0,         0.0,        1.0 "
        " );"

        "uniform mat4 TransformMatrix;\n"
        "uniform mat4 OrientationMatrix;\n"
        "uniform mat3 TexCoordsMaps[1];\n"
        "vec4 vlc_texture(vec2 pic_coords)\n"
        "{ "
        " vec4 v_in, v_out;"
        /* Homogeneous (oriented) coordinates */
        " vec3 pic_hcoords = vec3((TransformMatrix * OrientationMatrix * vec4(pic_coords, 0.0, 1.0)).st, 1.0);\n"
        " vec2 tex_coords = (TexCoordsMaps[0] * pic_hcoords).st;\n"
        " v_in  = texture2D(Textures[0], tex_coords);\n"
        " v_in = pow(v_in, xyz_gamma);"
        " v_out = matrix_xyz_rgb * v_in ;"
        " v_out = pow(v_out, rgb_gamma) ;"
        " v_out = clamp(v_out, 0.0, 1.0) ;"
        " return v_out;"
        "}\n";

    sampler->shader.body = strdup(template);
    if (!sampler->shader.body)
        return VLC_ENOMEM;

    return VLC_SUCCESS;
}

static int
opengl_init_swizzle(struct vlc_gl_sampler *sampler,
                    const char *swizzle_per_tex[],
                    vlc_fourcc_t chroma,
                    const vlc_chroma_description_t *desc)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);

    GLint oneplane_texfmt;
    if (vlc_gl_StrHasToken(priv->api->extensions, "GL_ARB_texture_rg"))
        oneplane_texfmt = GL_RED;
    else
        oneplane_texfmt = GL_LUMINANCE;

    if (desc->plane_count == 3)
        swizzle_per_tex[0] = swizzle_per_tex[1] = swizzle_per_tex[2] = "r";
    else if (desc->plane_count == 2)
    {
        if (oneplane_texfmt == GL_RED)
        {
            swizzle_per_tex[0] = "r";
            swizzle_per_tex[1] = "rg";
        }
        else
        {
            swizzle_per_tex[0] = "x";
            swizzle_per_tex[1] = "xa";
        }
    }
    else if (desc->plane_count == 1)
    {
        /*
         * Set swizzling in Y1 U V order
         * R  G  B  A
         * U  Y1 V  Y2 => GRB
         * Y1 U  Y2 V  => RGA
         * V  Y1 U  Y2 => GBR
         * Y1 V  Y2 U  => RAG
         */
        switch (chroma)
        {
            case VLC_CODEC_UYVY:
                swizzle_per_tex[0] = "grb";
                break;
            case VLC_CODEC_YUYV:
                swizzle_per_tex[0] = "rga";
                break;
            case VLC_CODEC_VYUY:
                swizzle_per_tex[0] = "gbr";
                break;
            case VLC_CODEC_YVYU:
                swizzle_per_tex[0] = "rag";
                break;
            default:
                assert(!"missing chroma");
                return VLC_EGENERIC;
        }
    }
    return VLC_SUCCESS;
}

static void
InitOrientationMatrix(GLfloat matrix[static 4*4],
                      video_orientation_t orientation)
{
    memcpy(matrix, MATRIX4_IDENTITY, sizeof(MATRIX4_IDENTITY));

/**
 * / C0R0  C1R0     0  C3R0 \
 * | C0R1  C1R1     0  C3R1 |
 * |    0     0     1     0 |  <-- keep the z coordinate unchanged
 * \    0     0     0     1 /
 *                  ^
 *                  |
 *                  z never impacts the orientation
 *
 * (note that in memory, the matrix is stored in column-major order)
 */
#define MATRIX_SET(C0R0, C1R0, C3R0, \
                   C0R1, C1R1, C3R1) \
    matrix[0*4 + 0] = C0R0; \
    matrix[1*4 + 0] = C1R0; \
    matrix[3*4 + 0] = C3R0; \
    matrix[0*4 + 1] = C0R1; \
    matrix[1*4 + 1] = C1R1; \
    matrix[3*4 + 1] = C3R1;

    /**
     * The following schemas show how the video picture is oriented in the
     * texture, according to the "orientation" value:
     *
     *     video         texture
     *    picture        storage
     *
     *     1---2          2---3
     *     |   |   --->   |   |
     *     4---3          1---4
     *
     * In addition, they show how the orientation transforms video picture
     * coordinates axis (x,y) into texture axis (X,Y):
     *
     *   y         --->         X
     *   |                      |
     *   +---x              Y---+
     *
     * The resulting coordinates undergo the reverse of the transformation
     * applied to the axis, so expressing (x,y) in terms of (X,Y) gives the
     * orientation matrix coefficients.
     */

    switch (orientation) {
        case ORIENT_ROTATED_90:
            /**
             *     1---2          2---3
             *   y |   |   --->   |   | X
             *   | 4---3          1---4 |
             *   +---x              Y---+
             *
             *          x = 1-Y
             *          y = X
             */
                     /* X  Y  1 */
            MATRIX_SET( 0,-1, 1, /* 1-Y */
                        1, 0, 0) /* X */
            break;
        case ORIENT_ROTATED_180:
            /**
             *                      X---+
             *     1---2          3---4 |
             *   y |   |   --->   |   | Y
             *   | 4---3          2---1
             *   +---x
             *
             *          x = 1-X
             *          y = 1-Y
             */
                     /* X  Y  1 */
            MATRIX_SET(-1, 0, 1, /* 1-X */
                        0,-1, 1) /* 1-Y */
            break;
        case ORIENT_ROTATED_270:
            /**
             *                    +---Y
             *     1---2          | 4---1
             *   y |   |   --->   X |   |
             *   | 4---3            3---2
             *   +---x
             *
             *          x = Y
             *          y = 1-X
             */
                     /* X  Y  1 */
            MATRIX_SET( 0, 1, 0, /* Y */
                       -1, 0, 1) /* 1-X */
            break;
        case ORIENT_HFLIPPED:
            /**
             *     1---2          2---1
             *   y |   |   --->   |   | Y
             *   | 4---3          3---4 |
             *   +---x              X---+
             *
             *          x = 1-X
             *          y = Y
             */
                     /* X  Y  1 */
            MATRIX_SET(-1, 0, 1, /* 1-X */
                        0, 1, 0) /* Y */
            break;
        case ORIENT_VFLIPPED:
            /**
             *                    +---X
             *     1---2          | 4---3
             *   y |   |   --->   Y |   |
             *   | 4---3            1---2
             *   +---x
             *
             *          x = X
             *          y = 1-Y
             */
                     /* X  Y  1 */
            MATRIX_SET( 1, 0, 0, /* X */
                        0,-1, 1) /* 1-Y */
            break;
        case ORIENT_TRANSPOSED:
            /**
             *                      Y---+
             *     1---2          1---4 |
             *   y |   |   --->   |   | X
             *   | 4---3          2---3
             *   +---x
             *
             *          x = 1-Y
             *          y = 1-X
             */
                     /* X  Y  1 */
            MATRIX_SET( 0,-1, 1, /* 1-Y */
                       -1, 0, 1) /* 1-X */
            break;
        case ORIENT_ANTI_TRANSPOSED:
            /**
             *     1---2            3---2
             *   y |   |   --->   X |   |
             *   | 4---3          | 4---1
             *   +---x            +---Y
             *
             *          x = Y
             *          y = X
             */
                     /* X  Y  1 */
            MATRIX_SET( 0, 1, 0, /* Y */
                        1, 0, 0) /* X */
            break;
        default:
            break;
    }
}

static void
GetNames(GLenum tex_target, const char **glsl_sampler, const char **texture)
{
    switch (tex_target)
    {
        case GL_TEXTURE_EXTERNAL_OES:
            *glsl_sampler = "samplerExternalOES";
            *texture = "texture2D";
            break;
        case GL_TEXTURE_2D:
            *glsl_sampler = "sampler2D";
            *texture = "texture2D";
            break;
        case GL_TEXTURE_RECTANGLE:
            *glsl_sampler = "sampler2DRect";
            *texture = "texture2DRect";
            break;
        default:
            vlc_assert_unreachable();
    }
}

static int
InitShaderExtensions(struct vlc_gl_sampler *sampler, GLenum tex_target)
{
    if (tex_target == GL_TEXTURE_EXTERNAL_OES)
    {
        sampler->shader.extensions =
            strdup("#extension GL_OES_EGL_image_external : require\n");
        if (!sampler->shader.extensions)
            return VLC_EGENERIC;
    }
    else
        sampler->shader.extensions = NULL;

    return VLC_SUCCESS;
}

static void
sampler_planes_fetch_locations(struct vlc_gl_sampler *sampler, GLuint program)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);

    const opengl_vtable_t *vt = priv->vt;

    priv->uloc.TransformMatrix =
        vt->GetUniformLocation(program, "TransformMatrix");
    assert(priv->uloc.TransformMatrix != -1);

    priv->uloc.OrientationMatrix =
        vt->GetUniformLocation(program, "OrientationMatrix");
    assert(priv->uloc.OrientationMatrix != -1);

    priv->uloc.Textures[0] = vt->GetUniformLocation(program, "Texture");
    assert(priv->uloc.Textures[0] != -1);

    priv->uloc.TexCoordsMaps[0] =
        vt->GetUniformLocation(program, "TexCoordsMap");
    assert(priv->uloc.TexCoordsMaps[0] != -1);

    if (priv->tex_target == GL_TEXTURE_RECTANGLE)
    {
        priv->uloc.TexSizes[0] = vt->GetUniformLocation(program, "TexSize");
        assert(priv->uloc.TexSizes[0] != -1);
    }
}

static void
sampler_planes_load(const struct vlc_gl_sampler *sampler)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);
    unsigned plane = priv->plane;

    const opengl_vtable_t *vt = priv->vt;

    vt->Uniform1i(priv->uloc.Textures[0], 0);

    assert(priv->textures[plane] != 0);
    vt->ActiveTexture(GL_TEXTURE0);
    vt->BindTexture(priv->tex_target, priv->textures[plane]);

    vt->UniformMatrix3fv(priv->uloc.TexCoordsMaps[0], 1, GL_FALSE,
                         priv->var.TexCoordsMaps[0]);

    /* Return the expected transform matrix if interop == NULL */
    const GLfloat *tm = GetTransformMatrix(priv->interop);
    vt->UniformMatrix4fv(priv->uloc.TransformMatrix, 1, GL_FALSE, tm);

    vt->UniformMatrix4fv(priv->uloc.OrientationMatrix, 1, GL_FALSE,
                         priv->var.OrientationMatrix);

    if (priv->tex_target == GL_TEXTURE_RECTANGLE)
    {
        vt->Uniform2f(priv->uloc.TexSizes[0], priv->tex_widths[plane],
                      priv->tex_heights[plane]);
    }
}

static char *
sampler_planes_GetFragmentPart(struct vlc_gl_sampler *sampler)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);
    GLenum tex_target = priv->tex_target;

    struct vlc_memstream ms;
    if (vlc_memstream_open(&ms))
        return NULL;

#define ADD(x) vlc_memstream_puts(&ms, x)
#define ADDF(x, ...) vlc_memstream_printf(&ms, x, ##__VA_ARGS__)

    const char *sampler_type;
    const char *texture_fn;
    GetNames(tex_target, &sampler_type, &texture_fn);

    ADDF("uniform %s Texture;\n", sampler_type);
    ADD("uniform mat3 TexCoordsMap;\n"
        "uniform mat4 TransformMatrix;\n"
        "uniform mat4 OrientationMatrix;\n");

    if (tex_target == GL_TEXTURE_RECTANGLE)
        ADD("uniform vec2 TexSize;\n");

    ADDF("#define vlc_texture(coords) %s(Texture, coords)\n", texture_fn);
    //ADD("vec4 vlc_texture(vec2 pic_coords) {\n"
    //    /* Homogeneous (oriented) coordinates */
    //    "  vec3 pic_hcoords = vec3((TransformMatrix * OrientationMatrix * vec4(pic_coords, 0.0, 1.0)).st, 1.0);\n"
    //    "  vec2 tex_coords = (TexCoordsMap * pic_hcoords).st;\n");

    //if (tex_target == GL_TEXTURE_RECTANGLE)
    //{
    //    /* The coordinates are in texels values, not normalized */
    //    ADD(" tex_coords = vec2(tex_coords.x * TexSize.x,\n"
    //        "                   tex_coords.y * TexSize.y);\n");
    //}

    //ADDF("  return %s(Texture, tex_coords);\n", texture_fn);
    //ADD("}\n");

#undef ADD
#undef ADDF

    if (vlc_memstream_close(&ms) != 0)
        return NULL;

    return ms.ptr;
}

char *sampler_planes_GetVertexPart(struct vlc_gl_sampler *sampler)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);
    GLenum tex_target = priv->tex_target;

    struct vlc_memstream ms;
    if (vlc_memstream_open(&ms))
        return NULL;

#define ADD(x) vlc_memstream_puts(&ms, x)
#define ADDF(x, ...) vlc_memstream_printf(&ms, x, ##__VA_ARGS__)

    const char *sampler_type;
    const char *texture_fn;
    GetNames(tex_target, &sampler_type, &texture_fn);

    ADDF("uniform %s Texture;\n", sampler_type);
    ADD("uniform mat3 TexCoordsMap;\n"
        "uniform mat4 TransformMatrix;\n"
        "uniform mat4 OrientationMatrix;\n");

    if (tex_target == GL_TEXTURE_RECTANGLE)
        ADD("uniform vec2 TexSize;\n");

    ADD("vec2 vlc_texture_coords(vec2 pic_coords) {\n"
        /* Homogeneous (oriented) coordinates */
        "  vec3 pic_hcoords = vec3((TransformMatrix * OrientationMatrix * vec4(pic_coords, 0.0, 1.0)).st, 1.0);\n"
        "  vec2 computed_coords = (TexCoordsMap * pic_hcoords).st;\n");

    if (tex_target == GL_TEXTURE_RECTANGLE)
    {
        /* The coordinates are in texels values, not normalized */
        ADD("  computed_coords = vec2(computed_coords.x * TexSize.x,\n"
            "                         computed_coords.y * TexSize.y);\n");
    }

    ADDF("  return computed_coords;\n");
    ADD("}\n");

#undef ADD
#undef ADDF

    if (vlc_memstream_close(&ms) != 0)
        return NULL;

    return ms.ptr;
}

static int
sampler_planes_init(struct vlc_gl_sampler *sampler)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);
    GLenum tex_target = priv->tex_target;

    sampler->shader.body = sampler_planes_GetFragmentPart(sampler);
    sampler->shader.vertex_body = sampler_planes_GetVertexPart(sampler);

    if (!sampler->shader.body || !sampler->shader.vertex_body)
        goto error;

    int ret = InitShaderExtensions(sampler, tex_target);
    if (ret != VLC_SUCCESS)
        goto error;

    static const struct vlc_gl_sampler_ops ops = {
        .fetch_locations = sampler_planes_fetch_locations,
        .load = sampler_planes_load,
    };
    sampler->ops = &ops;

    return VLC_SUCCESS;

error:
    free(sampler->shader.body);
    free(sampler->shader.vertex_body);
    return VLC_EGENERIC;
}

static const char *
opengl_shader_GetVertexPart(struct vlc_gl_sampler *sampler, bool is_yuv)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);
    GLenum tex_target = priv->tex_target;

    struct vlc_memstream ms;
    if (vlc_memstream_open(&ms) != 0)
        return NULL;

#define ADD(x) vlc_memstream_puts(&ms, x)
#define ADDF(x, ...) vlc_memstream_printf(&ms, x, ##__VA_ARGS__)

    ADDF("uniform mat3 TexCoordsMaps[%u];\n", sampler->tex_count);
    if (tex_target == GL_TEXTURE_RECTANGLE)
        ADDF("uniform vec2 TexSizes[%u];\n", sampler->tex_count);

    ADD("uniform mat4 TransformMatrix;\n"
        "uniform mat4 OrientationMatrix;\n"

        "vec2 vlc_texture_coords(vec2 pic_coords) {\n"
        "  vec2 pic_hcoords = (TransformMatrix * OrientationMatrix * vec4(pic_coords, 0.0, 1.0)).st;\n");

    if (!is_yuv)
    {
        ADD("  pic_hcoords = (TexCoordsMaps[0] * vec3(pic_hcoords, 1.0)).st;\n");
        if (tex_target == GL_TEXTURE_RECTANGLE)
            ADD("   pic_hcoords *= TexSizes[0];\n");
    }

    ADD("  return pic_hcoords;\n"
        "}\n");

    if (vlc_memstream_close(&ms) != 0)
        return NULL;

    return ms.ptr;
}

static int
opengl_fragment_shader_init(struct vlc_gl_sampler *sampler, GLenum tex_target,
                            const video_format_t *fmt, bool expose_planes)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);

    priv->tex_target = tex_target;
    priv->expose_planes = expose_planes;
    priv->plane = 0;

    vlc_fourcc_t chroma = fmt->i_chroma;
    video_color_space_t yuv_space = fmt->space;
    video_orientation_t orientation = fmt->orientation;

    const char *swizzle_per_tex[PICTURE_PLANE_MAX] = { NULL, };
    const bool is_yuv = vlc_fourcc_IsYUV(chroma);
    int ret;

    const vlc_chroma_description_t *desc = vlc_fourcc_GetChromaDescription(chroma);
    if (desc == NULL)
        return VLC_EGENERIC;

    unsigned tex_count = desc->plane_count;
    sampler->tex_count = tex_count;

    InitOrientationMatrix(priv->var.OrientationMatrix, orientation);

    if (expose_planes)
        return sampler_planes_init(sampler);

    if (chroma == VLC_CODEC_XYZ12)
        return xyz12_shader_init(sampler);

    if (is_yuv)
    {
        ret = sampler_yuv_base_init(sampler, chroma, desc, yuv_space);
        if (ret != VLC_SUCCESS)
            return ret;
        ret = opengl_init_swizzle(sampler, swizzle_per_tex, chroma, desc);
        if (ret != VLC_SUCCESS)
            return ret;
    }

    const char *glsl_sampler, *lookup;
    GetNames(tex_target, &glsl_sampler, &lookup);

    struct vlc_memstream ms;
    if (vlc_memstream_open(&ms) != 0)
        return VLC_EGENERIC;

#define ADD(x) vlc_memstream_puts(&ms, x)
#define ADDF(x, ...) vlc_memstream_printf(&ms, x, ##__VA_ARGS__)

    ADD("uniform mat4 TransformMatrix;\n"
        "uniform mat4 OrientationMatrix;\n");
    ADDF("uniform %s Textures[%u];\n", glsl_sampler, tex_count);
    ADDF("uniform mat3 TexCoordsMaps[%u];\n", tex_count);

#ifdef HAVE_LIBPLACEBO
    if (priv->pl_sh) {
        struct pl_shader *sh = priv->pl_sh;
        struct pl_color_map_params color_params = pl_color_map_default_params;
        color_params.intent = var_InheritInteger(priv->gl, "rendering-intent");
        color_params.tone_mapping_algo = var_InheritInteger(priv->gl, "tone-mapping");
        color_params.tone_mapping_param = var_InheritFloat(priv->gl, "tone-mapping-param");
#    if PL_API_VER >= 10
        color_params.desaturation_strength = var_InheritFloat(priv->gl, "desat-strength");
        color_params.desaturation_exponent = var_InheritFloat(priv->gl, "desat-exponent");
        color_params.desaturation_base = var_InheritFloat(priv->gl, "desat-base");
#    else
        color_params.tone_mapping_desaturate = var_InheritFloat(priv->gl, "tone-mapping-desat");
#    endif
        color_params.gamut_warning = var_InheritBool(priv->gl, "tone-mapping-warn");

        struct pl_color_space dst_space = pl_color_space_unknown;
        dst_space.primaries = var_InheritInteger(priv->gl, "target-prim");
        dst_space.transfer = var_InheritInteger(priv->gl, "target-trc");

        pl_shader_color_map(sh, &color_params,
                vlc_placebo_ColorSpace(fmt),
                dst_space, NULL, false);

        struct pl_shader_obj *dither_state = NULL;
        int method = var_InheritInteger(priv->gl, "dither-algo");
        if (method >= 0) {

            unsigned out_bits = 0;
            int override = var_InheritInteger(priv->gl, "dither-depth");
            if (override > 0)
                out_bits = override;
            else
            {
                GLint fb_depth = 0;
#if !defined(USE_OPENGL_ES2)
                const opengl_vtable_t *vt = priv->vt;
                /* fetch framebuffer depth (we are already bound to the default one). */
                if (vt->GetFramebufferAttachmentParameteriv != NULL)
                    vt->GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_BACK_LEFT,
                                                            GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE,
                                                            &fb_depth);
#endif
                if (fb_depth <= 0)
                    fb_depth = 8;
                out_bits = fb_depth;
            }

            pl_shader_dither(sh, out_bits, &dither_state, &(struct pl_dither_params) {
                .method   = method,
                .lut_size = 4, // avoid too large values, since this gets embedded
            });
        }

        const struct pl_shader_res *res = priv->pl_sh_res = pl_shader_finalize(sh);
        pl_shader_obj_destroy(&dither_state);

        FREENULL(priv->uloc.pl_vars);
        priv->uloc.pl_vars = calloc(res->num_variables, sizeof(GLint));
        for (int i = 0; i < res->num_variables; i++) {
            struct pl_shader_var sv = res->variables[i];
            const char *glsl_type_name = pl_var_glsl_type_name(sv.var);
            ADDF("uniform %s %s;\n", glsl_type_name, sv.var.name);
        }

        // We can't handle these yet, but nothing we use requires them, either
        assert(res->num_vertex_attribs == 0);
        assert(res->num_descriptors == 0);

        ADD(res->glsl);
    }
#else
    if (fmt->transfer == TRANSFER_FUNC_SMPTE_ST2084 ||
        fmt->primaries == COLOR_PRIMARIES_BT2020)
    {
        // no warning for HLG because it's more or less backwards-compatible
        msg_Warn(priv->gl, "VLC needs to be built with support for libplacebo "
                 "in order to display wide gamut or HDR signals correctly.");
    }
#endif

    if (tex_target == GL_TEXTURE_RECTANGLE)
        ADDF("uniform vec2 TexSizes[%u];\n", tex_count);

    if (is_yuv)
        ADD("uniform mat4 ConvMatrix;\n");

    ADD("vec4 vlc_texture(vec2 pic_coords) {\n"
        /* Homogeneous (oriented) coordinates */
        " vec3 pic_hcoords = vec3(pic_coords, 1.0);\n"
        " vec2 tex_coords;\n");

    unsigned color_count;
    if (is_yuv) {
        ADD(" vec4 texel;\n"
            " vec4 pixel = vec4(0.0, 0.0, 0.0, 1.0);\n");
        unsigned color_idx = 0;
        for (unsigned i = 0; i < tex_count; ++i)
        {
            const char *swizzle = swizzle_per_tex[i];
            assert(swizzle);
            size_t swizzle_count = strlen(swizzle);
            ADDF(" tex_coords = (TexCoordsMaps[%u] * pic_hcoords).st;\n", i);
            if (tex_target == GL_TEXTURE_RECTANGLE)
            {
                /* The coordinates are in texels values, not normalized */
                ADDF(" tex_coords = vec2(tex_coords.x * TexSizes[%u].x,\n"
                     "                   tex_coords.y * TexSizes[%u].y);\n", i, i);
            }
            ADDF(" texel = %s(Textures[%u], tex_coords);\n", lookup, i);
            for (unsigned j = 0; j < swizzle_count; ++j)
            {
                ADDF(" pixel[%u] = texel.%c;\n", color_idx, swizzle[j]);
                color_idx++;
                assert(color_idx <= PICTURE_PLANE_MAX);
            }
        }
        ADD(" vec4 result = ConvMatrix * pixel;\n");
        color_count = color_idx;
    }
    else
    {
        ADDF(" vec4 result = %s(Textures[0], pic_coords);\n", lookup);
        color_count = 1;
    }
    assert(yuv_space == COLOR_SPACE_UNDEF || color_count == 3);

#ifdef HAVE_LIBPLACEBO
    if (priv->pl_sh_res) {
        const struct pl_shader_res *res = priv->pl_sh_res;
        assert(res->input  == PL_SHADER_SIG_COLOR);
        assert(res->output == PL_SHADER_SIG_COLOR);
        ADDF(" result = %s(result);\n", res->name);
    }
#endif

    ADD(" return result;\n"
        "}\n");

#undef ADD
#undef ADDF

    if (vlc_memstream_close(&ms) != 0)
        return VLC_EGENERIC;

    ret = InitShaderExtensions(sampler, tex_target);
    if (ret != VLC_SUCCESS)
    {
        free(ms.ptr);
        return VLC_EGENERIC;
    }
    sampler->shader.body = ms.ptr;

    sampler->shader.vertex_body = opengl_shader_GetVertexPart(sampler, is_yuv);

    static const struct vlc_gl_sampler_ops ops = {
        .fetch_locations = sampler_base_fetch_locations,
        .load = sampler_base_load,
    };
    sampler->ops = &ops;

    return VLC_SUCCESS;
}

static struct vlc_gl_sampler *
CreateSampler(struct vlc_gl_interop *interop, struct vlc_gl_t *gl,
              const struct vlc_gl_api *api, const video_format_t *fmt,
              unsigned tex_target, bool expose_planes)
{
    struct vlc_gl_sampler_priv *priv = calloc(1, sizeof(*priv));
    if (!priv)
        return NULL;

    struct vlc_gl_sampler *sampler = &priv->sampler;

    priv->uloc.pl_vars = NULL;
    priv->pl_ctx = NULL;
    priv->pl_sh = NULL;
    priv->pl_sh_res = NULL;

    priv->interop = interop;
    priv->gl = gl;
    priv->api = api;
    priv->vt = &api->vt;

    /* Formats with palette are not supported. This also allows to copy
     * video_format_t without possibility of failure. */
    assert(!sampler->fmt.p_palette);

    sampler->fmt = *fmt;

    sampler->shader.extensions = NULL;
    sampler->shader.body = NULL;
    sampler->shader.vertex_body = NULL;

    /* Expose the texture sizes publicly */
    sampler->tex_widths = priv->tex_widths;
    sampler->tex_heights = priv->tex_heights;

#ifdef HAVE_LIBPLACEBO
    // Create the main libplacebo context
    priv->pl_ctx = vlc_placebo_CreateContext(VLC_OBJECT(gl));
    if (priv->pl_ctx) {
#   if PL_API_VER >= 20
        priv->pl_sh = pl_shader_alloc(priv->pl_ctx, &(struct pl_shader_params) {
            .glsl = {
#       ifdef USE_OPENGL_ES2
                .version = 100,
                .gles = true,
#       else
                .version = 120,
#       endif
            },
        });
#   elif PL_API_VER >= 6
        priv->pl_sh = pl_shader_alloc(priv->pl_ctx, NULL, 0);
#   else
        priv->pl_sh = pl_shader_alloc(priv->pl_ctx, NULL, 0, 0);
#   endif
    }
#endif

    int ret = opengl_fragment_shader_init(sampler, tex_target, fmt,
                                          expose_planes);
    if (ret != VLC_SUCCESS)
    {
        free(sampler);
        return NULL;
    }

    unsigned tex_count = sampler->tex_count;
    assert(!interop || interop->tex_count == tex_count);

    for (unsigned i = 0; i < tex_count; ++i)
    {
        /* This might be updated in UpdatePicture for non-direct samplers */
        memcpy(&priv->var.TexCoordsMaps[i], MATRIX3_IDENTITY,
               sizeof(MATRIX3_IDENTITY));
    }

    if (interop)
    {
        /* Texture size */
        for (unsigned j = 0; j < interop->tex_count; j++) {
            const GLsizei w = interop->fmt_out.i_visible_width  * interop->texs[j].w.num
                            / interop->texs[j].w.den;
            const GLsizei h = interop->fmt_out.i_visible_height * interop->texs[j].h.num
                            / interop->texs[j].h.den;
            priv->visible_widths[j] = w;
            priv->visible_heights[j] = h;
            if (interop->api->supports_npot) {
                priv->tex_widths[j]  = w;
                priv->tex_heights[j] = h;
            } else {
                priv->tex_widths[j]  = vlc_align_pot(w);
                priv->tex_heights[j] = vlc_align_pot(h);
            }
        }

        if (!interop->handle_texs_gen)
        {
            ret = vlc_gl_interop_GenerateTextures(interop, priv->tex_widths,
                                                  priv->tex_heights,
                                                  priv->textures);
            if (ret != VLC_SUCCESS)
            {
                free(sampler);
                return NULL;
            }
        }
    }

    return sampler;
}

struct vlc_gl_sampler *
vlc_gl_sampler_NewFromInterop(struct vlc_gl_interop *interop,
                              bool expose_planes)
{
    return CreateSampler(interop, interop->gl, interop->api, &interop->fmt_out,
                         interop->tex_target, expose_planes);
}

struct vlc_gl_sampler *
vlc_gl_sampler_NewFromTexture2D(struct vlc_gl_t *gl,
                                const struct vlc_gl_api *api,
                                const video_format_t *fmt, bool expose_planes)
{
    return CreateSampler(NULL, gl, api, fmt, GL_TEXTURE_2D, expose_planes);
}

void
vlc_gl_sampler_Delete(struct vlc_gl_sampler *sampler)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);

    struct vlc_gl_interop *interop = priv->interop;
    if (interop && !interop->handle_texs_gen)
    {
        const opengl_vtable_t *vt = interop->vt;
        vt->DeleteTextures(interop->tex_count, priv->textures);
    }

#ifdef HAVE_LIBPLACEBO
    FREENULL(priv->uloc.pl_vars);
    if (priv->pl_ctx)
        pl_context_destroy(&priv->pl_ctx);
#endif

    free(sampler->shader.extensions);
    free(sampler->shader.body);
    free(sampler->shader.vertex_body);

    free(priv);
}

int
vlc_gl_sampler_UpdatePicture(struct vlc_gl_sampler *sampler, picture_t *picture)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);

    const struct vlc_gl_interop *interop = priv->interop;
    assert(interop);

    const video_format_t *source = &picture->format;

    if (source->i_x_offset != priv->last_source.i_x_offset
     || source->i_y_offset != priv->last_source.i_y_offset
     || source->i_visible_width != priv->last_source.i_visible_width
     || source->i_visible_height != priv->last_source.i_visible_height)
    {
        memset(priv->var.TexCoordsMaps, 0, sizeof(priv->var.TexCoordsMaps));
        for (unsigned j = 0; j < interop->tex_count; j++)
        {
            float scale_w = (float)interop->texs[j].w.num / interop->texs[j].w.den
                          / priv->tex_widths[j];
            float scale_h = (float)interop->texs[j].h.num / interop->texs[j].h.den
                          / priv->tex_heights[j];

            /* Warning: if NPOT is not supported a larger texture is
               allocated. This will cause right and bottom coordinates to
               land on the edge of two texels with the texels to the
               right/bottom uninitialized by the call to
               glTexSubImage2D. This might cause a green line to appear on
               the right/bottom of the display.
               There are two possible solutions:
               - Manually mirror the edges of the texture.
               - Add a "-1" when computing right and bottom, however the
               last row/column might not be displayed at all.
            */
            float left   = (source->i_x_offset +                       0 ) * scale_w;
            float top    = (source->i_y_offset +                       0 ) * scale_h;
            float right  = (source->i_x_offset + source->i_visible_width ) * scale_w;
            float bottom = (source->i_y_offset + source->i_visible_height) * scale_h;

            /**
             * This matrix converts from picture coordinates (in range [0; 1])
             * to textures coordinates where the picture is actually stored
             * (removing paddings).
             *
             *        texture           (in texture coordinates)
             *       +----------------+--- 0.0
             *       |                |
             *       |  +---------+---|--- top
             *       |  | picture |   |
             *       |  +---------+---|--- bottom
             *       |  .         .   |
             *       |  .         .   |
             *       +----------------+--- 1.0
             *       |  .         .   |
             *      0.0 left  right  1.0  (in texture coordinates)
             *
             * In particular:
             *  - (0.0, 0.0) is mapped to (left, top)
             *  - (1.0, 1.0) is mapped to (right, bottom)
             *
             * This is an affine 2D transformation, so the input coordinates
             * are given as a 3D vector in the form (x, y, 1), and the output
             * is (x', y', 1).
             *
             * The paddings are l (left), r (right), t (top) and b (bottom).
             *
             *               / (r-l)   0     l \
             *      matrix = |   0   (b-t)   t |
             *               \   0     0     1 /
             *
             * It is stored in column-major order.
             */
            GLfloat *matrix = priv->var.TexCoordsMaps[j];
#define COL(x) (x*3)
#define ROW(x) (x)
            matrix[COL(0) + ROW(0)] = right - left;
            matrix[COL(1) + ROW(1)] = bottom - top;
            matrix[COL(2) + ROW(0)] = left;
            matrix[COL(2) + ROW(1)] = top;
            matrix[COL(2) + ROW(2)] = 1;
#undef COL
#undef ROW
        }

        priv->last_source.i_x_offset = source->i_x_offset;
        priv->last_source.i_y_offset = source->i_y_offset;
        priv->last_source.i_visible_width = source->i_visible_width;
        priv->last_source.i_visible_height = source->i_visible_height;
    }

    /* Update the texture */
    return interop->ops->update_textures(interop, priv->textures,
                                         priv->visible_widths,
                                         priv->visible_heights, picture, NULL);
}

int
vlc_gl_sampler_UpdateTextures(struct vlc_gl_sampler *sampler, GLuint textures[],
                              GLsizei tex_widths[], GLsizei tex_heights[])
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);
    assert(!priv->interop);

    unsigned tex_count = sampler->tex_count;
    memcpy(priv->textures, textures, tex_count * sizeof(textures[0]));
    memcpy(priv->tex_widths, tex_widths, tex_count * sizeof(tex_widths[0]));
    memcpy(priv->tex_heights, tex_heights, tex_count * sizeof(tex_heights[0]));

    return VLC_SUCCESS;
}

void
vlc_gl_sampler_SelectPlane(struct vlc_gl_sampler *sampler, unsigned plane)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);
    priv->plane = plane;
}
