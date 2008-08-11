/*****************************************************************************
 * vlc_strings.h: String functions
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea at videolan dot org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_STRINGS_H
#define VLC_STRINGS_H 1

/**
 * \file
 * This file defines functions and structures handling misc strings
 */

/**
 * \defgroup strings Strings
 * @{
 */

VLC_EXPORT( void, resolve_xml_special_chars, ( char *psz_value ) );
VLC_EXPORT( char *, convert_xml_special_chars, ( const char *psz_content ) );

VLC_EXPORT( char *, vlc_b64_encode_binary, ( const uint8_t *, size_t ) );
VLC_EXPORT( char *, vlc_b64_encode, ( const char * ) );

VLC_EXPORT( size_t, vlc_b64_decode_binary_to_buffer, ( uint8_t *p_dst, size_t i_dst_max, const char *psz_src ) );
VLC_EXPORT( size_t, vlc_b64_decode_binary, ( uint8_t **pp_dst, const char *psz_src ) );
VLC_EXPORT( char *, vlc_b64_decode, ( const char *psz_src ) );

VLC_EXPORT( char *, str_format_time, ( const char * ) );
#define str_format_meta( a, b ) __str_format_meta( VLC_OBJECT( a ), b )
VLC_EXPORT( char *, __str_format_meta, ( vlc_object_t *, const char * ) );
#define str_format( a, b ) __str_format( VLC_OBJECT( a ), b )
VLC_EXPORT( char *, __str_format, ( vlc_object_t *, const char * ) );

VLC_EXPORT( void, filename_sanitize, ( char * ) );
VLC_EXPORT( void, path_sanitize, ( char * ) );

/**
 * @}
 */

#endif
