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

#include <vlc_common.h>
#include "../libvlc.h"

#if 0
#include <stdio.h>
#include <assert.h>
#include <pthread.h>

static void set_libvlc_path (void)
{
    static char libvlc_path[PATH_MAX];

    assert (strlen (LIBDIR) < sizeof (libvlc_path));
    strcpy (libvlc_path, LIBDIR); /* fail safe */
    vlc_global ()->psz_vlcpath = libvlc_path;

    /* Find the path to libvlc (i.e. ourselves) */
    FILE *maps = fopen ("/proc/self/maps", "rt");
    if (maps == NULL)
        return;

    for (;;)
    {
        char buf[5000], *dir, *end;

        if (fgets (buf, sizeof (buf), maps) == NULL)
            break;

        dir = strchr (buf, '/');
        if (dir == NULL)
            continue;
        end = strrchr (dir, '/');
        if (end == NULL)
            continue;
        if (strncmp (end + 1, "libvlc.so.", 10))
            continue;

        *end = '\0';
        printf ("libvlc at %s\n", dir);
        if (strlen (dir) < sizeof (libvlc_path))
            strcpy (libvlc_path, dir);
        break;
    }

    fclose (maps);
}
#endif

void system_Init (libvlc_int_t *libvlc, int *argc, const char *argv[])
{
#if 0
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once (&once, set_libvlc_path);
#endif
    (void)libvlc; (void)argc; (void)argv;
}

void system_Configure (libvlc_int_t *libvlc, int *argc, const char *argv[])
{
    (void)libvlc; (void)argc; (void)argv;
}

void system_End (libvlc_int_t *libvlc)
{
    (void)libvlc;
}

