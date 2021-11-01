/*****************************************************************************
 * vlc_opengl_filter.h:
 *****************************************************************************
 * Copyright (C) 2019-2021 Videolabs
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

#ifndef VLC_OPENGL_FILTER_H
#define VLC_OPENGL_FILTER_H

#include <vlc_picture.h>

struct vlc_gl_interop;
typedef struct vlc_gl_t vlc_gl_t;

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
                         unsigned textures[], const size_t tex_width[],
                         const size_t tex_height[]);

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
                       unsigned textures[], const size_t tex_width[],
                       const size_t tex_height[], picture_t *pic,
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

struct opengl_vtable_t;
struct vlc_gl_interop {
    vlc_object_t obj;
    module_t *module;

    vlc_gl_t *gl;
    const struct vlc_gl_api *api;
    const struct opengl_vtable_t *vt; /* for convenience, same as &api->vt */
    int tex_target;

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
    struct vlc_video_context *vctx;

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

        int internal;
        int format;
        int type;
    } texs[PICTURE_PLANE_MAX];
    unsigned tex_count;

    void *priv;
    const struct vlc_gl_interop_ops *ops;

    /* Set by the caller to opengl_interop_init_impl().
     * This avoids each module to link against opengl_interop_init_impl()
     * directly. */
    int
    (*init)(struct vlc_gl_interop *interop, int tex_target,
            vlc_fourcc_t chroma, video_color_space_t yuv_space);
};

VLC_API struct vlc_gl_interop *
vlc_gl_interop_New(struct vlc_gl_t *gl, const struct vlc_gl_api *api,
                   vlc_video_context *context, const video_format_t *fmt);

VLC_API struct vlc_gl_interop *
vlc_gl_interop_NewForSubpictures(struct vlc_gl_t *gl,
                                 const struct vlc_gl_api *api);

VLC_API void
vlc_gl_interop_Delete(struct vlc_gl_interop *interop);

VLC_API int
vlc_gl_interop_GenerateTextures(const struct vlc_gl_interop *interop,
                                const size_t *tex_width,
                                const size_t *tex_height, unsigned *textures);

VLC_API void
vlc_gl_interop_DeleteTextures(const struct vlc_gl_interop *interop,
                              unsigned *textures);

static inline int
opengl_interop_init(struct vlc_gl_interop *interop, int tex_target,
                    vlc_fourcc_t chroma, video_color_space_t yuv_space)
{
    return interop->init(interop, tex_target, chroma, yuv_space);
}


/**
 * The purpose of a sampler is to provide pixel values of a VLC input picture,
 * stored in any format.
 *
 * Concretely, a GLSL function:
 *
 *     vec4 vlc_texture(vec2 coords)
 *
 * returns the RGBA values for the given coordinates.
 *
 * Contrary to the standard GLSL function:
 *
 *     vec4 texture2D(sampler2D sampler, vec2 coords)
 *
 * it does not take a sampler2D as parameter. The role of the sampler is to
 * abstract the input picture from the renderer, so the input picture is
 * implicitly available.
 */
struct vlc_gl_sampler {
    /* Input format */
    video_format_t fmt;

    /* Number of input planes */
    unsigned tex_count;

    /* Texture sizes (arrays of tex_count values) */
    const size_t *tex_widths;
    const size_t *tex_heights;

    struct {
        /**
         * Piece of fragment shader code declaration OpenGL extensions.
         *
         * It is initialized by the sampler, and may be NULL if no extensions
         * are required.
         *
         * If non-NULL, users of this sampler must inject this provided code
         * into their fragment shader, immediately after the "version" line.
         */
        char *extensions;

        /**
         * Piece of fragment shader code providing the GLSL function
         * vlc_texture(vec2 coords).
         *
         * It is initialized by the sampler, and is never NULL.
         *
         * Users of this sampler should inject this provided code into their
         * fragment shader, before any call to vlc_texture().
         */
        char *body;

        /**
         * Piece of vertex shader code providing the GLSL function
         * vlc_texture_coords(vec2 coords).
         *
         * It is initialized by the sampler, and is never NULL.
         *
         * Users of this sampler should inject this provided code into their
         * vertex shader, before any call to vlc_texture_coords().
         */
        char *vertex_body;
    } shader;

    const struct vlc_gl_sampler_ops *ops;
};

struct vlc_gl_sampler_ops {
    /**
     * Callback to fetch locations of uniform or attributes variables
     *
     * This function pointer cannot be NULL. This callback is called one time
     * after init.
     *
     * \param sampler the sampler
     * \param program linked program that will be used by this sampler
     */
    void
    (*fetch_locations)(struct vlc_gl_sampler *sampler, unsigned program);

    /**
     * Callback to load sampler data
     *
     * This function pointer cannot be NULL. This callback can be used to
     * specify values of uniform variables.
     *
     * \param sampler the sampler
     */
    void
    (*load)(const struct vlc_gl_sampler *sampler);
};

static inline void
vlc_gl_sampler_FetchLocations(struct vlc_gl_sampler *sampler, unsigned program)
{
    sampler->ops->fetch_locations(sampler, program);
}

static inline void
vlc_gl_sampler_Load(const struct vlc_gl_sampler *sampler)
{
    sampler->ops->load(sampler);
}

VLC_API void
vlc_sampler_yuv2rgb_matrix(float conv_matrix_out[],
                           video_color_space_t color_space,
                           video_color_range_t color_range);


static const float MATRIX4_IDENTITY[4*4] = {
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1,
};

static const float MATRIX3_IDENTITY[3*3] = {
    1, 0, 0,
    0, 1, 0,
    0, 0, 1,
};

/** Return the smallest larger or equal power of 2 */
static inline unsigned vlc_align_pot(unsigned x)
{
    unsigned align = 1 << (8 * sizeof (unsigned) - vlc_clz(x));
    return ((align >> 1) == x) ? x : align;
}

/**
 * Build an OpenGL program
 *
 * Both the fragment shader and fragment shader are passed as a list of
 * strings, forming the shader source code once concatenated, like
 * glShaderSource().
 *
 * \param obj a VLC object, used to log messages
 * \param vt the OpenGL virtual table
 * \param vstring_count the number of strings in vstrings
 * \param vstrings a list of NUL-terminated strings containing the vertex
 *                 shader source code
 * \param fstring_count the number of strings in fstrings
 * \param fstrings a list of NUL-terminated strings containing the fragment
 *                 shader source code
 */
VLC_API unsigned
vlc_gl_BuildProgram(vlc_object_t *obj, const struct opengl_vtable_t *vt,
                    size_t vstring_count, const char **vstrings,
                    size_t fstring_count, const char **fstrings);

#endif
