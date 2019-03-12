/*****************************************************************************
 * viewpoint.c: test for viewpoint
 *****************************************************************************
 * Copyright (C) 2019 VLC Authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <vlc_viewpoint.h>
#include <math.h>
#include "../../libvlc/test.h"

static bool
compare_angles(float epsilon, const float a1[3], const float a2[3])
{
    const float MAX_YAW   = 180.f;
    const float MAX_PITCH = 360.f;
    const float MAX_ROLL  = 180.f;

    /* We add MAX_YAW, MAX_PITCH and MAX_ROLL to ensure the value for fmodf
     * will stay positive. The value will be between 0 and {MAX_ANGLE}. */
    float dy = fmodf(MAX_YAW   + (a1[0] - a2[0]), MAX_YAW);
    float dp = fmodf(MAX_PITCH + (a1[1] - a2[1]), MAX_PITCH);
    float dr = fmodf(MAX_ROLL  + (a1[2] - a2[2]), MAX_ROLL);

    /* Check the two borders of the torus, 0.f and 180.f or 360.f depending
     * on the range of the compared value. */
    return (dy < epsilon || MAX_YAW   - dy < epsilon) &&
           (dp < epsilon || MAX_PITCH - dp < epsilon) &&
           (dr < epsilon || MAX_ROLL  - dr < epsilon);
}

/**
 * Execute conversion back and forth from Euler angles to quaternion.
 * Check that the original angles are preserved by the conversion methods.
 */
static bool
reciprocal_euler(float epsilon, float yaw, float pitch, float roll)
{
    vlc_viewpoint_t vp;
    vlc_viewpoint_from_euler(&vp, yaw, pitch, roll);

    float yaw2, pitch2, roll2;
    vlc_viewpoint_to_euler(&vp, &yaw2, &pitch2, &roll2);

    fprintf(stderr, "==========================================\n");
    fprintf(stderr, "original:   yaw=%f, pitch=%f, roll=%f\n", yaw, pitch, roll);
    fprintf(stderr, "converted:  yaw=%f, pitch=%f, roll=%f\n", yaw2, pitch2, roll2);
    fprintf(stderr, "==========================================\n");

    return compare_angles(epsilon,
            (float[]){yaw, pitch, roll},
            (float[]){yaw, pitch, roll});
}

static void test_conversion()
{
    const float epsilon = 0.1f;
    assert(reciprocal_euler(epsilon, 0.f,  0.f,  0.f));
    assert(reciprocal_euler(epsilon, 45.f, 0.f,  0.f));
    assert(reciprocal_euler(epsilon, 0.f,  45.f, 0.f));
    assert(reciprocal_euler(epsilon, 0.f,  0.f,  45.f));
    assert(reciprocal_euler(epsilon, 45.f, 45.f, 0.f));
    assert(reciprocal_euler(epsilon, 0.f,  45.f, 45.f));
    assert(reciprocal_euler(epsilon, 45.f, 45.f, 45.f));

    assert(reciprocal_euler(epsilon, -45.f,  0.f,  0.f));
    assert(reciprocal_euler(epsilon,  0.f,  -45.f, 0.f));
    assert(reciprocal_euler(epsilon,  0.f,   0.f,  -45.f));
    assert(reciprocal_euler(epsilon, -45.f, -45.f,  0.f));
    assert(reciprocal_euler(epsilon,  0.f,  -45.f, -45.f));
    assert(reciprocal_euler(epsilon, -45.f, -45.f, -45.f));
}

int main( void )
{
    test_init();

    test_conversion();

    return 0;
}
