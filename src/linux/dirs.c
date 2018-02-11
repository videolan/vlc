/*****************************************************************************
 * linux/dirs.c: Linux-specific directories
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
#include <pthread.h>
#include <linux/limits.h>

#include <vlc_common.h>
#include "libvlc.h"
#include "config/configuration.h"

static char *config_GetLibDirRaw(void)
{
    char *path = getenv("VLC_LIB_PATH");
    if (path != NULL)
        return strdup(path);

    /* Find the path to libvlc (i.e. ourselves) */
    FILE *maps = fopen ("/proc/self/maps", "rte");
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
    if (path == NULL)
        path = strdup(PKGLIBDIR);
    return path;
}

static char cached_path[PATH_MAX] = PKGLIBDIR;

static void config_GetLibDirOnce(void)
{
    char *path = config_GetLibDirRaw();
    if (likely(path != NULL && sizeof (cached_path) > strlen(path)))
        strcpy(cached_path, path);
    free(path);
}

char *config_GetLibDir(void)
{
    static pthread_once_t once = PTHREAD_ONCE_INIT;

    /* Reading and parsing /proc/self/maps is slow, so cache the value since
     * it's guaranteed not to change during the life-time of the process. */
    pthread_once(&once, config_GetLibDirOnce);
    return strdup(cached_path);
}

char *config_GetDataDir (void)
{
    const char *path = getenv ("VLC_DATA_PATH");
    if (path != NULL)
        return strdup (path);

    char *libdir = config_GetLibDir ();
    if (libdir == NULL)
        return NULL; /* OOM */

    /* Look for common prefix between lib and data directories. */
    size_t prefix_len = 0;
    while (PKGLIBDIR[prefix_len] == PKGDATADIR[prefix_len])
    {
        if (PKGLIBDIR[prefix_len] == '\0')
            return libdir; /* corner case: directories are identical */
        prefix_len++;
    }

    char *datadir = NULL;

    char *p = strstr(libdir, PKGLIBDIR + prefix_len);
    if (p != NULL)
    {
        if (unlikely(asprintf(&datadir, "%.*s%s", (int)(p - libdir), libdir,
                              PKGDATADIR + prefix_len) == -1))
            datadir = NULL;
    }
    free (libdir);
    return (datadir != NULL) ? datadir : strdup (PKGDATADIR);
}
