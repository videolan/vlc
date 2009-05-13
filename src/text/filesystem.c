/*****************************************************************************
 * filesystem.c: File system helpers
 *****************************************************************************
 * Copyright (C) 2005-2006 the VideoLAN team
 * Copyright © 2005-2008 Rémi Denis-Courmont
 *
 * Authors: Rémi Denis-Courmont <rem # videolan.org>
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
#include <vlc_charset.h>
#include "libvlc.h" /* utf8_mkdir */
#include <vlc_rand.h>

#include <assert.h>

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#ifdef HAVE_DIRENT_H
#  include <dirent.h>
#endif
#ifdef UNDER_CE
#  include <tchar.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifdef WIN32
# include <io.h>
# ifndef UNDER_CE
#  include <direct.h>
# endif
#else
# include <unistd.h>
#endif

#ifndef HAVE_LSTAT
# define lstat( a, b ) stat(a, b)
#endif

#ifdef WIN32
static int convert_path (const char *restrict path, wchar_t *restrict wpath)
{
    if (!MultiByteToWideChar (CP_UTF8, 0, path, -1, wpath, MAX_PATH))
    {
        errno = ENOENT;
        return -1;
    }
    wpath[MAX_PATH] = L'\0';
    return 0;
}
# define CONVERT_PATH(path, wpath, err) \
  wchar_t wpath[MAX_PATH+1]; \
  if (convert_path (path, wpath)) \
      return (err)
#endif

/**
 * Opens a system file handle.
 *
 * @param filename file path to open (with UTF-8 encoding)
 * @param flags open() flags, see the C library open() documentation
 * @param mode file permissions if creating a new file
 * @return a file handle on success, -1 on error (see errno).
 */
int utf8_open (const char *filename, int flags, mode_t mode)
{
#ifdef UNDER_CE
    /*_open translates to wchar internally on WinCE*/
    return _open (filename, flags, mode);
#elif defined (WIN32)
    /*
     * open() cannot open files with non-“ANSI” characters on Windows.
     * We use _wopen() instead. Same thing for mkdir() and stat().
     */
    CONVERT_PATH(filename, wpath, -1);
    return _wopen (wpath, flags, mode);

#endif
    const char *local_name = ToLocale (filename);

    if (local_name == NULL)
    {
        errno = ENOENT;
        return -1;
    }

    int fd;

#ifdef O_CLOEXEC
    fd = open (local_name, flags | O_CLOEXEC, mode);
    if (fd == -1 && errno == EINVAL)
#endif
    {
        fd = open (local_name, flags, mode);
#ifdef HAVE_FCNTL
        if (fd != -1)
        {
            int flags = fcntl (fd, F_GETFD);
            fcntl (fd, F_SETFD, FD_CLOEXEC | ((flags != -1) ? flags : 0));
        }
#endif
    }

    LocaleFree (local_name);
    return fd;
}

/**
 * Opens a FILE pointer.
 * @param filename file path, using UTF-8 encoding
 * @param mode fopen file open mode
 * @return NULL on error, an open FILE pointer on success.
 */
FILE *utf8_fopen (const char *filename, const char *mode)
{
    int rwflags = 0, oflags = 0;
    bool append = false;

    for (const char *ptr = mode; *ptr; ptr++)
    {
        switch (*ptr)
        {
            case 'r':
                rwflags = O_RDONLY;
                break;

            case 'a':
                rwflags = O_WRONLY;
                oflags |= O_CREAT;
                append = true;
                break;

            case 'w':
                rwflags = O_WRONLY;
                oflags |= O_CREAT | O_TRUNC;
                break;

            case '+':
                rwflags = O_RDWR;
                break;

#ifdef O_TEXT
            case 't':
                oflags |= O_TEXT;
                break;
#endif
        }
    }

    int fd = utf8_open (filename, rwflags | oflags, 0666);
    if (fd == -1)
        return NULL;

    if (append && (lseek (fd, 0, SEEK_END) == -1))
    {
        close (fd);
        return NULL;
    }

    FILE *stream = fdopen (fd, mode);
    if (stream == NULL)
        close (fd);

    return stream;
}

/**
 * Creates a directory using UTF-8 paths.
 *
 * @param dirname a UTF-8 string with the name of the directory that you
 *        want to create.
 * @param mode directory permissions
 * @return 0 on success, -1 on error (see errno).
 */
int utf8_mkdir( const char *dirname, mode_t mode )
{
#if defined (UNDER_CE)
    (void) mode;
    /* mkdir converts internally to wchar */
    return _mkdir(dirname);
#elif defined (WIN32)
    (void) mode;
    CONVERT_PATH (dirname, wpath, -1);
    return _wmkdir (wpath);

#else
    char *locname = ToLocale( dirname );
    int res;

    if( locname == NULL )
    {
        errno = ENOENT;
        return -1;
    }
    res = mkdir( locname, mode );

    LocaleFree( locname );
    return res;
#endif
}

/**
 * Opens a DIR pointer.
 *
 * @param dirname UTF-8 representation of the directory name
 * @return a pointer to the DIR struct, or NULL in case of error.
 * Release with standard closedir().
 */
DIR *utf8_opendir( const char *dirname )
{
#ifdef WIN32
    CONVERT_PATH (dirname, wpath, NULL);
    return (DIR *)vlc_wopendir (wpath);

#else
    const char *local_name = ToLocale( dirname );

    if( local_name != NULL )
    {
        DIR *dir = opendir( local_name );
        LocaleFree( local_name );
        return dir;
    }
#endif

    errno = ENOENT;
    return NULL;
}

/**
 * Reads the next file name from an open directory.
 *
 * @param dir The directory that is being read
 *
 * @return a UTF-8 string of the directory entry.
 * Use free() to free this memory.
 */
char *utf8_readdir( DIR *dir )
{
#ifdef WIN32
    struct _wdirent *ent = vlc_wreaddir (dir);
    if (ent == NULL)
        return NULL;

    return FromWide (ent->d_name);
#else
    struct dirent *ent;

    ent = readdir( (DIR *)dir );
    if( ent == NULL )
        return NULL;

    return vlc_fix_readdir( ent->d_name );
#endif
}

static int dummy_select( const char *str )
{
    (void)str;
    return 1;
}

/**
 * Does the same as utf8_scandir(), but takes an open directory pointer
 * instead of a directory path.
 */
int utf8_loaddir( DIR *dir, char ***namelist,
                  int (*select)( const char * ),
                  int (*compar)( const char **, const char ** ) )
{
    if( select == NULL )
        select = dummy_select;

    if( dir == NULL )
        return -1;
    else
    {
        char **tab = NULL;
        char *entry;
        unsigned num = 0;

        rewinddir( dir );

        while( ( entry = utf8_readdir( dir ) ) != NULL )
        {
            char **newtab;

            if( !select( entry ) )
            {
                free( entry );
                continue;
            }

            newtab = realloc( tab, sizeof( char * ) * (num + 1) );
            if( newtab == NULL )
            {
                free( entry );
                goto error;
            }
            tab = newtab;
            tab[num++] = entry;
        }

        if( compar != NULL )
            qsort( tab, num, sizeof( tab[0] ),
                   (int (*)( const void *, const void *))compar );

        *namelist = tab;
        return num;

    error:{
        unsigned i;

        for( i = 0; i < num; i++ )
            free( tab[i] );
        if( tab != NULL )
            free( tab );
        }
    }
    return -1;
}

/**
 * Selects file entries from a directory, as GNU C scandir().
 *
 * @param dirname UTF-8 diretory path
 * @param pointer [OUT] pointer set, on succesful completion, to the address
 * of a table of UTF-8 filenames. All filenames must be freed with free().
 * The table itself must be freed with free() as well.
 *
 * @return How many file names were selected (possibly 0),
 * or -1 in case of error.
 */
int utf8_scandir( const char *dirname, char ***namelist,
                  int (*select)( const char * ),
                  int (*compar)( const char **, const char ** ) )
{
    DIR *dir = utf8_opendir (dirname);
    int val = -1;

    if (dir != NULL)
    {
        val = utf8_loaddir (dir, namelist, select, compar);
        closedir (dir);
    }
    return val;
}

static int utf8_statEx( const char *filename, struct stat *buf,
                        bool deref )
{
#ifdef UNDER_CE
    /*_stat translates to wchar internally on WinCE*/
    return _stat( filename, buf );
#elif defined (WIN32)
    CONVERT_PATH (filename, wpath, -1);
    return _wstati64 (wpath, buf);

#endif
#ifdef HAVE_SYS_STAT_H
    const char *local_name = ToLocale( filename );

    if( local_name != NULL )
    {
        int res = deref ? stat( local_name, buf )
                       : lstat( local_name, buf );
        LocaleFree( local_name );
        return res;
    }
    errno = ENOENT;
#endif
    return -1;
}

/**
 * Finds file/inode informations, as stat().
 * Consider using fstat() instead, if possible.
 *
 * @param filename UTF-8 file path
 */
int utf8_stat( const char *filename, struct stat *buf)
{
    return utf8_statEx( filename, buf, true );
}

/**
 * Finds file/inode informations, as lstat().
 * Consider using fstat() instead, if possible.
 *
 * @param filename UTF-8 file path
 */
int utf8_lstat( const char *filename, struct stat *buf)
{
    return utf8_statEx( filename, buf, false );
}

/**
 * Removes a file.
 *
 * @param filename a UTF-8 string with the name of the file you want to delete.
 * @return A 0 return value indicates success. A -1 return value indicates an
 *        error, and an error code is stored in errno
 */
int utf8_unlink( const char *filename )
{
#ifdef UNDER_CE
    /*_open translates to wchar internally on WinCE*/
    return _unlink( filename );
#elif defined (WIN32)
    CONVERT_PATH (filename, wpath, -1);
    return _wunlink (wpath);

#endif
    const char *local_name = ToLocale( filename );

    if( local_name == NULL )
    {
        errno = ENOENT;
        return -1;
    }

    int ret = unlink( local_name );
    LocaleFree( local_name );
    return ret;
}

/**
 * Moves a file atomically. This only works within a single file system.
 *
 * @param oldpath path to the file before the move
 * @param newpath intended path to the file after the move
 * @return A 0 return value indicates success. A -1 return value indicates an
 *        error, and an error code is stored in errno
 */
int utf8_rename (const char *oldpath, const char *newpath)
{
#if defined (WIN32)
    CONVERT_PATH (oldpath, wold, -1);
    CONVERT_PATH (newpath, wnew, -1);
# ifdef UNDER_CE
    /* FIXME: errno support */
    if (MoveFileW (wold, wnew))
        return 0;
    else
        return -1;
#else
    return _wrename (wold, wnew);
#endif

#endif
    const char *lo = ToLocale (oldpath);
    if (lo == NULL)
        goto error;

    const char *ln = ToLocale (newpath);
    if (ln == NULL)
    {
        LocaleFree (lo);
error:
        errno = ENOENT;
        return -1;
    }

    int ret = rename (lo, ln);
    LocaleFree (lo);
    LocaleFree (ln);
    return ret;
}

int utf8_mkstemp( char *template )
{
    static const char digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static const int i_digits = sizeof(digits)/sizeof(*digits) - 1;

    /* */
    assert( template );

    /* Check template validity */
    const size_t i_length = strlen( template );
    char *psz_rand = &template[i_length-6];

    if( i_length < 6 || strcmp( psz_rand, "XXXXXX" ) )
    {
        errno = EINVAL;
        return -1;
    }

    /* */
    for( int i = 0; i < 256; i++ )
    {
        /* Create a pseudo random file name */
        uint8_t pi_rand[6];

        vlc_rand_bytes( pi_rand, sizeof(pi_rand) );
        for( int j = 0; j < 6; j++ )
            psz_rand[j] = digits[pi_rand[j] % i_digits];

        /* */
        int fd = utf8_open( template, O_CREAT | O_EXCL | O_RDWR, 0600 );
        if( fd >= 0 )
            return fd;
        if( errno != EEXIST )
            return -1;
    }

    errno = EEXIST;
    return -1;
}

