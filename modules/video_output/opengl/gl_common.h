/*****************************************************************************
+ * gl_common.h
+ *****************************************************************************
+ * Copyright (C) 2019 VLC authors and VideoLAN
+ *
+ * This program is free software; you can redistribute it and/or modify it
+ * under the terms of the GNU Lesser General Public License as published by
+ * the Free Software Foundation; either version 2.1 of the License, or
+ * (at your option) any later version.
+ *
+ * This program is distributed in the hope that it will be useful,
+ * but WITHOUT ANY WARRANTY; without even the implied warranty of
+ * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
+ * GNU Lesser General Public License for more details.
+ *
+ * You should have received a copy of the GNU Lesser General Public License
+ * along with this program; if not, write to the Free Software Foundation,
+ * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
+ *****************************************************************************/

#ifndef VLC_GL_COMMON_H
#define VLC_GL_COMMON_H

#include <assert.h>

/* if USE_OPENGL_ES2 is defined, OpenGL ES version 2 will be used, otherwise
 * normal OpenGL will be used */
#ifdef __APPLE__
# include <TargetConditionals.h>
# if !TARGET_OS_IPHONE
#  undef USE_OPENGL_ES2
#  define MACOS_OPENGL
#  include <OpenGL/gl.h>
# else /* Force ESv2 on iOS */
#  define USE_OPENGL_ES2
#  include <OpenGLES/ES1/gl.h>
#  include <OpenGLES/ES2/gl.h>
#  include <OpenGLES/ES2/glext.h>
# endif
#else /* !defined (__APPLE__) */
# if defined (USE_OPENGL_ES2)
#  include <GLES2/gl2.h>
#  include <GLES2/gl2ext.h>
# else
#  ifdef HAVE_GL_WGLEW_H
#   include <GL/glew.h>
#  endif
#  include <GL/gl.h>
# endif
#endif

#define VLCGL_PICTURE_MAX 128

#ifndef GL_TEXTURE_RECTANGLE
# define GL_TEXTURE_RECTANGLE 0x84F5
#endif
#ifndef GL_TEXTURE_EXTERNAL_OES
# define GL_TEXTURE_EXTERNAL_OES 0x8D65
#endif
#ifndef GL_RED
# define GL_RED 0x1903
#endif
#ifndef GL_RG
# define GL_RG 0x8227
#endif
#ifndef GL_R16
# define GL_R16 0x822A
#endif
#ifndef GL_BGRA
# define GL_BGRA 0x80E1
#endif
#ifndef GL_RG16
# define GL_RG16 0x822C
#endif
#ifndef GL_LUMINANCE16
# define GL_LUMINANCE16 0x8042
#endif
#ifndef GL_TEXTURE_RED_SIZE
# define GL_TEXTURE_RED_SIZE 0x805C
#endif
#ifndef GL_TEXTURE_LUMINANCE_SIZE
# define GL_TEXTURE_LUMINANCE_SIZE 0x8060
#endif

#ifndef GL_CLAMP_TO_EDGE
# define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_UNPACK_ROW_LENGTH
# define GL_UNPACK_ROW_LENGTH 0x0CF2
#endif
#ifndef GL_PIXEL_UNPACK_BUFFER
# define GL_PIXEL_UNPACK_BUFFER 0x88EC
#endif
#ifndef GL_DYNAMIC_DRAW
# define GL_DYNAMIC_DRAW 0x88E8
#endif

#ifndef GL_READ_FRAMEBUFFER
# define GL_READ_FRAMEBUFFER 0x8CA8
#endif

#ifndef GL_READ_FRAMEBUFFER_BINDING
# define GL_READ_FRAMEBUFFER_BINDING 0x8CAA
#endif

#ifndef GL_DRAW_FRAMEBUFFER
# define GL_DRAW_FRAMEBUFFER 0x8CA9
#endif

#ifndef GL_DRAW_FRAMEBUFFER_BINDING
# define GL_DRAW_FRAMEBUFFER_BINDING 0x8CA6
#endif

#ifndef GL_MULTISAMPLE
# define GL_MULTISAMPLE 0x809D
#endif

#ifndef GL_COLOR_ATTACHMENT0
# define GL_COLOR_ATTACHMENT0 0x8CE0
#endif

#ifndef GL_COLOR_ATTACHMENT1
# define GL_COLOR_ATTACHMENT1 0x8CE1
#endif

#ifndef GL_COLOR_ATTACHMENT2
# define GL_COLOR_ATTACHMENT2 0x8CE2
#endif

#ifndef GL_COLOR_ATTACHMENT3
# define GL_COLOR_ATTACHMENT3 0x8CE3
#endif

#ifndef GL_COLOR_ATTACHMENT4
# define GL_COLOR_ATTACHMENT4 0x8CE4
#endif

#ifndef GL_COLOR_ATTACHMENT5
# define GL_COLOR_ATTACHMENT5 0x8CE5
#endif

#ifndef GL_COLOR_ATTACHMENT6
# define GL_COLOR_ATTACHMENT6 0x8CE6
#endif

#ifndef GL_COLOR_ATTACHMENT7
# define GL_COLOR_ATTACHMENT7 0x8CE7
#endif

#ifndef APIENTRY
# define APIENTRY
#endif

/* FIXME: GL_ASSERT_NOERROR disabled for now because:
 * Proper GL error handling need to be implemented
 * glClear(GL_COLOR_BUFFER_BIT) throws a GL_INVALID_FRAMEBUFFER_OPERATION on macOS
 * assert fails on vout_display_opengl_Delete on iOS
 */
#if 0
# define HAVE_GL_ASSERT_NOERROR
#endif

#ifdef HAVE_GL_ASSERT_NOERROR
# define GL_ASSERT_NOERROR(vt) do { \
    GLenum glError = (vt)->GetError(); \
    switch (glError) \
    { \
        case GL_NO_ERROR: break; \
        case GL_INVALID_ENUM: assert(!"GL_INVALID_ENUM"); \
        case GL_INVALID_VALUE: assert(!"GL_INVALID_VALUE"); \
        case GL_INVALID_OPERATION: assert(!"GL_INVALID_OPERATION"); \
        case GL_INVALID_FRAMEBUFFER_OPERATION: assert(!"GL_INVALID_FRAMEBUFFER_OPERATION"); \
        case GL_OUT_OF_MEMORY: assert(!"GL_OUT_OF_MEMORY"); \
        default: assert(!"GL_UNKNOWN_ERROR"); \
    } \
} while(0)
#else
# define GL_ASSERT_NOERROR(vt)
#endif

/* Core OpenGL/OpenGLES functions: the following functions pointers typedefs
 * are not defined. */
#if !defined(_WIN32) /* Already defined on Win32 */
typedef void (*PFNGLACTIVETEXTUREPROC) (GLenum texture);
#endif
typedef void (APIENTRY *PFNGLBINDTEXTUREPROC) (GLenum target, GLuint texture);
typedef void (APIENTRY *PFNGLBLENDFUNCPROC) (GLenum sfactor, GLenum dfactor);
typedef void (APIENTRY *PFNGLBUFFERSTORAGEPROC) (GLenum target, GLsizeiptr size, const void *data, GLbitfield flags);
typedef void (APIENTRY *PFNGLCLEARCOLORPROC) (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void (APIENTRY *PFNGLCLEARPROC) (GLbitfield mask);
typedef void (APIENTRY *PFNGLDELETETEXTURESPROC) (GLsizei n, const GLuint *textures);
typedef void (APIENTRY *PFNGLDEPTHMASKPROC) (GLboolean flag);
typedef void (APIENTRY *PFNGLDISABLEPROC) (GLenum cap);
typedef void (APIENTRY *PFNGLDRAWARRAYSPROC) (GLenum mode, GLint first, GLsizei count);
typedef void (APIENTRY *PFNGLDRAWELEMENTSPROC) (GLenum mode, GLsizei count, GLenum type, const void *indices);
typedef void (APIENTRY *PFNGLENABLEPROC) (GLenum cap);
typedef void (APIENTRY *PFNGLFINISHPROC)(void);
typedef void (APIENTRY *PFNGLFLUSHPROC)(void);
typedef void (APIENTRY *PFNGLGENTEXTURESPROC) (GLsizei n, GLuint *textures);
typedef GLenum (APIENTRY *PFNGLGETERRORPROC) (void);
typedef void (APIENTRY *PFNGLGETINTEGERVPROC) (GLenum pname, GLint *data);
typedef const GLubyte *(APIENTRY *PFNGLGETSTRINGPROC) (GLenum name);
typedef void (APIENTRY *PFNGLGETTEXLEVELPARAMETERIVPROC) (GLenum target, GLint level, GLenum pname, GLint *params);
typedef void (APIENTRY *PFNGLPIXELSTOREIPROC) (GLenum pname, GLint param);
typedef void (APIENTRY *PFNGLTEXENVFPROC)(GLenum target, GLenum pname, GLfloat param);
typedef void (APIENTRY *PFNGLTEXIMAGE2DPROC) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels);
typedef void (APIENTRY *PFNGLTEXPARAMETERFPROC) (GLenum target, GLenum pname, GLfloat param);
typedef void (APIENTRY *PFNGLTEXPARAMETERIPROC) (GLenum target, GLenum pname, GLint param);
typedef void (APIENTRY *PFNGLTEXSUBIMAGE2DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels);
typedef void (APIENTRY *PFNGLVIEWPORTPROC) (GLint x, GLint y, GLsizei width, GLsizei height);

/* The following are defined in glext.h but not for GLES2 or on Apple systems */
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
#   define PFNGLUNIFORMMATRIX3FVPROC         typeof(glUniformMatrix3fv)*
#   define PFNGLUNIFORMMATRIX2FVPROC         typeof(glUniformMatrix2fv)*
#   define PFNGLUNIFORM4FVPROC               typeof(glUniform4fv)*
#   define PFNGLUNIFORM4FPROC                typeof(glUniform4f)*
#   define PFNGLUNIFORM3FPROC                typeof(glUniform3f)*
#   define PFNGLUNIFORM2FPROC                typeof(glUniform2f)*
#   define PFNGLUNIFORM1FPROC                typeof(glUniform1f)*
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
#   define PFNGLBUFFERSUBDATAPROC            typeof(glBufferSubData)*
#   define PFNGLDELETEBUFFERSPROC            typeof(glDeleteBuffers)*
#   define PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC typeof(glGetFramebufferAttachmentParameteriv)*
#if defined(__APPLE__)
#   import <CoreFoundation/CoreFoundation.h>
#endif
#endif

/* The following are defined in glext.h but doesn't exist in GLES2 */
#if defined(USE_OPENGL_ES2) || defined(__APPLE__)
typedef struct __GLsync *GLsync;
typedef uint64_t GLuint64;
typedef void *(APIENTRY *PFNGLMAPBUFFERRANGEPROC) (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
typedef void (APIENTRY *PFNGLFLUSHMAPPEDBUFFERRANGEPROC) (GLenum target, GLintptr offset, GLsizeiptr length);
typedef GLboolean (APIENTRY *PFNGLUNMAPBUFFERPROC) (GLenum target);
typedef GLsync (APIENTRY *PFNGLFENCESYNCPROC) (GLenum condition, GLbitfield flags);
typedef void (APIENTRY *PFNGLDELETESYNCPROC) (GLsync sync);
typedef GLenum (APIENTRY *PFNGLCLIENTWAITSYNCPROC) (GLsync sync, GLbitfield flags, GLuint64 timeout);
#endif

/**
 * Structure containing function pointers to shaders commands
 */
typedef struct {
    /*
     * GL / GLES core functions
     */
    PFNGLBINDTEXTUREPROC    BindTexture;
    PFNGLBLENDFUNCPROC      BlendFunc;
    PFNGLCLEARCOLORPROC     ClearColor;
    PFNGLCLEARPROC          Clear;
    PFNGLDELETETEXTURESPROC DeleteTextures;
    PFNGLDEPTHMASKPROC      DepthMask;
    PFNGLDISABLEPROC        Disable;
    PFNGLDRAWARRAYSPROC     DrawArrays;
    PFNGLDRAWELEMENTSPROC   DrawElements;
    PFNGLENABLEPROC         Enable;
    PFNGLFINISHPROC         Finish;
    PFNGLFLUSHPROC          Flush;
    PFNGLGENTEXTURESPROC    GenTextures;
    PFNGLGETERRORPROC       GetError;
    PFNGLGETINTEGERVPROC    GetIntegerv;
    PFNGLGETSTRINGPROC      GetString;
    PFNGLPIXELSTOREIPROC    PixelStorei;
    PFNGLTEXIMAGE2DPROC     TexImage2D;
    PFNGLTEXPARAMETERFPROC  TexParameterf;
    PFNGLTEXPARAMETERIPROC  TexParameteri;
    PFNGLTEXSUBIMAGE2DPROC  TexSubImage2D;
    PFNGLVIEWPORTPROC       Viewport;

    /* GL only core functions: NULL for GLES2 */
    PFNGLGETTEXLEVELPARAMETERIVPROC GetTexLevelParameteriv; /* Can be NULL */
    PFNGLTEXENVFPROC                TexEnvf; /* Can be NULL */

    /*
     * GL / GLES extensions
     */

    /* Shader commands */
    PFNGLCREATESHADERPROC   CreateShader;
    PFNGLSHADERSOURCEPROC   ShaderSource;
    PFNGLCOMPILESHADERPROC  CompileShader;
    PFNGLATTACHSHADERPROC   AttachShader;
    PFNGLDELETESHADERPROC   DeleteShader;

    /* Shader log commands */
    PFNGLGETPROGRAMIVPROC       GetProgramiv;
    PFNGLGETSHADERIVPROC        GetShaderiv;
    PFNGLGETPROGRAMINFOLOGPROC  GetProgramInfoLog;
    PFNGLGETSHADERINFOLOGPROC   GetShaderInfoLog;

    /* Shader variables commands */
    PFNGLGETUNIFORMLOCATIONPROC      GetUniformLocation;
    PFNGLGETATTRIBLOCATIONPROC       GetAttribLocation;
    PFNGLVERTEXATTRIBPOINTERPROC     VertexAttribPointer;
    PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray;
    PFNGLUNIFORMMATRIX4FVPROC        UniformMatrix4fv;
    PFNGLUNIFORMMATRIX3FVPROC        UniformMatrix3fv;
    PFNGLUNIFORMMATRIX2FVPROC        UniformMatrix2fv;
    PFNGLUNIFORM4FVPROC              Uniform4fv;
    PFNGLUNIFORM4FPROC               Uniform4f;
    PFNGLUNIFORM3FPROC               Uniform3f;
    PFNGLUNIFORM2FPROC               Uniform2f;
    PFNGLUNIFORM1FPROC               Uniform1f;
    PFNGLUNIFORM1IPROC               Uniform1i;

    /* Program commands */
    PFNGLCREATEPROGRAMPROC CreateProgram;
    PFNGLLINKPROGRAMPROC   LinkProgram;
    PFNGLUSEPROGRAMPROC    UseProgram;
    PFNGLDELETEPROGRAMPROC DeleteProgram;

    /* Texture commands */
    PFNGLACTIVETEXTUREPROC ActiveTexture;

    /* Buffers commands */
    PFNGLGENBUFFERSPROC    GenBuffers;
    PFNGLBINDBUFFERPROC    BindBuffer;
    PFNGLBUFFERDATAPROC    BufferData;
    PFNGLDELETEBUFFERSPROC DeleteBuffers;

    /* Framebuffers commands */
    PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC GetFramebufferAttachmentParameteriv;

    /* Commands used for PBO and/or Persistent mapping */
    PFNGLBUFFERSUBDATAPROC          BufferSubData; /* can be NULL */
    PFNGLBUFFERSTORAGEPROC          BufferStorage; /* can be NULL */
    PFNGLMAPBUFFERRANGEPROC         MapBufferRange; /* can be NULL */
    PFNGLFLUSHMAPPEDBUFFERRANGEPROC FlushMappedBufferRange; /* can be NULL */
    PFNGLUNMAPBUFFERPROC            UnmapBuffer; /* can be NULL */
    PFNGLFENCESYNCPROC              FenceSync; /* can be NULL */
    PFNGLDELETESYNCPROC             DeleteSync; /* can be NULL */
    PFNGLCLIENTWAITSYNCPROC         ClientWaitSync; /* can be NULL */
} opengl_vtable_t;

#endif
