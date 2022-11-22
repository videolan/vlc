/*****************************************************************************
 * importer.c
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_picture.h>

#include "importer_priv.h"

#include "gl_api.h"
#include "gl_common.h"
#include "gl_util.h"
#include "interop.h"
#include "picture.h"

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
        GLsizei w = (interop->fmt_out.i_visible_width + interop->fmt_out.i_x_offset) * interop->texs[j].w.num
                  / interop->texs[j].w.den;
        GLsizei h = (interop->fmt_out.i_visible_height + interop->fmt_out.i_y_offset) *  interop->texs[j].h.num
                  / interop->texs[j].h.den;
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
