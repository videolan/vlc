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
#include <vlc_fs.h>
#include <vlc_opengl_filter.h>
#include <vlc_opengl_platform.h>
#include <vlc_filter.h>

#include "internal.h"
#include "sampler_priv.h"

#ifdef HAVE_LIBPLACEBO_GL
#include <libplacebo/opengl.h>
#endif

bool vlc_gl_HasExtension(
    struct vlc_gl_extension_vt *vt,
    const char *name
){
    if (vt->GetStringi == NULL)
    {
        const GLubyte *extensions = vt->GetString(GL_EXTENSIONS);
        return vlc_gl_StrHasToken((const char *)extensions, name);
    }

    int32_t count = 0;
    vt->GetIntegerv(GL_NUM_EXTENSIONS, &count);
    for (int i = 0; i < count; ++i)
    {
        const uint8_t *extension = vt->GetStringi(GL_EXTENSIONS, i);
        if (strcmp((const char *)extension, name) == 0)
            return true;
    }
    return false;
}

unsigned vlc_gl_GetVersionMajor(struct vlc_gl_extension_vt *vt)
{
    GLint version;
    vt->GetIntegerv(GL_MAJOR_VERSION, &version);
    uint32_t error = vt->GetError();

    if (error != GL_NO_ERROR)
        version = 2;

    /* Drain the errors before continuing. */
    while (error != GL_NO_ERROR)
        error = vt->GetError();

    return version;
}

void vlc_gl_LoadExtensionFunctions(vlc_gl_t *gl, struct vlc_gl_extension_vt *vt)
{
    vt->GetString = vlc_gl_GetProcAddress(gl, "glGetString");
    vt->GetIntegerv = vlc_gl_GetProcAddress(gl, "glGetIntegerv");
    vt->GetError = vlc_gl_GetProcAddress(gl, "glGetError");
    vt->GetStringi = NULL;

    unsigned version = vlc_gl_GetVersionMajor(vt);

    /* glGetStringi is available in OpenGL>=3 and GLES>=3.
     * https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glGetString.xhtml
     * https://www.khronos.org/registry/OpenGL-Refpages/es3/html/glGetString.xhtml
     */
    if (version >= 3)
        vt->GetStringi = vlc_gl_GetProcAddress(gl, "glGetStringi");
}

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
    GET_PROC_ADDR(Uniform3fv);
    GET_PROC_ADDR(Uniform2fv);
    GET_PROC_ADDR(Uniform1fv);
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

    struct vlc_gl_extension_vt extension_vt;
    vlc_gl_LoadExtensionFunctions(gl, &extension_vt);

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
        api->supports_npot = vlc_gl_HasExtension(&extension_vt, "GL_ARB_texture_non_power_of_two") ||
                             vlc_gl_HasExtension(&extension_vt, "GL_APPLE_texture_2D_limited_npot");
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

#if 0
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
#endif

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

module_t *
vlc_gl_WrapOpenGLFilter(filter_t *filter, const char *opengl_filter_name)
{
    const config_chain_t *prev_chain = filter->p_cfg;
    var_Create(filter, "opengl-filter", VLC_VAR_STRING);
    var_SetString(filter, "opengl-filter", opengl_filter_name);

    filter->p_cfg = NULL;
    module_t *module = module_need(filter, "video filter", "opengl", true);
    filter->p_cfg = prev_chain;

    var_Destroy(filter, "opengl-filter");
    return module;
}

struct vlc_gl_interop_private
{
    struct vlc_gl_interop interop;

#define OPENGL_VTABLE_F(X) \
        X(PFNGLDELETETEXTURESPROC, DeleteTextures) \
        X(PFNGLGENTEXTURESPROC,    GenTextures) \
        X(PFNGLBINDTEXTUREPROC,    BindTexture) \
        X(PFNGLTEXIMAGE2DPROC,     TexImage2D) \
        X(PFNGLTEXENVFPROC,        TexEnvf) \
        X(PFNGLTEXPARAMETERFPROC,  TexParameterf) \
        X(PFNGLTEXPARAMETERIPROC,  TexParameteri) \
        X(PFNGLGETERRORPROC,       GetError) \
        X(PFNGLGETTEXLEVELPARAMETERIVPROC, GetTexLevelParameteriv) \

    struct {
#define DECLARE_SYMBOL(type, name) type name;
        OPENGL_VTABLE_F(DECLARE_SYMBOL)
    } gl;
};


static int GetTexFormatSize(struct vlc_gl_interop *interop, int target,
                            int tex_format, int tex_internal, int tex_type);

struct vlc_gl_interop *
vlc_gl_interop_New(struct vlc_gl_t *gl, vlc_video_context *context,
                   const video_format_t *fmt)
{
    struct vlc_gl_interop_private *priv = vlc_object_create(gl, sizeof *priv);
    if (priv == NULL)
        return NULL;

    struct vlc_gl_interop *interop = &priv->interop;

    interop->get_tex_format_size = GetTexFormatSize;
    interop->ops = NULL;
    interop->fmt_out = interop->fmt_in = *fmt;
    interop->gl = gl;
    /* this is the only allocated field, and we don't need it */
    interop->fmt_out.p_palette = interop->fmt_in.p_palette = NULL;

    const vlc_chroma_description_t *desc =
        vlc_fourcc_GetChromaDescription(fmt->i_chroma);

    if (desc == NULL)
    {
        vlc_object_delete(interop);
        return NULL;
    }

#define LOAD_SYMBOL(type, name) \
    priv->gl.name = vlc_gl_GetProcAddress(interop->gl, "gl" # name);
    OPENGL_VTABLE_F(LOAD_SYMBOL);
#undef LOAD_SYMBOL

    if (desc->plane_count == 0)
    {
        /* Opaque chroma: load a module to handle it */
        assert(context);
        interop->vctx = vlc_video_context_Hold(context);
        interop->module = module_need_var(interop, "glinterop", "glinterop");
    }
    else
    {
        interop->vctx = NULL;
        interop->module = module_need(interop, "opengl sw interop", NULL, false);
    }

    if (interop->module == NULL)
        goto error;

    return interop;

error:
    vlc_object_delete(interop);
    return NULL;
}

struct vlc_gl_interop *
vlc_gl_interop_NewForSubpictures(struct vlc_gl_t *gl)
{
    struct vlc_gl_interop_private *priv = vlc_object_create(gl, sizeof(*priv));
    if (!priv)
        return NULL;

    struct vlc_gl_interop *interop = &priv->interop;
    interop->ops = NULL;
    interop->gl = gl;

    video_format_Init(&interop->fmt_in, VLC_CODEC_RGB32);
    interop->fmt_out = interop->fmt_in;

#define LOAD_SYMBOL(type, name) \
    priv->gl.name = vlc_gl_GetProcAddress(interop->gl, "gl" # name);
    OPENGL_VTABLE_F(LOAD_SYMBOL);
#undef LOAD_SYMBOL

    interop->module = module_need(interop, "opengl sw interop", "sw", true);

    if (interop->module == NULL)
        goto error;

    return interop;
error:
    vlc_object_delete(interop);
    return NULL;
}



void vlc_gl_interop_Delete(struct vlc_gl_interop *interop)
{
    if (interop->ops && interop->ops->close)
        interop->ops->close(interop);
    if (interop->vctx)
        vlc_video_context_Release(interop->vctx);
    if (interop->module)
        module_unneed(interop, interop->module);
    vlc_object_delete(interop);
}

int
vlc_gl_interop_GenerateTextures(const struct vlc_gl_interop *interop,
                                const size_t *tex_width,
                                const size_t *tex_height, unsigned *textures)
{
    struct vlc_gl_interop_private *priv =
        container_of(interop, struct vlc_gl_interop_private, interop);

    priv->gl.GenTextures(interop->tex_count, textures);

    for (unsigned i = 0; i < interop->tex_count; i++)
    {
        priv->gl.BindTexture(interop->tex_target, textures[i]);

#if !defined(USE_OPENGL_ES2)
        /* Set the texture parameters */
        priv->gl.TexParameterf(interop->tex_target, GL_TEXTURE_PRIORITY, 1.0);
        priv->gl.TexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
#endif

        priv->gl.TexParameteri(interop->tex_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        priv->gl.TexParameteri(interop->tex_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        priv->gl.TexParameteri(interop->tex_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        priv->gl.TexParameteri(interop->tex_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    if (interop->ops->allocate_textures != NULL)
    {
        int ret = interop->ops->allocate_textures(interop, textures, tex_width, tex_height);
        if (ret != VLC_SUCCESS)
        {
            priv->gl.DeleteTextures(interop->tex_count, textures);
            memset(textures, 0, interop->tex_count * sizeof(GLuint));
            return ret;
        }
    }
    return VLC_SUCCESS;
}

void
vlc_gl_interop_DeleteTextures(const struct vlc_gl_interop *interop,
                              GLuint *textures)
{
    struct vlc_gl_interop_private *priv =
        container_of(interop, struct vlc_gl_interop_private, interop);
    priv->gl.DeleteTextures(interop->tex_count, textures);
    memset(textures, 0, interop->tex_count * sizeof(GLuint));
}

static int GetTexFormatSize(struct vlc_gl_interop *interop, int target,
                            int tex_format, int tex_internal, int tex_type)
{
    struct vlc_gl_interop_private *priv =
        container_of(interop, struct vlc_gl_interop_private, interop);

    GL_ASSERT_NOERROR(&priv->gl);

    if (!priv->gl.GetTexLevelParameteriv)
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

    priv->gl.GenTextures(1, &texture);
    priv->gl.BindTexture(target, texture);
    priv->gl.TexImage2D(target, 0, tex_internal, 64, 64, 0, tex_format, tex_type, NULL);
    GLint size = 0;
    priv->gl.GetTexLevelParameteriv(target, 0, tex_param_size, &size);

    priv->gl.DeleteTextures(1, &texture);

    bool has_error = false;
    while (priv->gl.GetError() != GL_NO_ERROR)
        has_error = true;

    if (has_error)
        return -1;

    return size > 0 ? size * mul : size;
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

#undef HAVE_LIBPLACEBO
#undef HAVE_LIBPLACEBO_GL

#ifdef HAVE_LIBPLACEBO
#include <libplacebo/opengl.h>
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

    struct vlc_gl_picture pic;

    struct {
        GLint Textures[PICTURE_PLANE_MAX];
        GLint TexSizes[PICTURE_PLANE_MAX]; /* for GL_TEXTURE_RECTANGLE */
        GLint ConvMatrix;
        GLint *pl_vars, *pl_descs; /* for pl_sh_res */
    } uloc;

    bool yuv_color;
    GLfloat conv_matrix[4*4];

#ifdef HAVE_LIBPLACEBO_GL
    /* libplacebo context */
    pl_log pl_log;
    pl_opengl pl_opengl;
    pl_shader pl_sh;
    pl_shader_obj dither_state, tone_map_state, lut_state;
    const struct pl_shader_res *pl_sh_res;
#endif

    /* If set, vlc_texture() exposes a single plane (without chroma
     * conversion), selected by vlc_gl_sampler_SetCurrentPlane(). */
    bool expose_planes;
    unsigned plane;

    struct vlc_gl_extension_vt extension_vt;
};

static inline struct vlc_gl_sampler_priv *
PRIV(struct vlc_gl_sampler *sampler)
{
    return container_of(sampler, struct vlc_gl_sampler_priv, sampler);
}

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

    struct vlc_gl_format *glfmt = &sampler->glfmt;

    assert(glfmt->tex_count < 10); /* to guarantee variable names length */
    for (unsigned int i = 0; i < glfmt->tex_count; ++i)
    {
        char name[sizeof("Textures[X]")];
        snprintf(name, sizeof(name), "Textures[%1u]", i);
        priv->uloc.Textures[i] = vt->GetUniformLocation(program, name);
        assert(priv->uloc.Textures[i] != -1);

        if (glfmt->tex_target == GL_TEXTURE_RECTANGLE)
        {
            snprintf(name, sizeof(name), "TexSizes[%1u]", i);
            priv->uloc.TexSizes[i] = vt->GetUniformLocation(program, name);
            assert(priv->uloc.TexSizes[i] != -1);
        }
    }

#ifdef HAVE_LIBPLACEBO_GL
    const struct pl_shader_res *res = priv->pl_sh_res;
    for (int i = 0; res && i < res->num_variables; i++) {
        struct pl_shader_var sv = res->variables[i];
        priv->uloc.pl_vars[i] = vt->GetUniformLocation(program, sv.var.name);
    }

    for (int i = 0; res && i < res->num_descriptors; i++) {
        struct pl_shader_desc sd = res->descriptors[i];
        priv->uloc.pl_descs[i] = vt->GetUniformLocation(program, sd.desc.name);
    }
#endif
}

static void
sampler_base_load(struct vlc_gl_sampler *sampler)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);

    const opengl_vtable_t *vt = priv->vt;
    const struct vlc_gl_format *glfmt = &sampler->glfmt;
    struct vlc_gl_picture *pic = &priv->pic;

    if (priv->yuv_color)
        vt->UniformMatrix4fv(priv->uloc.ConvMatrix, 1, GL_FALSE,
                             priv->conv_matrix);

    for (unsigned i = 0; i < glfmt->tex_count; ++i)
    {
        vt->Uniform1i(priv->uloc.Textures[i], i);

        assert(pic->textures[i] != 0);
        vt->ActiveTexture(GL_TEXTURE0 + i);
        vt->BindTexture(glfmt->tex_target, pic->textures[i]);
    }

    if (glfmt->tex_target == GL_TEXTURE_RECTANGLE)
    {
        for (unsigned i = 0; i < glfmt->tex_count; ++i)
            vt->Uniform2f(priv->uloc.TexSizes[i], glfmt->tex_widths[i],
                          glfmt->tex_heights[i]);
    }

#ifdef HAVE_LIBPLACEBO_GL
    const struct pl_shader_res *res = priv->pl_sh_res;
    for (int i = 0; res && i < res->num_variables; i++) {
        GLint loc = priv->uloc.pl_vars[i];
        if (loc == -1) // uniform optimized out
            continue;

        struct pl_shader_var sv = res->variables[i];
        struct pl_var var = sv.var;
        // libplacebo doesn't need anything else anyway
        assert(var.type == PL_VAR_FLOAT);
        assert(var.dim_m == 1 || var.dim_m == var.dim_v);

        const float *f = sv.data;
        switch (var.dim_m) {
        case 4: vt->UniformMatrix4fv(loc, var.dim_a, GL_FALSE, f); break;
        case 3: vt->UniformMatrix3fv(loc, var.dim_a, GL_FALSE, f); break;
        case 2: vt->UniformMatrix2fv(loc, var.dim_a, GL_FALSE, f); break;
        case 1:
            switch (var.dim_v) {
            case 1: vt->Uniform1fv(loc, var.dim_a, f); break;
            case 2: vt->Uniform2fv(loc, var.dim_a, f); break;
            case 3: vt->Uniform3fv(loc, var.dim_a, f); break;
            case 4: vt->Uniform4fv(loc, var.dim_a, f); break;
            }
            break;
        }
    }
    for (int i = 0; res && i < res->num_descriptors; i++) {
        GLint loc = priv->uloc.pl_descs[i];
        if (loc == -1)
            continue;
        struct pl_shader_desc sd = res->descriptors[i];
        assert(sd.desc.type == PL_DESC_SAMPLED_TEX);
        pl_tex tex = sd.binding.object;
        int texid = glfmt->tex_count + i; // first free texture unit
        unsigned gltex, target;
        gltex = pl_opengl_unwrap(priv->pl_opengl->gpu, tex, &target, NULL, NULL);
        vt->Uniform1i(loc, texid);
        vt->ActiveTexture(GL_TEXTURE0 + texid);
        vt->BindTexture(target, gltex);

        static const GLint wraps[PL_TEX_ADDRESS_MODE_COUNT] = {
            [PL_TEX_ADDRESS_CLAMP]  = GL_CLAMP_TO_EDGE,
            [PL_TEX_ADDRESS_REPEAT] = GL_REPEAT,
            [PL_TEX_ADDRESS_MIRROR] = GL_MIRRORED_REPEAT,
        };

        static const GLint filters[PL_TEX_SAMPLE_MODE_COUNT] = {
            [PL_TEX_SAMPLE_NEAREST] = GL_NEAREST,
            [PL_TEX_SAMPLE_LINEAR]  = GL_LINEAR,
        };

        GLint filter = filters[sd.binding.sample_mode];
        GLint wrap = wraps[sd.binding.address_mode];
        vt->TexParameteri(target, GL_TEXTURE_MIN_FILTER, filter);
        vt->TexParameteri(target, GL_TEXTURE_MAG_FILTER, filter);
        switch (pl_tex_params_dimension(tex->params)) {
        case 3: vt->TexParameteri(target, GL_TEXTURE_WRAP_R, wrap); // fall through
        case 2: vt->TexParameteri(target, GL_TEXTURE_WRAP_T, wrap); // fall through
        case 1: vt->TexParameteri(target, GL_TEXTURE_WRAP_S, wrap); break;
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
}

static void
sampler_xyz12_load(const struct vlc_gl_sampler *sampler)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);
    const opengl_vtable_t *vt = priv->vt;
    struct vlc_gl_format *glfmt = &sampler->glfmt;
    struct vlc_gl_picture *pic = &priv->pic;

    vt->Uniform1i(priv->uloc.Textures[0], 0);

    assert(pic->textures[0] != 0);
    vt->ActiveTexture(GL_TEXTURE0);
    vt->BindTexture(glfmt->tex_target, pic->textures[0]);
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

        "vec4 vlc_texture(vec2 tex_coords)\n"
        "{ "
        " vec4 v_in, v_out;"
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
    if (desc->plane_count == 3)
        swizzle_per_tex[0] = swizzle_per_tex[1] = swizzle_per_tex[2] = "r";
    else if (desc->plane_count == 2)
    {
        swizzle_per_tex[0] = "r";
        if (sampler->glfmt.formats[1] == GL_RG)
            swizzle_per_tex[1] = "rg";
        else
            swizzle_per_tex[1] = "ra";
    }
    else if (desc->plane_count == 1)
    {
        /*
         * One plane, but uploaded into two separate textures for Y and UV.
         *
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
                swizzle_per_tex[0] = "g";
                swizzle_per_tex[1] = "rb";
                break;
            case VLC_CODEC_YUYV:
                swizzle_per_tex[0] = "r";
                swizzle_per_tex[1] = "ga";
                break;
            case VLC_CODEC_VYUY:
                swizzle_per_tex[0] = "g";
                swizzle_per_tex[1] = "br";
                break;
            case VLC_CODEC_YVYU:
                swizzle_per_tex[0] = "r";
                swizzle_per_tex[1] = "ag";
                break;
            default:
                assert(!"missing chroma");
                return VLC_EGENERIC;
        }
    }
    return VLC_SUCCESS;
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
    struct vlc_gl_format *glfmt = &sampler->glfmt;

    priv->uloc.Textures[0] = vt->GetUniformLocation(program, "Texture");
    assert(priv->uloc.Textures[0] != -1);

    if (glfmt->tex_target == GL_TEXTURE_RECTANGLE)
    {
        priv->uloc.TexSizes[0] = vt->GetUniformLocation(program, "TexSize");
        assert(priv->uloc.TexSizes[0] != -1);
    }
}

static void
sampler_planes_load(struct vlc_gl_sampler *sampler)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);
    unsigned plane = priv->plane;

    const opengl_vtable_t *vt = priv->vt;
    struct vlc_gl_format *glfmt = &sampler->glfmt;
    struct vlc_gl_picture *pic = &priv->pic;

    vt->Uniform1i(priv->uloc.Textures[0], 0);

    assert(pic->textures[plane] != 0);
    vt->ActiveTexture(GL_TEXTURE0);
    vt->BindTexture(glfmt->tex_target, pic->textures[plane]);

    if (glfmt->tex_target == GL_TEXTURE_RECTANGLE)
    {
        vt->Uniform2f(priv->uloc.TexSizes[0], glfmt->tex_widths[plane],
                      glfmt->tex_heights[plane]);
    }
}

static int
sampler_planes_init(struct vlc_gl_sampler *sampler)
{
    struct vlc_gl_format *glfmt = &sampler->glfmt;
    GLenum tex_target = glfmt->tex_target;

    struct vlc_memstream ms;
    if (vlc_memstream_open(&ms))
        return VLC_EGENERIC;

#define ADD(x) vlc_memstream_puts(&ms, x)
#define ADDF(x, ...) vlc_memstream_printf(&ms, x, ##__VA_ARGS__)

    const char *sampler_type;
    const char *texture_fn;
    GetNames(tex_target, &sampler_type, &texture_fn);

    ADDF("uniform %s Texture;\n", sampler_type);

    if (tex_target == GL_TEXTURE_RECTANGLE)
        ADD("uniform vec2 TexSize;\n");

    ADD("vec4 vlc_texture(vec2 tex_coords) {\n");

    if (tex_target == GL_TEXTURE_RECTANGLE)
    {
        /* The coordinates are in texels values, not normalized */
        ADD(" tex_coords = TexSize * tex_coords;\n");
    }

    ADDF("  return %s(Texture, tex_coords);\n", texture_fn);
    ADD("}\n");

#undef ADD
#undef ADDF

    if (vlc_memstream_close(&ms) != 0)
        return VLC_EGENERIC;

    int ret = InitShaderExtensions(sampler, tex_target);
    if (ret != VLC_SUCCESS)
    {
        free(ms.ptr);
        return VLC_EGENERIC;
    }
    sampler->shader.body = ms.ptr;

    static const struct vlc_gl_sampler_ops ops = {
        .fetch_locations = sampler_planes_fetch_locations,
        .load = sampler_planes_load,
    };
    sampler->ops = &ops;

    return VLC_SUCCESS;
}

#ifdef HAVE_LIBPLACEBO_GL
static struct pl_custom_lut *LoadCustomLUT(struct vlc_gl_sampler *sampler,
                                           const char *filepath)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);
    if (!filepath || !filepath[0])
        return NULL;

    FILE *fs = vlc_fopen(filepath, "rb");
    struct pl_custom_lut *lut = NULL;
    char *lut_file = NULL;
    if (!fs)
        goto error;
    int ret = fseek(fs, 0, SEEK_END);
    if (ret == -1)
        goto error;
    long length = ftell(fs);
    if (length < 0)
        goto error;
    rewind(fs);

    lut_file = vlc_alloc(length, sizeof(*lut_file));
    if (!lut_file)
        goto error;
    ret = fread(lut_file, length, 1, fs);
    if (ret != 1)
        goto error;

    lut = pl_lut_parse_cube(priv->pl_log, lut_file, length);
    // fall through

error:
    if (fs)
        fclose(fs);
    free(lut_file);
    return lut;
}
#endif

static int
opengl_fragment_shader_init(struct vlc_gl_sampler *sampler, bool expose_planes)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);
    struct vlc_gl_format *glfmt = &sampler->glfmt;
    const video_format_t *fmt = &glfmt->fmt;

    GLenum tex_target = glfmt->tex_target;

    priv->expose_planes = expose_planes;
    priv->plane = 0;

    vlc_fourcc_t chroma = fmt->i_chroma;
    video_color_space_t yuv_space = fmt->space;

    const char *swizzle_per_tex[PICTURE_PLANE_MAX] = { NULL, };
    const bool is_yuv = vlc_fourcc_IsYUV(chroma);
    int ret;

    const vlc_chroma_description_t *desc = vlc_fourcc_GetChromaDescription(chroma);
    if (desc == NULL)
        return VLC_EGENERIC;

    unsigned tex_count = glfmt->tex_count;

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

    ADDF("uniform %s Textures[%u];\n", glsl_sampler, tex_count);

#ifdef HAVE_LIBPLACEBO_GL
    if (priv->pl_sh) {
        pl_shader sh = priv->pl_sh;
        struct pl_color_map_params color_params;
        vlc_placebo_ColorMapParams(VLC_OBJECT(priv->gl), "gl", &color_params);

        struct pl_color_space src_space = vlc_placebo_ColorSpace(fmt);
        struct pl_color_space dst_space = pl_color_space_unknown;
        dst_space.primaries = var_InheritInteger(priv->gl, "target-prim");
        dst_space.transfer = var_InheritInteger(priv->gl, "target-trc");

        char *lut_file = var_InheritString(priv->gl, "gl-lut-file");
        struct pl_custom_lut *lut = LoadCustomLUT(sampler, lut_file);
        if (lut) {
            // Transform from the video input to the LUT input color space,
            // defaulting to a no-op if LUT input color space info is unknown
            dst_space = lut->color_in;
            pl_color_space_merge(&dst_space, &src_space);
        }

        pl_shader_color_map(sh, &color_params, src_space, dst_space,
                            &priv->tone_map_state, false);

        if (lut) {
            pl_shader_custom_lut(sh, lut, &priv->lut_state);
            pl_lut_free(&lut);
        }

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

            pl_shader_dither(sh, out_bits, &priv->dither_state, &(struct pl_dither_params) {
                .method   = method,
            });
        }

        const struct pl_shader_res *res = priv->pl_sh_res = pl_shader_finalize(sh);

        FREENULL(priv->uloc.pl_vars);
        priv->uloc.pl_vars = calloc(res->num_variables, sizeof(GLint));
        for (int i = 0; i < res->num_variables; i++) {
            struct pl_shader_var sv = res->variables[i];
            const char *glsl_type_name = pl_var_glsl_type_name(sv.var);
            ADDF("uniform %s %s", glsl_type_name, sv.var.name);
            if (sv.var.dim_a > 1) {
                ADDF("[%d];\n", sv.var.dim_a);
            } else {
                ADDF(";\n");
            }
        }

        FREENULL(priv->uloc.pl_descs);
        priv->uloc.pl_descs = calloc(res->num_descriptors, sizeof(GLint));
        for (int i = 0; i < res->num_descriptors; i++) {
            struct pl_shader_desc sd = res->descriptors[i];
            assert(sd.desc.type == PL_DESC_SAMPLED_TEX);
            pl_tex tex = sd.binding.object;
            assert(tex->sampler_type == PL_SAMPLER_NORMAL);
            int dims = pl_tex_params_dimension(tex->params);
            ADDF("uniform sampler%dD %s;\n", dims, sd.desc.name);
        }

        // We can't handle these yet, but nothing we use requires them, either
        assert(res->num_vertex_attribs == 0);

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

    ADD("vec4 vlc_texture(vec2 tex_coords) {\n");

    unsigned color_count;
    if (is_yuv) {
        ADD(" vec4 pixel = vec4(\n");
        color_count = 0;
        for (unsigned i = 0; i < tex_count; ++i)
        {
            const char *swizzle = swizzle_per_tex[i];
            assert(swizzle);
            color_count += strlen(swizzle);
            assert(color_count < PICTURE_PLANE_MAX);
            if (tex_target == GL_TEXTURE_RECTANGLE)
            {
                /* The coordinates are in texels values, not normalized */
                ADDF("  %s(Textures[%u], TexSizes[%u] * tex_coords).%s,\n", lookup, i, i, swizzle);
            }
            else
            {
                ADDF("  %s(Textures[%u], tex_coords).%s,\n", lookup, i, swizzle);
            }
        }
        ADD("  1.0);\n");
        ADD(" vec4 result = ConvMatrix * pixel;\n");
    }
    else
    {
        ADDF(" vec4 result = %s(Textures[0], tex_coords);\n", lookup);
        color_count = 1;
    }
    assert(yuv_space == COLOR_SPACE_UNDEF || color_count == 3);

#ifdef HAVE_LIBPLACEBO_GL
    if (priv->pl_sh_res) {
        const struct pl_shader_res *res = priv->pl_sh_res;
        if (res->input != PL_SHADER_SIG_NONE) {
            assert(res->input  == PL_SHADER_SIG_COLOR);
            assert(res->output == PL_SHADER_SIG_COLOR);
            ADDF(" result = %s(result);\n", res->name);
        }
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

    static const struct vlc_gl_sampler_ops ops = {
        .fetch_locations = sampler_base_fetch_locations,
        .load = sampler_base_load,
    };
    sampler->ops = &ops;

    return VLC_SUCCESS;
}

struct vlc_gl_sampler *
vlc_gl_sampler_New(struct vlc_gl_t *gl, const struct vlc_gl_api *api,
                   const struct vlc_gl_format *glfmt, bool expose_planes)
{
    struct vlc_gl_sampler_priv *priv = calloc(1, sizeof(*priv));
    if (!priv)
        return NULL;

    struct vlc_gl_sampler *sampler = &priv->sampler;
    vlc_gl_LoadExtensionFunctions(gl, &priv->extension_vt);

    priv->gl = gl;
    priv->api = api;
    priv->vt = &api->vt;

    struct vlc_gl_picture *pic = &priv->pic;
    memcpy(pic->mtx, MATRIX2x3_IDENTITY, sizeof(MATRIX2x3_IDENTITY));
    priv->pic.mtx_has_changed = true;

    sampler->pic_to_tex_matrix = pic->mtx;

    /* Formats with palette are not supported. This also allows to copy
     * video_format_t without possibility of failure. */
    assert(!glfmt->fmt.p_palette);

    sampler->glfmt = *glfmt;

    sampler->shader.extensions = NULL;
    sampler->shader.body = NULL;

#ifdef HAVE_LIBPLACEBO_GL
    priv->uloc.pl_vars = NULL;
    priv->uloc.pl_descs = NULL;
    priv->pl_sh_res = NULL;
    priv->pl_log = vlc_placebo_CreateLog(VLC_OBJECT(gl));
    priv->pl_opengl = pl_opengl_create(priv->pl_log, NULL);
    if (!priv->pl_opengl)
    {
        vlc_gl_sampler_Delete(sampler);
        return NULL;
    }

    priv->pl_sh = pl_shader_alloc(priv->pl_log, &(struct pl_shader_params) {
        .gpu = priv->pl_opengl->gpu,
        .glsl = {
#   ifdef USE_OPENGL_ES2
            .version = 100,
            .gles = true,
#   else
            .version = 120,
#   endif
        },
    });
#endif

    int ret = opengl_fragment_shader_init(sampler, expose_planes);
    if (ret != VLC_SUCCESS)
    {
        vlc_gl_sampler_Delete(sampler);
        return NULL;
    }

    return sampler;
}

void
vlc_gl_sampler_Delete(struct vlc_gl_sampler *sampler)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);

#ifdef HAVE_LIBPLACEBO_GL
    FREENULL(priv->uloc.pl_vars);
    FREENULL(priv->uloc.pl_descs);
    pl_shader_free(&priv->pl_sh);
    pl_shader_obj_destroy(&priv->lut_state);
    pl_shader_obj_destroy(&priv->tone_map_state);
    pl_shader_obj_destroy(&priv->dither_state);
    pl_opengl_destroy(&priv->pl_opengl);
    pl_log_destroy(&priv->pl_log);
#endif

    free(sampler->shader.extensions);
    free(sampler->shader.body);

    free(priv);
}

int
vlc_gl_sampler_Update(struct vlc_gl_sampler *sampler,
                      const struct vlc_gl_picture *picture)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);
    priv->pic = *picture;
    return VLC_SUCCESS;
}

void
vlc_gl_sampler_SelectPlane(struct vlc_gl_sampler *sampler, unsigned plane)
{
    struct vlc_gl_sampler_priv *priv = PRIV(sampler);
    priv->plane = plane;
}

void
vlc_gl_picture_ToTexCoords(const struct vlc_gl_picture *pic,
                           unsigned coords_count, const float *pic_coords,
                           float *tex_coords_out)
{
    const float *mtx = pic->mtx;
    assert(mtx);

#define MTX(ROW,COL) mtx[(COL)*2+(ROW)]
    for (unsigned i = 0; i < coords_count; ++i)
    {
        /* Store the coordinates, in case the transform must be applied in
         * place (i.e. with pic_coords == tex_coords_out) */
        float x = pic_coords[0];
        float y = pic_coords[1];
        tex_coords_out[0] = MTX(0,0) * x + MTX(0,1) * y + MTX(0,2);
        tex_coords_out[1] = MTX(1,0) * x + MTX(1,1) * y + MTX(1,2);
        pic_coords += 2;
        tex_coords_out += 2;
    }
}

void
vlc_gl_picture_ComputeDirectionMatrix(const struct vlc_gl_picture *pic,
                                      float direction[static 2*2])
{
    /**
     * The direction matrix is extracted from pic->mtx:
     *
     *    mtx = / a b c \
     *          \ d e f /
     *
     * The last column (the offset part of the affine transformation) is
     * discarded, and the 2 remaining column vectors are normalized to remove
     * any scaling:
     *
     *    direction = / a/unorm  b/vnorm \
     *                \ d/unorm  e/vnorm /
     *
     * where unorm = norm( / a \ ) and vnorm = norm( / b \ ).
     *                     \ d /                     \ e /
     */

    float ux = pic->mtx[0];
    float uy = pic->mtx[1];
    float vx = pic->mtx[2];
    float vy = pic->mtx[3];

    float unorm = sqrt(ux * ux + uy * uy);
    float vnorm = sqrt(vx * vx + vy * vy);

    direction[0] = ux / unorm;
    direction[1] = uy / unorm;
    direction[2] = vx / vnorm;
    direction[3] = vy / vnorm;
}

struct vlc_gl_importer {
    struct vlc_gl_format glfmt;
    struct vlc_gl_interop *interop;

    /* For convenience, same as interop->api and interop->api->vt */
    const struct vlc_gl_api *api;
    const opengl_vtable_t *vt;

    struct vlc_gl_picture pic;

    struct {
        unsigned int i_x_offset;
        unsigned int i_y_offset;
        unsigned int i_visible_width;
        unsigned int i_visible_height;
    } last_source;

    /* All matrices below are stored in column-major order. */

    float mtx_orientation[2*3];
    float mtx_coords_map[2*3];

    float mtx_transform[2*3];
    bool mtx_transform_defined;

    /**
     * The complete transformation matrix is stored in pic.mtx.
     *
     * tex_coords =  pic_to_tex × pic_coords
     *
     *  / tex_x \    / a b c \    / pic_x \
     *  \ tex_y / =  \ d e f /  × | pic_y |
     *                            \   1   /
     *
     * Semantically, it represents the result of:
     *
     *     get_transform_matrix() * mtx_coords_map * mtx_orientation
     *
     * (The intermediate matrices are implicitly expanded to 3x3 with [0 0 1]
     * as the last row.)
     *
     * It is stored in column-major order: [a, d, b, e, c, f].
     */
    bool pic_mtx_defined;
};

static const GLfloat *
GetTransformMatrix(const struct vlc_gl_interop *interop)
{
    const GLfloat *tm = NULL;
    if (interop && interop->ops && interop->ops->get_transform_matrix)
        tm = interop->ops->get_transform_matrix(interop);
    return tm;
}

static void
InitOrientationMatrix(float matrix[static 2*3], video_orientation_t orientation)
{
/**
 * / C0R0  C1R0  C3R0 \
 * \ C0R1  C1R1  C3R1 /
 *
 * (note that in memory, the matrix is stored in column-major order)
 */
#define MATRIX_SET(C0R0, C1R0, C3R0, \
                   C0R1, C1R1, C3R1) \
    matrix[0*2 + 0] = C0R0; \
    matrix[1*2 + 0] = C1R0; \
    matrix[2*2 + 0] = C3R0; \
    matrix[0*2 + 1] = C0R1; \
    matrix[1*2 + 1] = C1R1; \
    matrix[2*2 + 1] = C3R1;

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
        case ORIENT_NORMAL:
            /* No transformation */
            memcpy(matrix, MATRIX2x3_IDENTITY, sizeof(MATRIX2x3_IDENTITY));
            break;
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

struct vlc_gl_importer *
vlc_gl_importer_New(struct vlc_gl_interop *interop)
{
    assert(interop);

    struct vlc_gl_importer *importer = malloc(sizeof(*importer));
    if (!importer)
        return NULL;

    importer->interop = interop;

    importer->mtx_transform_defined = false;
    importer->pic_mtx_defined = false;

    struct vlc_gl_format *glfmt = &importer->glfmt;
    struct vlc_gl_picture *pic = &importer->pic;

    /* Formats with palette are not supported. This also allows to copy
     * video_format_t without possibility of failure. */
    assert(!interop->fmt_out.p_palette);

    glfmt->fmt = interop->fmt_out;
    glfmt->tex_target = interop->tex_target;
    glfmt->tex_count = interop->tex_count;

    /* This matrix may be updated on new pictures */
    memcpy(&importer->mtx_coords_map, MATRIX2x3_IDENTITY,
           sizeof(MATRIX2x3_IDENTITY));

    InitOrientationMatrix(importer->mtx_orientation, glfmt->fmt.orientation);

    struct vlc_gl_extension_vt extension_vt;
    vlc_gl_LoadExtensionFunctions(interop->gl, &extension_vt);

    /* OpenGL ES 2 includes support for non-power of 2 textures by specification. */
    bool supports_npot = interop->gl->api_type == VLC_OPENGL_ES2
        || vlc_gl_HasExtension(&extension_vt, "GL_ARB_texture_non_power_of_two")
        || vlc_gl_HasExtension(&extension_vt, "GL_APPLE_texture_2D_limited_npot");

    /* Texture size */
    for (unsigned j = 0; j < interop->tex_count; j++) {
        GLsizei vw = interop->fmt_out.i_visible_width  * interop->texs[j].w.num
                  / interop->texs[j].w.den;
        GLsizei vh = interop->fmt_out.i_visible_height * interop->texs[j].h.num
                  / interop->texs[j].h.den;
        GLsizei w = (interop->fmt_out.i_visible_width + interop->fmt_out.i_x_offset) * interop->texs[j].w.num
                  / interop->texs[j].w.den;
        GLsizei h = (interop->fmt_out.i_visible_height + interop->fmt_out.i_y_offset) *  interop->texs[j].h.num
                  / interop->texs[j].h.den;
        glfmt->visible_widths[j] = vw;
        glfmt->visible_heights[j] = vh;
        if (supports_npot) {
            glfmt->tex_widths[j]  = w;
            glfmt->tex_heights[j] = h;
        } else {
            glfmt->tex_widths[j]  = vlc_align_pot(w);
            glfmt->tex_heights[j] = vlc_align_pot(h);
        }

        glfmt->formats[j] = interop->texs[j].format;
    }

    if (!interop->handle_texs_gen)
    {
        int ret = vlc_gl_interop_GenerateTextures(interop, glfmt->tex_widths,
                                                  glfmt->tex_heights,
                                                  pic->textures);
        if (ret != VLC_SUCCESS)
        {
            free(importer);
            return NULL;
        }
    }

    return importer;
}

void
vlc_gl_importer_Delete(struct vlc_gl_importer *importer)
{
    struct vlc_gl_interop *interop = importer->interop;

    if (interop && !interop->handle_texs_gen)
    {
        void (*DeleteTextures)(uint32_t, uint32_t*) =
            vlc_gl_GetProcAddress(interop->gl, "glDeleteTextures");
        (*DeleteTextures)(interop->tex_count, importer->pic.textures);
    }

    free(importer);
}

/**
 * Compute out = a * b, as if the 2x3 matrices were expanded to 3x3 with
 *  [0 0 1] as the last row.
 */
static void
MatrixMultiply(float out[static 2*3],
               const float a[static 2*3], const float b[static 2*3])
{
    /* All matrices are stored in column-major order. */
    for (unsigned i = 0; i < 3; ++i)
        for (unsigned j = 0; j < 2; ++j)
            out[i*2+j] = a[0*2+j] * b[i*2+0]
                       + a[1*2+j] * b[i*2+1];

    /* Multiply the last implicit row [0 0 1] of b, expanded to 3x3 */
    out[2*2+0] += a[2*2+0];
    out[2*2+1] += a[2*2+1];
}

static void
UpdatePictureMatrix(struct vlc_gl_importer *importer)
{
    float tmp[2*3];

    struct vlc_gl_picture *pic = &importer->pic;

    float *out = importer->mtx_transform_defined ? tmp : pic->mtx;
    /* out = mtx_coords_map * mtx_orientation */
    MatrixMultiply(out, importer->mtx_coords_map, importer->mtx_orientation);

    if (importer->mtx_transform_defined)
        /* mtx_all = mtx_transform * tmp */
        MatrixMultiply(pic->mtx, importer->mtx_transform, tmp);
}

int
vlc_gl_importer_Update(struct vlc_gl_importer *importer, picture_t *picture)
{
    struct vlc_gl_interop *interop = importer->interop;
    struct vlc_gl_format *glfmt = &importer->glfmt;
    struct vlc_gl_picture *pic = &importer->pic;

    const video_format_t *source = &picture->format;

    bool mtx_changed = false;

    if (!importer->pic_mtx_defined
     || source->i_x_offset != importer->last_source.i_x_offset
     || source->i_y_offset != importer->last_source.i_y_offset
     || source->i_visible_width != importer->last_source.i_visible_width
     || source->i_visible_height != importer->last_source.i_visible_height)
    {
        memset(importer->mtx_coords_map, 0, sizeof(importer->mtx_coords_map));

        /* The transformation is the same for all planes, even with power-of-two
         * textures. */
        /* FIXME The first plane may have a ratio != 1:1, because with YUV 4:2:2
         * formats, the Y2 value is ignored so half the horizontal resolution
         * is lost, see interop_yuv_base_init(). Once this is fixed, the
         * multiplication by den/num may be removed. */
        float scale_w = glfmt->tex_widths[0] * interop->texs[0].w.den
                                             / interop->texs[0].w.num;
        float scale_h = glfmt->tex_heights[0] * interop->texs[0].h.den
                                              / interop->texs[0].h.num;

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
        float left   = (source->i_x_offset +                       0 ) / scale_w;
        float top    = (source->i_y_offset +                       0 ) / scale_h;
        float right  = (source->i_x_offset + source->i_visible_width ) / scale_w;
        float bottom = (source->i_y_offset + source->i_visible_height) / scale_h;

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
         * is (x', y').
         *
         * The paddings are l (left), r (right), t (top) and b (bottom).
         *
         *      matrix = / (r-l)   0     l \
         *               \   0   (b-t)   t /
         *
         * It is stored in column-major order.
         */
        float *matrix = importer->mtx_coords_map;
#define COL(x) (x*2)
#define ROW(x) (x)
        matrix[COL(0) + ROW(0)] = right - left;
        matrix[COL(1) + ROW(1)] = bottom - top;
        matrix[COL(2) + ROW(0)] = left;
        matrix[COL(2) + ROW(1)] = top;
#undef COL
#undef ROW

        mtx_changed = true;

        importer->last_source.i_x_offset = source->i_x_offset;
        importer->last_source.i_y_offset = source->i_y_offset;
        importer->last_source.i_visible_width = source->i_visible_width;
        importer->last_source.i_visible_height = source->i_visible_height;
    }

    /* Update the texture */
    int ret = interop->ops->update_textures(interop, pic->textures,
                                            glfmt->tex_widths,
                                            glfmt->tex_heights, picture,
                                            NULL);

    const float *tm = GetTransformMatrix(interop);
    if (tm) {
        memcpy(importer->mtx_transform, tm, sizeof(importer->mtx_transform));
        importer->mtx_transform_defined = true;
        mtx_changed = true;
    }
    else if (importer->mtx_transform_defined)
    {
        importer->mtx_transform_defined = false;
        mtx_changed = true;
    }

    if (!importer->pic_mtx_defined || mtx_changed)
    {
        UpdatePictureMatrix(importer);
        importer->pic_mtx_defined = true;
        pic->mtx_has_changed = true;
    }
    else
        pic->mtx_has_changed = false;

    return ret;
}
