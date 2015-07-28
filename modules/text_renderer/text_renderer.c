/*****************************************************************************
 * freetype.c : Put text on the video, using freetype2
 *****************************************************************************
 * Copyright (C) 2002 - 2012 VLC authors and VideoLAN
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <vlc_common.h>
#include <vlc_variables.h>
#include <vlc_filter.h>                                      /* filter_sys_t */
#include <vlc_xml.h>                                         /* xml_reader */
#include <vlc_charset.h>                                     /* ToCharset */
#include <vlc_strings.h>                         /* resolve_xml_special_chars */

#include "text_renderer.h"

text_style_t *CreateStyle( char *psz_fontname, int i_font_size,
                                  uint32_t i_font_color, uint32_t i_karaoke_bg_color,
                                  int i_style_flags )
{
    text_style_t *p_style = text_style_New();
    if( !p_style )
        return NULL;

    p_style->psz_fontname = psz_fontname ? strdup( psz_fontname ) : NULL;
    p_style->i_font_size  = i_font_size;
    p_style->i_font_color = (i_font_color & 0x00ffffff) >>  0;
    p_style->i_font_alpha = (i_font_color & 0xff000000) >> 24;
    p_style->i_karaoke_background_color = (i_karaoke_bg_color & 0x00ffffff) >>  0;
    p_style->i_karaoke_background_alpha = (i_karaoke_bg_color & 0xff000000) >> 24;
    p_style->i_style_flags |= i_style_flags;
    return p_style;
}

bool FaceStyleEquals( const text_style_t *p_style1,
                             const text_style_t *p_style2 )
{
    if( !p_style1 || !p_style2 )
        return false;
    if( p_style1 == p_style2 )
        return true;

    const int i_style_mask = STYLE_BOLD | STYLE_ITALIC;
    return (p_style1->i_style_flags & i_style_mask) == (p_style2->i_style_flags & i_style_mask) &&
           !strcmp( p_style1->psz_fontname, p_style2->psz_fontname );
}

