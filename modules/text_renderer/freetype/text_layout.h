/*****************************************************************************
 * text_layout.h : Text shaping and layout
 *****************************************************************************
 * Copyright (C) 2015 VLC authors and VideoLAN
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

/** \ingroup freetype
 * @{
 * \file
 * Text shaping and layout
 */

#include "freetype.h"

typedef struct ruby_block_t ruby_block_t;
typedef struct line_desc_t line_desc_t;

typedef struct
{
    FT_BitmapGlyph p_glyph;
    FT_BitmapGlyph p_outline;
    FT_BitmapGlyph p_shadow;
    FT_BBox        bbox;
    const text_style_t *p_style;
    const ruby_block_t *p_ruby;
    int            i_line_offset;       /* underline/strikethrough offset */
    int            i_line_thickness;    /* underline/strikethrough thickness */
} line_character_t;

struct line_desc_t
{
    line_desc_t      *p_next;
    int              i_width;
    int              i_height;
    int              i_base_line;
    int              i_character_count;
    int              i_first_visible_char_index;
    int              i_last_visible_char_index;
    line_character_t *p_character;
    FT_BBox          bbox;
};

void FreeLines( line_desc_t *p_lines );
line_desc_t *NewLine( int i_count );

/**
 * \struct layout_ruby_t
 * \brief LayoutText parameters
 */
struct ruby_block_t
{
    uni_char_t *p_uchars;       /*!< array of size \p i_count character codepoints */
    size_t i_count;             /*!< length of the array */
    text_style_t *p_style;      /*!< own style */
    line_desc_t *p_laid;
};

/**
 * \struct layout_text_block_t
 * \brief LayoutText parameters
 */
typedef struct
{
    uni_char_t *p_uchars;       /*!< array of size \p i_count character codepoints */
    text_style_t **pp_styles;   /*!< array of size \p i_count character styles */
    ruby_block_t **pp_ruby;     /*!< array of size \p  */
    size_t i_count;             /*!< length of the arrays */

    bool b_balanced;            /*!< true for grid-mode text */
    bool b_grid;                /*!< true for balanced wrapped lines */
    unsigned i_max_width;       /*!< maximum available width to layout text */
    unsigned i_max_height;      /*!< maximum available height to layout text */

    line_desc_t *p_laid;
} layout_text_block_t;

/**
 * Layout the text with shaping, bidirectional support, and font fallback if available.
 *
 * \param p_filter the FreeType module object [IN]
 * \param p_textblock layout_text_block_t describing text to layout [IN]
 * \param pp_lines the list of line_desc_t's with rendered glyphs [OUT]
 * \param p_bbox the bounding box of all the lines [OUT]
 * \param pi_max_face_height maximum line height [OUT]
 */
int LayoutTextBlock( filter_t *p_filter, const layout_text_block_t *p_textblock,
                     line_desc_t **pp_lines, FT_BBox *p_bbox, int *pi_max_face_height );
