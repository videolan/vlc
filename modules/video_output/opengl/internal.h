/*****************************************************************************
 * opengl_internal.h: OpenGL internal header
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

#ifndef VLC_OPENGL_INTERNAL_H
#define VLC_OPENGL_INTERNAL_H

#include "vout_helper.h"

#if defined(USE_OPENGL_ES2)
#   define GLSL_VERSION "100"
#   define VLCGL_TEXTURE_COUNT 1
#   define PRECISION "precision highp float;"
#   define VLCGL_PICTURE_MAX 128
#   define glClientActiveTexture(x)
#else
#   define GLSL_VERSION "120"
#   define VLCGL_TEXTURE_COUNT 1
#   define VLCGL_PICTURE_MAX 128
#   define PRECISION ""
#endif

#if defined(USE_OPENGL_ES2) || defined(__APPLE__)
#   define PFNGLGETPROGRAMIVPROC             typeof(glGetProgramiv)*
#   define PFNGLGETPROGRAMINFOLOGPROC        typeof(glGetProgramInfoLog)*
#   define PFNGLGETSHADERIVPROC              typeof(glGetShaderiv)*
#   define PFNGLGETSHADERINFOLOGPROC         typeof(glGetShaderInfoLog)*
#   define PFNGLGETUNIFORMLOCATIONPROC       typeof(glGetUniformLocation)*
#   define PFNGLGETATTRIBLOCATIONPROC        typeof(glGetAttribLocation)*
#   define PFNGLVERTEXATTRIBPOINTERPROC      typeof(glVertexAttribPointer)*
#   define PFNGLENABLEVERTEXATTRIBARRAYPROC  typeof(glEnableVertexAttribArray)*
#   define PFNGLUNIFORMMATRIX4FVPROC         typeof(glUniformMatrix4fv)*
#   define PFNGLUNIFORM4FVPROC               typeof(glUniform4fv)*
#   define PFNGLUNIFORM4FPROC                typeof(glUniform4f)*
#   define PFNGLUNIFORM1IPROC                typeof(glUniform1i)*
#   define PFNGLCREATESHADERPROC             typeof(glCreateShader)*
#   define PFNGLSHADERSOURCEPROC             typeof(glShaderSource)*
#   define PFNGLCOMPILESHADERPROC            typeof(glCompileShader)*
#   define PFNGLDELETESHADERPROC             typeof(glDeleteShader)*
#   define PFNGLCREATEPROGRAMPROC            typeof(glCreateProgram)*
#   define PFNGLLINKPROGRAMPROC              typeof(glLinkProgram)*
#   define PFNGLUSEPROGRAMPROC               typeof(glUseProgram)*
#   define PFNGLDELETEPROGRAMPROC            typeof(glDeleteProgram)*
#   define PFNGLATTACHSHADERPROC             typeof(glAttachShader)*
#   define PFNGLGENBUFFERSPROC               typeof(glGenBuffers)*
#   define PFNGLBINDBUFFERPROC               typeof(glBindBuffer)*
#   define PFNGLBUFFERDATAPROC               typeof(glBufferData)*
#   define PFNGLDELETEBUFFERSPROC            typeof(glDeleteBuffers)*
#if defined(__APPLE__)
#   import <CoreFoundation/CoreFoundation.h>
#endif
#endif

/**
 * Structure containing function pointers to shaders commands
 */
typedef struct {
    /* Shader variables commands*/
    PFNGLGETUNIFORMLOCATIONPROC      GetUniformLocation;
    PFNGLGETATTRIBLOCATIONPROC       GetAttribLocation;
    PFNGLVERTEXATTRIBPOINTERPROC     VertexAttribPointer;
    PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray;

    PFNGLUNIFORMMATRIX4FVPROC   UniformMatrix4fv;
    PFNGLUNIFORM4FVPROC         Uniform4fv;
    PFNGLUNIFORM4FPROC          Uniform4f;
    PFNGLUNIFORM1IPROC          Uniform1i;

    /* Shader command */
    PFNGLCREATESHADERPROC CreateShader;
    PFNGLSHADERSOURCEPROC ShaderSource;
    PFNGLCOMPILESHADERPROC CompileShader;
    PFNGLDELETESHADERPROC   DeleteShader;

    PFNGLCREATEPROGRAMPROC CreateProgram;
    PFNGLLINKPROGRAMPROC   LinkProgram;
    PFNGLUSEPROGRAMPROC    UseProgram;
    PFNGLDELETEPROGRAMPROC DeleteProgram;

    PFNGLATTACHSHADERPROC  AttachShader;

    /* Shader log commands */
    PFNGLGETPROGRAMIVPROC  GetProgramiv;
    PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog;
    PFNGLGETSHADERIVPROC   GetShaderiv;
    PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog;

    PFNGLGENBUFFERSPROC    GenBuffers;
    PFNGLBINDBUFFERPROC    BindBuffer;
    PFNGLBUFFERDATAPROC    BufferData;
    PFNGLDELETEBUFFERSPROC DeleteBuffers;

#if defined(_WIN32)
    PFNGLACTIVETEXTUREPROC  ActiveTexture;
    PFNGLCLIENTACTIVETEXTUREPROC  ClientActiveTexture;
#   undef glClientActiveTexture
#   undef glActiveTexture
#   define glActiveTexture tc->api->ActiveTexture
#   define glClientActiveTexture tc->api->ClientActiveTexture
#endif

} opengl_shaders_api_t;

#endif /* include-guard */
