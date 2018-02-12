/*****************************************************************************
 * filesystem.c: POSIX file system helpers
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <stdio.h>
#include <limits.h> /* NAME_MAX */
#include <errno.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifndef HAVE_LSTAT
# define lstat(a, b) stat(a, b)
#endif
#include <dirent.h>
#include <sys/socket.h>
#ifndef O_TMPFILE
# define O_TMPFILE 0
#endif

#include <vlc_common.h>
#include <vlc_fs.h>

#if !defined(HAVE_ACCEPT4) || !defined HAVE_MKOSTEMP
static inline void vlc_cloexec(int fd)
{
    fcntl(fd, F_SETFD, FD_CLOEXEC | fcntl(fd, F_GETFD));
}
#endif

int vlc_open (const char *filename, int flags, ...)
{
    unsigned int mode = 0;
    va_list ap;

    va_start (ap, flags);
    if (flags & (O_CREAT|O_TMPFILE))
        mode = va_arg (ap, unsigned int);
    va_end (ap);

#ifdef O_CLOEXEC
    return open(filename, flags, mode | O_CLOEXEC);
#else
    int fd = open(filename, flags, mode);
    if (fd != -1)
        vlc_cloexec(fd);
    return -1;
#endif
}

int vlc_openat (int dir, const char *filename, int flags, ...)
{
    unsigned int mode = 0;
    va_list ap;

    va_start (ap, flags);
    if (flags & (O_CREAT|O_TMPFILE))
        mode = va_arg (ap, unsigned int);
    va_end (ap);

#ifdef HAVE_OPENAT
    return openat(dir, filename, flags, mode | O_CLOEXEC);
#else
    VLC_UNUSED (dir);
    VLC_UNUSED (filename);
    VLC_UNUSED (mode);
    errno = ENOSYS;
    return -1;
#endif
}

int vlc_mkstemp (char *template)
{
#if defined (HAVE_MKOSTEMP) && defined (O_CLOEXEC)
    return mkostemp(template, O_CLOEXEC);
#else
    int fd = mkstemp(template);
    if (fd != -1)
        vlc_cloexec(fd);
    return fd;
#endif
}

int vlc_memfd (void)
{
    int fd;
#if O_TMPFILE
    fd = vlc_open ("/tmp", O_RDWR|O_TMPFILE, S_IRUSR|S_IWUSR);
    if (fd != -1)
        return fd;
    /* ENOENT means either /tmp is missing (!) or the kernel does not support
     * O_TMPFILE. EISDIR means /tmp exists but the kernel does not support
     * O_TMPFILE. EOPNOTSUPP means the kernel supports O_TMPFILE but the /tmp
     * filesystem does not. Do not fallback on other errors. */
    if (errno != ENOENT && errno != EISDIR && errno != EOPNOTSUPP)
        return -1;
#endif

    char bufpath[] = "/tmp/"PACKAGE_NAME"XXXXXX";

    fd = vlc_mkstemp (bufpath);
    if (fd != -1)
        unlink (bufpath);
    return fd;
}

int vlc_close (int fd)
{
    int ret;
#ifdef POSIX_CLOSE_RESTART
    ret = posix_close(fd, 0);
#else
    ret = close(fd);
    /* POSIX.2008 (and earlier) does not specify if the file descriptor is
     * closed on failure. Assume it is as on Linux and most other common OSes.
     * Also emulate the correct error code as per newer POSIX versions. */
    if (unlikely(ret != 0) && unlikely(errno == EINTR))
        errno = EINPROGRESS;
#endif
    assert(ret == 0 || errno != EBADF); /* something is corrupt? */
    return ret;
}

int vlc_mkdir (const char *dirname, mode_t mode)
{
    return mkdir (dirname, mode);
}

DIR *vlc_opendir (const char *dirname)
{
    return opendir (dirname);
}

const char *vlc_readdir(DIR *dir)
{
    struct dirent *ent = readdir (dir);
    return (ent != NULL) ? ent->d_name : NULL;
}

int vlc_stat (const char *filename, struct stat *buf)
{
    return stat (filename, buf);
}

int vlc_lstat (const char *filename, struct stat *buf)
{
    return lstat (filename, buf);
}

int vlc_unlink (const char *filename)
{
    return unlink (filename);
}

int vlc_rename (const char *oldpath, const char *newpath)
{
    return rename (oldpath, newpath);
}

char *vlc_getcwd (void)
{
    long path_max = pathconf (".", _PC_PATH_MAX);
    size_t size = (path_max == -1 || path_max > 4096) ? 4096 : path_max;

    for (;; size *= 2)
    {
        char *buf = malloc (size);
        if (unlikely(buf == NULL))
            break;

        if (getcwd (buf, size) != NULL)
            return buf;
        free (buf);

        if (errno != ERANGE)
            break;
    }
    return NULL;
}

int vlc_dup (int oldfd)
{
#ifdef F_DUPFD_CLOEXEC
    return fcntl (oldfd, F_DUPFD_CLOEXEC, 0);
#else
    int newfd = dup (oldfd);
    if (newfd != -1)
        vlc_cloexec(oldfd);
    return newfd;
#endif
}

int vlc_pipe (int fds[2])
{
#ifdef HAVE_PIPE2
    return pipe2(fds, O_CLOEXEC);
#else
    int ret = pipe(fds);
    if (ret == 0)
    {
        vlc_cloexec(fds[0]);
        vlc_cloexec(fds[1]);
    }
    return ret;
#endif
}

ssize_t vlc_write(int fd, const void *buf, size_t len)
{
    struct iovec iov = { .iov_base = (void *)buf, .iov_len = len };

    return vlc_writev(fd, &iov, 1);
}

ssize_t vlc_writev(int fd, const struct iovec *iov, int count)
{
    sigset_t set, oset;

    sigemptyset(&set);
    sigaddset(&set, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &set, &oset);

    ssize_t val = writev(fd, iov, count);
    if (val < 0 && errno == EPIPE)
    {
#if (_POSIX_REALTIME_SIGNALS > 0)
        siginfo_t info;
        struct timespec ts = { 0, 0 };

        while (sigtimedwait(&set, &info, &ts) >= 0 || errno != EAGAIN);
#else
        for (;;)
        {
            sigset_t s;
            int num;

            sigpending(&s);
            if (!sigismember(&s, SIGPIPE))
                break;

            sigwait(&set, &num);
            assert(num == SIGPIPE);
        }
#endif
    }

    if (!sigismember(&oset, SIGPIPE)) /* Restore the signal mask if changed */
        pthread_sigmask(SIG_SETMASK, &oset, NULL);
    return val;
}

#include <vlc_network.h>

#ifndef HAVE_ACCEPT4
static void vlc_socket_setup(int fd, bool nonblock)
{
    vlc_cloexec(fd);

    if (nonblock)
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

#ifdef SO_NOSIGPIPE
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &(int){ 1 }, sizeof (int));
#endif
}
#endif

int vlc_socket (int pf, int type, int proto, bool nonblock)
{
#ifdef SOCK_CLOEXEC
    if (nonblock)
        type |= SOCK_NONBLOCK;

    int fd = socket(pf, type | SOCK_CLOEXEC, proto);
# ifdef SO_NOSIGPIPE
    if (fd != -1)
        setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &(int){ 1 }, sizeof (int));
# endif
#else
    int fd = socket (pf, type, proto);
    if (fd != -1)
        vlc_socket_setup(fd, nonblock);
#endif
    return fd;
}

int vlc_socketpair(int pf, int type, int proto, int fds[2], bool nonblock)
{
#ifdef SOCK_CLOEXEC
    if (nonblock)
        type |= SOCK_NONBLOCK;

    int ret = socketpair(pf, type | SOCK_CLOEXEC, proto, fds);
# ifdef SO_NOSIGPIPE
    if (ret == 0)
    {
        const int val = 1;

        setsockopt(fds[0], SOL_SOCKET, SO_NOSIGPIPE, &val, sizeof (val));
        setsockopt(fds[1], SOL_SOCKET, SO_NOSIGPIPE, &val, sizeof (val));
    }
# endif
#else
    int ret = socketpair(pf, type, proto, fds);
    if (ret == 0)
    {
        vlc_socket_setup(fds[0], nonblock);
        vlc_socket_setup(fds[1], nonblock);
    }
#endif
    return ret;
}

int vlc_accept (int lfd, struct sockaddr *addr, socklen_t *alen, bool nonblock)
{
#ifdef HAVE_ACCEPT4
    int flags = SOCK_CLOEXEC;
    if (nonblock)
        flags |= SOCK_NONBLOCK;

    int fd = accept4(lfd, addr, alen, flags);
# ifdef SO_NOSIGPIPE
    if (fd != -1)
        setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &(int){ 1 }, sizeof (int));
# endif
#else
    int fd = accept(lfd, addr, alen);
    if (fd != -1)
        vlc_socket_setup(fd, nonblock);
#endif
    return fd;
}
