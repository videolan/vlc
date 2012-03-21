/*****************************************************************************
 * linux_specific.c: Linux-specific initialization
 *****************************************************************************
 * Copyright © 2008-2012 Rémi Denis-Courmont
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

#include <stdio.h>
#include <string.h>

#include <vlc_common.h>
#include "libvlc.h"
#include "config/configuration.h"

char *config_GetLibDir (void)
{
    char *path = NULL;

    /* Find the path to libvlc (i.e. ourselves) */
    FILE *maps = fopen ("/proc/self/maps", "rt");
    if (maps == NULL)
        goto error;

    char *line = NULL;
    size_t linelen = 0;
    uintptr_t needle = (uintptr_t)config_GetLibDir;

    for (;;)
    {
        ssize_t len = getline (&line, &linelen, maps);
        if (len == -1)
            break;

        void *start, *end;
        if (sscanf (line, "%p-%p", &start, &end) < 2)
            continue;
        /* This mapping contains the address of this function. */
        if (needle < (uintptr_t)start || (uintptr_t)end <= needle)
            continue;

        char *dir = strchr (line, '/');
        if (dir == NULL)
            continue;

        char *file = strrchr (line, '/');
        if (end == NULL)
            continue;
        *file = '\0';

        if (asprintf (&path, "%s/"PACKAGE, dir) == -1)
            path = NULL;
        break;
    }

    free (line);
    fclose (maps);
error:
    return (path != NULL) ? path : strdup (PKGLIBDIR);
}

char *config_GetDataDir (void)
{
    const char *path = getenv ("VLC_DATA_PATH");
    if (path != NULL)
        return strdup (path);

    char *libdir = config_GetLibDir ();
    if (libdir == NULL)
        return NULL; /* OOM */

    char *datadir = NULL;

    /* There are no clean ways to do this, are there?
     * Due to multilibs, we cannot simply append ../share/. */
    char *p = strstr (libdir, "/lib/");
    if (p != NULL)
    {
        char *p2;
        /* Deal with nested "lib" directories. Grmbl. */
        while ((p2 = strstr (p + 4, "/lib/")) != NULL)
            p = p2;
        *p = '\0';

        if (unlikely(asprintf (&datadir, "%s/share/"PACKAGE, libdir) == -1))
            datadir = NULL;
    }
    free (libdir);
    return (datadir != NULL) ? datadir : strdup (PKGDATADIR);
}
