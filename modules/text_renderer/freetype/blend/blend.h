/*****************************************************************************
 * blend.h :
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
#ifndef VLC_FREETYPE_BLEND_H
#define VLC_FREETYPE_BLEND_H

typedef void (*ft_glyph_blender)( picture_t *, int, int,
                                  int, int, int, int, FT_BitmapGlyph );
typedef void (*ft_surface_filler)( picture_t *, int, int, int, int,
                                   int, int, int, int);
typedef void (*ft_components_extractor)( uint32_t, uint8_t *, uint8_t *, uint8_t * );

typedef struct
{
    ft_components_extractor extract;
    ft_surface_filler fill;
    ft_glyph_blender blend;
} ft_drawing_functions;

#define DECL_PIXEL_BLENDER( name, APOS, XPOS, YPOS, ZPOS, APLANE, XPLANE, YPLANE, ZPLANE ) \
static inline void Blend##name##Pixel( uint8_t **dst, int a, int x, int y, int z, int glyph_a )\
{\
    if( glyph_a == 0 )\
        return;\
\
    int i_an = a * glyph_a / 255;\
\
    int i_ao = dst[APLANE][APOS];\
    if( i_ao == 0 )\
    {\
        dst[XPLANE][XPOS] = x;\
        dst[YPLANE][YPOS] = y;\
        dst[ZPLANE][ZPOS] = z;\
        dst[APLANE][APOS] = i_an;\
    }\
    else\
    {\
        int i_ani = 255 - i_an;\
        dst[APLANE][APOS] = 255 - (255 - i_ao) * i_ani / 255;\
        if( dst[APLANE][APOS] != 0 )\
        {\
            int i_aoni = i_ao * i_ani / 255;\
            dst[XPLANE][XPOS] = ( dst[XPLANE][XPOS] * i_aoni + x * i_an ) / dst[APLANE][APOS];\
            dst[YPLANE][YPOS] = ( dst[YPLANE][YPOS] * i_aoni + y * i_an ) / dst[APLANE][APOS];\
            dst[ZPLANE][ZPOS] = ( dst[ZPLANE][ZPOS] * i_aoni + z * i_an ) / dst[APLANE][APOS];\
        }\
    }\
}

static void BlendAXYZLine( picture_t *p_picture,
                           int i_picture_x, int i_picture_y,
                           int i_a, int i_x, int i_y, int i_z,
                           const line_character_t *p_current,
                           const line_character_t *p_next,
                           ft_drawing_functions draw )
{
    int i_line_width = p_current->p_glyph->bitmap.width;
    if( p_next )
        i_line_width = p_next->p_glyph->left - p_current->p_glyph->left;

    draw.fill( p_picture,
               i_a, i_x, i_y, i_z,
               i_picture_x, i_picture_y + p_current->i_line_offset,
               i_line_width, p_current->i_line_thickness );
}

#endif // VLC_FREETYPE_BLEND_H
