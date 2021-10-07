/*****************************************************************************
 * picture.c
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "picture.h"

void
vlc_gl_picture_ToTexCoords(const struct vlc_gl_picture *pic,
                           unsigned coords_count, const float *pic_coords,
                           float *tex_coords_out)
{
    const float *mtx = pic->mtx;
    assert(mtx);

#define MTX(ROW,COL) mtx[(COL)*2+(ROW)]
    for (unsigned i = 0; i < coords_count; ++i)
    {
        /* Store the coordinates, in case the transform must be applied in
         * place (i.e. with pic_coords == tex_coords_out) */
        float x = pic_coords[0];
        float y = pic_coords[1];
        tex_coords_out[0] = MTX(0,0) * x + MTX(0,1) * y + MTX(0,2);
        tex_coords_out[1] = MTX(1,0) * x + MTX(1,1) * y + MTX(1,2);
        pic_coords += 2;
        tex_coords_out += 2;
    }
}

void
vlc_gl_picture_ComputeDirectionMatrix(const struct vlc_gl_picture *pic,
                                      float direction[static 2*2])
{
    /**
     * The direction matrix is extracted from pic->mtx:
     *
     *    mtx = / a b c \
     *          \ d e f /
     *
     * The last column (the offset part of the affine transformation) is
     * discarded, and the 2 remaining column vectors are normalized to remove
     * any scaling:
     *
     *    direction = / a/unorm  b/vnorm \
     *                \ d/unorm  e/vnorm /
     *
     * where unorm = norm( / a \ ) and vnorm = norm( / b \ ).
     *                     \ d /                     \ e /
     */

    float ux = pic->mtx[0];
    float uy = pic->mtx[1];
    float vx = pic->mtx[2];
    float vy = pic->mtx[3];

    float unorm = sqrt(ux * ux + uy * uy);
    float vnorm = sqrt(vx * vx + vy * vy);

    direction[0] = ux / unorm;
    direction[1] = uy / unorm;
    direction[2] = vx / vnorm;
    direction[3] = vy / vnorm;
}
