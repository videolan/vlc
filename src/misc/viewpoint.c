/*****************************************************************************
 * viewpoint.c: viewpoint helpers for conversions and transformations
 *****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Alexandre Janniaux <ajanni@videolabs.io>
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

#include <vlc_viewpoint.h>

void vlc_viewpoint_to_4x4( const vlc_viewpoint_t *vp, float *m )
{
    float yaw   = vp->yaw   * (float)M_PI / 180.f + (float)M_PI_2;
    float pitch = vp->pitch * (float)M_PI / 180.f;
    float roll  = vp->roll  * (float)M_PI / 180.f;

    float s, c;

    s = sinf(pitch);
    c = cosf(pitch);
    const float x_rot[4][4] = {
        { 1.f,    0.f,    0.f,    0.f },
        { 0.f,    c,      -s,      0.f },
        { 0.f,    s,      c,      0.f },
        { 0.f,    0.f,    0.f,    1.f } };

    s = sinf(yaw);
    c = cosf(yaw);
    const float y_rot[4][4] = {
        { c,      0.f,    s,     0.f },
        { 0.f,    1.f,    0.f,    0.f },
        { -s,      0.f,    c,      0.f },
        { 0.f,    0.f,    0.f,    1.f } };

    s = sinf(roll);
    c = cosf(roll);
    const float z_rot[4][4] = {
        { c,      s,      0.f,    0.f },
        { -s,     c,      0.f,    0.f },
        { 0.f,    0.f,    1.f,    0.f },
        { 0.f,    0.f,    0.f,    1.f } };

    /**
     * Column-major matrix multiplication mathematically equal to
     * z_rot * x_rot * y_rot
     */
    memset(m, 0, 16 * sizeof(float));
    for (int i=0; i<4; ++i)
        for (int j=0; j<4; ++j)
            for (int k=0; k<4; ++k)
                for (int l=0; l<4; ++l)
                    m[4*i+l] += y_rot[i][j] * x_rot[j][k] * z_rot[k][l];
}
