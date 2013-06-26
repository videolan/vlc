/*****************************************************************************
 * lib.c : libv4l2 run-time
 *****************************************************************************
 * Copyright (C) 2012 Rémi Denis-Courmont
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

#include <pthread.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <vlc_common.h>

#include "v4l2.h"

static void *v4l2_handle = NULL;
static int (*v4l2_fd_open_) (int, int);
int (*v4l2_close) (int);
int (*v4l2_ioctl) (int, unsigned long int, ...);
ssize_t (*v4l2_read) (int, void *, size_t);
void * (*v4l2_mmap) (void *, size_t, int, int, int, int64_t);
int (*v4l2_munmap) (void *, size_t);

static int fd_open (int fd, int flags)
{
    (void) flags;
    return fd;
}

static void v4l2_lib_load (void)
{
    void *h = dlopen ("libv4l2.so.0", RTLD_LAZY | RTLD_LOCAL);
    if (h == NULL)
        goto fallback;

    v4l2_fd_open_ = dlsym (h, "v4l2_fd_open");
    v4l2_close = dlsym (h, "v4l2_close");
    v4l2_ioctl = dlsym (h, "v4l2_ioctl");
    v4l2_read = dlsym (h, "v4l2_read");
    v4l2_mmap = dlsym (h, "v4l2_mmap");
    v4l2_munmap = dlsym (h, "v4l2_munmap");

    if (v4l2_fd_open_ != NULL && v4l2_close != NULL && v4l2_ioctl != NULL
     && v4l2_read != NULL && v4l2_mmap != NULL && v4l2_munmap != NULL)
    {
        v4l2_handle = h;
        return;
    }

    dlclose (h);
fallback:
    v4l2_fd_open_ = fd_open;
    v4l2_close = close;
    v4l2_ioctl = ioctl;
    v4l2_read = read;
    v4l2_mmap = mmap;
    v4l2_munmap = munmap;
}

__attribute__((destructor))
static void v4l2_lib_unload (void)
{
    if (v4l2_handle != NULL)
        dlclose (v4l2_handle);
}

int v4l2_fd_open (int fd, int flags)
{
    static pthread_once_t once = PTHREAD_ONCE_INIT;

    pthread_once (&once, v4l2_lib_load);
    return v4l2_fd_open_ (fd, flags);
}
