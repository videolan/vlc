/*****************************************************************************
 * libc.c: Extra libc function for some systems.
 *****************************************************************************
 * Copyright (C) 2002 the VideoLAN team
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Christophe Massiot <massiot@via.ecp.fr>
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

#ifdef HAVE_DIRENT_H
#   include <dirent.h>
#endif

#ifdef HAVE_FORK
#   include <sys/time.h>
#   include <unistd.h>
#   include <errno.h>
#   include <sys/wait.h>
#endif

#if defined(WIN32) || defined(UNDER_CE)
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#endif

#ifdef UNDER_CE
#   define strcoll strcmp
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
 * vlc_*dir_wrapper: wrapper under Windows to return the list of drive letters
 * when called with an empty argument or just '\'
 *****************************************************************************/
#if defined(WIN32) && !defined(UNDER_CE)
typedef struct vlc_DIR
{
    DIR *p_real_dir;
    int i_drives;
    struct dirent dd_dir;
    vlc_bool_t b_insert_back;
} vlc_DIR;

void *vlc_opendir_wrapper( const char *psz_path )
{
    vlc_DIR *p_dir;
    DIR *p_real_dir;

    if ( psz_path == NULL || psz_path[0] == '\0'
          || (psz_path[0] == '\\' && psz_path[1] == '\0') )
    {
        /* Special mode to list drive letters */
        p_dir = malloc( sizeof(vlc_DIR) );
        p_dir->p_real_dir = NULL;
        p_dir->i_drives = GetLogicalDrives();
        return (void *)p_dir;
    }

    p_real_dir = opendir( psz_path );
    if ( p_real_dir == NULL )
        return NULL;

    p_dir = malloc( sizeof(vlc_DIR) );
    p_dir->p_real_dir = p_real_dir;
    p_dir->b_insert_back = ( psz_path[1] == ':' && psz_path[2] == '\\'
                              && psz_path[3] =='\0' );
    return (void *)p_dir;
}

struct dirent *vlc_readdir_wrapper( void *_p_dir )
{
    vlc_DIR *p_dir = (vlc_DIR *)_p_dir;
    unsigned int i;
    DWORD i_drives;

    if ( p_dir->p_real_dir != NULL )
    {
        if ( p_dir->b_insert_back )
        {
            p_dir->dd_dir.d_ino = 0;
            p_dir->dd_dir.d_reclen = 0;
            p_dir->dd_dir.d_namlen = 2;
            strcpy( p_dir->dd_dir.d_name, ".." );
            p_dir->b_insert_back = VLC_FALSE;
            return &p_dir->dd_dir;
        }

        return readdir( p_dir->p_real_dir );
    }

    /* Drive letters mode */
    i_drives = p_dir->i_drives;
    if ( !i_drives )
        return NULL; /* end */

    for ( i = 0; i < sizeof(DWORD)*8; i++, i_drives >>= 1 )
        if ( i_drives & 1 ) break;

    if ( i >= 26 )
        return NULL; /* this should not happen */

    sprintf( p_dir->dd_dir.d_name, "%c:\\", 'A' + i );
    p_dir->dd_dir.d_namlen = strlen(p_dir->dd_dir.d_name);
    p_dir->i_drives &= ~(1UL << i);
    return &p_dir->dd_dir;
}

int vlc_closedir_wrapper( void *_p_dir )
{
    vlc_DIR *p_dir = (vlc_DIR *)_p_dir;

    if ( p_dir->p_real_dir != NULL )
    {
        int i_ret = closedir( p_dir->p_real_dir );
        free( p_dir );
        return i_ret;
    }

    free( p_dir );
    return 0;
}
#else
void *vlc_opendir_wrapper( const char *psz_path )
{
    return (void *)opendir( psz_path );
}
struct dirent *vlc_readdir_wrapper( void *_p_dir )
{
    return readdir( (DIR *)_p_dir );
}
int vlc_closedir_wrapper( void *_p_dir )
{
    return closedir( (DIR *)_p_dir );
}
#endif

/*****************************************************************************
 * scandir: scan a directory alpha-sorted
 *****************************************************************************/
#if !defined( HAVE_SCANDIR )
int vlc_alphasort( const struct dirent **a, const struct dirent **b )
{
    return strcoll( (*a)->d_name, (*b)->d_name );
}

int vlc_scandir( const char *name, struct dirent ***namelist,
                    int (*filter) ( const struct dirent * ),
                    int (*compar) ( const struct dirent **,
                                    const struct dirent ** ) )
{
    DIR            * p_dir;
    struct dirent  * p_content;
    struct dirent ** pp_list;
    int              ret, size;

    if( !namelist || !( p_dir = vlc_opendir_wrapper( name ) ) ) return -1;

    ret     = 0;
    pp_list = NULL;
    while( ( p_content = vlc_readdir_wrapper( p_dir ) ) )
    {
        if( filter && !filter( p_content ) )
        {
            continue;
        }
        pp_list = realloc( pp_list, ( ret + 1 ) * sizeof( struct dirent * ) );
        size = sizeof( struct dirent ) + strlen( p_content->d_name ) + 1;
        pp_list[ret] = malloc( size );
        memcpy( pp_list[ret], p_content, size );
        ret++;
    }

    vlc_closedir_wrapper( p_dir );

    if( compar )
    {
        qsort( pp_list, ret, sizeof( struct dirent * ),
               (int (*)(const void *, const void *)) compar );
    }

    *namelist = pp_list;
    return ret;
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
    int i_bytes;

    if (inbytesleft == NULL || outbytesleft == NULL)
    {
        return 0;
    }

    i_bytes = __MIN(*inbytesleft, *outbytesleft);
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
vlc_bool_t vlc_ureduce( unsigned *pi_dst_nom, unsigned *pi_dst_den,
                        uint64_t i_nom, uint64_t i_den, uint64_t i_max )
{
    vlc_bool_t b_exact = 1;
    uint64_t i_gcd;

    if( i_den == 0 )
    {
        *pi_dst_nom = 0;
        *pi_dst_den = 1;
        return 1;
    }

    i_gcd = GCD( i_nom, i_den );
    i_nom /= i_gcd;
    i_den /= i_gcd;

    if( i_max == 0 ) i_max = I64C(0xFFFFFFFF);

    if( i_nom > i_max || i_den > i_max )
    {
        uint64_t i_a0_num = 0, i_a0_den = 1, i_a1_num = 1, i_a1_den = 0;
        b_exact = 0;

        for( ; ; )
        {
            uint64_t i_x = i_nom / i_den;
            uint64_t i_a2n = i_x * i_a1_num + i_a0_num;
            uint64_t i_a2d = i_x * i_a1_den + i_a0_den;

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

/*************************************************************************
 * vlc_execve: Execute an external program with a given environment,
 * wait until it finishes and return its standard output
 *************************************************************************/
int __vlc_execve( vlc_object_t *p_object, int i_argc, char **ppsz_argv,
                  char **ppsz_env, char *psz_cwd, char *p_in, int i_in,
                  char **pp_data, int *pi_data )
{
#ifdef HAVE_FORK
    int pi_stdin[2];
    int pi_stdout[2];
    pid_t i_child_pid;

    pipe( pi_stdin );
    pipe( pi_stdout );

    if ( (i_child_pid = fork()) == -1 )
    {
        msg_Err( p_object, "unable to fork (%s)", strerror(errno) );
        return -1;
    }

    if ( i_child_pid == 0 )
    {
        close(0);
        dup(pi_stdin[1]);
        close(pi_stdin[0]);

        close(1);
        dup(pi_stdout[1]);
        close(pi_stdout[0]);

        close(2);

        if ( psz_cwd != NULL )
            chdir( psz_cwd );
        execve( ppsz_argv[0], ppsz_argv, ppsz_env );
        exit(1);
    }

    close(pi_stdin[1]);
    close(pi_stdout[1]);
    if ( !i_in )
        close( pi_stdin[0] );

    *pi_data = 0;
    *pp_data = malloc( 1025 );  /* +1 for \0 */

    while ( !p_object->b_die )
    {
        int i_ret, i_status;
        fd_set readfds, writefds;
        struct timeval tv;

        FD_ZERO( &readfds );
        FD_ZERO( &writefds );
        FD_SET( pi_stdout[0], &readfds );
        if ( i_in )
            FD_SET( pi_stdin[0], &writefds );

        tv.tv_sec = 0;
        tv.tv_usec = 10000;
        
        i_ret = select( pi_stdin[0] > pi_stdout[0] ? pi_stdin[0] + 1 :
                        pi_stdout[0] + 1, &readfds, &writefds, NULL, &tv );
        if ( i_ret > 0 )
        {
            if ( FD_ISSET( pi_stdout[0], &readfds ) )
            {
                ssize_t i_read = read( pi_stdout[0], &(*pp_data)[*pi_data],
                                       1024 );
                if ( i_read > 0 )
                {
                    *pi_data += i_read;
                    *pp_data = realloc( *pp_data, *pi_data + 1025 );
                }
            }
            if ( FD_ISSET( pi_stdin[0], &writefds ) )
            {
                ssize_t i_write = write( pi_stdin[0], p_in, __MIN(i_in, 1024) );

                if ( i_write > 0 )
                {
                    p_in += i_write;
                    i_in -= i_write;
                }
                if ( !i_in )
                    close( pi_stdin[0] );
            }
        }

        if ( waitpid( i_child_pid, &i_status, WNOHANG ) == i_child_pid )
        {
            if ( WIFEXITED( i_status ) )
            {
                if ( WEXITSTATUS( i_status ) )
                {
                    msg_Warn( p_object,
                              "child %s returned with error code %d",
                              ppsz_argv[0], WEXITSTATUS( i_status ) );
                }
            }
            else
            {
                if ( WIFSIGNALED( i_status ) )
                {
                    msg_Warn( p_object,
                              "child %s quit on signal %d", ppsz_argv[0],
                              WTERMSIG( i_status ) );
                }
            }
            if ( i_in )
                close( pi_stdin[0] );
            close( pi_stdout[0] );
            break;
        }

        if ( i_ret < 0 && errno != EINTR )
        {
            msg_Warn( p_object, "select failed (%s)", strerror(errno) );
        }
    }

#elif defined( WIN32 ) && !defined( UNDER_CE )
    SECURITY_ATTRIBUTES saAttr; 
    PROCESS_INFORMATION piProcInfo; 
    STARTUPINFO siStartInfo;
    BOOL bFuncRetn = FALSE; 
    HANDLE hChildStdinRd, hChildStdinWr, hChildStdoutRd, hChildStdoutWr;
    DWORD i_status;
    char *psz_cmd, *p_env, *p;
    char **ppsz_parser;
    int i_size;

    /* Set the bInheritHandle flag so pipe handles are inherited. */
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
    saAttr.bInheritHandle = TRUE; 
    saAttr.lpSecurityDescriptor = NULL; 

    /* Create a pipe for the child process's STDOUT. */
    if ( !CreatePipe( &hChildStdoutRd, &hChildStdoutWr, &saAttr, 0 ) ) 
    {
        msg_Err( p_object, "stdout pipe creation failed" ); 
        return -1;
    }

    /* Ensure the read handle to the pipe for STDOUT is not inherited. */
    SetHandleInformation( hChildStdoutRd, HANDLE_FLAG_INHERIT, 0 );

    /* Create a pipe for the child process's STDIN. */
    if ( !CreatePipe( &hChildStdinRd, &hChildStdinWr, &saAttr, 0 ) ) 
    {
        msg_Err( p_object, "stdin pipe creation failed" ); 
        return -1;
    }

    /* Ensure the write handle to the pipe for STDIN is not inherited. */
    SetHandleInformation( hChildStdinWr, HANDLE_FLAG_INHERIT, 0 );

    /* Set up members of the PROCESS_INFORMATION structure. */
    ZeroMemory( &piProcInfo, sizeof(PROCESS_INFORMATION) );
 
    /* Set up members of the STARTUPINFO structure. */
    ZeroMemory( &siStartInfo, sizeof(STARTUPINFO) );
    siStartInfo.cb = sizeof(STARTUPINFO); 
    siStartInfo.hStdError = hChildStdoutWr;
    siStartInfo.hStdOutput = hChildStdoutWr;
    siStartInfo.hStdInput = hChildStdinRd;
    siStartInfo.wShowWindow = SW_HIDE;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;

    /* Set up the command line. */
    psz_cmd = malloc(32768);
    psz_cmd[0] = '\0';
    i_size = 32768;
    ppsz_parser = &ppsz_argv[0];
    while ( ppsz_parser[0] != NULL && i_size > 0 )
    {
        /* Protect the last argument with quotes ; the other arguments
         * are supposed to be already protected because they have been
         * passed as a command-line option. */
        if ( ppsz_parser[1] == NULL )
        {
            strncat( psz_cmd, "\"", i_size );
            i_size--;
        }
        strncat( psz_cmd, *ppsz_parser, i_size );
        i_size -= strlen( *ppsz_parser );
        if ( ppsz_parser[1] == NULL )
        {
            strncat( psz_cmd, "\"", i_size );
            i_size--;
        }
        strncat( psz_cmd, " ", i_size );
        i_size--;
        ppsz_parser++;
    }

    /* Set up the environment. */
    p = p_env = malloc(32768);
    i_size = 32768;
    ppsz_parser = &ppsz_env[0];
    while ( *ppsz_parser != NULL && i_size > 0 )
    {
        memcpy( p, *ppsz_parser,
                __MIN((int)(strlen(*ppsz_parser) + 1), i_size) );
        p += strlen(*ppsz_parser) + 1;
        i_size -= strlen(*ppsz_parser) + 1;
        ppsz_parser++;
    }
    *p = '\0';
 
    /* Create the child process. */
    bFuncRetn = CreateProcess( NULL,
          psz_cmd,       // command line 
          NULL,          // process security attributes 
          NULL,          // primary thread security attributes 
          TRUE,          // handles are inherited 
          0,             // creation flags 
          p_env,
          psz_cwd,
          &siStartInfo,  // STARTUPINFO pointer 
          &piProcInfo ); // receives PROCESS_INFORMATION 

    free( psz_cmd );
    free( p_env );
   
    if ( bFuncRetn == 0 ) 
    {
        msg_Err( p_object, "child creation failed" ); 
        return -1;
    }

    /* Read from a file and write its contents to a pipe. */
    while ( i_in > 0 && !p_object->b_die )
    {
        DWORD i_written;
        if ( !WriteFile( hChildStdinWr, p_in, i_in, &i_written, NULL ) )
            break;
        i_in -= i_written;
        p_in += i_written;
    }

    /* Close the pipe handle so the child process stops reading. */
    CloseHandle(hChildStdinWr);

    /* Close the write end of the pipe before reading from the
     * read end of the pipe. */
    CloseHandle(hChildStdoutWr);
 
    /* Read output from the child process. */
    *pi_data = 0;
    *pp_data = malloc( 1025 );  /* +1 for \0 */

    while ( !p_object->b_die )
    {
        DWORD i_read;
        if ( !ReadFile( hChildStdoutRd, &(*pp_data)[*pi_data], 1024, &i_read, 
                        NULL )
              || i_read == 0 )
            break; 
        *pi_data += i_read;
        *pp_data = realloc( *pp_data, *pi_data + 1025 );
    }

    while ( !p_object->b_die
             && !GetExitCodeProcess( piProcInfo.hProcess, &i_status )
             && i_status != STILL_ACTIVE )
        msleep( 10000 );

    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);

    if ( i_status )
        msg_Warn( p_object,
                  "child %s returned with error code %ld",
                  ppsz_argv[0], i_status );

#else
    msg_Err( p_object, "vlc_execve called but no implementation is available" );
    return -1;

#endif

    (*pp_data)[*pi_data] = '\0';

    return 0;
}
