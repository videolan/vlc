/*****************************************************************************
 * iso_lang.c: function to decode language code (in dvd or a52 for instance).
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: iso_lang.c,v 1.4 2002/05/14 19:33:54 bozo Exp $
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

#include <videolan/vlc.h>

#include "iso_lang.h"

/*****************************************************************************
 * Local tables
 *****************************************************************************/

static iso639_lang_t p_iso_languages[] =
{
#define DEFINE_LANGUAGE_CODE(engName, nativeName, iso1, iso2T, iso2B) \
          { engName, nativeName, #iso1, #iso2T, #iso2B },
    { "", "", "", "", "" },
#include "iso-639.def"
};


/*****************************************************************************
 * DecodeLanguage: gives the long language name from the two-letters
 *                 ISO-639 code
 *****************************************************************************/
char * DecodeLanguage( u16 i_code )
{
    u8 code[2];
    iso639_lang_t * p_iso;
    code[0] = i_code >> 8;
    code[1] = i_code;
    p_iso = GetLang_1( code );
    if( p_iso )
    {
        if( p_iso->psz_native_name[0] )
            return p_iso->psz_native_name;
        else
            return p_iso->psz_eng_name;
    }
    return p_iso_languages[sizeof( p_iso_languages ) /
                           sizeof( iso639_lang_t ) - 1].psz_native_name;
}


iso639_lang_t * GetLang_1( const char * psz_iso639_1 )
{
    unsigned int i;
    for( i = 0; i < sizeof( p_iso_languages ) / sizeof( iso639_lang_t ); i++ )
    {
        if( !strncmp( p_iso_languages[i].psz_iso639_1, psz_iso639_1, 2 ) )
            break;
    }
    if( i < sizeof( p_iso_languages ) / sizeof( iso639_lang_t ) )
        return &p_iso_languages[i];
    else
        return NULL;
}

iso639_lang_t * GetLang_2T( const char * psz_iso639_2T )
{
    unsigned int i;
    for( i = 0; i < sizeof( p_iso_languages ) / sizeof( iso639_lang_t ); i++ )
    {
        if( !strncmp( p_iso_languages[i].psz_iso639_2T, psz_iso639_2T, 2 ) )
            break;
    }
    if( i < sizeof( p_iso_languages ) / sizeof( iso639_lang_t ) )
        return &p_iso_languages[i];
    else
        return NULL;
}

iso639_lang_t * GetLang_2B( const char * psz_iso639_2B )
{
    unsigned int i;
    for( i = 0; i < sizeof( p_iso_languages ) / sizeof( iso639_lang_t ); i++ )
    {
        if( !strncmp( p_iso_languages[i].psz_iso639_2B, psz_iso639_2B, 2 ) )
            break;
    }
    if( i < sizeof( p_iso_languages ) / sizeof( iso639_lang_t ) )
        return &p_iso_languages[i];
    else
        return NULL;
}

