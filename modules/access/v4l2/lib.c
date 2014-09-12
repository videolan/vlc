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

#include <dlfcn.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <vlc_common.h>

#include "v4l2.h"

static int fd_open (int fd, int flags)
{
    (void) flags;
    return fd;
}

static void *v4l2_handle = NULL;

int (*v4l2_fd_open) (int, int) = fd_open;
//int (*v4l2_open) (const char *, int, ...) = open;
//int (*v4l2_dup) (const char *, int, ...) = dup;
int (*v4l2_close) (int) = close;
int (*v4l2_ioctl) (int, unsigned long int, ...) = ioctl;
ssize_t (*v4l2_read) (int, void *, size_t) = read;
//ssize_t (*v4l2_write) (int, const void *, size_t) = write;
void * (*v4l2_mmap) (void *, size_t, int, int, int, int64_t) = mmap;
int (*v4l2_munmap) (void *, size_t) = munmap;

__attribute__((constructor))
static void v4l2_lib_load (void)
{
    void *h;

    h = dlopen ("libmediaclient.so", RTLD_LAZY | RTLD_LOCAL | RTLD_NOLOAD);
    if (h == NULL)
        h = dlopen ("libv4l2.so.0", RTLD_LAZY | RTLD_LOCAL);
    if (h == NULL)
        return;

    void *sym;
#define SYM(name) \
    sym = dlsym (h, "v4l2_"#name); \
    if (sym != NULL) v4l2_##name = sym

    SYM(fd_open); /*SYM(open); SYM(dup);*/ SYM(close); SYM(ioctl);
    SYM(read); /*SYM(write);*/ SYM(mmap); SYM(munmap);

    v4l2_handle = h;
}

__attribute__((destructor))
static void v4l2_lib_unload (void)
{
    if (v4l2_handle != NULL)
        dlclose (v4l2_handle);
}
