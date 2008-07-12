/*****************************************************************************
 * strings.c: String related functions
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea at videolan dot org>
 *          Daniel Stranger <vlc at schmaller dot de>
 *          Rémi Denis-Courmont <rem # videolan org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <assert.h>

/* Needed by str_format_time */
#include <time.h>

/* Needed by str_format_meta */
#include <vlc_input.h>
#include <vlc_meta.h>
#include <vlc_playlist.h>
#include <vlc_aout.h>

#include <vlc_strings.h>
#include <vlc_url.h>
#include <vlc_charset.h>

/**
 * Unescape URI encoded string
 * \return decoded duplicated string
 */
char *unescape_URI_duplicate( const char *psz )
{
    char *psz_dup = strdup( psz );
    unescape_URI( psz_dup );
    return psz_dup;
}

/**
 * Unescape URI encoded string in place
 * \return nothing
 */
void unescape_URI( char *psz )
{
    unsigned char *in = (unsigned char *)psz, *out = in, c;
    if( psz == NULL )
        return;

    while( ( c = *in++ ) != '\0' )
    {
        switch( c )
        {
            case '%':
            {
                char val[5], *pval = val;
                unsigned long cp;

                switch( c = *in++ )
                {
                    case '\0':
                        return;

                    case 'u':
                    case 'U':
                        if( ( *pval++ = *in++ ) == '\0' )
                            return;
                        if( ( *pval++ = *in++ ) == '\0' )
                            return;
                        c = *in++;

                    default:
                        *pval++ = c;
                        if( ( *pval++ = *in++ ) == '\0' )
                            return;
                        *pval = '\0';
                }

                cp = strtoul( val, NULL, 0x10 );
                if( cp < 0x80 )
                    *out++ = cp;
                else
                if( cp < 0x800 )
                {
                    *out++ = (( cp >>  6)         | 0xc0);
                    *out++ = (( cp        & 0x3f) | 0x80);
                }
                else
                {
                    assert( cp < 0x10000 );
                    *out++ = (( cp >> 12)         | 0xe0);
                    *out++ = (((cp >>  6) & 0x3f) | 0x80);
                    *out++ = (( cp        & 0x3f) | 0x80);
                }
                break;
            }

            /* + is not a special case - it means plus, not space. */

            default:
                /* Inserting non-ASCII or non-printable characters is unsafe,
                 * and no sane browser will send these unencoded */
                if( ( c < 32 ) || ( c > 127 ) )
                    *out++ = '?';
                else
                    *out++ = c;
        }
    }
    *out = '\0';
}

/**
 * Decode encoded URI string
 * \return decoded duplicated string
 */
char *decode_URI_duplicate( const char *psz )
{
    char *psz_dup = strdup( psz );
    decode_URI( psz_dup );
    return psz_dup;
}

/**
 * Decode encoded URI string in place
 * \return nothing
 */
void decode_URI( char *psz )
{
    unsigned char *in = (unsigned char *)psz, *out = in, c;
    if( psz == NULL )
        return;

    while( ( c = *in++ ) != '\0' )
    {
        switch( c )
        {
            case '%':
            {
                char hex[3];

                if( ( ( hex[0] = *in++ ) == 0 )
                 || ( ( hex[1] = *in++ ) == 0 ) )
                    return;

                hex[2] = '\0';
                *out++ = (unsigned char)strtoul( hex, NULL, 0x10 );
                break;
            }

            case '+':
                *out++ = ' ';
                break;

            default:
                /* Inserting non-ASCII or non-printable characters is unsafe,
                 * and no sane browser will send these unencoded */
                if( ( c < 32 ) || ( c > 127 ) )
                    *out++ = '?';
                else
                    *out++ = c;
        }
    }
    *out = '\0';
    EnsureUTF8( psz );
}

static inline int isurlsafe( int c )
{
    return ( (unsigned char)( c - 'a' ) < 26 )
            || ( (unsigned char)( c - 'A' ) < 26 )
            || ( (unsigned char)( c - '0' ) < 10 )
        /* Hmm, we should not encode character that are allowed in URLs
         * (even if they are not URL-safe), nor URL-safe characters.
         * We still encode some of them because of Microsoft's crap browser.
         */
            || ( strchr( "-_.", c ) != NULL );
}

static inline char url_hexchar( int c )
{
    return ( c < 10 ) ? c + '0' : c + 'A' - 10;
}

/**
 * encode_URI_component
 * Encodes an URI component.
 *
 * @param psz_url nul-terminated UTF-8 representation of the component.
 * Obviously, you can't pass an URI containing a nul character, but you don't
 * want to do that, do you?
 *
 * @return encoded string (must be free()'d)
 */
char *encode_URI_component( const char *psz_url )
{
    char psz_enc[3 * strlen( psz_url ) + 1], *out = psz_enc;
    const uint8_t *in;

    for( in = (const uint8_t *)psz_url; *in; in++ )
    {
        uint8_t c = *in;

        if( isurlsafe( c ) )
            *out++ = (char)c;
        else
        if ( c == ' ')
            *out++ = '+';
        else
        {
            *out++ = '%';
            *out++ = url_hexchar( c >> 4 );
            *out++ = url_hexchar( c & 0xf );
        }
    }
    *out++ = '\0';

    return strdup( psz_enc );
}

/**
 * Converts "&lt;", "&gt;" and "&amp;" to "<", ">" and "&"
 * \param string to convert
 */
void resolve_xml_special_chars( char *psz_value )
{
    char *p_pos = psz_value;

    while ( *psz_value )
    {
        if( *psz_value == '&' )
        {
#define TRY_CHAR( src, len, dst )                   \
            if( !strncmp( psz_value, src, len ) )   \
            {                                       \
                *p_pos = dst;                       \
                psz_value += len;                   \
            }
#define TRY_LONGCHAR( src, len, dst )                   \
            if( !strncmp( psz_value, src, len ) )       \
            {                                           \
                strncpy( p_pos, dst, strlen( dst ) );   \
                p_pos += strlen( dst ) - 1;             \
                psz_value += len;                       \
            }
            TRY_CHAR( "&lt;", 4, '<' )
            else TRY_CHAR( "&gt;", 4, '>' )
            else TRY_CHAR( "&amp;", 5, '&' )
            else TRY_CHAR( "&quot;", 6, '"' )
            else TRY_CHAR( "&apos;", 6, '\'' )
            else if( psz_value[1] == '#' )
            {
                char *psz_end;
                int i = strtol( psz_value+2, &psz_end, 10 );
                if( *psz_end == ';' )
                {
                    if( i >= 32 && i <= 126 )
                    {
                        *p_pos = (char)i;
                        psz_value = psz_end+1;
                    }
                    else
                    {
                        /* Unhandled code, FIXME */
                        *p_pos = *psz_value;
                        psz_value++;
                    }
                }
                else
                {
                    /* Invalid entity number */
                    *p_pos = *psz_value;
                    psz_value++;
                }
            }
            else TRY_LONGCHAR( "&Agrave;", 8, "À" )
            else TRY_LONGCHAR( "&Aacute;", 8, "Á" )
            else TRY_LONGCHAR( "&Acirc;", 7, "Â" )
            else TRY_LONGCHAR( "&Atilde;", 8, "Ã" )
            else TRY_LONGCHAR( "&Auml;", 6, "Ä" )
            else TRY_LONGCHAR( "&Aring;", 7, "Å" )
            else TRY_LONGCHAR( "&AElig;", 7, "Æ" )
            else TRY_LONGCHAR( "&Ccedil;", 8, "Ç" )
            else TRY_LONGCHAR( "&Egrave;", 8, "È" )
            else TRY_LONGCHAR( "&Eacute;", 8, "É" )
            else TRY_LONGCHAR( "&Ecirc;", 7, "Ê" )
            else TRY_LONGCHAR( "&Euml;", 6, "Ë" )
            else TRY_LONGCHAR( "&Igrave;", 8, "Ì" )
            else TRY_LONGCHAR( "&Iacute;", 8, "Í" )
            else TRY_LONGCHAR( "&Icirc;", 7, "Î" )
            else TRY_LONGCHAR( "&Iuml;", 6, "Ï" )
            else TRY_LONGCHAR( "&ETH;", 5, "Ð" )
            else TRY_LONGCHAR( "&Ntilde;", 8, "Ñ" )
            else TRY_LONGCHAR( "&Ograve;", 8, "Ò" )
            else TRY_LONGCHAR( "&Oacute;", 8, "Ó" )
            else TRY_LONGCHAR( "&Ocirc;", 7, "Ô" )
            else TRY_LONGCHAR( "&Otilde;", 8, "Õ" )
            else TRY_LONGCHAR( "&Ouml;", 6, "Ö" )
            else TRY_LONGCHAR( "&Oslash;", 8, "Ø" )
            else TRY_LONGCHAR( "&Ugrave;", 8, "Ù" )
            else TRY_LONGCHAR( "&Uacute;", 8, "Ú" )
            else TRY_LONGCHAR( "&Ucirc;", 7, "Û" )
            else TRY_LONGCHAR( "&Uuml;", 6, "Ü" )
            else TRY_LONGCHAR( "&Yacute;", 8, "Ý" )
            else TRY_LONGCHAR( "&THORN;", 7, "Þ" )
            else TRY_LONGCHAR( "&szlig;", 7, "ß" )
            else TRY_LONGCHAR( "&agrave;", 8, "à" )
            else TRY_LONGCHAR( "&aacute;", 8, "á" )
            else TRY_LONGCHAR( "&acirc;", 7, "â" )
            else TRY_LONGCHAR( "&atilde;", 8, "ã" )
            else TRY_LONGCHAR( "&auml;", 6, "ä" )
            else TRY_LONGCHAR( "&aring;", 7, "å" )
            else TRY_LONGCHAR( "&aelig;", 7, "æ" )
            else TRY_LONGCHAR( "&ccedil;", 8, "ç" )
            else TRY_LONGCHAR( "&egrave;", 8, "è" )
            else TRY_LONGCHAR( "&eacute;", 8, "é" )
            else TRY_LONGCHAR( "&ecirc;", 7, "ê" )
            else TRY_LONGCHAR( "&euml;", 6, "ë" )
            else TRY_LONGCHAR( "&igrave;", 8, "ì" )
            else TRY_LONGCHAR( "&iacute;", 8, "í" )
            else TRY_LONGCHAR( "&icirc;", 7, "î" )
            else TRY_LONGCHAR( "&iuml;", 6, "ï" )
            else TRY_LONGCHAR( "&eth;", 5, "ð" )
            else TRY_LONGCHAR( "&ntilde;", 8, "ñ" )
            else TRY_LONGCHAR( "&ograve;", 8, "ò" )
            else TRY_LONGCHAR( "&oacute;", 8, "ó" )
            else TRY_LONGCHAR( "&ocirc;", 7, "ô" )
            else TRY_LONGCHAR( "&otilde;", 8, "õ" )
            else TRY_LONGCHAR( "&ouml;", 6, "ö" )
            else TRY_LONGCHAR( "&oslash;", 8, "ø" )
            else TRY_LONGCHAR( "&ugrave;", 8, "ù" )
            else TRY_LONGCHAR( "&uacute;", 8, "ú" )
            else TRY_LONGCHAR( "&ucirc;", 7, "û" )
            else TRY_LONGCHAR( "&uuml;", 6, "ü" )
            else TRY_LONGCHAR( "&yacute;", 8, "ý" )
            else TRY_LONGCHAR( "&thorn;", 7, "þ" )
            else TRY_LONGCHAR( "&yuml;", 6, "ÿ" )
            else TRY_LONGCHAR( "&iexcl;", 7, "¡" )
            else TRY_LONGCHAR( "&curren;", 8, "¤" )
            else TRY_LONGCHAR( "&cent;", 6, "¢" )
            else TRY_LONGCHAR( "&pound;", 7, "£" )
            else TRY_LONGCHAR( "&yen;", 5, "¥" )
            else TRY_LONGCHAR( "&brvbar;", 8, "¦" )
            else TRY_LONGCHAR( "&sect;", 6, "§" )
            else TRY_LONGCHAR( "&uml;", 5, "¨" )
            else TRY_LONGCHAR( "&copy;", 6, "©" )
            else TRY_LONGCHAR( "&ordf;", 6, "ª" )
            else TRY_LONGCHAR( "&laquo;", 7, "«" )
            else TRY_LONGCHAR( "&not;", 5, "¬" )
            else TRY_LONGCHAR( "&shy;", 5, "­" )
            else TRY_LONGCHAR( "&reg;", 5, "®" )
            else TRY_LONGCHAR( "&trade;", 7, "™" )
            else TRY_LONGCHAR( "&macr;", 6, "¯" )
            else TRY_LONGCHAR( "&deg;", 5, "°" )
            else TRY_LONGCHAR( "&plusmn;", 8, "±" )
            else TRY_LONGCHAR( "&sup2;", 6, "²" )
            else TRY_LONGCHAR( "&sup3;", 6, "³" )
            else TRY_LONGCHAR( "&acute;", 7, "´" )
            else TRY_LONGCHAR( "&micro;", 7, "µ" )
            else TRY_LONGCHAR( "&para;", 6, "¶" )
            else TRY_LONGCHAR( "&middot;", 8, "·" )
            else TRY_LONGCHAR( "&cedil;", 7, "¸" )
            else TRY_LONGCHAR( "&sup1;", 6, "¹" )
            else TRY_LONGCHAR( "&ordm;", 6, "º" )
            else TRY_LONGCHAR( "&raquo;", 7, "»" )
            else TRY_LONGCHAR( "&frac14;", 8, "¼" )
            else TRY_LONGCHAR( "&frac12;", 8, "½" )
            else TRY_LONGCHAR( "&frac34;", 8, "¾" )
            else TRY_LONGCHAR( "&iquest;", 8, "¿" )
            else TRY_LONGCHAR( "&times;", 7, "×" )
            else TRY_LONGCHAR( "&divide;", 8, "÷" )
            else TRY_LONGCHAR( "&OElig;", 7, "Œ" )
            else TRY_LONGCHAR( "&oelig;", 7, "œ" )
            else TRY_LONGCHAR( "&Scaron;", 8, "Š" )
            else TRY_LONGCHAR( "&scaron;", 8, "š" )
            else TRY_LONGCHAR( "&Yuml;", 6, "Ÿ" )
            else TRY_LONGCHAR( "&circ;", 6, "ˆ" )
            else TRY_LONGCHAR( "&tilde;", 7, "˜" )
            else TRY_LONGCHAR( "&ndash;", 7, "–" )
            else TRY_LONGCHAR( "&mdash;", 7, "—" )
            else TRY_LONGCHAR( "&lsquo;", 7, "‘" )
            else TRY_LONGCHAR( "&rsquo;", 7, "’" )
            else TRY_LONGCHAR( "&sbquo;", 7, "‚" )
            else TRY_LONGCHAR( "&ldquo;", 7, "“" )
            else TRY_LONGCHAR( "&rdquo;", 7, "”" )
            else TRY_LONGCHAR( "&bdquo;", 7, "„" )
            else TRY_LONGCHAR( "&dagger;", 8, "†" )
            else TRY_LONGCHAR( "&Dagger;", 8, "‡" )
            else TRY_LONGCHAR( "&hellip;", 8, "…" )
            else TRY_LONGCHAR( "&permil;", 8, "‰" )
            else TRY_LONGCHAR( "&lsaquo;", 8, "‹" )
            else TRY_LONGCHAR( "&rsaquo;", 8, "›" )
            else TRY_LONGCHAR( "&euro;", 6, "€" )
            else
            {
                *p_pos = *psz_value;
                psz_value++;
            }
        }
        else
        {
            *p_pos = *psz_value;
            psz_value++;
        }

        p_pos++;
    }

    *p_pos = '\0';
}

/**
 * Converts '<', '>', '\"', '\'' and '&' to their html entities
 * \param psz_content simple element content that is to be converted
 */
char *convert_xml_special_chars( const char *psz_content )
{
    char *psz_temp = malloc( 6 * strlen( psz_content ) + 1 );
    const char *p_from = psz_content;
    char *p_to   = psz_temp;

    while ( *p_from )
    {
        if ( *p_from == '<' )
        {
            strcpy( p_to, "&lt;" );
            p_to += 4;
        }
        else if ( *p_from == '>' )
        {
            strcpy( p_to, "&gt;" );
            p_to += 4;
        }
        else if ( *p_from == '&' )
        {
            strcpy( p_to, "&amp;" );
            p_to += 5;
        }
        else if( *p_from == '\"' )
        {
            strcpy( p_to, "&quot;" );
            p_to += 6;
        }
        else if( *p_from == '\'' )
        {
            strcpy( p_to, "&#039;" );
            p_to += 6;
        }
        else
        {
            *p_to = *p_from;
            p_to++;
        }
        p_from++;
    }
    *p_to = '\0';

    return psz_temp;
}

/* Base64 encoding */
char *vlc_b64_encode_binary( const uint8_t *src, size_t i_src )
{
    static const char b64[] =
           "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    char *ret = malloc( ( i_src + 4 ) * 4 / 3 );
    char *dst = ret;

    if( dst == NULL )
        return NULL;

    while( i_src > 0 )
    {
        /* pops (up to) 3 bytes of input, push 4 bytes */
        uint32_t v;

        /* 1/3 -> 1/4 */
        v = *src++ << 24;
        *dst++ = b64[v >> 26];
        v = v << 6;

        /* 2/3 -> 2/4 */
        if( i_src >= 2 )
            v |= *src++ << 22;
        *dst++ = b64[v >> 26];
        v = v << 6;

        /* 3/3 -> 3/4 */
        if( i_src >= 3 )
            v |= *src++ << 20; // 3/3
        *dst++ = ( i_src >= 2 ) ? b64[v >> 26] : '='; // 3/4
        v = v << 6;

        /* -> 4/4 */
        *dst++ = ( i_src >= 3 ) ? b64[v >> 26] : '='; // 4/4

        if( i_src <= 3 )
            break;
        i_src -= 3;
    }

    *dst = '\0';

    return ret;
}

char *vlc_b64_encode( const char *src )
{
    if( src )
        return vlc_b64_encode_binary( (const uint8_t*)src, strlen(src) );
    else
        return vlc_b64_encode_binary( (const uint8_t*)"", 0 );
}

/* Base64 decoding */
size_t vlc_b64_decode_binary_to_buffer( uint8_t *p_dst, size_t i_dst, const char *p_src )
{
    static const int b64[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 00-0F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 10-1F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,  /* 20-2F */
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,  /* 30-3F */
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,  /* 40-4F */
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,  /* 50-5F */
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,  /* 60-6F */
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,  /* 70-7F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 80-8F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 90-9F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* A0-AF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* B0-BF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* C0-CF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* D0-DF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* E0-EF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1   /* F0-FF */
    };
    uint8_t *p_start = p_dst;
    uint8_t *p = (uint8_t *)p_src;

    int i_level;
    int i_last;

    for( i_level = 0, i_last = 0; (size_t)( p_dst - p_start ) < i_dst && *p != '\0'; p++ )
    {
        const int c = b64[(unsigned int)*p];
        if( c == -1 )
            continue;

        switch( i_level )
        {
            case 0:
                i_level++;
                break;
            case 1:
                *p_dst++ = ( i_last << 2 ) | ( ( c >> 4)&0x03 );
                i_level++;
                break;
            case 2:
                *p_dst++ = ( ( i_last << 4 )&0xf0 ) | ( ( c >> 2 )&0x0f );
                i_level++;
                break;
            case 3:
                *p_dst++ = ( ( i_last &0x03 ) << 6 ) | c;
                i_level = 0;
        }
        i_last = c;
    }

    return p_dst - p_start;
}
size_t vlc_b64_decode_binary( uint8_t **pp_dst, const char *psz_src )
{
    const int i_src = strlen( psz_src );
    uint8_t   *p_dst;

    *pp_dst = p_dst = malloc( i_src );
    if( !p_dst )
        return 0;
    return  vlc_b64_decode_binary_to_buffer( p_dst, i_src, psz_src );
}
char *vlc_b64_decode( const char *psz_src )
{
    const int i_src = strlen( psz_src );
    char *p_dst = malloc( i_src + 1 );
    size_t i_dst;
    if( !p_dst )
        return NULL;

    i_dst = vlc_b64_decode_binary_to_buffer( (uint8_t*)p_dst, i_src, psz_src );
    p_dst[i_dst] = '\0';

    return p_dst;
}

/****************************************************************************
 * String formating functions
 ****************************************************************************/
char *str_format_time( const char *tformat )
{
    char buffer[255];
    time_t curtime;
    struct tm loctime;

    /* Get the current time.  */
    curtime = time( NULL );

    /* Convert it to local time representation.  */
    localtime_r( &curtime, &loctime );
    strftime( buffer, 255, tformat, &loctime );
    return strdup( buffer );
}

#define INSERT_STRING( string )                                     \
                    if( string != NULL )                            \
                    {                                               \
                        int len = strlen( string );                 \
                        dst = realloc( dst, i_size = i_size + len );\
                        memcpy( (dst+d), string, len );             \
                        d += len;                                   \
                        free( string );                             \
                    }                                               \
                    else                                            \
                    {                                               \
                        *(dst+d) = '-';                             \
                        d++;                                        \
                    }                                               \

/* same than INSERT_STRING, except that string won't be freed */
#define INSERT_STRING_NO_FREE( string )                             \
                    {                                               \
                        int len = strlen( string );                 \
                        dst = realloc( dst, i_size = i_size + len );\
                        memcpy( dst+d, string, len );               \
                        d += len;                                   \
                    }
char *__str_format_meta( vlc_object_t *p_object, const char *string )
{
    const char *s = string;
    int b_is_format = 0;
    int b_empty_if_na = 0;
    char buf[10];
    int i_size = strlen( string ) + 1; /* +1 to store '\0' */
    char *dst = strdup( string );
    if( !dst ) return NULL;
    int d = 0;

    playlist_t *p_playlist = pl_Yield( p_object );
    input_thread_t *p_input = playlist_CurrentInput( p_playlist );
    input_item_t *p_item = NULL;
    pl_Release( p_object );
    if( p_input )
    {
        p_item = input_GetItem(p_input);
    }

    while( *s )
    {
        if( b_is_format )
        {
            switch( *s )
            {
                case 'a':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetArtist( p_item ) );
                    }
                    break;
                case 'b':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetAlbum( p_item ) );
                    }
                    break;
                case 'c':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetCopyright( p_item ) );
                    }
                    break;
                case 'd':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetDescription( p_item ) );
                    }
                    break;
                case 'e':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetEncodedBy( p_item ) );
                    }
                    break;
                case 'f':
                    if( p_item && p_item->p_stats )
                    {
                        snprintf( buf, 10, "%d",
                                  p_item->p_stats->i_displayed_pictures );
                    }
                    else
                    {
                        sprintf( buf, b_empty_if_na ? "" : "-" );
                    }
                    INSERT_STRING_NO_FREE( buf );
                    break;
                case 'g':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetGenre( p_item ) );
                    }
                    break;
                case 'l':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetLanguage( p_item ) );
                    }
                    break;
                case 'n':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetTrackNum( p_item ) );
                    }
                    break;
                case 'p':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetNowPlaying( p_item ) );
                    }
                    break;
                case 'r':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetRating( p_item ) );
                    }
                    break;
                case 's':
                {
                    char *lang = NULL;
                    if( p_input )
                        lang = var_GetNonEmptyString( p_input, "sub-language" );
                    if( lang == NULL )
                        lang = strdup( b_empty_if_na ? "" : "-" );
                    INSERT_STRING( lang );
                    break;
                }
                case 't':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetTitle( p_item ) );
                    }
                    break;
                case 'u':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetURL( p_item ) );
                    }
                    break;
                case 'A':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetDate( p_item ) );
                    }
                    break;
                case 'B':
                    if( p_input )
                    {
                        snprintf( buf, 10, "%d",
                                  var_GetInteger( p_input, "bit-rate" )/1000 );
                    }
                    else
                    {
                        sprintf( buf, b_empty_if_na ? "" : "-" );
                    }
                    INSERT_STRING_NO_FREE( buf );
                    break;
                case 'C':
                    if( p_input )
                    {
                        snprintf( buf, 10, "%d",
                                  var_GetInteger( p_input, "chapter" ) );
                    }
                    else
                    {
                        sprintf( buf, b_empty_if_na ? "" : "-" );
                    }
                    INSERT_STRING_NO_FREE( buf );
                    break;
                case 'D':
                    if( p_item )
                    {
                        mtime_t i_duration = input_item_GetDuration( p_item );
                        sprintf( buf, "%02d:%02d:%02d",
                                 (int)(i_duration/(3600000000)),
                                 (int)((i_duration/(60000000))%60),
                                 (int)((i_duration/1000000)%60) );
                    }
                    else
                    {
                        sprintf( buf, b_empty_if_na ? "" : "--:--:--" );
                    }
                    INSERT_STRING_NO_FREE( buf );
                    break;
                case 'F':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetURI( p_item ) );
                    }
                    break;
                case 'I':
                    if( p_input )
                    {
                        snprintf( buf, 10, "%d",
                                  var_GetInteger( p_input, "title" ) );
                    }
                    else
                    {
                        sprintf( buf, b_empty_if_na ? "" : "-" );
                    }
                    INSERT_STRING_NO_FREE( buf );
                    break;
                case 'L':
                    if( p_item && p_input )
                    {
                        mtime_t i_duration = input_item_GetDuration( p_item );
                        int64_t i_time = p_input->i_time;
                        sprintf( buf, "%02d:%02d:%02d",
                     (int)( ( i_duration - i_time ) / 3600000000 ),
                     (int)( ( ( i_duration - i_time ) / 60000000 ) % 60 ),
                     (int)( ( ( i_duration - i_time ) / 1000000 ) % 60 ) );
                    }
                    else
                    {
                        sprintf( buf, b_empty_if_na ? "" : "--:--:--" );
                    }
                    INSERT_STRING_NO_FREE( buf );
                    break;
                case 'N':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetName( p_item ) );
                    }
                    break;
                case 'O':
                {
                    char *lang = NULL;
                    if( p_input )
                        lang = var_GetNonEmptyString( p_input,
                                                      "audio-language" );
                    if( lang == NULL )
                        lang = strdup( b_empty_if_na ? "" : "-" );
                    INSERT_STRING( lang );
                    break;
                }
                case 'P':
                    if( p_input )
                    {
                        snprintf( buf, 10, "%2.1lf",
                                  var_GetFloat( p_input, "position" ) * 100. );
                    }
                    else
                    {
                        sprintf( buf, b_empty_if_na ? "" : "--.-%%" );
                    }
                    INSERT_STRING_NO_FREE( buf );
                    break;
                case 'R':
                    if( p_input )
                    {
                        int r = var_GetInteger( p_input, "rate" );
                        snprintf( buf, 10, "%d.%d", r/1000, r%1000 );
                    }
                    else
                    {
                        sprintf( buf, b_empty_if_na ? "" : "-" );
                    }
                    INSERT_STRING_NO_FREE( buf );
                    break;
                case 'S':
                    if( p_input )
                    {
                        int r = var_GetInteger( p_input, "sample-rate" );
                        snprintf( buf, 10, "%d.%d", r/1000, (r/100)%10 );
                    }
                    else
                    {
                        sprintf( buf, b_empty_if_na ? "" : "-" );
                    }
                    INSERT_STRING_NO_FREE( buf );
                    break;
                case 'T':
                    if( p_input )
                    {
                        sprintf( buf, "%02d:%02d:%02d",
                            (int)( p_input->i_time / ( 3600000000 ) ),
                            (int)( ( p_input->i_time / ( 60000000 ) ) % 60 ),
                            (int)( ( p_input->i_time / 1000000 ) % 60 ) );
                    }
                    else
                    {
                        sprintf( buf, b_empty_if_na ? "" :  "--:--:--" );
                    }
                    INSERT_STRING_NO_FREE( buf );
                    break;
                case 'U':
                    if( p_item )
                    {
                        INSERT_STRING( input_item_GetPublisher( p_item ) );
                    }
                    break;
                case 'V':
                {
                    audio_volume_t volume;
                    aout_VolumeGet( p_object, &volume );
                    snprintf( buf, 10, "%d", volume );
                    INSERT_STRING_NO_FREE( buf );
                    break;
                }
                case '_':
                    *(dst+d) = '\n';
                    d++;
                    break;

                case ' ':
                    b_empty_if_na = 1;
                    break;

                default:
                    *(dst+d) = *s;
                    d++;
                    break;
            }
            if( *s != ' ' )
                b_is_format = 0;
        }
        else if( *s == '$' )
        {
            b_is_format = 1;
            b_empty_if_na = 0;
        }
        else
        {
            *(dst+d) = *s;
            d++;
        }
        s++;
    }
    *(dst+d) = '\0';

    if( p_input )
        vlc_object_release( p_input );

    return dst;
}

/**
 * Apply str format time and str format meta
 */
char *__str_format( vlc_object_t *p_this, const char *psz_src )
{
    char *psz_buf1, *psz_buf2;
    psz_buf1 = str_format_time( psz_src );
    psz_buf2 = str_format_meta( p_this, psz_buf1 );
    free( psz_buf1 );
    return psz_buf2;
}

/**
 * Remove forbidden characters from filenames (including slashes)
 */
void filename_sanitize( char *str )
{
    if( *str == '.' && (str[1] == '\0' || (str[1] == '.' && str[2] == '\0' ) ) )
    {
        while( *str )
        {
            *str = '_';
            str++;
        }
        return;
    }

    while( *str )
    {
        switch( *str )
        {
            case '/':
#if defined( __APPLE__ )
            case ':':
#elif defined( WIN32 )
            case '\\':
            case '*':
            case '"':
            case '?':
            case ':':
            case '|':
            case '<':
            case '>':
#endif
                *str = '_';
        }
        str++;
    }
}

/**
 * Remove forbidden characters from full paths (leaves slashes)
 */
void path_sanitize( char *str )
{
#if 0
    /*
     * Uncomment the two blocks to prevent /../ or /./, i'm not sure that we
     * want to.
     */
    char *prev = str - 1;
#endif
#ifdef WIN32
    /* check drive prefix if path is absolute */
    if( isalpha(*str) && (':' == *(str+1)) )
        str += 2;
#endif
    while( *str )
    {
#if defined( __APPLE__ )
        if( *str == ':' )
            *str = '_';
#elif defined( WIN32 )
        switch( *str )
        {
            case '*':
            case '"':
            case '?':
            case ':':
            case '|':
            case '<':
            case '>':
                *str = '_';
        }
#endif
#if 0
        if( *str == '/'
#ifdef WIN32
            || *str == '\\'
#endif
            )
        {
            if( str - prev == 2 && prev[1] == '.' )
            {
                prev[1] = '.';
            }
            else if( str - prev == 3 && prev[1] == '.' && prev[2] == '.' )
            {
                prev[1] = '_';
                prev[2] = '_';
            }
            prev = str;
        }
#endif
        str++;
    }
}
