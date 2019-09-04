/*****************************************************************************
 * filesystem.c: Linux file system helpers
 *****************************************************************************
 * Copyright © 2018 Rémi Denis-Courmont
 *
 * Authors: Rémi Denis-Courmont
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

#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <vlc_common.h>
#include <vlc_fs.h>

int vlc_memfd(void)
{
    int fd;
#ifdef HAVE_MEMFD_CREATE
    fd = memfd_create(PACKAGE_NAME"-memfd", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd != -1 || errno != ENOSYS)
        return fd;
#endif

    /* Fallback to open with O_TMPFILE, */
    fd = open("/tmp", O_RDWR | O_CLOEXEC | O_TMPFILE, S_IRUSR | S_IWUSR);
    if (fd != -1 || (errno != EISDIR && errno != EOPNOTSUPP))
        return fd;

    /* Fallback to POSIX implementation if O_TMPFILE is not supported (errno is
     * EISDIR, or EOPNOTSUPP, cf. man open(2). */
    char bufpath[] = "/tmp/"PACKAGE_NAME"XXXXXX";
    fd = vlc_mkstemp(bufpath);
    if (fd != -1)
        unlink(bufpath);
    return fd;
}
