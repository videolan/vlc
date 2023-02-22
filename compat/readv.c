/*****************************************************************************
 * readv.c: POSIX readv() replacement
 *****************************************************************************
 * Copyright © 2022 Rémi Denis-Courmont
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
{
    if (iovcnt == 1)
        return read(fd, iov->iov_base, iov->iov_len);

    /*
     * The whole point of readv() is that multiple consecutive reads are *not*
     * generally equivalent to one read of the same total size. We have to read
     * all at once to preserve atomicity and packet boundaries.
     */
    size_t size = 0;

    for (int i = 0; i < iovcnt; i++) {
        /* TODO: use ckd_add() */
        size += iov[i].iov_len;

        if (iov[i].iov_len > size) {
            errno = EINVAL;
            return -1;
        }
    }

    unsigned char *buf = malloc(size ? size : 1);
    if (buf == NULL) {
        errno = ENOBUFS;
        return -1;
    }

    ssize_t ret = read(fd, buf, size);
    int saved_errno = errno;

    for (ssize_t len = ret; len > 0; iov++) {
        size_t copy = iov->iov_len;

        if (copy > (size_t)len)
            copy = len;

        memcpy(iov->iov_base, buf, copy);
        buf += copy;
        len -= copy;
    }

    free(buf);
    errno = saved_errno;
    return ret;
}
