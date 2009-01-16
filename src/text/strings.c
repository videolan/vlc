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

static inline bool isurisafe( int c )
{
    /* These are the _unreserved_ URI characters (RFC3986 §2.3) */
    return ( (unsigned char)( c - 'a' ) < 26 )
            || ( (unsigned char)( c - 'A' ) < 26 )
            || ( (unsigned char)( c - '0' ) < 10 )
            || ( strchr( "-._~", c ) != NULL );
}

/**
 * Encodes an URI component (RFC3986 §2).
 *
 * @param psz_uri nul-terminated UTF-8 representation of the component.
 * Obviously, you can't pass an URI containing a nul character, but you don't
 * want to do that, do you?
 *
 * @return encoded string (must be free()'d), or NULL for ENOMEM.
 */
char *encode_URI_component( const char *psz_uri )
{
    char *psz_enc = malloc ((3 * strlen (psz_uri)) + 1), *out = psz_enc;

    if (psz_enc == NULL)
        return NULL;

    while (*psz_uri)
    {
        static const char hex[16] = "0123456789ABCDEF";
        uint8_t c = *psz_uri;

        if( isurisafe( c ) )
            *out++ = c;
        /* This is URI encoding, not HTTP forms:
         * Space is encoded as '%20', not '+'. */
        else
        {
            *out++ = '%';
            *out++ = hex[c >> 4];
            *out++ = hex[c & 0xf];
        }
        psz_uri++;
    }
    *out++ = '\0';

    out = realloc (psz_enc, out - psz_enc);
    return out ? out : psz_enc; /* realloc() can fail (safe) */
}

static const struct xml_entity_s
{
    char    psz_entity[8];
    char    psz_char[4];
} xml_entities[] = {
    /* Important: this list has to be in alphabetical order (psz_entity-wise) */
    { "AElig;",  "Æ" },
    { "Aacute;", "Á" },
    { "Acirc;",  "Â" },
    { "Agrave;", "À" },
    { "Aring;",  "Å" },
    { "Atilde;", "Ã" },
    { "Auml;",   "Ä" },
    { "Ccedil;", "Ç" },
    { "Dagger;", "‡" },
    { "ETH;",    "Ð" },
    { "Eacute;", "É" },
    { "Ecirc;",  "Ê" },
    { "Egrave;", "È" },
    { "Euml;",   "Ë" },
    { "Iacute;", "Í" },
    { "Icirc;",  "Î" },
    { "Igrave;", "Ì" },
    { "Iuml;",   "Ï" },
    { "Ntilde;", "Ñ" },
    { "OElig;",  "Œ" },
    { "Oacute;", "Ó" },
    { "Ocirc;",  "Ô" },
    { "Ograve;", "Ò" },
    { "Oslash;", "Ø" },
    { "Otilde;", "Õ" },
    { "Ouml;",   "Ö" },
    { "Scaron;", "Š" },
    { "THORN;",  "Þ" },
    { "Uacute;", "Ú" },
    { "Ucirc;",  "Û" },
    { "Ugrave;", "Ù" },
    { "Uuml;",   "Ü" },
    { "Yacute;", "Ý" },
    { "Yuml;",   "Ÿ" },
    { "aacute;", "á" },
    { "acirc;",  "â" },
    { "acute;",  "´" },
    { "aelig;",  "æ" },
    { "agrave;", "à" },
    { "amp;",    "&" },
    { "apos;",   "'" },
    { "aring;",  "å" },
    { "atilde;", "ã" },
    { "auml;",   "ä" },
    { "bdquo;",  "„" },
    { "brvbar;", "¦" },
    { "ccedil;", "ç" },
    { "cedil;",  "¸" },
    { "cent;",   "¢" },
    { "circ;",   "ˆ" },
    { "copy;",   "©" },
    { "curren;", "¤" },
    { "dagger;", "†" },
    { "deg;",    "°" },
    { "divide;", "÷" },
    { "eacute;", "é" },
    { "ecirc;",  "ê" },
    { "egrave;", "è" },
    { "eth;",    "ð" },
    { "euml;",   "ë" },
    { "euro;",   "€" },
    { "frac12;", "½" },
    { "frac14;", "¼" },
    { "frac34;", "¾" },
    { "gt;",     ">" },
    { "hellip;", "…" },
    { "iacute;", "í" },
    { "icirc;",  "î" },
    { "iexcl;",  "¡" },
    { "igrave;", "ì" },
    { "iquest;", "¿" },
    { "iuml;",   "ï" },
    { "laquo;",  "«" },
    { "ldquo;",  "“" },
    { "lsaquo;", "‹" },
    { "lsquo;",  "‘" },
    { "lt;",     "<" },
    { "macr;",   "¯" },
    { "mdash;",  "—" },
    { "micro;",  "µ" },
    { "middot;", "·" },
    { "nbsp;",   "\xc2\xa0" },
    { "ndash;",  "–" },
    { "not;",    "¬" },
    { "ntilde;", "ñ" },
    { "oacute;", "ó" },
    { "ocirc;",  "ô" },
    { "oelig;",  "œ" },
    { "ograve;", "ò" },
    { "ordf;",   "ª" },
    { "ordm;",   "º" },
    { "oslash;", "ø" },
    { "otilde;", "õ" },
    { "ouml;",   "ö" },
    { "para;",   "¶" },
    { "permil;", "‰" },
    { "plusmn;", "±" },
    { "pound;",  "£" },
    { "quot;",   "\"" },
    { "raquo;",  "»" },
    { "rdquo;",  "”" },
    { "reg;",    "®" },
    { "rsaquo;", "›" },
    { "rsquo;",  "’" },
    { "sbquo;",  "‚" },
    { "scaron;", "š" },
    { "sect;",   "§" },
    { "shy;",    "­" },
    { "sup1;",   "¹" },
    { "sup2;",   "²" },
    { "sup3;",   "³" },
    { "szlig;",  "ß" },
    { "thorn;",  "þ" },
    { "tilde;",  "˜" },
    { "times;",  "×" },
    { "trade;",  "™" },
    { "uacute;", "ú" },
    { "ucirc;",  "û" },
    { "ugrave;", "ù" },
    { "uml;",    "¨" },
    { "uuml;",   "ü" },
    { "yacute;", "ý" },
    { "yen;",    "¥" },
    { "yuml;",   "ÿ" },
};

static int cmp_entity (const void *key, const void *elem)
{
    const struct xml_entity_s *ent = elem;
    const char *name = key;

    return strncmp (name, ent->psz_entity, strlen (ent->psz_entity));
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
            const char *psz_value1 = psz_value + 1;
            if( *psz_value1 == '#' )
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
            else
            {
                const struct xml_entity_s *ent;

                ent = bsearch (psz_value1, xml_entities,
                               sizeof (xml_entities) / sizeof (*ent),
                               sizeof (*ent), cmp_entity);
                if (ent != NULL)
                {
                    size_t olen = strlen (ent->psz_char);
                    memcpy (p_pos, ent->psz_char, olen);
                    p_pos += olen - 1;
                    psz_value += strlen (ent->psz_entity) + 1;
                }
                else
                {   /* No match */
                    *p_pos = *psz_value;
                    psz_value++;
                }
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

/**
 * Formats current time into a heap-allocated string.
 * @param tformat time format (as with C strftime())
 * @return an allocated string (must be free()'d), or NULL on memory error.
 */
char *str_format_time( const char *tformat )
{
    time_t curtime;
    struct tm loctime;

    if (strcmp (tformat, "") == 0)
        return strdup (""); /* corner case w.r.t. strftime() return value */

    /* Get the current time.  */
    time( &curtime );

    /* Convert it to local time representation.  */
    localtime_r( &curtime, &loctime );
    for (size_t buflen = strlen (tformat) + 32;; buflen += 32)
    {
        char *str = malloc (buflen);
        if (str == NULL)
            return NULL;

        size_t len = strftime (str, buflen, tformat, &loctime);
        if (len > 0)
        {
            char *ret = realloc (str, len + 1);
            return ret ? ret : str; /* <- this cannot fail */
        }
    }
    assert (0);
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
                    else if( !b_empty_if_na )                       \
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
    bool b_is_format = false;
    bool b_empty_if_na = false;
    char buf[10];
    int i_size = strlen( string ) + 1; /* +1 to store '\0' */
    char *dst = strdup( string );
    if( !dst ) return NULL;
    int d = 0;

    playlist_t *p_playlist = pl_Hold( p_object );
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
                        int64_t i_time = var_GetInteger( p_input, "time" );
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
                        int64_t i_time = var_GetInteger( p_input, "time" );
                        sprintf( buf, "%02d:%02d:%02d",
                            (int)( i_time / ( 3600000000 ) ),
                            (int)( ( i_time / ( 60000000 ) ) % 60 ),
                            (int)( ( i_time / 1000000 ) % 60 ) );
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
                    b_empty_if_na = true;
                    break;

                default:
                    *(dst+d) = *s;
                    d++;
                    break;
            }
            if( *s != ' ' )
                b_is_format = false;
        }
        else if( *s == '$' )
        {
            b_is_format = true;
            b_empty_if_na = false;
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
#undef INSERT_STRING
#undef INSERT_STRING_NO_FREE

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
