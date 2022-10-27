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

#include <vlc_es.h>
#include <vlc_picture.h>
#include <vlc_opengl_interop.h>
#include <vlc_opengl_platform.h>

struct vlc_gl_extension_vt {
    PFNGLGETSTRINGPROC      GetString;
    PFNGLGETSTRINGIPROC     GetStringi;
    PFNGLGETINTEGERVPROC    GetIntegerv;
    PFNGLGETERRORPROC       GetError;
};

#if 0
static inline void
vlc_gl_LoadExtensionFunctions(vlc_gl_t *gl, struct vlc_gl_extension_vt *vt)
{
    vt->GetString = vlc_gl_GetProcAddress(gl, "glGetString");
    vt->GetIntegerv = vlc_gl_GetProcAddress(gl, "glGetIntegerv");
    vt->GetError = vlc_gl_GetProcAddress(gl, "glGetError");
    vt->GetStringi = NULL;

    GLint version;
    vt->GetIntegerv(GL_MAJOR_VERSION, &version);
    uint32_t error = vt->GetError();
    if (error != GL_NO_ERROR)
        version = 2;
    /* Drain the errors before continuing. */
    while (error != GL_NO_ERROR)
        error = vt->GetError();

    /* glGetStringi is available in OpenGL>=3 and GLES>=3.
     * https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glGetString.xhtml
     * https://www.khronos.org/registry/OpenGL-Refpages/es3/html/glGetString.xhtml
     */
    if (version >= 3)
        vt->GetStringi = vlc_gl_GetProcAddress(gl, "glGetStringi");
}

static inline bool
vlc_gl_HasExtension(
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
#endif



/**
 * Format of an OpenGL picture
 */
struct vlc_gl_format {
    video_format_t fmt;

    GLenum tex_target;

    unsigned tex_count;
    GLsizei tex_widths[PICTURE_PLANE_MAX];
    GLsizei tex_heights[PICTURE_PLANE_MAX];

    GLsizei visible_widths[PICTURE_PLANE_MAX];
    GLsizei visible_heights[PICTURE_PLANE_MAX];

    uint32_t formats[PICTURE_PLANE_MAX];
};

/**
 * OpenGL picture
 *
 * It can only be properly used if its format, described by a vlc_gl_format, is
 * known.
 */
struct vlc_gl_picture {
    GLuint textures[PICTURE_PLANE_MAX];

    /**
     * Matrix to convert from 2D pictures coordinates to texture coordinates
     *
     * tex_coords =     mtx    × pic_coords
     *
     *  / tex_x \    / a b c \   / pic_x \
     *  \ tex_y / =  \ d e f / × | pic_y |
     *                           \   1   /
     *
     * It is stored in column-major order: [a, d, b, e, c, f].
     */
    float mtx[2*3];

    /**
     * Indicate if the transform to convert picture coordinates to textures
     * coordinates have changed due to the last picture.
     *
     * The filters should check this flag on every draw() call, and update
     * their coordinates if necessary.
     *
     * It is guaranteed to be true for the first picture.
     */
    bool mtx_has_changed;
};

/**
 * Convert from picture coordinates to texture coordinates, which can be used to
 * sample at the correct location.
 *
 * This is a equivalent to retrieve the matrix and multiply manually.
 *
 * The picture and texture coords may point to the same memory, in that case
 * the transformation is applied in place (overwriting the picture coordinates
 * by the texture coordinates).
 *
 * \param picture the OpenGL picture
 * \param coords_count the number of coordinates (x,y) coordinates to convert
 * \param pic_coords picture coordinates as an array of 2*coords_count floats
 * \param tex_coords_out texture coordinates as an array of 2*coords_count
 *                       floats
 */
VLC_API void
vlc_gl_picture_ToTexCoords(const struct vlc_gl_picture *pic,
                           unsigned coords_count, const float *pic_coords,
                           float *tex_coords_out);

/**
 * Return a matrix to orient texture coordinates
 *
 * This matrix is 2x2 and is stored in column-major order.
 *
 * While pic_to_tex_matrix transforms any picture coordinates into texture
 * coordinates, it may be useful for example for vertex or fragment shaders to
 * sample one pixel to the left of the current one, or two pixels to the top.
 * Since the input texture may be rotated or flipped, the shaders need to
 * know in which direction is the top and in which direction is the right of
 * the picture.
 *
 * This 2x2 matrix allows to transform a 2D vector expressed in picture
 * coordinates into a 2D vector expressed in texture coordinates.
 *
 * Concretely, it contains the coordinates (U, V) of the transformed unit
 * vectors u = / 1 \ and v = / 0 \:
 *             \ 0 /         \ 1 /
 *
 *     / Ux Vx \
 *     \ Uy Vy /
 *
 * It is guaranteed that:
 *  - both U and V are unit vectors (this matrix does not change the scaling);
 *  - only one of their components have a non-zero value (they may not be
 *    oblique); in other words, here are the possible values for U and V:
 *
 *        /  0 \  or  / 0 \  or  / 1 \  or  / -1 \
 *        \ -1 /      \ 1 /      \ 0 /      \  0 /
 *
 *  - U and V are orthogonal.
 *
 * Therefore, there are 8 possible matrices (4 possible rotations, flipped or
 * not).
 *
 * It may theoretically change on every picture (the transform matrix provided
 * by Android may change). If it has changed since the last picture, then
 * pic->mtx_has_changed is true.
 */
VLC_API void
vlc_gl_picture_ComputeDirectionMatrix(const struct vlc_gl_picture *pic,
                                      float direction[2*2]);

VLC_API void
vlc_gl_picture_ToTexCoords(const struct vlc_gl_picture *pic,
                           unsigned coords_count, const float *pic_coords,
                           float *tex_coords_out);

VLC_API void
vlc_gl_picture_ComputeDirectionMatrix(const struct vlc_gl_picture *pic,
                                      float direction[static 2*2]);

typedef struct vlc_gl_t vlc_gl_t;

#if 0
VLC_API struct vlc_gl_interop *
vlc_gl_interop_New(struct vlc_gl_t *gl, vlc_video_context *context,
                   const video_format_t *fmt);

VLC_API struct vlc_gl_interop *
vlc_gl_interop_NewForSubpictures(struct vlc_gl_t *gl);

VLC_API void
vlc_gl_interop_Delete(struct vlc_gl_interop *interop);

VLC_API int
vlc_gl_interop_GenerateTextures(const struct vlc_gl_interop *interop,
                                const size_t *tex_width,
                                const size_t *tex_height, unsigned *textures);

VLC_API void
vlc_gl_interop_DeleteTextures(const struct vlc_gl_interop *interop,
                              unsigned *textures);
#endif

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
    struct vlc_gl_format glfmt;

    /**
     * Matrix to convert from picture coordinates to texture coordinates
     *
     * The matrix is 2x3 and is stored in column-major order:
     *
     *     / a b c \
     *     \ d e f /
     *
     * It is stored as an array of 6 floats:
     *
     *     [a, d, b, e, c, f]
     *
     * To compute texture coordinates, left-multiply the picture coordinates
     * by this matrix:
     *
     *     tex_coords = pic_to_tex_matrix × pic_coords
     *
     *      / tex_x \       / a b c \       / pic_x \
     *      \ tex_y / =     \ d e f /     × | pic_y |
     *                                      \   1   /
     *
     * It is NULL before the first picture is available and may theoretically
     * change on every picture (the transform matrix provided by Android may
     * change). If it has changed since the last picture, then
     * vlc_gl_sampler_MustRecomputeCoords() will return true.
     */
    const float *pic_to_tex_matrix;

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

VLC_API struct vlc_gl_sampler *
vlc_gl_sampler_New(struct vlc_gl_t *gl, const struct vlc_gl_api *api,
                   const struct vlc_gl_format *glfmt, bool expose_planes);

VLC_API void
vlc_gl_sampler_Delete(struct vlc_gl_sampler *sampler);

VLC_API int
vlc_gl_sampler_Update(struct vlc_gl_sampler *sampler,
                      const struct vlc_gl_picture *picture);


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

/**
 * Create a new sampler
 *
 * \param gl the OpenGL context
 * \param api the OpenGL API
 * \param glfmt the input format
 * \param expose_planes if set, vlc_texture() exposes a single plane at a time
 *                      (selected by vlc_gl_sampler_SetCurrentPlane())
 */
VLC_API struct vlc_gl_sampler *
vlc_gl_sampler_New(struct vlc_gl_t *gl, const struct vlc_gl_api *api,
                   const struct vlc_gl_format *glfmt, bool expose_planes);

/**
 * Delete a sampler
 *
 * \param sampler the sampler
 */
VLC_API void
vlc_gl_sampler_Delete(struct vlc_gl_sampler *sampler);

/**
 * Update the input textures
 *
 * \param sampler the sampler
 * \param picture the OpenGL picture
 */
VLC_API int
vlc_gl_sampler_Update(struct vlc_gl_sampler *sampler,
                      const struct vlc_gl_picture *picture);

/**
 * Select the plane to expose
 *
 * If the sampler exposes planes separately (for plane filters), select the
 * plane to expose via the GLSL function vlc_texture().
 *
 * \param sampler the sampler
 * \param plane the plane number
 */
VLC_API void
vlc_gl_sampler_SelectPlane(struct vlc_gl_sampler *sampler, unsigned plane);

VLC_API void
vlc_sampler_yuv2rgb_matrix(float conv_matrix_out[],
                           video_color_space_t color_space,
                           video_color_range_t color_range);


VLC_API void
vlc_gl_sampler_SelectPlane(struct vlc_gl_sampler *sampler, unsigned plane);


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

/* In column-major order */
static const float MATRIX2x3_IDENTITY[2*3] = {
    1, 0,
    0, 1,
    0, 0,
};

/** Return the smallest larger or equal power of 2 */
static inline unsigned vlc_align_pot(unsigned x)
{
    unsigned align = 1 << (8 * sizeof (unsigned) - vlc_clz(x));
    return ((align >> 1) == x) ? x : align;
}

struct opengl_vtable_t;
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

/**
 * Wrap an OpenGL filter from a video filter
 *
 * Open an OpenGL filter (with capability "opengl filter") from a video filter
 * (with capability "video filter").
 *
 * This internally uses the "opengl" video filter to load the OpenGL filter
 * with the given name.
 */
VLC_API module_t *
vlc_gl_WrapOpenGLFilter(filter_t *filter, const char *opengl_filter_name);

struct vlc_gl_extension_vt;

VLC_API unsigned
vlc_gl_GetVersionMajor(struct vlc_gl_extension_vt *vt);

VLC_API void
vlc_gl_LoadExtensionFunctions(vlc_gl_t *gl, struct vlc_gl_extension_vt *vt);

VLC_API bool
vlc_gl_HasExtension(struct vlc_gl_extension_vt *vt,
                    const char *name);

/**
 * An importer uses an interop to convert picture_t to a valid vlc_gl_picture,
 * with all necessary transformations computed.
 */
struct vlc_gl_importer;
struct vlc_gl_interop;

VLC_API struct vlc_gl_importer *
vlc_gl_importer_New(struct vlc_gl_interop *interop);

VLC_API void
vlc_gl_importer_Delete(struct vlc_gl_importer *importer);

VLC_API int
vlc_gl_importer_Update(struct vlc_gl_importer *importer, picture_t *picture);


#endif
