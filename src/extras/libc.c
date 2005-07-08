/*****************************************************************************
 * libc.c: Extra libc function for some systems.
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@videolan.org>
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
#include <string.h>                                              /* strdup() */
#include <stdlib.h>
#include <ctype.h>

#include <vlc/vlc.h>

#undef iconv_t
#undef iconv_open
#undef iconv
#undef iconv_close

#if defined(HAVE_ICONV)
#   include <iconv.h>
#endif

/*****************************************************************************
 * getenv: just in case, but it should never be called
 *****************************************************************************/
#if !defined( HAVE_GETENV )
char *vlc_getenv( const char *name )
{
    return NULL;
}
#endif

/*****************************************************************************
 * strdup: returns a malloc'd copy of a string
 *****************************************************************************/
#if !defined( HAVE_STRDUP )
char *vlc_strdup( const char *string )
{
    return strndup( string, strlen( string ) );
}
#endif

/*****************************************************************************
 * strndup: returns a malloc'd copy of at most n bytes of string
 * Does anyone know whether or not it will be present in Jaguar?
 *****************************************************************************/
#if !defined( HAVE_STRNDUP )
char *vlc_strndup( const char *string, size_t n )
{
    char *psz;
    size_t len = strlen( string );

    len = __MIN( len, n );
    psz = (char*)malloc( len + 1 );

    if( psz != NULL )
    {
        memcpy( (void*)psz, (const void*)string, len );
        psz[ len ] = 0;
    }

    return psz;
}
#endif

/*****************************************************************************
 * strcasecmp: compare two strings ignoring case
 *****************************************************************************/
#if !defined( HAVE_STRCASECMP ) && !defined( HAVE_STRICMP )
int vlc_strcasecmp( const char *s1, const char *s2 )
{
    int c1, c2;
    if( !s1 || !s2 ) return  -1;

    while( *s1 && *s2 )
    {
        c1 = tolower(*s1);
        c2 = tolower(*s2);

        if( c1 != c2 ) return (c1 < c2 ? -1 : 1);
        s1++; s2++;
    }

    if( !*s1 && !*s2 ) return 0;
    else return (*s1 ? 1 : -1);
}
#endif

/*****************************************************************************
 * strncasecmp: compare n chars from two strings ignoring case
 *****************************************************************************/
#if !defined( HAVE_STRNCASECMP ) && !defined( HAVE_STRNICMP )
int vlc_strncasecmp( const char *s1, const char *s2, size_t n )
{
    int c1, c2;
    if( !s1 || !s2 ) return  -1;

    while( n > 0 && *s1 && *s2 )
    {
        c1 = tolower(*s1);
        c2 = tolower(*s2);

        if( c1 != c2 ) return (c1 < c2 ? -1 : 1);
        s1++; s2++; n--;
    }

    if( !n || (!*s1 && !*s2) ) return 0;
    else return (*s1 ? 1 : -1);
}
#endif

/******************************************************************************
 * strcasestr: find a substring (little) in another substring (big)
 * Case sensitive. Return NULL if not found, return big if little == null
 *****************************************************************************/
#if !defined( HAVE_STRCASESTR ) && !defined( HAVE_STRISTR )
char * vlc_strcasestr( const char *psz_big, const char *psz_little )
{
    char *p_pos = (char *)psz_big;

    if( !psz_big || !psz_little || !*psz_little ) return p_pos;
 
    while( *p_pos ) 
    {
        if( toupper( *p_pos ) == toupper( *psz_little ) )
        {
            char * psz_cur1 = p_pos + 1;
            char * psz_cur2 = (char *)psz_little + 1;
            while( *psz_cur1 && *psz_cur2 &&
                   toupper( *psz_cur1 ) == toupper( *psz_cur2 ) )
            {
                psz_cur1++;
                psz_cur2++;
            }
            if( !*psz_cur2 ) return p_pos;
        }
        p_pos++;
    }
    return NULL;
}
#endif

/*****************************************************************************
 * vasprintf:
 *****************************************************************************/
#if !defined(HAVE_VASPRINTF) || defined(SYS_DARWIN) || defined(SYS_BEOS)
int vlc_vasprintf(char **strp, const char *fmt, va_list ap)
{
    /* Guess we need no more than 100 bytes. */
    int     i_size = 100;
    char    *p = malloc( i_size );
    int     n;

    if( p == NULL )
    {
        *strp = NULL;
        return -1;
    }

    for( ;; )
    {
        /* Try to print in the allocated space. */
        n = vsnprintf( p, i_size, fmt, ap );

        /* If that worked, return the string. */
        if (n > -1 && n < i_size)
        {
            *strp = p;
            return strlen( p );
        }
        /* Else try again with more space. */
        if (n > -1)    /* glibc 2.1 */
        {
           i_size = n+1; /* precisely what is needed */
        }
        else           /* glibc 2.0 */
        {
           i_size *= 2;  /* twice the old size */
        }
        if( (p = realloc( p, i_size ) ) == NULL)
        {
            *strp = NULL;
            return -1;
        }
    }
}
#endif

/*****************************************************************************
 * asprintf:
 *****************************************************************************/
#if !defined(HAVE_ASPRINTF) || defined(SYS_DARWIN) || defined(SYS_BEOS)
int vlc_asprintf( char **strp, const char *fmt, ... )
{
    va_list args;
    int i_ret;

    va_start( args, fmt );
    i_ret = vasprintf( strp, fmt, args );
    va_end( args );

    return i_ret;
}
#endif

/*****************************************************************************
 * atof: convert a string to a double.
 *****************************************************************************/
#if !defined( HAVE_ATOF )
double vlc_atof( const char *nptr )
{
    double f_result;
    wchar_t *psz_tmp;
    int i_len = strlen( nptr ) + 1;

    psz_tmp = malloc( i_len * sizeof(wchar_t) );
    MultiByteToWideChar( CP_ACP, 0, nptr, -1, psz_tmp, i_len );
    f_result = wcstod( psz_tmp, NULL );
    free( psz_tmp );

    return f_result;
}
#endif

/*****************************************************************************
 * strtoll: convert a string to a 64 bits int.
 *****************************************************************************/
#if !defined( HAVE_STRTOLL )
int64_t vlc_strtoll( const char *nptr, char **endptr, int base )
{
    int64_t i_value = 0;
    int sign = 1, newbase = base ? base : 10;

    while( isspace(*nptr) ) nptr++;

    if( *nptr == '-' )
    {
        sign = -1;
        nptr++;
    }

    /* Try to detect base */
    if( *nptr == '0' )
    {
        newbase = 8;
        nptr++;

        if( *nptr == 'x' )
        {
            newbase = 16;
            nptr++;
        }
    }

    if( base && newbase != base )
    {
        if( endptr ) *endptr = (char *)nptr;
        return i_value;
    }

    switch( newbase )
    {
        case 10:
            while( *nptr >= '0' && *nptr <= '9' )
            {
                i_value *= 10;
                i_value += ( *nptr++ - '0' );
            }
            if( endptr ) *endptr = (char *)nptr;
            break;

        case 16:
            while( (*nptr >= '0' && *nptr <= '9') ||
                   (*nptr >= 'a' && *nptr <= 'f') ||
                   (*nptr >= 'A' && *nptr <= 'F') )
            {
                int i_valc = 0;
                if(*nptr >= '0' && *nptr <= '9') i_valc = *nptr - '0';
                else if(*nptr >= 'a' && *nptr <= 'f') i_valc = *nptr - 'a' +10;
                else if(*nptr >= 'A' && *nptr <= 'F') i_valc = *nptr - 'A' +10;
                i_value *= 16;
                i_value += i_valc;
                nptr++;
            }
            if( endptr ) *endptr = (char *)nptr;
            break;

        default:
            i_value = strtol( nptr, endptr, newbase );
            break;
    }

    return i_value * sign;
}
#endif

/*****************************************************************************
 * atoll: convert a string to a 64 bits int.
 *****************************************************************************/
#if !defined( HAVE_ATOLL )
int64_t vlc_atoll( const char *nptr )
{
    return strtoll( nptr, (char **)NULL, 10 );
}
#endif

/*****************************************************************************
 * dgettext: gettext for plugins.
 *****************************************************************************/
char *vlc_dgettext( const char *package, const char *msgid )
{
#if defined( ENABLE_NLS ) \
     && ( defined(HAVE_GETTEXT) || defined(HAVE_INCLUDED_GETTEXT) )
    return dgettext( package, msgid );
#else
    return (char *)msgid;
#endif
}

/*****************************************************************************
 * count_utf8_string: returns the number of characters in the string.
 *****************************************************************************/
static int count_utf8_string( const char *psz_string )
{
    int i = 0, i_count = 0;
    while( psz_string[ i ] != 0 )
    {
        if( ((unsigned char *)psz_string)[ i ] <  0x80UL ) i_count++;
        i++;
    }
    return i_count;
}

/*****************************************************************************
 * wraptext: inserts \n at convenient places to wrap the text.
 *           Returns the modified string in a new buffer.
 *****************************************************************************/
char *vlc_wraptext( const char *psz_text, int i_line, vlc_bool_t b_utf8 )
{
    int i_len;
    char *psz_line, *psz_new_text;

    psz_line = psz_new_text = strdup( psz_text );

    if( b_utf8 )
        i_len = count_utf8_string( psz_text );
    else
        i_len = strlen( psz_text );

    while( i_len > i_line )
    {
        /* Look if there is a newline somewhere. */
        char *psz_parser = psz_line;
        int i_count = 0;
        while( i_count <= i_line && *psz_parser != '\n' )
        {
            if( b_utf8 )
            {
                while( *((unsigned char *)psz_parser) >= 0x80UL ) psz_parser++;
            }
            psz_parser++;
            i_count++;
        }
        if( *psz_parser == '\n' )
        {
            i_len -= (i_count + 1);
            psz_line = psz_parser + 1;
            continue;
        }

        /* Find the furthest space. */
        while( psz_parser > psz_line && *psz_parser != ' ' )
        {
            if( b_utf8 )
            {
                while( *((unsigned char *)psz_parser) >= 0x80UL ) psz_parser--;
            }
            psz_parser--;
            i_count--;
        }
        if( *psz_parser == ' ' )
        {
            *psz_parser = '\n';
            i_len -= (i_count + 1);
            psz_line = psz_parser + 1;
            continue;
        }

        /* Wrapping has failed. Find the first space or newline */
        while( i_count < i_len && *psz_parser != ' ' && *psz_parser != '\n' )
        {
            if( b_utf8 )
            {
                while( *((unsigned char *)psz_parser) >= 0x80UL ) psz_parser++;
            }
            psz_parser++;
            i_count++;
        }
        if( i_count < i_len ) *psz_parser = '\n';
        i_len -= (i_count + 1);
        psz_line = psz_parser + 1;
    }

    return psz_new_text;
}

/*****************************************************************************
 * iconv wrapper
 *****************************************************************************/
vlc_iconv_t vlc_iconv_open( const char *tocode, const char *fromcode )
{
#if defined(HAVE_ICONV)
    return iconv_open( tocode, fromcode );
#else
    return NULL;
#endif
}

size_t vlc_iconv( vlc_iconv_t cd, char **inbuf, size_t *inbytesleft,
                  char **outbuf, size_t *outbytesleft )
{
#if defined(HAVE_ICONV)
    return iconv( cd, inbuf, inbytesleft, outbuf, outbytesleft );
#else
    int i_bytes = __MIN(*inbytesleft, *outbytesleft);
    if( !inbuf || !outbuf || !i_bytes ) return (size_t)(-1);
    memcpy( *outbuf, *inbuf, i_bytes );
    inbuf += i_bytes;
    outbuf += i_bytes;
    inbytesleft -= i_bytes;
    outbytesleft -= i_bytes;
    return i_bytes;
#endif
}

int vlc_iconv_close( vlc_iconv_t cd )
{
#if defined(HAVE_ICONV)
    return iconv_close( cd );
#else
    return 0;
#endif
}

/*****************************************************************************
 * reduce a fraction
 *   (adapted from libavcodec, author Michael Niedermayer <michaelni@gmx.at>)
 *****************************************************************************/
vlc_bool_t vlc_reduce( int *pi_dst_nom, int *pi_dst_den,
                       int64_t i_nom, int64_t i_den, int64_t i_max )
{
    vlc_bool_t b_exact = 1, b_sign = 0;
    int64_t i_gcd;

    if( i_den == 0 )
    {
        *pi_dst_nom = 0;
        *pi_dst_den = 1;
        return 1;
    }

    if( i_den < 0 )
    {
        i_den = - i_den;
        i_nom = - i_nom;
    }

    if( i_nom < 0 )
    {
        i_nom = - i_nom;
        b_sign = 1;
    }

    i_gcd = GCD( i_nom, i_den );
    i_nom /= i_gcd;
    i_den /= i_gcd;

    if( i_max == 0 ) i_max = I64C(0xFFFFFFFF);

    if( i_nom > i_max || i_den > i_max )
    {
        int i_a0_num = 0, i_a0_den = 1, i_a1_num = 1, i_a1_den = 0;
        b_exact = 0;

        for( ; ; )
        {
            int64_t i_x = i_nom / i_den;
            int64_t i_a2n = i_x * i_a1_num + i_a0_num;
            int64_t i_a2d = i_x * i_a1_den + i_a0_den;

            if( i_a2n > i_max || i_a2d > i_max ) break;

            i_nom %= i_den;

            i_a0_num = i_a1_num; i_a0_den = i_a1_den;
            i_a1_num = i_a2n; i_a1_den = i_a2d;
            if( i_nom == 0 ) break;
            i_x = i_nom; i_nom = i_den; i_den = i_x;
        }
        i_nom = i_a1_num;
        i_den = i_a1_den;
    }

    if( b_sign ) i_nom = - i_nom;

    *pi_dst_nom = i_nom;
    *pi_dst_den = i_den;

    return b_exact;
}

/*************************************************************************
 * vlc_parse_cmdline: Command line parsing into elements.
 *
 * The command line is composed of space/tab separated arguments.
 * Quotes can be used as argument delimiters and a backslash can be used to
 * escape a quote.
 *************************************************************************/
static void find_end_quote( char **s, char **ppsz_parser, int i_quote )
{
    int i_bcount = 0;

    while( **s )
    {
        if( **s == '\\' )
        {
            **ppsz_parser = **s;
            (*ppsz_parser)++; (*s)++;
            i_bcount++;
        }
        else if( **s == '"' || **s == '\'' )
        {
            /* Preceeded by a number of '\' which we erase. */
            *ppsz_parser -= i_bcount / 2;
            if( i_bcount & 1 )
            {
                /* '\\' followed by a '"' or '\'' */
                *ppsz_parser -= 1;
                **ppsz_parser = **s;
                (*ppsz_parser)++; (*s)++;
                i_bcount = 0;
                continue;
            }

            if( **s == i_quote )
            {
                /* End */
                return;
            }
            else
            {
                /* Different quoting */
                int i_quote = **s;
                **ppsz_parser = **s;
                (*ppsz_parser)++; (*s)++;
                find_end_quote( s, ppsz_parser, i_quote );
                **ppsz_parser = **s;
                (*ppsz_parser)++; (*s)++;
            }

            i_bcount = 0;
        }
        else
        {
            /* A regular character */
            **ppsz_parser = **s;
            (*ppsz_parser)++; (*s)++;
            i_bcount = 0;
        }
    }
}

char **vlc_parse_cmdline( const char *psz_cmdline, int *i_args )
{
    int argc = 0;
    char **argv = 0;
    char *s, *psz_parser, *psz_arg, *psz_orig;
    int i_bcount = 0;

    if( !psz_cmdline ) return 0;
    psz_orig = strdup( psz_cmdline );
    psz_arg = psz_parser = s = psz_orig;

    while( *s )
    {
        if( *s == '\t' || *s == ' ' )
        {
            /* We have a complete argument */
            *psz_parser = 0;
            TAB_APPEND( argc, argv, strdup(psz_arg) );

            /* Skip trailing spaces/tabs */
            do{ s++; } while( *s == '\t' || *s == ' ' );

            /* New argument */
            psz_arg = psz_parser = s;
            i_bcount = 0;
        }
        else if( *s == '\\' )
        {
            *psz_parser++ = *s++;
            i_bcount++;
        }
        else if( *s == '"' || *s == '\'' )
        {
            if( ( i_bcount & 1 ) == 0 )
            {
                /* Preceeded by an even number of '\', this is half that
                 * number of '\', plus a quote which we erase. */
                int i_quote = *s;
                psz_parser -= i_bcount / 2;
                s++;
                find_end_quote( &s, &psz_parser, i_quote );
                s++;
            }
            else
            {
                /* Preceeded by an odd number of '\', this is half that
                 * number of '\' followed by a '"' */
                psz_parser = psz_parser - i_bcount/2 - 1;
                *psz_parser++ = '"';
                s++;
            }
            i_bcount = 0;
        }
        else
        {
            /* A regular character */
            *psz_parser++ = *s++;
            i_bcount = 0;
        }
    }

    /* Take care of the last arg */
    if( *psz_arg )
    {
        *psz_parser = '\0';
        TAB_APPEND( argc, argv, strdup(psz_arg) );
    }

    if( i_args ) *i_args = argc;
    free( psz_orig );
    return argv;
}
