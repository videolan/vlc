/*****************************************************************************
 * iso_lang.c: function to decode language code (in dvd or a52 for instance).
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: iso_lang.c,v 1.6 2002/06/01 12:32:01 sam Exp $
 *
 * Author: Stéphane Borel <stef@via.ecp.fr>
 *         Arnaud de Bossoreille de Ribou <bozo@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdio.h>

#include <vlc/vlc.h>

#include "iso_lang.h"

/*****************************************************************************
 * Local tables
 *****************************************************************************/

#define DEFINE_LANGUAGE_CODE(engName, nativeName, iso1, iso2T, iso2B) \
          { engName, nativeName, #iso1, #iso2T, #iso2B },

static const iso639_lang_t p_languages[] =
{
#include "iso-639.def"
    { NULL, NULL, NULL, NULL, NULL }
};

static const iso639_lang_t unknown_language =
    { "Unknown", "Unknown", "??", "???", "???" };

/*****************************************************************************
 * DecodeLanguage: gives the long language name from the two-letter
 *                 ISO-639 code
 *****************************************************************************/
const char * DecodeLanguage( u16 i_code )
{
    const iso639_lang_t * p_lang;
    u8 psz_code[3];

    psz_code[0] = i_code >> 8;
    psz_code[1] = i_code;
    psz_code[2] = '\0';

    for( p_lang = p_languages; p_lang->psz_eng_name; p_lang++ )
    {
        if( !strncmp( p_lang->psz_iso639_1, psz_code, 2 ) )
        {
            if( *p_lang->psz_native_name )
            {
                return p_lang->psz_native_name;
            }

            return p_lang->psz_eng_name;
        }
    }

    return "Unknown";
}

const iso639_lang_t * GetLang_1( const char * psz_code )
{
    const iso639_lang_t *p_lang;

    for( p_lang = p_languages; p_lang->psz_eng_name; p_lang++ )
        if( !strncmp( p_lang->psz_iso639_1, psz_code, 2 ) )
            return p_lang;

    return &unknown_language;
}

const iso639_lang_t * GetLang_2T( const char * psz_code )
{
    const iso639_lang_t *p_lang;
    
    for( p_lang = p_languages; p_lang->psz_eng_name; p_lang++ )
        if( !strncmp( p_lang->psz_iso639_2T, psz_code, 3 ) )
            return p_lang;

    return &unknown_language;
}

const iso639_lang_t * GetLang_2B( const char * psz_code )
{
    const iso639_lang_t *p_lang;

    for( p_lang = p_languages; p_lang->psz_eng_name; p_lang++ )
        if( !strncmp( p_lang->psz_iso639_2B, psz_code, 3 ) )
            return p_lang;

    return &unknown_language;
}

