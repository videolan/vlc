/*****************************************************************************
 * libc.c: Extra libc function for some systems.
 *****************************************************************************
 * Copyright (C) 2002-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Rémi Denis-Courmont <rem à videolan.org>
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include <ctype.h>


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

#ifdef HAVE_SIGNAL_H
#   include <signal.h>
#endif

#ifdef HAVE_FORK
#   include <sys/time.h>
#   include <unistd.h>
#   include <errno.h>
#   include <sys/wait.h>
#   include <fcntl.h>
#   include <sys/socket.h>
#   include <sys/poll.h>
#endif

#if defined(WIN32) || defined(UNDER_CE)
#   undef _wopendir
#   undef _wreaddir
#   undef _wclosedir
#   undef rewinddir
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#endif

/******************************************************************************
 * strcasestr: find a substring (little) in another substring (big)
 * Case sensitive. Return NULL if not found, return big if little == null
 *****************************************************************************/
char * vlc_strcasestr( const char *psz_big, const char *psz_little )
{
#if defined (HAVE_STRCASESTR) || defined (HAVE_STRISTR)
    return strcasestr (psz_big, psz_little);
#else
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
#endif
}

/*****************************************************************************
 * strtoll: convert a string to a 64 bits int.
 *****************************************************************************/
long long vlc_strtoll( const char *nptr, char **endptr, int base )
{
#if defined( HAVE_STRTOLL )
    return strtoll( nptr, endptr, base );
#else
    long long i_value = 0;
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
#endif
}

/**
 * Copy a string to a sized buffer. The result is always nul-terminated
 * (contrary to strncpy()).
 *
 * @param dest destination buffer
 * @param src string to be copied
 * @param len maximum number of characters to be copied plus one for the
 * terminating nul.
 *
 * @return strlen(src)
 */
extern size_t vlc_strlcpy (char *tgt, const char *src, size_t bufsize)
{
#ifdef HAVE_STRLCPY
    return strlcpy (tgt, src, bufsize);
#else
    size_t length;

    for (length = 1; (length < bufsize) && *src; length++)
        *tgt++ = *src++;

    if (bufsize)
        *tgt = '\0';

    while (*src++)
        length++;

    return length - 1;
#endif
}

/*****************************************************************************
 * vlc_*dir_wrapper: wrapper under Windows to return the list of drive letters
 * when called with an empty argument or just '\'
 *****************************************************************************/
#if defined(WIN32)
#   include <assert.h>

typedef struct vlc_DIR
{
    _WDIR *p_real_dir;
    int i_drives;
    struct _wdirent dd_dir;
    bool b_insert_back;
} vlc_DIR;

void *vlc_wopendir( const wchar_t *wpath )
{
    vlc_DIR *p_dir = NULL;
    _WDIR *p_real_dir = NULL;

    if ( wpath == NULL || wpath[0] == '\0'
          || (wcscmp (wpath, L"\\") == 0) )
    {
        /* Special mode to list drive letters */
        p_dir = malloc( sizeof(vlc_DIR) );
        if( !p_dir )
            return NULL;
        p_dir->p_real_dir = NULL;
#if defined(UNDER_CE)
        p_dir->i_drives = NULL;
#else
        p_dir->i_drives = GetLogicalDrives();
#endif
        return (void *)p_dir;
    }

    p_real_dir = _wopendir( wpath );
    if ( p_real_dir == NULL )
        return NULL;

    p_dir = malloc( sizeof(vlc_DIR) );
    if( !p_dir )
    {
        _wclosedir( p_real_dir );
        return NULL;
    }
    p_dir->p_real_dir = p_real_dir;

    assert (wpath[0]); // wpath[1] is defined
    p_dir->b_insert_back = !wcscmp (wpath + 1, L":\\");
    return (void *)p_dir;
}

struct _wdirent *vlc_wreaddir( void *_p_dir )
{
    vlc_DIR *p_dir = (vlc_DIR *)_p_dir;
    DWORD i_drives;

    if ( p_dir->p_real_dir != NULL )
    {
        if ( p_dir->b_insert_back )
        {
            /* Adds "..", gruik! */
            p_dir->dd_dir.d_ino = 0;
            p_dir->dd_dir.d_reclen = 0;
            p_dir->dd_dir.d_namlen = 2;
            wcscpy( p_dir->dd_dir.d_name, L".." );
            p_dir->b_insert_back = false;
            return &p_dir->dd_dir;
        }

        return _wreaddir( p_dir->p_real_dir );
    }

    /* Drive letters mode */
    i_drives = p_dir->i_drives;
#ifdef UNDER_CE
    swprintf( p_dir->dd_dir.d_name, L"\\");
    p_dir->dd_dir.d_namlen = wcslen(p_dir->dd_dir.d_name);
#else
    unsigned int i;
    if ( !i_drives )
        return NULL; /* end */

    for ( i = 0; i < sizeof(DWORD)*8; i++, i_drives >>= 1 )
        if ( i_drives & 1 ) break;

    if ( i >= 26 )
        return NULL; /* this should not happen */

    swprintf( p_dir->dd_dir.d_name, L"%c:\\", 'A' + i );
    p_dir->dd_dir.d_namlen = wcslen(p_dir->dd_dir.d_name);
    p_dir->i_drives &= ~(1UL << i);
#endif
    return &p_dir->dd_dir;
}

void vlc_rewinddir( void *_p_dir )
{
    vlc_DIR *p_dir = (vlc_DIR *)_p_dir;

    if ( p_dir->p_real_dir != NULL )
        _wrewinddir( p_dir->p_real_dir );
}
#endif

/* This one is in the libvlccore exported symbol list */
int vlc_wclosedir( void *_p_dir )
{
#if defined(WIN32)
    vlc_DIR *p_dir = (vlc_DIR *)_p_dir;
    int i_ret = 0;

    if ( p_dir->p_real_dir != NULL )
        i_ret = _wclosedir( p_dir->p_real_dir );

    free( p_dir );
    return i_ret;
#else
    return closedir( _p_dir );
#endif
}

/**
 * In-tree plugins share their gettext domain with LibVLC.
 */
char *vlc_gettext( const char *msgid )
{
#ifdef ENABLE_NLS
    return dgettext( PACKAGE_NAME, msgid );
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
char *vlc_wraptext( const char *psz_text, int i_line )
{
    int i_len;
    char *psz_line, *psz_new_text;

    psz_line = psz_new_text = strdup( psz_text );

    i_len = count_utf8_string( psz_text );

    while( i_len > i_line )
    {
        /* Look if there is a newline somewhere. */
        char *psz_parser = psz_line;
        int i_count = 0;
        while( i_count <= i_line && *psz_parser != '\n' )
        {
            while( *((unsigned char *)psz_parser) >= 0x80UL ) psz_parser++;
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
            while( *((unsigned char *)psz_parser) >= 0x80UL ) psz_parser--;
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
            while( *((unsigned char *)psz_parser) >= 0x80UL ) psz_parser++;
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
    return (vlc_iconv_t)(-1);
#endif
}

size_t vlc_iconv( vlc_iconv_t cd, const char **inbuf, size_t *inbytesleft,
                  char **outbuf, size_t *outbytesleft )
{
#if defined(HAVE_ICONV)
    return iconv( cd, (ICONV_CONST char **)inbuf, inbytesleft,
                  outbuf, outbytesleft );
#else
    abort ();
#endif
}

int vlc_iconv_close( vlc_iconv_t cd )
{
#if defined(HAVE_ICONV)
    return iconv_close( cd );
#else
    abort ();
#endif
}

/*****************************************************************************
 * reduce a fraction
 *   (adapted from libavcodec, author Michael Niedermayer <michaelni@gmx.at>)
 *****************************************************************************/
bool vlc_ureduce( unsigned *pi_dst_nom, unsigned *pi_dst_den,
                        uint64_t i_nom, uint64_t i_den, uint64_t i_max )
{
    bool b_exact = 1;
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

    if( i_max == 0 ) i_max = INT64_C(0xFFFFFFFF);

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
 * vlc_execve: Execute an external program with a given environment,
 * wait until it finishes and return its standard output
 *************************************************************************/
int __vlc_execve( vlc_object_t *p_object, int i_argc, char *const *ppsz_argv,
                  char *const *ppsz_env, const char *psz_cwd,
                  const char *p_in, size_t i_in,
                  char **pp_data, size_t *pi_data )
{
    (void)i_argc; // <-- hmph
#ifdef HAVE_FORK
# define BUFSIZE 1024
    int fds[2], i_status;

    if (socketpair (AF_LOCAL, SOCK_STREAM, 0, fds))
        return -1;

    pid_t pid = -1;
    if ((fds[0] > 2) && (fds[1] > 2))
        pid = fork ();

    switch (pid)
    {
        case -1:
            msg_Err (p_object, "unable to fork (%m)");
            close (fds[0]);
            close (fds[1]);
            return -1;

        case 0:
        {
            sigset_t set;
            sigemptyset (&set);
            pthread_sigmask (SIG_SETMASK, &set, NULL);

            /* NOTE:
             * Like it or not, close can fail (and not only with EBADF)
             */
            if ((close (0) == 0) && (close (1) == 0) && (close (2) == 0)
             && (dup (fds[1]) == 0) && (dup (fds[1]) == 1)
             && (open ("/dev/null", O_RDONLY) == 2)
             && ((psz_cwd == NULL) || (chdir (psz_cwd) == 0)))
                execve (ppsz_argv[0], ppsz_argv, ppsz_env);

            exit (EXIT_FAILURE);
        }
    }

    close (fds[1]);

    *pi_data = 0;
    if (*pp_data)
        free (*pp_data);
    *pp_data = NULL;

    if (i_in == 0)
        shutdown (fds[0], SHUT_WR);

    while (!p_object->b_die)
    {
        struct pollfd ufd[1];
        memset (ufd, 0, sizeof (ufd));
        ufd[0].fd = fds[0];
        ufd[0].events = POLLIN;

        if (i_in > 0)
            ufd[0].events |= POLLOUT;

        if (poll (ufd, 1, 10) <= 0)
            continue;

        if (ufd[0].revents & ~POLLOUT)
        {
            char *ptr = realloc (*pp_data, *pi_data + BUFSIZE + 1);
            if (ptr == NULL)
                break; /* safely abort */

            *pp_data = ptr;

            ssize_t val = read (fds[0], ptr + *pi_data, BUFSIZE);
            switch (val)
            {
                case -1:
                case 0:
                    shutdown (fds[0], SHUT_RD);
                    break;

                default:
                    *pi_data += val;
            }
        }

        if (ufd[0].revents & POLLOUT)
        {
            ssize_t val = write (fds[0], p_in, i_in);
            switch (val)
            {
                case -1:
                case 0:
                    i_in = 0;
                    shutdown (fds[0], SHUT_WR);
                    break;

                default:
                    i_in -= val;
                    p_in += val;
            }
        }
    }

    close (fds[0]);

    while (waitpid (pid, &i_status, 0) == -1);

    if (WIFEXITED (i_status))
    {
        i_status = WEXITSTATUS (i_status);
        if (i_status)
            msg_Warn (p_object,  "child %s (PID %d) exited with error code %d",
                      ppsz_argv[0], (int)pid, i_status);
    }
    else
    if (WIFSIGNALED (i_status)) // <-- this should be redumdant a check
    {
        i_status = WTERMSIG (i_status);
        msg_Warn (p_object, "child %s (PID %d) exited on signal %d (%s)",
                  ppsz_argv[0], (int)pid, i_status, strsignal (i_status));
    }

#elif defined( WIN32 ) && !defined( UNDER_CE )
    SECURITY_ATTRIBUTES saAttr;
    PROCESS_INFORMATION piProcInfo;
    STARTUPINFO siStartInfo;
    BOOL bFuncRetn = FALSE;
    HANDLE hChildStdinRd, hChildStdinWr, hChildStdoutRd, hChildStdoutWr;
    DWORD i_status;
    char *psz_cmd = NULL, *p_env = NULL, *p = NULL;
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
    if( !psz_cmd )
        return -1;
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
    if( !p )
    {
        free( psz_cmd );
        return -1;
    }

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
    if( *pp_data )
        free( pp_data );
    *pp_data = NULL;
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

    if (*pp_data == NULL)
        return -1;

    (*pp_data)[*pi_data] = '\0';
    return 0;
}
