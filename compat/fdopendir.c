/*****************************************************************************
 * fdopendir.c: POSIX fdopendir replacement
 *****************************************************************************
 * Copyright © 2011 Rémi Denis-Courmont
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
# include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <dirent.h>

DIR *fdopendir (int fd)
{
#ifdef F_GETFL
    /* Check read permission on file descriptor */
    int mode = fcntl (fd, F_GETFL);
    if (mode == -1 || (mode & O_ACCMODE) == O_WRONLY)
    {
        errno = EBADF;
        return NULL;
    }
#endif
    /* Check directory file type */
    struct stat st;
    if (fstat (fd, &st))
        return NULL;

    if (!S_ISDIR (st.st_mode))
    {
        errno = ENOTDIR;
        return NULL;
    }

    /* Try to open the directory through /proc where available.
     * Not all operating systems support this. Fix your libc! */
    char path[sizeof ("/proc/self/fd/") + 3 * sizeof (int)];
    sprintf (path, "/proc/self/fd/%u", fd);

    DIR *dir = opendir (path);
    if (dir != NULL)
    {
        close (fd);
        return dir;
    }

    /* Hide impossible errors for fdopendir() */
    switch (errno)
    {
        case EACCES:
#ifdef ELOOP
        case ELOOP:
#endif
        case ENAMETOOLONG:
        case ENOENT:
        case EMFILE:
        case ENFILE:
            errno = EIO;
    }
    return NULL;
}
