/*****************************************************************************
 * interop.h
 *****************************************************************************
 * Copyright (C) 2019 Videolabs
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

#ifndef VLC_GL_INTEROP_H
#define VLC_GL_INTEROP_H

#include <vlc_common.h>
#include <vlc_opengl.h>
#include <vlc_picture.h>
#include <vlc_picture_pool.h>

#include "gl_common.h"

struct vlc_gl_interop;

struct vlc_gl_interop_ops {
    /**
     * Callback to allocate data for bound textures
     *
     * This function pointer can be NULL. Software converters should call
     * glTexImage2D() to allocate textures data (it will be deallocated by the
     * caller when calling glDeleteTextures()). Won't be called if
     * handle_texs_gen is true.
     *
     * \param interop the OpenGL interop
     * \param textures array of textures to bind (one per plane)
     * \param tex_width array of tex width (one per plane)
     * \param tex_height array of tex height (one per plane)
     * \return VLC_SUCCESS or a VLC error
     */
    int
    (*allocate_textures)(const struct vlc_gl_interop *interop,
                         GLuint textures[], const GLsizei tex_width[],
                         const GLsizei tex_height[]);

    /**
     * Callback to update a picture
     *
     * This function pointer cannot be NULL. The implementation should upload
     * every planes of the picture.
     *
     * \param interop the OpenGL interop
     * \param textures array of textures to bind (one per plane)
     * \param tex_width array of tex width (one per plane)
     * \param tex_height array of tex height (one per plane)
     * \param pic picture to update
     * \param plane_offset offsets of each picture planes to read data from
     * (one per plane, can be NULL)
     * \return VLC_SUCCESS or a VLC error
     */
    int
    (*update_textures)(const struct vlc_gl_interop *interop,
                       GLuint textures[], const GLsizei tex_width[],
                       const GLsizei tex_height[], picture_t *pic,
                       const size_t plane_offsets[]);

    /**
     * Callback to retrieve the transform matrix to apply to texture coordinates
     *
     * This function pointer can be NULL. If it is set, it may return NULL.
     *
     * Otherwise, it must return a 4x4 matrix, as an array of 16 floats in
     * column-major order.
     *
     * This transform matrix maps 2D homogeneous texture coordinates of the
     * form (s, t, 0, 1) with s and t in the inclusive range [0, 1] to the
     * texture coordinate that should be used to sample that location from the
     * texture.
     *
     * The returned pointer is owned by the converter module, and must not be
     * freed before the module is closed.
     *
     * \param interop the OpenGL interop
     * \return a 4x4 transformatoin matrix (possibly NULL)
     */
    const float *
    (*get_transform_matrix)(const struct vlc_gl_interop *interop);

    /**
     * Called before the interop is destroyed
     *
     * This function pointer can be NULL.
     *
     * \param interop the OpenGL interop
     */
    void
    (*close)(struct vlc_gl_interop *interop);
};

struct vlc_gl_interop {
    vlc_object_t obj;
    module_t *module;

    vlc_gl_t *gl;
    const struct vlc_gl_api *api;
    const opengl_vtable_t *vt; /* for convenience, same as &api->vt */
    GLenum tex_target;

    /* Input format
     *
     * This is the format of the pictures received from the core.
     *
     * It can be modified from the module open function to request changes from
     * the core.
     */
    video_format_t fmt_in;

    /* Output format
     *
     * This is the format of the pictures exposed by the interop to the sampler.
     *
     * It may differ from the input format:
     *  - the orientation may be vertically flipped
     *  - the chroma contains the "software" chroma if the input chroma is opaque
     *  - the chroma may also be changed internally to a fallback (see
     *    opengl_interop_generic_init())
     */
    video_format_t fmt_out;

    /* Pointer to decoder video context, set by the caller (can be NULL) */
    vlc_video_context *vctx;

    /* Set to true if textures are generated from pf_update() */
    bool handle_texs_gen;

    /* Initialized by the interop */
    struct vlc_gl_tex_cfg {
        /*
         * Texture scale factor, cannot be 0.
         * In 4:2:0, 1/1 for the Y texture and 1/2 for the UV texture(s)
         */
        vlc_rational_t w;
        vlc_rational_t h;

        GLint internal;
        GLenum format;
        GLenum type;
    } texs[PICTURE_PLANE_MAX];
    unsigned tex_count;

    void *priv;
    const struct vlc_gl_interop_ops *ops;

    /* Set by the caller to opengl_interop_init_impl().
     * This avoids each module to link against opengl_interop_init_impl()
     * directly. */
    int
    (*init)(struct vlc_gl_interop *interop, GLenum tex_target,
            vlc_fourcc_t chroma, video_color_space_t yuv_space);
};

struct vlc_gl_interop *
vlc_gl_interop_New(struct vlc_gl_t *gl, const struct vlc_gl_api *api,
                   vlc_video_context *context, const video_format_t *fmt);

struct vlc_gl_interop *
vlc_gl_interop_NewForSubpictures(struct vlc_gl_t *gl,
                                 const struct vlc_gl_api *api);

void
vlc_gl_interop_Delete(struct vlc_gl_interop *interop);

int
vlc_gl_interop_GenerateTextures(const struct vlc_gl_interop *interop,
                                const GLsizei *tex_width,
                                const GLsizei *tex_height, GLuint *textures);

void
vlc_gl_interop_DeleteTextures(const struct vlc_gl_interop *interop,
                              GLuint *textures);

static inline int
opengl_interop_init(struct vlc_gl_interop *interop, GLenum tex_target,
                    vlc_fourcc_t chroma, video_color_space_t yuv_space)
{
    return interop->init(interop, tex_target, chroma, yuv_space);
}

#endif
