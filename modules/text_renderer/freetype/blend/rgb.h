/*****************************************************************************
 * rgb.h :
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

static void RGBFromRGB( uint32_t i_argb,
                        uint8_t *pi_r, uint8_t *pi_g, uint8_t *pi_b )
{
    *pi_r = ( i_argb & 0x00ff0000 ) >> 16;
    *pi_g = ( i_argb & 0x0000ff00 ) >>  8;
    *pi_b = ( i_argb & 0x000000ff );
}

#define DECL_RGB_FILLER( name, APOS, XPOS, YPOS, ZPOS ) \
static void Fill##name##Picture( picture_t *p_picture,\
                                 int a, int r, int g, int b,\
                                 int x, int y, int w, int h )\
{\
    uint8_t *row = &p_picture->p[0].p_pixels[y * p_picture->p->i_pitch + x * 4];\
\
    if (unlikely(a == 0 || (a == r && b == g && r == g)))\
    {   /* fast path */\
        memset(row, a, h * p_picture->p[0].i_pitch);\
        return;\
    }\
\
    for( int dy = 0; dy < h; dy++ )\
    {\
        uint8_t *p = row;\
        for( int dx = 0; dx < w; dx++ )\
        {\
            p[XPOS] = r;\
            p[YPOS] = g;\
            p[ZPOS] = b;\
            p[APOS] = a;\
            p += 4;\
        }\
        row += p_picture->p->i_pitch;\
    }\
}

DECL_RGB_FILLER( RGBA, 3, 0, 1, 2 );
DECL_PIXEL_BLENDER(RGBA, 3, 0, 1, 2, 0, 0, 0, 0);

DECL_RGB_FILLER( ARGB, 0, 1, 2, 3 );
DECL_PIXEL_BLENDER(ARGB, 0, 1, 2, 3, 0, 0, 0, 0);

#undef DECL_RGB_FILLER

static inline void BlendGlyphToRGB( picture_t *p_picture,
                                    int i_picture_x, int i_picture_y,
                                    int i_a, int i_x, int i_y, int i_z,
                                    FT_BitmapGlyph p_glyph,
                                    void (*BlendPixel)( uint8_t **, int, int, int, int, int ) )
{
    const uint8_t *srcrow = p_glyph->bitmap.buffer;
    int i_pitch_src = p_glyph->bitmap.pitch;
    int i_pitch_dst = p_picture->p[0].i_pitch;
    uint8_t *dstrow = &p_picture->p[0].p_pixels[i_picture_y * i_pitch_dst + 4 * i_picture_x];

    for( unsigned int dy = 0; dy < p_glyph->bitmap.rows; dy++ )
    {
        const uint8_t *src = srcrow;
        uint8_t *dst = dstrow;
        for( unsigned int dx = 0; dx < p_glyph->bitmap.width; dx++ )
        {
            BlendPixel( &dst, i_a, i_x, i_y, i_z, *src++ );
            dst += 4;
        }
        srcrow += i_pitch_src;
        dstrow += i_pitch_dst;
    }
}

static void BlendGlyphToRGBA( picture_t *p_picture,
                              int i_picture_x, int i_picture_y,
                              int i_a, int i_x, int i_y, int i_z,
                              FT_BitmapGlyph p_glyph )
{
    BlendGlyphToRGB( p_picture, i_picture_x, i_picture_y,
                     i_a, i_x, i_y, i_z, p_glyph,
                     BlendRGBAPixel );
}

static void BlendGlyphToARGB( picture_t *p_picture,
                              int i_picture_x, int i_picture_y,
                              int i_a, int i_x, int i_y, int i_z,
                              FT_BitmapGlyph p_glyph )
{
    BlendGlyphToRGB( p_picture, i_picture_x, i_picture_y,
                     i_a, i_x, i_y, i_z, p_glyph,
                     BlendARGBPixel );
}
