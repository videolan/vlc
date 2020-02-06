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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "sampler.h"

#include <vlc_common.h>
#include <vlc_es.h>
#include <vlc_opengl.h>

#ifdef HAVE_LIBPLACEBO
#include <libplacebo/shaders.h>
#include <libplacebo/shaders/colorspace.h>
#include "../placebo_utils.h"
#endif

#include "gl_api.h"
#include "gl_common.h"
#include "gl_util.h"
#include "internal.h"
#include "interop.h"

struct vlc_gl_sampler *
vlc_gl_sampler_New(struct vlc_gl_interop *interop)
{
    struct vlc_gl_sampler *sampler = calloc(1, sizeof(*sampler));
    if (!sampler)
        return NULL;

    sampler->uloc.pl_vars = NULL;
    sampler->pl_ctx = NULL;
    sampler->pl_sh = NULL;
    sampler->pl_sh_res = NULL;

    sampler->interop = interop;
    sampler->gl = interop->gl;
    sampler->vt = interop->vt;

    sampler->shader.extensions = NULL;
    sampler->shader.body = NULL;

#ifdef HAVE_LIBPLACEBO
    // Create the main libplacebo context
    sampler->pl_ctx = vlc_placebo_Create(VLC_OBJECT(interop->gl));
    if (sampler->pl_ctx) {
#   if PL_API_VER >= 20
        sampler->pl_sh = pl_shader_alloc(sampler->pl_ctx, &(struct pl_shader_params) {
            .glsl = {
#       ifdef USE_OPENGL_ES2
                .version = 100,
                .gles = true,
#       else
                .version = 120,
#       endif
            },
        });
#   elif PL_API_VER >= 6
        sampler->pl_sh = pl_shader_alloc(sampler->pl_ctx, NULL, 0);
#   else
        sampler->pl_sh = pl_shader_alloc(sampler->pl_ctx, NULL, 0, 0);
#   endif
    }
#endif

    int ret =
        opengl_fragment_shader_init(sampler, interop->tex_target,
                                    interop->sw_fmt.i_chroma,
                                    interop->sw_fmt.space,
                                    interop->sw_fmt.orientation);
    if (ret != VLC_SUCCESS)
    {
        free(sampler);
        return NULL;
    }

    /* Texture size */
    for (unsigned j = 0; j < interop->tex_count; j++) {
        const GLsizei w = interop->fmt.i_visible_width  * interop->texs[j].w.num
                        / interop->texs[j].w.den;
        const GLsizei h = interop->fmt.i_visible_height * interop->texs[j].h.num
                        / interop->texs[j].h.den;
        if (interop->api->supports_npot) {
            sampler->tex_width[j]  = w;
            sampler->tex_height[j] = h;
        } else {
            sampler->tex_width[j]  = vlc_align_pot(w);
            sampler->tex_height[j] = vlc_align_pot(h);
        }
    }

    if (!interop->handle_texs_gen)
    {
        ret = vlc_gl_interop_GenerateTextures(interop, sampler->tex_width,
                                              sampler->tex_height,
                                              sampler->textures);
        if (ret != VLC_SUCCESS)
        {
            free(sampler);
            return NULL;
        }
    }

    return sampler;
}

void
vlc_gl_sampler_Delete(struct vlc_gl_sampler *sampler)
{
    struct vlc_gl_interop *interop = sampler->interop;
    const opengl_vtable_t *vt = interop->vt;

    if (!interop->handle_texs_gen)
        vt->DeleteTextures(interop->tex_count, sampler->textures);

#ifdef HAVE_LIBPLACEBO
    FREENULL(sampler->uloc.pl_vars);
    if (sampler->pl_ctx)
        pl_context_destroy(&sampler->pl_ctx);
#endif

    free(sampler->shader.extensions);
    free(sampler->shader.body);

    free(sampler);
}

int
vlc_gl_sampler_Update(struct vlc_gl_sampler *sampler, picture_t *picture)
{
    const struct vlc_gl_interop *interop = sampler->interop;
    const video_format_t *source = &picture->format;

    if (source->i_x_offset != sampler->last_source.i_x_offset
     || source->i_y_offset != sampler->last_source.i_y_offset
     || source->i_visible_width != sampler->last_source.i_visible_width
     || source->i_visible_height != sampler->last_source.i_visible_height)
    {
        memset(sampler->var.TexCoordsMap, 0,
               sizeof(sampler->var.TexCoordsMap));
        for (unsigned j = 0; j < interop->tex_count; j++)
        {
            float scale_w = (float)interop->texs[j].w.num / interop->texs[j].w.den
                          / sampler->tex_width[j];
            float scale_h = (float)interop->texs[j].h.num / interop->texs[j].h.den
                          / sampler->tex_height[j];

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
            float left   = (source->i_x_offset +                       0 ) * scale_w;
            float top    = (source->i_y_offset +                       0 ) * scale_h;
            float right  = (source->i_x_offset + source->i_visible_width ) * scale_w;
            float bottom = (source->i_y_offset + source->i_visible_height) * scale_h;

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
             * is (x', y', 1).
             *
             * The paddings are l (left), r (right), t (top) and b (bottom).
             *
             *               / (r-l)   0     l \
             *      matrix = |   0   (b-t)   t |
             *               \   0     0     1 /
             *
             * It is stored in column-major order.
             */
            GLfloat *matrix = sampler->var.TexCoordsMap[j];
#define COL(x) (x*3)
#define ROW(x) (x)
            matrix[COL(0) + ROW(0)] = right - left;
            matrix[COL(1) + ROW(1)] = bottom - top;
            matrix[COL(2) + ROW(0)] = left;
            matrix[COL(2) + ROW(1)] = top;
            matrix[COL(2) + ROW(2)] = 1;
#undef COL
#undef ROW
        }

        sampler->last_source.i_x_offset = source->i_x_offset;
        sampler->last_source.i_y_offset = source->i_y_offset;
        sampler->last_source.i_visible_width = source->i_visible_width;
        sampler->last_source.i_visible_height = source->i_visible_height;
    }

    /* Update the texture */
    return interop->ops->update_textures(interop, sampler->textures,
                                         sampler->tex_width,
                                         sampler->tex_height, picture,
                                         NULL);
}
