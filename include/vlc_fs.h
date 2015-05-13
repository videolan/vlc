/*****************************************************************************
 * vlc_fs.h: File system helpers
 *****************************************************************************
 * Copyright © 2006-2010 Rémi Denis-Courmont
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

#ifndef VLC_FS_H
#define VLC_FS_H 1

/**
 * \file
 * The functions in this file help with using low-level Unix-style file
 * descriptors, BSD sockets and directories. In general, they retain the
 * prototype and most semantics from their respective standard equivalents.
 * However, there are a few differences:
 *  - On Windows, file path arguments are expected in UTF-8 format.
 *    They are converted to UTF-16 internally, thus enabling access to paths
 *    outside of the local Windows ANSI code page.
 *  - On POSIX systems, file descriptors are created with the close-on-exec
 *    flag set (atomically where possible), so that they do not leak to
 *    child process after fork-and-exec.
 *  - vlc_scandir(), inspired by GNU scandir(), passes file names rather than
 *    dirent structure pointers to its callbacks.
 *  - vlc_accept() takes an extra boolean for nonblocking mode (compare with
 *    the flags parameter in POSIX.next accept4()).
 *  - Writing functions do not emit a SIGPIPE signal in case of broken pipe.
 */

#include <sys/types.h>
#include <dirent.h>

VLC_API int vlc_open( const char *filename, int flags, ... ) VLC_USED;
VLC_API FILE * vlc_fopen( const char *filename, const char *mode ) VLC_USED;
VLC_API int vlc_openat( int fd, const char *filename, int flags, ... ) VLC_USED;

VLC_API DIR * vlc_opendir( const char *dirname ) VLC_USED;
VLC_API char * vlc_readdir( DIR *dir ) VLC_USED;
VLC_API int vlc_loaddir( DIR *dir, char ***namelist, int (*select)( const char * ), int (*compar)( const char **, const char ** ) );
VLC_API int vlc_scandir( const char *dirname, char ***namelist, int (*select)( const char * ), int (*compar)( const char **, const char ** ) );
VLC_API int vlc_mkdir( const char *filename, mode_t mode );

VLC_API int vlc_unlink( const char *filename );
VLC_API int vlc_rename( const char *oldpath, const char *newpath );
VLC_API char *vlc_getcwd( void ) VLC_USED;

#if defined( _WIN32 )
typedef struct vlc_DIR
{
    _WDIR *wdir; /* MUST be first, see <vlc_fs.h> */
    char *entry;
    union
    {
        DWORD drives;
        bool insert_dot_dot;
    } u;
} vlc_DIR;

static inline int vlc_closedir( DIR *dir )
{
    vlc_DIR *vdir = (vlc_DIR *)dir;
    _WDIR *wdir = vdir->wdir;

    free( vdir->entry );
    free( vdir );
    return (wdir != NULL) ? _wclosedir( wdir ) : 0;
}
# undef closedir
# define closedir vlc_closedir

static inline void vlc_rewinddir( DIR *dir )
{
    _WDIR *wdir = *(_WDIR **)dir;

    _wrewinddir( wdir );
}
# undef rewinddir
# define rewinddir vlc_rewinddir

# include <sys/stat.h>
# ifndef stat
#  define stat _stati64
# endif
# ifndef fstat
#  define fstat _fstati64
# endif
# ifndef _MSC_VER
#  undef lseek
#  define lseek _lseeki64
# endif
#endif

#ifdef __ANDROID__
# define lseek lseek64
#endif

struct stat;
struct iovec;

VLC_API int vlc_stat( const char *filename, struct stat *buf );
VLC_API int vlc_lstat( const char *filename, struct stat *buf );

VLC_API int vlc_mkstemp( char * );

VLC_API int vlc_dup( int );
VLC_API int vlc_pipe( int[2] );
VLC_API ssize_t vlc_write( int, const void *, size_t );
VLC_API ssize_t vlc_writev( int, const struct iovec *, int );
#endif
