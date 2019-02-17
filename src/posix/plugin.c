/*****************************************************************************
 * plugin.c : Low-level dynamic library handling
 *****************************************************************************
 * Copyright (C) 2001-2007 VLC authors and VideoLAN
 *
 * Authors: Sam Hocevar <sam@zoy.org>
 *          Ethan C. Baldridge <BaldridgeE@cadmus.com>
 *          Hans-Peter Jansen <hpj@urpla.net>
 *          Gildas Bazin <gbazin@videolan.org>
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
#include <vlc_common.h>
#include "modules/modules.h"

#include <sys/types.h>
#include <dlfcn.h>

#ifdef HAVE_VALGRIND_VALGRIND_H
# include <valgrind/valgrind.h>
#endif

void *vlc_dlopen(const char *path, bool lazy)
{
#if defined (RTLD_NOW)
    const int flags = lazy ? RTLD_LAZY : RTLD_NOW;
#elif defined (DL_LAZY)
    const int flags = DL_LAZY;
    VLC_UNUSED(lazy);
#else
    const int flags = 0;
    VLC_UNUSED(lazy);
#endif
    return dlopen (path, flags);
}

int vlc_dlclose(void *handle)
{
#if !defined(__SANITIZE_ADDRESS__)
#ifdef HAVE_VALGRIND_VALGRIND_H
    if( RUNNING_ON_VALGRIND > 0 )
        return 0; /* do not dlclose() so that we get proper stack traces */
#endif
    int err = dlclose( handle );
    assert(err == 0);
    return err;
#else
    (void) handle;
    return 0;
#endif
}

void *vlc_dlsym(void *handle, const char *name)
{
    return dlsym(handle, name);
}

char *vlc_dlerror(void)
{
    const char *errmsg = dlerror();
    /* XXX: This is not thread-safe at all. POSIX is helpless here. */
    return (errmsg != NULL) ? strdup(errmsg) : NULL;
}
