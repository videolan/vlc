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


static void mat4x4_for_angles( float *m, float *angles )
{
    const float yaw   = -angles[0] * (float)M_PI / 180.f;
    const float pitch = -angles[1] * (float)M_PI / 180.f;
    const float roll  = -angles[2] * (float)M_PI / 180.f;

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

static int fuzzy_memcmp(const float *a, const float *b,
                         size_t size, float epsilon)
{
    for (size_t i=0; i < size; ++i)
    {
        if (fabs(a[i]-b[i]) > epsilon)
        {
            fprintf(stderr, "Difference at %zu, a[%zu]=%f, b[%zu]=%f\n",
                    i, i, a[i], i, b[i]);
            return a[i] < b[i] ? -1 : 1;
        }
    }
    return 0;
}

struct example_mat4x4
{
    float angles[3];
};

struct example_mat4x4 examples_mat4x4[] = {
    { .angles = {  0.f,   45.f,  45.f } },
    { .angles = { 45.f,   45.f,  0.f  } },
    { .angles = { 45.f,   0.f,   0.f  } },
    { .angles = { -45.f,  0.f,   0.f  } },
    { .angles = {  0.f,   45.f,  0.f  } },
    { .angles = {  0.f,    0.f,  45.f } },
    { .angles = { 90.f, 0.f, 0.f } },
    { .angles = { 0.f, 90.f, 0.f } },
    { .angles = { 0.f, 0.f, 90.f } },
    { .angles = { 90.f, 90.f, 0.f } },
    { .angles = { 90.f, 0.f, 90.f } },
    { .angles = { 0.f, 90.f, 90.f } },
    { .angles = { 90.f, 90.f, 90.f } },
};

static void
test_conversion_viewpoint_mat4x4()
{
    const float epsilon = 0.1f;
    vlc_viewpoint_t vp;
    float mat[16];

#define MATLINE "[%f %f %f %f]\n"
#define MAT MATLINE MATLINE MATLINE MATLINE
#define printmat(title, mat) \
    fprintf(stderr, title ":\n" MAT "\n", \
           mat[0],  mat[1],  mat[2], mat[3], \
           mat[4],  mat[5],  mat[6], mat[7], \
           mat[8],  mat[9],  mat[10], mat[11], \
           mat[12], mat[13], mat[14], mat[15]);

    bool success = true;
    for (size_t i=0; i<ARRAY_SIZE(examples_mat4x4); ++i)
    {
        struct example_mat4x4 *ex = &examples_mat4x4[i];
        vlc_viewpoint_from_euler(&vp,
                                 ex->angles[0],
                                 ex->angles[1],
                                 ex->angles[2]);
        vlc_viewpoint_to_4x4(&vp, mat);
        fprintf(stderr, "angles: %f %f %f\n",
                ex->angles[0], ex->angles[1],
                ex->angles[2]);

        float expect_mat[16];
        mat4x4_for_angles(expect_mat, ex->angles);

        bool diff = fuzzy_memcmp(mat, expect_mat,
                                 ARRAY_SIZE(mat), epsilon);
        if (!diff)
            continue;

        printmat("EXPECT COMPUTED", expect_mat);
        printmat("RESULT", mat);
        success = false;
    }

    assert(success);
}

int main( void )
{
    test_init();

    test_conversion();
    test_conversion_viewpoint_mat4x4();

    return 0;
}
