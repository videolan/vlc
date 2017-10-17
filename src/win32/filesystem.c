/*****************************************************************************
 * filesystem.c: Windows file system helpers
 *****************************************************************************
 * Copyright (C) 2005-2006 VLC authors and VideoLAN
 * Copyright © 2005-2008 Rémi Denis-Courmont
 *
 * Authors: Rémi Denis-Courmont <rem # videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <winsock2.h>
#include <direct.h>

#include <vlc_common.h>
#include <vlc_charset.h>
#include <vlc_fs.h>
#include "libvlc.h" /* vlc_mkdir */

#ifdef _MSC_VER
# define __STDC__ 1
# include <io.h> /* _pipe */
#endif

static wchar_t *widen_path (const char *path)
{
    wchar_t *wpath;

    errno = 0;
    wpath = ToWide (path);
    if (wpath == NULL)
    {
        if (errno == 0)
            errno = ENOENT;
        return NULL;
    }
    return wpath;
}

#define CONVERT_PATH(path, wpath, err) \
    wchar_t *wpath = wide_path(path); \
    if (wpath == NULL) return (err)


int vlc_open (const char *filename, int flags, ...)
{
    int mode = 0;
    va_list ap;

    flags |= O_NOINHERIT; /* O_CLOEXEC */
    /* Defaults to binary mode */
    if ((flags & O_TEXT) == 0)
        flags |= O_BINARY;

    va_start (ap, flags);
    if (flags & O_CREAT)
    {
        int unixmode = va_arg(ap, int);
        if (unixmode & 0444)
            mode |= _S_IREAD;
        if (unixmode & 0222)
            mode |= _S_IWRITE;
    }
    va_end (ap);

    /*
     * open() cannot open files with non-“ANSI” characters on Windows.
     * We use _wopen() instead. Same thing for mkdir() and stat().
     */
    wchar_t *wpath = widen_path (filename);
    if (wpath == NULL)
        return -1;

    int fd = _wopen (wpath, flags, mode);
    free (wpath);
    return fd;
}

int vlc_openat (int dir, const char *filename, int flags, ...)
{
    (void) dir; (void) filename; (void) flags;
    errno = ENOSYS;
    return -1;
}

int vlc_memfd (void)
{
#if 0
    int fd, err;

    FILE *stream = tmpfile();
    if (stream == NULL)
        return -1;

    fd = vlc_dup(fileno(stream));
    err = errno;
    fclose(stream);
    errno = err;
    return fd;
#else /* Not currently used */
    errno = ENOSYS;
    return -1;
#endif
}

int vlc_close (int fd)
{
    return close (fd);
}

int vlc_mkdir( const char *dirname, mode_t mode )
{
    wchar_t *wpath = widen_path (dirname);
    if (wpath == NULL)
        return -1;

    int ret = _wmkdir (wpath);
    free (wpath);
    (void) mode;
    return ret;
}

char *vlc_getcwd (void)
{
#if VLC_WINSTORE_APP
    return NULL;
#else
    wchar_t *wdir = _wgetcwd (NULL, 0);
    if (wdir == NULL)
        return NULL;

    char *dir = FromWide (wdir);
    free (wdir);
    return dir;
#endif
}

/* Under Windows, these wrappers return the list of drive letters
 * when called with an empty argument or just '\'. */
DIR *vlc_opendir (const char *dirname)
{
    wchar_t *wpath = widen_path (dirname);
    if (wpath == NULL)
        return NULL;

    vlc_DIR *p_dir = malloc (sizeof (*p_dir));
    if (unlikely(p_dir == NULL))
    {
        free(wpath);
        return NULL;
    }

#if !VLC_WINSTORE_APP
    /* Special mode to list drive letters */
    if (wpath[0] == L'\0' || (wcscmp (wpath, L"\\") == 0))
    {
        free (wpath);
        p_dir->wdir = NULL;
        p_dir->u.drives = GetLogicalDrives ();
        p_dir->entry = NULL;
        return (void *)p_dir;
    }
#endif

    assert (wpath[0]); // wpath[1] is defined
    p_dir->u.insert_dot_dot = !wcscmp (wpath + 1, L":\\");

    _WDIR *wdir = _wopendir (wpath);
    free (wpath);
    if (wdir == NULL)
    {
        free (p_dir);
        return NULL;
    }
    p_dir->wdir = wdir;
    p_dir->entry = NULL;
    return (void *)p_dir;
}

const char *vlc_readdir (DIR *dir)
{
    vlc_DIR *p_dir = (vlc_DIR *)dir;

    free(p_dir->entry);

#if !VLC_WINSTORE_APP
    /* Drive letters mode */
    if (p_dir->wdir == NULL)
    {
        DWORD drives = p_dir->u.drives;
        if (drives == 0)
        {
            p_dir->entry = NULL;
            return NULL; /* end */
        }

        unsigned int i;
        for (i = 0; !(drives & 1); i++)
            drives >>= 1;
        p_dir->u.drives &= ~(1UL << i);
        assert (i < 26);

        if (asprintf (&p_dir->entry, "%c:\\", 'A' + i) == -1)
            p_dir->entry = NULL;
    }
    else
#endif
    if (p_dir->u.insert_dot_dot)
    {
        /* Adds "..", gruik! */
        p_dir->u.insert_dot_dot = false;
        p_dir->entry = strdup ("..");
    }
    else
    {
        struct _wdirent *ent = _wreaddir (p_dir->wdir);
        p_dir->entry = (ent != NULL) ? FromWide (ent->d_name) : NULL;
    }
    return p_dir->entry;
}

int vlc_stat (const char *filename, struct stat *buf)
{
    wchar_t *wpath = widen_path (filename);
    if (wpath == NULL)
        return -1;

    static_assert (sizeof (*buf) == sizeof (struct _stati64),
                   "Mismatched struct stat definition.");

    int ret = _wstati64 (wpath, buf);
    free (wpath);
    return ret;
}

int vlc_lstat (const char *filename, struct stat *buf)
{
    return vlc_stat (filename, buf);
}

int vlc_unlink (const char *filename)
{
    wchar_t *wpath = widen_path (filename);
    if (wpath == NULL)
        return -1;

    int ret = _wunlink (wpath);
    free (wpath);
    return ret;
}

int vlc_rename (const char *oldpath, const char *newpath)
{
    int ret = -1;

    wchar_t *wold = widen_path (oldpath), *wnew = widen_path (newpath);
    if (wold == NULL || wnew == NULL)
        goto out;

    if (_wrename (wold, wnew) && (errno == EACCES || errno == EEXIST))
    {   /* Windows does not allow atomic file replacement */
        if (_wremove (wnew))
        {
            errno = EACCES; /* restore errno */
            goto out;
        }
        if (_wrename (wold, wnew))
            goto out;
    }
    ret = 0;
out:
    free (wnew);
    free (wold);
    return ret;
}

int vlc_dup (int oldfd)
{
    int fd = dup (oldfd);
    if (fd != -1)
        setmode (fd, O_BINARY);
    return fd;
}

int vlc_pipe (int fds[2])
{
#if VLC_WINSTORE_APP
    _set_errno(EPERM);
    return -1;
#else
    return _pipe (fds, 32768, O_NOINHERIT | O_BINARY);
#endif
}

ssize_t vlc_write(int fd, const void *buf, size_t len)
{
    return write(fd, buf, len);
}

ssize_t vlc_writev(int fd, const struct iovec *iov, int count)
{
    vlc_assert_unreachable();
}

#include <vlc_network.h>

int vlc_socket (int pf, int type, int proto, bool nonblock)
{
    int fd = socket (pf, type, proto);
    if (fd == -1)
        return -1;

    if (nonblock)
        ioctlsocket (fd, FIONBIO, &(unsigned long){ 1 });
    return fd;
}

int vlc_socketpair(int pf, int type, int proto, int fds[2], bool nonblock)
{
    (void) pf; (void) type; (void) proto; (void) fds; (void) nonblock;
    errno = ENOSYS;
    return -1;
}

int vlc_accept (int lfd, struct sockaddr *addr, socklen_t *alen, bool nonblock)
{
    int fd = accept (lfd, addr, alen);
    if (fd != -1 && nonblock)
        ioctlsocket (fd, FIONBIO, &(unsigned long){ 1 });
    return fd;
}

#if !VLC_WINSTORE_APP
FILE *vlc_win32_tmpfile(void)
{
    TCHAR tmp_path[MAX_PATH-14];
    int i_ret = GetTempPath (MAX_PATH-14, tmp_path);
    if (i_ret == 0)
        return NULL;

    TCHAR tmp_name[MAX_PATH];
    i_ret = GetTempFileName(tmp_path, TEXT("VLC"), 0, tmp_name);
    if (i_ret == 0)
        return NULL;

    HANDLE hFile = CreateFile(tmp_name,
            GENERIC_READ | GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS,
            FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return NULL;

    int fd = _open_osfhandle((intptr_t)hFile, 0);
    if (fd == -1) {
        CloseHandle(hFile);
        return NULL;
    }

    FILE *stream = _fdopen(fd, "w+b");
    if (stream == NULL) {
        _close(fd);
        return NULL;
    }
    return stream;
}
#else
FILE *vlc_win32_tmpfile(void)
{
    return NULL;
}
#endif

