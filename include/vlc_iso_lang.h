/*****************************************************************************
 * vlc_iso_lang.h: function to decode language code (in dvd or a52 for instance).
 *****************************************************************************
 * Copyright (C) 1998-2001 VLC authors and VideoLAN
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

/**
 * \file
 * This file defines functions and structures for iso639 language codes
 */

struct iso639_lang_t
{
    const char *psz_eng_name;    /* Description in English */
    const char psz_iso639_1[3];  /* ISO-639-1 (2 characters) code */
    const char psz_iso639_2T[4]; /* ISO-639-2/T (3 characters) English code */
    const char psz_iso639_2B[4]; /* ISO-639-2/B (3 characters) native code */
};

#if defined( __cplusplus )
extern "C" {
#endif
VLC_API const iso639_lang_t * GetLang_1( const char * );
VLC_API const iso639_lang_t * GetLang_2T( const char * );
VLC_API const iso639_lang_t * GetLang_2B( const char * );
#if defined( __cplusplus )
}
#endif

