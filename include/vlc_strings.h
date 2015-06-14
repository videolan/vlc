/*****************************************************************************
 * vlc_strings.h: String functions
 *****************************************************************************
 * Copyright (C) 2006 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea at videolan dot org>
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

#ifndef VLC_STRINGS_H
#define VLC_STRINGS_H 1

/**
 * \defgroup strings String helpers
 * @{
 * \file
 * Helper functions for nul-terminated strings
 */

static inline int vlc_ascii_toupper( int c )
{
    if ( c >= 'a' && c <= 'z' )
        return c + ( 'A' - 'a' );
    else
        return c;
}

static inline int vlc_ascii_tolower( int c )
{
    if ( c >= 'A' && c <= 'Z' )
        return c + ( 'a' - 'A' );
    else
        return c;
}

/**
 * Compare two ASCII strings ignoring case.
 *
 * The result is independent of the locale. If there are non-ASCII
 * characters in the strings, their cases are NOT ignored in the
 * comparison.
 */
static inline int vlc_ascii_strcasecmp( const char *psz1, const char *psz2 )
{
    const char *s1 = psz1;
    const char *s2 = psz2;
    int d = vlc_ascii_tolower( *s1 ) - vlc_ascii_tolower( *s2 );
    while ( *s1 && d == 0)
    {
        s1++;
        s2++;
        d = vlc_ascii_tolower( *s1 ) - vlc_ascii_tolower( *s2 );
    }

    return d;
}

static inline int vlc_ascii_strncasecmp( const char *psz1, const char *psz2, size_t n )
{
    const char *s1 = psz1;
    const char *s2 = psz2;
    const char *s1end = psz1 + n;
    int d = vlc_ascii_tolower( *s1 ) - vlc_ascii_tolower( *s2 );
    while ( *s1 && s1 < s1end && d == 0)
    {
        s1++;
        s2++;
        d = vlc_ascii_tolower( *s1 ) - vlc_ascii_tolower( *s2 );
    }

    if (s1 == s1end)
        return 0;
    else
        return d;
}

VLC_API void resolve_xml_special_chars( char *psz_value );
VLC_API char * convert_xml_special_chars( const char *psz_content );

VLC_API char * vlc_b64_encode_binary( const uint8_t *, size_t );
VLC_API char * vlc_b64_encode( const char * );

VLC_API size_t vlc_b64_decode_binary_to_buffer( uint8_t *p_dst, size_t i_dst_max, const char *psz_src );
VLC_API size_t vlc_b64_decode_binary( uint8_t **pp_dst, const char *psz_src );
VLC_API char * vlc_b64_decode( const char *psz_src );

VLC_API char * str_format_time( const char * );
VLC_API char * str_format_meta( input_thread_t *, const char * );

static inline char *str_format( input_thread_t *input, const char *fmt )
{
    char *s1 = str_format_time( fmt );
    char *s2 = str_format_meta( input, s1 );
    free( s1 );
    return s2;
}

VLC_API void filename_sanitize( char * );
VLC_API void path_sanitize( char * );

VLC_API time_t str_duration( const char * );

/**
 * @}
 */

#endif
