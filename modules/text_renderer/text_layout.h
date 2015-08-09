/*****************************************************************************
 * text_layout.h : Text shaping and layout
 *****************************************************************************
 * Copyright (C) 2015 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@videolan.org>
 *          Sigmund Augdal Helberg <dnumgis@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Devin Heitmueller <dheitmueller@kernellabs.com>
 *          Salah-Eddin Shaban <salshaaban@gmail.com>
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

#include "freetype.h"

typedef struct
{
    FT_BitmapGlyph p_glyph;
    FT_BitmapGlyph p_outline;
    FT_BitmapGlyph p_shadow;
    uint32_t       i_color;             /* ARGB color */
    int            i_line_offset;       /* underline/strikethrough offset */
    int            i_line_thickness;    /* underline/strikethrough thickness */
} line_character_t;

typedef struct line_desc_t line_desc_t;
struct line_desc_t
{
    line_desc_t      *p_next;

    int              i_width;
    int              i_height;
    int              i_base_line;
    int              i_character_count;
    line_character_t *p_character;
    FT_BBox          bbox;
};

void FreeLines( line_desc_t *p_lines );
line_desc_t *NewLine( int i_count );

int LayoutText(filter_t *p_filter, line_desc_t **pp_lines,
                FT_BBox *p_bbox, int *pi_max_face_height,
                const uni_char_t *psz_text, const text_style_t **pp_styles,
                uint32_t *pi_k_dates, int i_len );
