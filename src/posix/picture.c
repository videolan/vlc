/*****************************************************************************
 * picture.c:
 *****************************************************************************
 * Copyright (C) 2018 Rémi Denis-Courmont
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
#include <sys/types.h>
#include <sys/mman.h>

#include <vlc_common.h>
#include <vlc_fs.h>
#include "misc/picture.h"

void *picture_Allocate(int *restrict fdp, size_t size)
{
    int fd = vlc_memfd();
    if (fd == -1)
        return NULL;

    if (ftruncate(fd, size)) {
error:
        vlc_close(fd);
        return NULL;
    }

    void *base = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (base == MAP_FAILED)
        goto error;

    *fdp = fd;
    return base;
}

void picture_Deallocate(int fd, void *base, size_t size)
{
    munmap(base, size);
    vlc_close(fd);
}
