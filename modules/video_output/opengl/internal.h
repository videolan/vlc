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

typedef struct opengl_tex_converter_t opengl_tex_converter_t;

/*
 * Callback to initialize an opengl_tex_converter_t struct
 *
 * The implementation should initialize every members of the struct in regards
 * of the video format.
 *
 * \param fmt video format
 * \param fc OpenGL tex converter that needs to be filled on success
 * \return VLC_SUCCESS or a VLC error
 */
typedef int (*opengl_tex_converter_init_cb)(const video_format_t *fmt,
                                            opengl_tex_converter_t *fc);

/*
 * Structure that is filled by an opengl_tex_converter_init_cb function
 */
struct opengl_tex_converter_t
{
    /* Pointer to object parent, set by the caller of the init cb */
    vlc_object_t *parent;
    /* Function pointer to shaders commands, set by the caller of the init cb */
    const opengl_shaders_api_t *api;
    /* Set it to request a special orientation (by default = fmt.orientation) */
    video_orientation_t orientation;

    /* Video chroma used by this configuration, cannot be 0 */
    vlc_fourcc_t chroma;

    /* Description of the chroma, cannot be NULL */
    const vlc_chroma_description_t *desc;
    /* Texture mapping (usually: GL_TEXTURE_2D), cannot be 0 */
    GLenum tex_target;

    /* The compiled fragment shader, cannot be 0 */
    GLuint fragment_shader;

    /* Private context */
    void *priv;

    /*
     * Callback to generate and prepare textures
     *
     * This function pointer cannot be NULL. The number of textures to generate
     * is specified by desc->plane_count.
     *
     * \param fc OpenGL tex converter
     * \param tex_width array of tex width (one per plane)
     * \param tex_height array of tex height (one per plane)
     * \param textures array of textures to generate (one per plane)
     * \return VLC_SUCCESS or a VLC error
     */
    int (*pf_gen_textures)(const opengl_tex_converter_t *fc,
                           const GLsizei *tex_width, const GLsizei *tex_height,
                           GLuint *textures);

    /*
     * Callback to delete textures generated by pf_gen_textures()
     *
     * This function pointer cannot be NULL.
     *
     * \param fc OpenGL tex converter
     * \param textures array of textures to delete (one per plane)
     */
    void (*pf_del_textures)(const opengl_tex_converter_t *fc,
                            const GLuint *textures);

    /*
     * Callback to allocate a picture pool
     *
     * This function pointer *can* be NULL. If NULL, A generic pool with
     * pictures allocated from the video_format_t will be used.
     *
     * \param fc OpenGL tex converter
     * \param fmt video format
     * \param requested_count number of pictures to allocate
     * \param textures textures generated by pf_gen_textures()
     * \return the picture pool or NULL in case of error
     */
    picture_pool_t *(*pf_get_pool)(const opengl_tex_converter_t *fc,
                                   const video_format_t *fmt,
                                   unsigned requested_count,
                                   const GLuint *textures);

    /*
     * Callback to update a picture
     *
     * This function pointer cannot be NULL. The implementation should upload
     * every planes of the picture.
     *
     * \param fc OpenGL tex converter
     * \param textures array of textures to bind (one per plane)
     * \param width width in pixels
     * \param height height in pixels
     * \param pic picture to update
     * \param plane_offset offsets of each picture planes to read data from
     * (one per plane, can be NULL)
     * \return VLC_SUCCESS or a VLC error
     */
    int (*pf_update)(const opengl_tex_converter_t *fc, const GLuint *textures,
                     unsigned width, unsigned height,
                     picture_t *pic, const size_t *plane_offset);

    /*
     * Callback to prepare the fragment shader
     *
     * This function pointer cannot be NULL. This callback can be used to
     * specify values of uniform variables for the program object that is
     * attached to the configured shader.
     *
     * \param fc OpenGL tex converter
     * \param alpha alpha value, used only for RGBA fragment shader
     * \param program current program object
     */
    void (*pf_prepare_shader)(const opengl_tex_converter_t *fc,
                              GLuint program, float alpha);

    /*
     * Callback to release the shader and the private context
     *
     * This function pointer cannot be NULL.
     * \param fc OpenGL tex converter
     */
    void (*pf_release)(const opengl_tex_converter_t *fc);
};

extern int
opengl_tex_converter_rgba_init(const video_format_t *,
                               opengl_tex_converter_t *);
extern int
opengl_tex_converter_yuv_init(const video_format_t *,
                              opengl_tex_converter_t *);
extern int
opengl_tex_converter_xyz12_init(const video_format_t *,
                                opengl_tex_converter_t *);

#ifdef __ANDROID__
extern int
opengl_tex_converter_anop_init(const video_format_t *,
                               opengl_tex_converter_t *);
#endif

#endif /* include-guard */
