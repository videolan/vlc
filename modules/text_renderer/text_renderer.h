/*****************************************************************************
 * text_renderer.h : fonts, text styles helpers
 *****************************************************************************
 * Copyright (C) 2002 - 2013 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Bernie Purcell <bitmap@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Felix Paul Kühne <fkuehne@videolan.org>
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

#include <vlc_text_style.h>                                   /* text_style_t*/

/* fonts and font_stack_t functions */
typedef struct font_stack_t font_stack_t;
struct font_stack_t
{
    char          *psz_name;
    int            i_size;
    uint32_t       i_color;            /* ARGB */
    uint32_t       i_karaoke_bg_color; /* ARGB */

    font_stack_t  *p_next;
};

int PushFont( font_stack_t **p_font, const char *psz_name, int i_size,
              uint32_t i_color, uint32_t i_karaoke_bg_color );

int PopFont( font_stack_t **p_font );

int PeekFont( font_stack_t **p_font, char **psz_name, int *i_size,
              uint32_t *i_color, uint32_t *i_karaoke_bg_color );

int HandleFontAttributes( xml_reader_t *p_xml_reader,
                          font_stack_t **p_fonts );

int HandleTT(font_stack_t **p_fonts, const char *p_fontfamily );

/* Turn any multiple-whitespaces into single spaces */
void HandleWhiteSpace( char *psz_node );

/* text_style_t functions */
text_style_t *CreateStyle( char *psz_fontname, int i_font_size,
                           uint32_t i_font_color, uint32_t i_karaoke_bg_color,
                           int i_style_flags );

text_style_t *GetStyleFromFontStack( filter_t *p_filter,
                                     font_stack_t **p_fonts,
                                     text_style_t *style,
                                     int i_style_flags );

#ifdef __OS2__
typedef uint16_t uni_char_t;
# define FREETYPE_TO_UCS    "UCS-2LE"
#else
typedef uint32_t uni_char_t;
# if defined(WORDS_BIGENDIAN)
#  define FREETYPE_TO_UCS   "UCS-4BE"
# else
#  define FREETYPE_TO_UCS   "UCS-4LE"
# endif
#endif

unsigned SetupText( filter_t *p_filter,
                    uni_char_t *psz_text_out,
                    text_style_t **pp_styles,
                    uint32_t *pi_k_dates,
                    const char *psz_text_in,
                    text_style_t *p_style,
                    uint32_t i_k_date );

bool FaceStyleEquals( const text_style_t *p_style1,
                      const text_style_t *p_style2 );

/* Parser */
int ProcessNodes( filter_t *p_filter,
                         uni_char_t *psz_text,
                         text_style_t **pp_styles,
                         uint32_t *pi_k_dates,
                         int *pi_len,
                         xml_reader_t *p_xml_reader,
                         text_style_t *p_font_style,
                         text_style_t *p_default_style );
