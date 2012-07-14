/*****************************************************************************
 * iso_lang.c: function to decode language code (in dvd or a52 for instance).
 *****************************************************************************
 * Copyright (C) 1998-2004 VLC authors and VideoLAN
 * $Id$
 *
 * Author: Stéphane Borel <stef@via.ecp.fr>
 *         Arnaud de Bossoreille de Ribou <bozo@via.ecp.fr>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>

#include <vlc_common.h>

#include "vlc_iso_lang.h"

/*****************************************************************************
 * Local tables
 *****************************************************************************/
#include "iso-639_def.h"

static const iso639_lang_t unknown_language =
    { "Unknown", "??", "???", "???" };

const iso639_lang_t * GetLang_1( const char * psz_code )
{
    const iso639_lang_t *p_lang;

    for( p_lang = p_languages; p_lang->psz_eng_name; p_lang++ )
        if( !strncasecmp( p_lang->psz_iso639_1, psz_code, 2 ) )
            return p_lang;

    return &unknown_language;
}

const iso639_lang_t * GetLang_2T( const char * psz_code )
{
    const iso639_lang_t *p_lang;

    for( p_lang = p_languages; p_lang->psz_eng_name; p_lang++ )
        if( !strncasecmp( p_lang->psz_iso639_2T, psz_code, 3 ) )
            return p_lang;

    return &unknown_language;
}

const iso639_lang_t * GetLang_2B( const char * psz_code )
{
    const iso639_lang_t *p_lang;

    for( p_lang = p_languages; p_lang->psz_eng_name; p_lang++ )
        if( !strncasecmp( p_lang->psz_iso639_2B, psz_code, 3 ) )
            return p_lang;

    return &unknown_language;
}

