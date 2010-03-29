/*****************************************************************************
 * linux_specific.c: Linux-specific initialization
 *****************************************************************************
 * Copyright © 2008 Rémi Denis-Courmont
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include <vlc_common.h>
#include "../libvlc.h"

static const char default_path[] = PKGLIBDIR;

static void set_libvlc_path (void)
{
    /* Find the path to libvlc (i.e. ourselves) */
    FILE *maps = fopen ("/proc/self/maps", "rt");
    if (maps == NULL)
        goto error;

    char *line = NULL;
    size_t linelen = 0;
    uintptr_t needle = (uintptr_t)set_libvlc_path;

    for (;;)
    {
        ssize_t len = getline (&line, &linelen, maps);
        if (len == -1)
            break;

        void *start, *end;
        if (sscanf (line, "%p-%p", &start, &end) < 2)
            continue;
        if (needle < (uintptr_t)start || (uintptr_t)end <= needle)
            continue;
        char *dir = strchr (line, '/');
        if (dir == NULL)
            continue;
        char *file = strrchr (line, '/');
        if (end == NULL)
            continue;
        *file = '\0';
        if (asprintf (&psz_vlcpath, "%s/"PACKAGE, dir) == -1)
            goto error;
        break;
    }
    free (line);
    fclose (maps);
    return;

error:
    psz_vlcpath = (char *)default_path; /* default, cannot fail */
}

static void unset_libvlc_path (void)
{
    if (psz_vlcpath != default_path)
        free (psz_vlcpath);
}

static struct
{
    vlc_mutex_t lock;
    unsigned refs;
} once = { VLC_STATIC_MUTEX, 0 };

#ifdef __GLIBC__
# include <gnu/libc-version.h>
# include <stdlib.h>
#endif

void system_Init (libvlc_int_t *libvlc, int *argc, const char *argv[])
{
#ifdef __GLIBC__
    const char *glcv = gnu_get_libc_version ();

    /* gettext in glibc 2.5-2.7 is not thread-safe. LibVLC keeps crashing,
     * especially in sterror_r(). Even if we have NLS disabled, the calling
     * process might have called setlocale(). */
    if (strverscmp (glcv, "2.5") >= 0 && strverscmp (glcv, "2.8") < 0)
    {
        fputs ("LibVLC has detected an unusable buggy GNU/libc version.\n"
               "Please update to version 2.8 or newer.\n", stderr);
        fflush (stderr);
#ifndef DISABLE_BUGGY_GLIBC_CHECK
        abort ();
#endif
    }
#endif

    vlc_mutex_lock (&once.lock);
    if (once.refs++ == 0)
        set_libvlc_path ();
    vlc_mutex_unlock (&once.lock);

    (void)libvlc; (void)argc; (void)argv;
}

void system_Configure (libvlc_int_t *libvlc,
                       int argc, const char *const argv[])
{
    (void)libvlc; (void)argc; (void)argv;
}

void system_End (libvlc_int_t *libvlc)
{
    vlc_mutex_lock (&once.lock);
    if (--once.refs == 0)
        unset_libvlc_path ();
    vlc_mutex_unlock (&once.lock);

    (void)libvlc;
}
