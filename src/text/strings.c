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
#include <vlc/vlc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
        if( !strncmp( psz_value, "&lt;", 4 ) )
        {
            *p_pos = '<';
            psz_value += 4;
        }
        else if( !strncmp( psz_value, "&gt;", 4 ) )
        {
            *p_pos = '>';
            psz_value += 4;
        }
        else if( !strncmp( psz_value, "&amp;", 5 ) )
        {
            *p_pos = '&';
            psz_value += 5;
        }
        else if( !strncmp( psz_value, "&quot;", 6 ) )
        {
            *p_pos = '\"';
            psz_value += 6;
        }
        else if( !strncmp( psz_value, "&#039;", 6 ) )
        {
            *p_pos = '\'';
            psz_value += 6;
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

    for( i_level = 0, i_last = 0; i_dst > 0 && *p != '\0'; i_dst--, p++ )
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
#if defined(HAVE_LOCALTIME_R)
    struct tm loctime;
#else
    struct tm *loctime;
#endif

    /* Get the current time.  */
    curtime = time( NULL );

    /* Convert it to local time representation.  */
#if defined(HAVE_LOCALTIME_R)
    localtime_r( &curtime, &loctime );
    strftime( buffer, 255, tformat, &loctime );
#else
    loctime = localtime( &curtime );
    strftime( buffer, 255, tformat, loctime );
#endif
    return strdup( buffer );
}

#define INSERT_STRING( check, string )                              \
                    if( check && string )                           \
                    {                                               \
                        int len = strlen( string );                 \
                        dst = realloc( dst,                         \
                                       i_size = i_size + len + 1 ); \
                        strncpy( d, string, len+1 );                \
                        d += len;                                   \
                    }                                               \
                    else                                            \
                    {                                               \
                        *d = '-';                                   \
                        d++;                                        \
                    }
char *__str_format_meta( vlc_object_t *p_object, const char *string )
{
    const char *s = string;
    char *dst = malloc( 1000 );
    char *d = dst;
    int b_is_format = 0;
    char buf[10];
    int i_size = strlen( string );

    playlist_t *p_playlist = pl_Yield( p_object );
    input_thread_t *p_input = p_playlist->p_input;
    input_item_t *p_item = NULL;
    pl_Release( p_object );
    if( p_input )
    {
        vlc_object_yield( p_input );
        p_item = input_GetItem(p_input);
        if( p_item )
            vlc_mutex_lock( &p_item->lock );
    }

    sprintf( dst, string );

    while( *s )
    {
        if( b_is_format )
        {
            switch( *s )
            {
                case 'a':
                    INSERT_STRING( p_item && p_item->p_meta,
                                   p_item->p_meta->psz_artist );
                    break;
                case 'b':
                    INSERT_STRING( p_item && p_item->p_meta,
                                   p_item->p_meta->psz_album );
                    break;
                case 'c':
                    INSERT_STRING( p_item && p_item->p_meta,
                                   p_item->p_meta->psz_copyright );
                    break;
                case 'd':
                    INSERT_STRING( p_item && p_item->p_meta,
                                   p_item->p_meta->psz_description );
                    break;
                case 'e':
                    INSERT_STRING( p_item && p_item->p_meta,
                                   p_item->p_meta->psz_encodedby );
                    break;
                case 'g':
                    INSERT_STRING( p_item && p_item->p_meta,
                                   p_item->p_meta->psz_genre );
                    break;
                case 'l':
                    INSERT_STRING( p_item && p_item->p_meta,
                                   p_item->p_meta->psz_language );
                    break;
                case 'n':
                    INSERT_STRING( p_item && p_item->p_meta,
                                   p_item->p_meta->psz_tracknum );
                    break;
                case 'p':
                    INSERT_STRING( p_item && p_item->p_meta,
                                   p_item->p_meta->psz_nowplaying );
                    break;
                case 'r':
                    INSERT_STRING( p_item && p_item->p_meta,
                                   p_item->p_meta->psz_rating );
                    break;
                case 's':
                {
                    char *lang;
                    if( p_input )
                    {
                        lang = var_GetString( p_input, "sub-language" );
                    }
                    else
                    {
                        lang = strdup( "-" );
                    }
                    INSERT_STRING( 1, lang );
                    free( lang );
                    break;
                }
                case 't':
                    INSERT_STRING( p_item && p_item->p_meta,
                                   p_item->p_meta->psz_title );
                    break;
                case 'u':
                    INSERT_STRING( p_item && p_item->p_meta,
                                   p_item->p_meta->psz_url );
                    break;
                case 'A':
                    INSERT_STRING( p_item && p_item->p_meta,
                                   p_item->p_meta->psz_date );
                    break;
                case 'B':
                    if( p_input )
                    {
                        snprintf( buf, 10, "%d",
                                  var_GetInteger( p_input, "bit-rate" )/1000 );
                    }
                    else
                    {
                        sprintf( buf, "-" );
                    }
                    INSERT_STRING( 1, buf );
                    break;
                case 'C':
                    if( p_input )
                    {
                        snprintf( buf, 10, "%d",
                                  var_GetInteger( p_input, "chapter" ) );
                    }
                    else
                    {
                        sprintf( buf, "-" );
                    }
                    INSERT_STRING( 1, buf );
                    break;
                case 'D':
                    if( p_item )
                    {
                        sprintf( buf, "%02d:%02d:%02d",
                                 (int)(p_item->i_duration/(3600000000)),
                                 (int)((p_item->i_duration/(60000000))%60),
                                 (int)((p_item->i_duration/1000000)%60) );
                    }
                    else
                    {
                        sprintf( buf, "--:--:--" );
                    }
                    INSERT_STRING( 1, buf );
                    break;
                case 'F':
                    INSERT_STRING( p_item, p_item->psz_uri );
                    break;
                case 'I':
                    if( p_input )
                    {
                        snprintf( buf, 10, "%d",
                                  var_GetInteger( p_input, "title" ) );
                    }
                    else
                    {
                        sprintf( buf, "-" );
                    }
                    INSERT_STRING( 1, buf );
                    break;
                case 'L':
                    if( p_item && p_input )
                    {
                        sprintf( buf, "%02d:%02d:%02d",
                     (int)((p_item->i_duration-p_input->i_time)/(3600000000)),
                     (int)(((p_item->i_duration-p_input->i_time)/(60000000))%60),
                     (int)(((p_item->i_duration-p_input->i_time)/1000000)%60) );
                    }
                    else
                    {
                        sprintf( buf, "--:--:--" );
                    }
                    INSERT_STRING( 1, buf );
                    break;
                case 'N':
                    INSERT_STRING( p_item, p_item->psz_name );
                    break;
                case 'O':
                {
                    char *lang;
                    if( p_input )
                    {
                        lang = var_GetString( p_input, "audio-language" );
                    }
                    else
                    {
                        lang = strdup( "-" );
                    }
                    INSERT_STRING( 1, lang );
                    free( lang );
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
                        sprintf( buf, "--.-%%" );
                    }
                    INSERT_STRING( 1, buf );
                    break;
                case 'R':
                    if( p_input )
                    {
                        int r = var_GetInteger( p_input, "rate" );
                        snprintf( buf, 10, "%d.%d", r/1000, r%1000 );
                    }
                    else
                    {
                        sprintf( buf, "-" );
                    }
                    INSERT_STRING( 1, buf );
                    break;
                case 'S':
                    if( p_input )
                    {
                        int r = var_GetInteger( p_input, "sample-rate" );
                        snprintf( buf, 10, "%d.%d", r/1000, (r/100)%10 );
                    }
                    else
                    {
                        sprintf( buf, "-" );
                    }
                    INSERT_STRING( 1, buf );
                    break;
                case 'T':
                    if( p_input )
                    {
                        sprintf( buf, "%02d:%02d:%02d",
                                 (int)(p_input->i_time/(3600000000)),
                                 (int)((p_input->i_time/(60000000))%60),
                                 (int)((p_input->i_time/1000000)%60) );
                    }
                    else
                    {
                        sprintf( buf, "--:--:--" );
                    }
                    INSERT_STRING( 1, buf );
                    break;
                case 'U':
                    INSERT_STRING( p_item && p_item->p_meta,
                                   p_item->p_meta->psz_publisher );
                    break;
                case 'V':
                {
                    audio_volume_t volume;
                    aout_VolumeGet( p_object, &volume );
                    snprintf( buf, 10, "%d", volume );
                    INSERT_STRING( 1, buf );
                    break;
                }
                case '_':
                    *d = '\n';
                    d++;
                    break;

                default:
                    *d = *s;
                    d++;
                    break;
            }
            b_is_format = 0;
        }
        else if( *s == '$' )
        {
            b_is_format = 1;
        }
        else
        {
            *d = *s;
            d++;
        }
        s++;
    }
    *d = '\0';

    if( p_input )
    {
        vlc_object_release( p_input );
        if( p_item )
            vlc_mutex_unlock( &p_item->lock );
    }

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
    while( *str )
    {
        switch( *str )
        {
            case '/':
#ifdef WIN32
            case '*':
            case '"':
            case '\\':
            case '[':
            case ']':
            case ':':
            case ';':
            case '|':
            case '=':
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
    while( *str )
    {
        switch( *str )
        {
#ifdef WIN32
            case '*':
            case '"':
            case '[':
            case ']':
            case ':':
            case ';':
            case '|':
            case '=':
#endif
                *str = '_';
        }
        str++;
    }
}
