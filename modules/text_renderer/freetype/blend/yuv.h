/*****************************************************************************
 * yuv.h :
 *****************************************************************************
 * Copyright (C) 2002 - 2020 VLC authors and VideoLAN
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include "blend.h"

static void YUVFromRGB( uint32_t i_argb,
                        uint8_t *pi_y, uint8_t *pi_u, uint8_t *pi_v )
{
    int i_red   = ( i_argb & 0x00ff0000 ) >> 16;
    int i_green = ( i_argb & 0x0000ff00 ) >>  8;
    int i_blue  = ( i_argb & 0x000000ff );

    /* 18.13 fixed point of
        Y  =      (0.257 * R) + (0.504 * G) + (0.098 * B) + 16
        Cb = U = -(0.148 * R) - (0.291 * G) + (0.439 * B) + 128
        Cr = V =  (0.439 * R) - (0.368 * G) - (0.071 * B) + 128 */
    uint8_t y = abs( 2104 * i_red  + 4130 * i_green +
                     802 * i_blue + 4096 + 131072 ) >> 13;
    uint8_t u = abs( -1214 * i_red  + -2384 * i_green +
                     3598 * i_blue + 4096 + 1048576) >> 13;
    uint8_t v = abs( 3598 * i_red + -3013 * i_green +
                    -585 * i_blue + 4096 + 1048576) >> 13;
    *pi_y = __MIN(y, 235);
    *pi_u = __MIN(u, 240);
    *pi_v = __MIN(v, 240);
}

static void FillYUVAPicture( picture_t *p_picture,
                             int i_a, int i_y, int i_u, int i_v,
                             int x, int y, int w, int h )
{
    int values[4] = { i_y, i_u, i_v, i_a };
    for( int i = 0; i < 4; i++ )
    {
        plane_t *plane = &p_picture->p[i];
        uint8_t *row = &plane->p_pixels[y * p_picture->p->i_pitch +
                                        x * plane->i_pixel_pitch];
        for( int dy = 0; dy < h; dy++ )
        {
            memset( row, values[i], plane->i_pixel_pitch * w );
            row += p_picture->p->i_pitch;
        }
    }
}

DECL_PIXEL_BLENDER(YUVA, 0, 0, 0, 0, A_PLANE, Y_PLANE, U_PLANE, V_PLANE);

static void BlendGlyphToYUVA( picture_t *p_picture,
                              int i_picture_x, int i_picture_y,
                              int i_a, int i_x, int i_y, int i_z,
                              FT_BitmapGlyph p_glyph )
{
    const uint8_t *srcrow = p_glyph->bitmap.buffer;
    int i_pitch_src = p_glyph->bitmap.pitch;

    uint8_t *dstrows[4];
    dstrows[0] = &p_picture->p[0].p_pixels[i_picture_y * p_picture->p[0].i_pitch +
                                           i_picture_x * p_picture->p[0].i_pixel_pitch];
    dstrows[1] = &p_picture->p[1].p_pixels[i_picture_y * p_picture->p[1].i_pitch +
                                           i_picture_x * p_picture->p[1].i_pixel_pitch];
    dstrows[2] = &p_picture->p[2].p_pixels[i_picture_y * p_picture->p[2].i_pitch +
                                           i_picture_x * p_picture->p[2].i_pixel_pitch];
    dstrows[3] = &p_picture->p[3].p_pixels[i_picture_y * p_picture->p[3].i_pitch +
                                           i_picture_x * p_picture->p[3].i_pixel_pitch];

    for( unsigned int dy = 0; dy < p_glyph->bitmap.rows; dy++ )
    {
        const uint8_t *src = srcrow;

        uint8_t *dst[4];
        memcpy(dst, dstrows, 4 * sizeof(dst[0]));
        for( unsigned int dx = 0; dx < p_glyph->bitmap.width; dx++ )
        {
            BlendYUVAPixel( dst, i_a, i_x, i_y, i_z, *src++ );
            dst[0] += p_picture->p[0].i_pixel_pitch;
            dst[1] += p_picture->p[1].i_pixel_pitch;
            dst[2] += p_picture->p[2].i_pixel_pitch;
            dst[3] += p_picture->p[3].i_pixel_pitch;
        }

        srcrow += i_pitch_src;
        dstrows[0] += p_picture->p[0].i_pitch;
        dstrows[1] += p_picture->p[1].i_pitch;
        dstrows[2] += p_picture->p[2].i_pitch;
        dstrows[3] += p_picture->p[3].i_pitch;
    }
}
