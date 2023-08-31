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
 * related to the \link mrl MRL-specification\endlink.
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
 * \param[in] payload the data to escape.
 * \return he created string on success, and NULL on error.
 **/
VLC_MALLOC static inline char *
mrl_EscapeFragmentIdentifier( char const* payload )
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
        return NULL;

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
        return NULL;

    return mstream.ptr;
}

static inline char *
mrl_AppendAnchorFragment( const char *anchor, char const *payload )
{
    struct vlc_memstream mstream;
    if( vlc_memstream_open( &mstream ) )
        return NULL;

    if( anchor )
        vlc_memstream_puts( &mstream, anchor );
    else
        vlc_memstream_putc( &mstream, '#' );

    char *escaped = mrl_EscapeFragmentIdentifier( payload );
    if( escaped == NULL )
    {
        if( !vlc_memstream_close( &mstream ) )
            free( mstream.ptr );
        return NULL;
    }

    vlc_memstream_puts( &mstream, "!+" );
    vlc_memstream_puts( &mstream, escaped );
    free( escaped );

    if( vlc_memstream_close( &mstream ) )
        return NULL;

    return mstream.ptr;
}

struct mrl_info
{
    vlc_array_t identifiers;
    vlc_array_t volumes;
    char const *extra;
};

static inline void
mrl_info_Clean( struct mrl_info *mrli )
{
    for( size_t i = 0; i < vlc_array_count( &mrli->identifiers ); ++i )
        free( vlc_array_item_at_index( &mrli->identifiers, i ) );
    vlc_array_clear( &mrli->identifiers );
    for( size_t i = 0; i < vlc_array_count( &mrli->volumes ); ++i )
        free( vlc_array_item_at_index( &mrli->volumes, i ) );
    vlc_array_clear( &mrli->volumes );
}

static inline void
mrl_info_Init( struct mrl_info *mrli )
{
    vlc_array_init( &mrli->identifiers );
    vlc_array_init( &mrli->volumes );
    mrli->extra = NULL;
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
mrl_FragmentSplit( struct mrl_info *mrli,
                   char const* payload )
{
    while( strncmp( payload, "!/", 2 ) == 0 )
    {
        payload += 2;

        int len = strcspn( payload, "!?" );
        char* decoded = strndup( payload, len );

        if( unlikely( !decoded ) || !vlc_uri_decode( decoded ) )
            goto error;

        if( vlc_array_append( &mrli->identifiers, decoded ) )
        {
            free( decoded );
            goto error;
        }
        payload += len;
    }

    while( strncmp( payload, "!+", 2 ) == 0 )
    {
        payload += 2;

        int len = strcspn( payload, "!?" );
        char* decoded = strndup( payload, len );

        if( unlikely( !decoded ) || !vlc_uri_decode( decoded ) )
            goto error;

        if( vlc_array_append( &mrli->volumes, decoded ) )
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

        if( *payload == '?' && vlc_array_count( &mrli->identifiers ) )
            ++payload;

        mrli->extra = payload;
    }

    return VLC_SUCCESS;

error:
    return VLC_EGENERIC;
}

/** @} */

#endif /* include-guard */
