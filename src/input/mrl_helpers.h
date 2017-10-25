/*****************************************************************************
 * mrl_helpers.h
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
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

#ifndef INPUT_MRL_HELPERS_H
#define INPUT_MRL_HELPERS_H

#include <string.h>
#include <stdlib.h>

#include <vlc_common.h>
#include <vlc_memstream.h>
#include <vlc_arrays.h>
#include <vlc_url.h>

/**
 * \defgroup mrl_helpers MRL helpers
 * \ingroup mrl
 *
 * Helper functions related to parsing, as well as generating, data
 * related to the \link MRL-specification\endlink.
 *
 * @{
 * \file
 **/

/**
 * Escape a fragment identifier for use within an MRL
 *
 * The function will generate a string that follows the \link mrl
 * MRL-specification\endlink regarding \em fragment-identifiers.
 *
 * See the \link mrl MRL-specification\endlink for a detailed
 * explanation of how `payload` will be escaped.
 *
 * \param[out] out `*out` will refer to the created string on success,
 *                  and an unspecified value on error.
 * \param[in] payload the data to escape.
 * \return VLC_SUCCESS on success, an error-code on failure.
 **/
static inline int
mrl_EscapeFragmentIdentifier( char** out, char const* payload )
{
    struct vlc_memstream mstream;

#define RFC3986_SUBDELIMS  "!" "$" "&" "'" "(" ")" \
                           "*" "+" "," ";" "="
#define RFC3986_ALPHA      "abcdefghijklmnopqrstuvwxyz" \
                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define RFC3986_DIGIT      "0123456789"
#define RFC3986_UNRESERVED RFC3986_ALPHA RFC3986_DIGIT "-" "." "_" "~"
#define RFC3986_PCHAR      RFC3986_UNRESERVED RFC3986_SUBDELIMS ":" "@"
#define RFC3986_FRAGMENT   RFC3986_PCHAR "/" "?"

    if( vlc_memstream_open( &mstream ) )
        return VLC_EGENERIC;

    for( char const* p = payload; *p; ++p )
    {
        vlc_memstream_printf( &mstream,
            ( strchr( "!?", *p ) == NULL &&
              strchr( RFC3986_FRAGMENT, *p ) ? "%c" : "%%%02hhx"), *p );
    }

#undef RFC3986_FRAGMENT
#undef RFC3986_PCHAR
#undef RFC3986_UNRESERVEd
#undef RFC3986_DIGIT
#undef RFC3986_ALPHA
#undef RFC3986_SUBDELIMS

    if( vlc_memstream_close( &mstream ) )
        return VLC_EGENERIC;

    *out = mstream.ptr;
    return VLC_SUCCESS;
}

/**
 * Split an \link mrl_technical_fragment MRL-fragment\endlink into identifiers
 *
 * Function used to split the fragment-data (also referred to as
 * anchor-data) into an array containing each of the files specified.
 *
 * See the \link mrl MRL-specification\endlink for detailed
 * information regarding how `payload` will be interpreted.
 *
 * \warning On success, the caller has ownership of the contents of *out_items
 *          which means that it is responsible for freeing the individual
 *          elements, as well as cleaning the array itself.
 *
 * \param[out] out_items storage for a vlc_array_t that will contain the
 *                       parsed identifiers on success.
 * \param[out] out_extra `*out_extra` will point to any remaining data (if any)
 * \param[in] payload the data to parse
 * \return VLC_SUCCESS on success, an error-code on failure
 **/
static inline int
mrl_FragmentSplit( vlc_array_t* out_items,
                   char const** out_extra,
                   char const* payload )
{
    char const* extra = NULL;

    vlc_array_init( out_items );

    while( strncmp( payload, "!/", 2 ) == 0 )
    {
        payload += 2;

        int len = strcspn( payload, "!?" );
        char* decoded = strndup( payload, len );

        if( unlikely( !decoded ) || !vlc_uri_decode( decoded ) )
            goto error;

        if( vlc_array_append( out_items, decoded ) )
        {
            free( decoded );
            goto error;
        }
        payload += len;
    }

    if( *payload )
    {
        if( *payload == '!' )
            goto error;

        if( *payload == '?' && vlc_array_count( out_items ) )
            ++payload;

        extra = payload;
    }

    *out_extra = extra;
    return VLC_SUCCESS;

error:
    for( size_t i = 0; i < vlc_array_count( out_items ); ++i )
        free( vlc_array_item_at_index( out_items, i ) );
    vlc_array_clear( out_items );
    return VLC_EGENERIC;;
}

/*
 * @}
 **/

#endif /* include-guard */
