/*****************************************************************************
 * iso_lang.c: function to decode language code (in dvd or a52 for instance).
 *****************************************************************************
 * Copyright (C) 1998-2004 VLC authors and VideoLAN
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

static const iso639_lang_t * GetLang_1( const char * psz_code )
{
    const iso639_lang_t *p_lang;

    for( p_lang = p_languages; p_lang->psz_eng_name; p_lang++ )
        if( !strcasecmp( p_lang->psz_iso639_1, psz_code ) )
            return p_lang;

    return NULL;
}

static const iso639_lang_t * GetLang_2T( const char * psz_code )
{
    const iso639_lang_t *p_lang;

    for( p_lang = p_languages; p_lang->psz_eng_name; p_lang++ )
        if( !strcasecmp( p_lang->psz_iso639_2T, psz_code ) )
            return p_lang;

    return NULL;
}

static const iso639_lang_t * GetLang_2B( const char * psz_code )
{
    const iso639_lang_t *p_lang;

    for( p_lang = p_languages; p_lang->psz_eng_name; p_lang++ )
        if( !strcasecmp( p_lang->psz_iso639_2B, psz_code ) )
            return p_lang;

    return NULL;
}

const iso639_lang_t * vlc_find_iso639( const char *code )
{
    const iso639_lang_t *result = NULL;
    size_t len = strlen(code);

    if (len == 2)
        result = GetLang_1(code);
    else if (len == 3)
    {
        result = GetLang_2B(code);
        if (result == NULL)
            result = GetLang_2T(code);
    }
    return result;
}
