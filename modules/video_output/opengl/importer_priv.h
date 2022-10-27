/*****************************************************************************
 * importer_priv.h
 *****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

#ifndef VLC_GL_IMPORTER_PRIV_H
#define VLC_GL_IMPORTER_PRIV_H

#include "importer.h"

#include <vlc_opengl_platform.h>
#include <vlc_opengl_filter.h>

#include "interop.h"
#include "picture.h"

struct vlc_gl_importer {
    struct vlc_gl_format glfmt;
    struct vlc_gl_interop *interop;

    /* For convenience, same as interop->api and interop->api->vt */
    const struct vlc_gl_api *api;
    const opengl_vtable_t *vt;

    struct vlc_gl_picture pic;

    struct {
        unsigned int i_x_offset;
        unsigned int i_y_offset;
        unsigned int i_visible_width;
        unsigned int i_visible_height;
    } last_source;

    /* All matrices below are stored in column-major order. */

    float mtx_orientation[2*3];
    float mtx_coords_map[2*3];

    float mtx_transform[2*3];
    bool mtx_transform_defined;

    /**
     * The complete transformation matrix is stored in pic.mtx.
     *
     * tex_coords =  pic_to_tex × pic_coords
     *
     *  / tex_x \    / a b c \    / pic_x \
     *  \ tex_y / =  \ d e f /  × | pic_y |
     *                            \   1   /
     *
     * Semantically, it represents the result of:
     *
     *     get_transform_matrix() * mtx_coords_map * mtx_orientation
     *
     * (The intermediate matrices are implicitly expanded to 3x3 with [0 0 1]
     * as the last row.)
     *
     * It is stored in column-major order: [a, d, b, e, c, f].
     */
    bool pic_mtx_defined;
};

#endif
