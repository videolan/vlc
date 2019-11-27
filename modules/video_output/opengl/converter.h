/*****************************************************************************
 * converter.h: OpenGL internal header
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

#ifndef VLC_OPENGL_CONVERTER_H
#define VLC_OPENGL_CONVERTER_H

#include <vlc_common.h>
#include <vlc_codec.h>
#include <vlc_opengl.h>
#include <vlc_picture_pool.h>
#include <vlc_plugin.h>
#include "gl_common.h"

struct pl_context;
struct pl_shader;
struct pl_shader_res;

/*
 * Structure that is filled by "glhw converter" module probe function
 * The implementation should initialize every members of the struct that are
 * not set by the caller
 */
typedef struct opengl_tex_converter_t opengl_tex_converter_t;
struct opengl_tex_converter_t
{
    struct vlc_object_t obj;

    module_t *p_module;

    /* Pointer to object gl, set by the caller */
    vlc_gl_t *gl;

    /* Pointer to decoder video context, set by the caller (can be NULL) */
    vlc_video_context *vctx;

    /* libplacebo context, created by the caller (optional) */
    struct pl_context *pl_ctx;

    /* Function pointers to OpenGL functions, set by the caller */
    const opengl_vtable_t *vt;

    /* True to dump shaders, set by the caller */
    bool b_dump_shaders;

    /* Function pointer to the shader init command, set by the caller, see
     * opengl_fragment_shader_init() documentation. */
    GLuint (*pf_fragment_shader_init)(opengl_tex_converter_t *, GLenum,
                                      vlc_fourcc_t, video_color_space_t);

    /* Available gl extensions (from GL_EXTENSIONS) */
    const char *glexts;

    /* True if the current API is OpenGL ES, set by the caller */
    bool is_gles;
    /* GLSL version, set by the caller. 100 for GLSL ES, 120 for desktop GLSL */
    unsigned glsl_version;
    /* Precision header, set by the caller. In OpenGLES, the fragment language
     * has no default precision qualifier for floating point types. */
    const char *glsl_precision_header;

    /* Can only be changed from the module open function */
    video_format_t fmt;

    /* Fragment shader, must be set from the module open function. It will be
     * deleted by the caller. */
    GLuint fshader;

    /* Number of textures, cannot be 0 */
    unsigned tex_count;

    /* Texture mapping (usually: GL_TEXTURE_2D), cannot be 0 */
    GLenum tex_target;

    /* Set to true if textures are generated from pf_update() */
    bool handle_texs_gen;

    struct opengl_tex_cfg {
        /* Texture scale factor, cannot be 0 */
        vlc_rational_t w;
        vlc_rational_t h;

        /* The following is used and filled by the opengl_fragment_shader_init
         * function. */
        GLint  internal;
        GLenum format;
        GLenum type;
    } texs[PICTURE_PLANE_MAX];

    /* The following is used and filled by the opengl_fragment_shader_init
     * function. */
    struct {
        GLint Texture[PICTURE_PLANE_MAX];
        GLint TexSize[PICTURE_PLANE_MAX]; /* for GL_TEXTURE_RECTANGLE */
        GLint Coefficients;
        GLint FillColor;
        GLint *pl_vars; /* for pl_sh_res */
    } uloc;
    bool yuv_color;
    GLfloat yuv_coefficients[16];

    struct pl_shader *pl_sh;
    const struct pl_shader_res *pl_sh_res;

    /* Private context */
    void *priv;

    /**
     * Callback to allocate data for bound textures
     *
     * This function pointer can be NULL. Software converters should call
     * glTexImage2D() to allocate textures data (it will be deallocated by the
     * caller when calling glDeleteTextures()). Won't be called if
     * handle_texs_gen is true.
     *
     * \param tc OpenGL tex converter
     * \param textures array of textures to bind (one per plane)
     * \param tex_width array of tex width (one per plane)
     * \param tex_height array of tex height (one per plane)
     * \return VLC_SUCCESS or a VLC error
     */
    int (*pf_allocate_textures)(const opengl_tex_converter_t *tc, GLuint *textures,
                                const GLsizei *tex_width, const GLsizei *tex_height);

    /**
     * Callback to allocate a picture pool
     *
     * This function pointer *can* be NULL. If NULL, A generic pool with
     * pictures allocated from the video_format_t will be used.
     *
     * \param tc OpenGL tex converter
     * \param requested_count number of pictures to allocate
     * \return the picture pool or NULL in case of error
     */
    picture_pool_t *(*pf_get_pool)(const opengl_tex_converter_t *tc,
                                   unsigned requested_count);

    /**
     * Callback to update a picture
     *
     * This function pointer cannot be NULL. The implementation should upload
     * every planes of the picture.
     *
     * \param tc OpenGL tex converter
     * \param textures array of textures to bind (one per plane)
     * \param tex_width array of tex width (one per plane)
     * \param tex_height array of tex height (one per plane)
     * \param pic picture to update
     * \param plane_offset offsets of each picture planes to read data from
     * (one per plane, can be NULL)
     * \return VLC_SUCCESS or a VLC error
     */
    int (*pf_update)(const opengl_tex_converter_t *tc, GLuint *textures,
                     const GLsizei *tex_width, const GLsizei *tex_height,
                     picture_t *pic, const size_t *plane_offset);

    /**
     * Callback to fetch locations of uniform or attributes variables
     *
     * This function pointer cannot be NULL. This callback is called one time
     * after init.
     *
     * \param tc OpenGL tex converter
     * \param program linked program that will be used by this tex converter
     * \return VLC_SUCCESS or a VLC error
     */
    int (*pf_fetch_locations)(opengl_tex_converter_t *tc, GLuint program);

    /**
     * Callback to prepare the fragment shader
     *
     * This function pointer cannot be NULL. This callback can be used to
     * specify values of uniform variables.
     *
     * \param tc OpenGL tex converter
     * \param tex_width array of tex width (one per plane)
     * \param tex_height array of tex height (one per plane)
     * \param alpha alpha value, used only for RGBA fragment shader
     */
    void (*pf_prepare_shader)(const opengl_tex_converter_t *tc,
                              const GLsizei *tex_width, const GLsizei *tex_height,
                              float alpha);
};

/**
 * Generate a fragment shader
 *
 * This utility function can be used by hw opengl tex converters that need a
 * generic fragment shader. It will compile a fragment shader generated from
 * the chroma and the tex target. This will initialize all elements of the
 * opengl_tex_converter_t struct except for priv, pf_allocate_texture,
 * pf_get_pool, pf_update
 *
 * \param tc OpenGL tex converter
 * \param tex_target GL_TEXTURE_2D or GL_TEXTURE_RECTANGLE
 * \param chroma chroma used to generate the fragment shader
 * \param yuv_space if not COLOR_SPACE_UNDEF, YUV planes will be converted to
 * RGB according to the color space
 * \return the compiled fragment shader or 0 in case of error
 */
static inline GLuint
opengl_fragment_shader_init(opengl_tex_converter_t *tc, GLenum tex_target,
                            vlc_fourcc_t chroma, video_color_space_t yuv_space)
{
    return tc->pf_fragment_shader_init(tc, tex_target, chroma, yuv_space);
}

#endif /* include-guard */
