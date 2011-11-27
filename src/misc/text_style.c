/*****************************************************************************
 * text_style.c
 *****************************************************************************
 * Copyright (C) 1999-2010 VLC authors and VideoLAN
 * $Id$
 *
 * Author: basOS G <noxelia 4t gmail , com>
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_text_style.h>

/* */
text_style_t *text_style_New( void )
{
    text_style_t *p_style = calloc( 1, sizeof(*p_style) );
    if( !p_style )
        return NULL;

    /* initialize to default text style */
    p_style->psz_fontname = NULL;
    p_style->i_font_size = 22;
    p_style->i_font_color = 0xffffff;
    p_style->i_font_alpha = 0xff;
    p_style->i_style_flags = STYLE_OUTLINE;
    p_style->i_outline_color = 0x000000;
    p_style->i_outline_alpha = 0xff;
    p_style->i_shadow_color = 0x000000;
    p_style->i_shadow_alpha = 0xff;
    p_style->i_background_color = 0xffffff;
    p_style->i_background_alpha = 0x80;
    p_style->i_karaoke_background_color = 0xffffff;
    p_style->i_karaoke_background_alpha = 0xff;
    p_style->i_outline_width = 1;
    p_style->i_shadow_width = 0;
    p_style->i_spacing = -1;

    return p_style;
}

text_style_t *text_style_Copy( text_style_t *p_dst, const text_style_t *p_src )
{
    if( !p_src )
        return p_dst;

    /* */
    *p_dst = *p_src;

    if( p_src->psz_fontname )
        p_dst->psz_fontname = strdup( p_src->psz_fontname );

    return p_dst;
}

text_style_t *text_style_Duplicate( const text_style_t *p_src )
{
    if( !p_src )
        return NULL;

    text_style_t *p_dst = calloc( 1, sizeof(*p_dst) );
    if( p_dst )
        text_style_Copy( p_dst, p_src );
    return p_dst;
}

void text_style_Delete( text_style_t *p_style )
{
    if( p_style )
        free( p_style->psz_fontname );
    free( p_style );
}

