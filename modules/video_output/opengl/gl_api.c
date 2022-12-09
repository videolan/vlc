/*****************************************************************************
 * gl_api.h
 *****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
 * Copyright (C) 2020 Videolabs
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

#include "gl_api.h"

#include <assert.h>
#include <vlc_common.h>
#include <vlc_opengl.h>

#include "gl_common.h"
#include "gl_util.h"

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
