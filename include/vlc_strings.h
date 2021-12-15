/*****************************************************************************
 * vlc_strings.h: String functions
 *****************************************************************************
 * Copyright (C) 2006 VLC authors and VideoLAN
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
 * \ingroup cext
 * @{
 * \file
 * Helper functions for nul-terminated strings
 */

typedef struct vlc_player_t vlc_player_t;

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

/**
 * Decodes XML entities.
 *
 * Decodes a null-terminated UTF-8 string of XML character data into a regular
 * nul-terminated UTF-8 string. In other words, replaces XML entities and
 * numerical character references with the corresponding characters.
 *
 * This function operates in place (the output is always of smaller or equal
 * length than the input) and always succeeds.
 *
 * \param str null-terminated string [IN/OUT]
 */
VLC_API void vlc_xml_decode(char *st);

/**
 * Encodes XML entities.
 *
 * Substitutes unsafe characters in a null-terminated UTF-8 strings with an
 * XML entity or numerical character reference.
 *
 * \param str null terminated UTF-8 string
 * \return On success, a heap-allocated null-terminated string is returned.
 * If the input string was not a valid UTF-8 sequence, NULL is returned and
 * errno is set to EILSEQ.
 * If there was not enough memory, NULL is returned and errno is to ENOMEM.
 */
VLC_API char *vlc_xml_encode(const char *str) VLC_MALLOC;

/**
 * Encode binary data as hex string
 *
 * Writes a given data buffer to the output buffer as a null terminated
 * string in hexadecimal representation.
 *
 * \param      input    Input buffer
 * \param      size     Input buffer size
 * \param[out] output   Output buffer to write the string to
 */
VLC_API void vlc_hex_encode_binary(const void *input, size_t size, char *output);

/**
 * Base64 encoding.
 *
 * Encodes a buffer into base64 as a (nul-terminated) string.
 *
 * \param base start address of buffer to encode
 * \param length length in bytes of buffer to encode
 * \return a heap-allocated nul-terminated string
 * (or NULL on allocation error).
 */
VLC_API char *vlc_b64_encode_binary(const void *base, size_t length)
VLC_USED VLC_MALLOC;

/**
 * Base64 encoding (string).
 *
 * Encodes a nul-terminated string into Base64.
 *
 * \param str nul-terminated string to encode
 * \return a heap-allocated nul-terminated string
 * (or NULL on allocation error).
 */
VLC_API char *vlc_b64_encode(const char *str) VLC_USED VLC_MALLOC;

VLC_API size_t vlc_b64_decode_binary_to_buffer(void *p_dst, size_t i_dst_max, const char *psz_src );
VLC_API size_t vlc_b64_decode_binary( uint8_t **pp_dst, const char *psz_src );
VLC_API char * vlc_b64_decode( const char *psz_src );

/**
 * Convenience wrapper for strftime().
 *
 * Formats the current time into a heap-allocated string.
 *
 * \param tformat time format (as with C strftime())
 * \return an allocated string (must be free()'d), or NULL on memory error.
 */
VLC_API char *vlc_strftime( const char * );

/**
 * Formats input meta-data.
 *
 * Formats input and input item meta-informations into a heap-allocated string
 * according to the given player format string.
 *
 * The player format string contains of replacement specifiers, each specifier begins
 * with the dollar character (`$`) followed by one of the following letters:
 *
 * Char  | Replacement
 * ----- | -------------------------------
 *  `a`  | Artist metadata
 *  `b`  | Album title metadata
 *  `c`  | Copyright information metadata
 *  `d`  | Description metadata
 *  `e`  | 'Encoded by' metadata
 *  `f`  | Displayed output frame (`-` if not available)
 *  `g`  | Genre metadata
 *  `l`  | Language metadata
 *  `n`  | Current Track number metadata
 *  `o`  | Total Track number metadata
 *  `p`  | Now playing metadata (i.e. currently playing title for livestreams)
 *  `r`  | Rating metadata
 *  `s`  | Selected subtitle language (`-` if not available)
 *  `t`  | Title metadata
 *  `u`  | URL metadata
 *  `A`  | Date metadata
 *  `B`  | Selected audio track bitrate (`-` if not available)
 *  `C`  | Current chapter index (`-` if not available)
 *  `D`  | Item duration (`--:--:--` if not available)
 *  `F`  | Item URI
 *  `I`  | Current title index (`-` if not available)
 *  `L`  | Item remaining time (`--:--:--` if not available)
 *  `N`  | Item name
 *  `O`  | Current audio track language (`-` if not available)
 *  `P`  | Current playback position (0.0 to 1.0, `--.-%` if not available)
 *  `R`  | Current playback speed (1.0 is normal speed, `-` if not available)
 *  `S`  | Current audio track samplerate (`-` if not available)
 *  `T`  | Current playback time (`--:--:--` if not available)
 *  `U`  | Publisher metadata
 *  `V`  | Volume (0 to 256, `---` if not available)
 *  `Z`  | Now playing or Artist/Title metadata depending what is available
 *  `_`  | Newline (`\n`)
 *
 * Additionally characters can be prepended with a whitespace (e.g. `$ T`), which will
 * cause a replacement with nothing, when not available, instead of the placeholders
 * documented above.
 *
 * \param player a locked player instance or NULL (player and item can't be
 * both NULL)
 * \param item a valid item or NULL (player and item can't be both NULL)
 * \param fmt format string
 * \return an allocated formatted string (must be free()'d), or NULL in case of error
 */
VLC_API char *vlc_strfplayer( vlc_player_t *player, input_item_t *item,
                              const char *fmt );

static inline char *str_format( vlc_player_t *player, input_item_t *item,
                                const char *fmt )
{
    char *s1 = vlc_strftime( fmt );
    char *s2 = vlc_strfplayer( player, item, s1 );
    free( s1 );
    return s2;
}

VLC_API int vlc_filenamecmp(const char *, const char *);

void filename_sanitize(char *);

/**
 * @}
 */

#endif
