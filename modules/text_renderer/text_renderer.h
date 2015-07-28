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

/* text_style_t functions */
text_style_t *CreateStyle( char *psz_fontname, int i_font_size,
                           uint32_t i_font_color, uint32_t i_karaoke_bg_color,
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

bool FaceStyleEquals( const text_style_t *p_style1,
                      const text_style_t *p_style2 );
