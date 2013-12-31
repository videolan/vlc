/*****************************************************************************
 * dirs.c: XDG directories configuration
 *****************************************************************************
 * Copyright (C) 2001-2007 VLC authors and VideoLAN
 * Copyright © 2007-2009 Rémi Denis-Courmont
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

#include <vlc_common.h>

#include "../libvlc.h"
#include "config/configuration.h"

#include <unistd.h>
#include <pwd.h>
#include <assert.h>
#include <limits.h>

#if !defined (__linux__)
/**
 * Determines the shared data directory
 *
 * @return a nul-terminated string or NULL. Use free() to release it.
 */
char *config_GetDataDir (void)
{
    const char *path = getenv ("VLC_DATA_PATH");
    return strdup ((path != NULL) ? path : PKGDATADIR);
}

/**
 * Determines the architecture-dependent data directory
 *
 * @return a string (always succeeds).
 */
char *config_GetLibDir (void)
{
    return strdup (PKGLIBDIR);
}
#endif

static char *config_GetHomeDir (void)
{
    /* 1/ Try $HOME  */
    const char *home = getenv ("HOME");
    if (home != NULL)
        return strdup (home);
#if defined(HAVE_GETPWUID_R)
    /* 2/ Try /etc/passwd */
    long max = sysconf (_SC_GETPW_R_SIZE_MAX);
    if (max != -1)
    {
        char buf[max];
        struct passwd pwbuf, *pw;

        if (getpwuid_r (getuid (), &pwbuf, buf, sizeof (buf), &pw) == 0
          && pw != NULL)
            return strdup (pw->pw_dir);
    }
#endif
    return NULL;
}

static char *config_GetAppDir (const char *xdg_name, const char *xdg_default)
{
    char *psz_dir;
    char var[sizeof ("XDG__HOME") + strlen (xdg_name)];

    /* XDG Base Directory Specification - Version 0.6 */
    snprintf (var, sizeof (var), "XDG_%s_HOME", xdg_name);

    const char *home = getenv (var);
    if (home != NULL)
    {
        if (asprintf (&psz_dir, "%s/vlc", home) == -1)
            psz_dir = NULL;
        return psz_dir;
    }

    char *psz_home = config_GetHomeDir ();
    if( psz_home == NULL
     || asprintf( &psz_dir, "%s/%s/vlc", psz_home, xdg_default ) == -1 )
        psz_dir = NULL;
    free (psz_home);
    return psz_dir;
}

static char *config_GetTypeDir (const char *xdg_name)
{
    const size_t namelen = strlen (xdg_name);
    const char *home = getenv ("HOME");
    const char *dir = getenv ("XDG_CONFIG_HOME");
    const char *file = "user-dirs.dirs";

    if (home == NULL)
        return NULL;
    if (dir == NULL)
    {
        dir = home;
        file = ".config/user-dirs.dirs";
    }

    char *path;
    if (asprintf (&path, "%s/%s", dir, file) == -1)
        return NULL;

    FILE *stream = fopen (path, "rt");
    free (path);
    path = NULL;
    if (stream != NULL)
    {
        char *linebuf = NULL;
        size_t linelen = 0;

        while (getline (&linebuf, &linelen, stream) != -1)
        {
            char *ptr = linebuf;
            ptr += strspn (ptr, " \t"); /* Skip whites */
            if (strncmp (ptr, "XDG_", 4))
                continue;
            ptr += 4; /* Skip XDG_ */
            if (strncmp (ptr, xdg_name, namelen))
                continue;
            ptr += namelen; /* Skip XDG type name */
            if (strncmp (ptr, "_DIR", 4))
                continue;
            ptr += 4; /* Skip _DIR */
            ptr += strspn (ptr, " \t"); /* Skip whites */
            if (*ptr != '=')
                continue;
            ptr++; /* Skip equality sign */
            ptr += strspn (ptr, " \t"); /* Skip whites */
            if (*ptr != '"')
                continue;
            ptr++; /* Skip quote */
            linelen -= ptr - linebuf;

            char *out;
            if (strncmp (ptr, "$HOME", 5))
            {
                path = malloc (linelen);
                if (path == NULL)
                    continue;
                out = path;
            }
            else
            {   /* Prefix with $HOME */
                const size_t homelen = strlen (home);
                ptr += 5;
                path = malloc (homelen + linelen - 5);
                if (path == NULL)
                    continue;
                memcpy (path, home, homelen);
                out = path + homelen;
            }

            while (*ptr != '"')
            {
                if (*ptr == '\\')
                    ptr++;
                if (*ptr == '\0')
                {
                    free (path);
                    path = NULL;
                    break;
                }
                *(out++) = *(ptr++);
            }
            if (path != NULL)
                *out = '\0';
            break;
        }
        free (linebuf);
        fclose (stream);
    }

    /* Default! */
    if (path == NULL)
    {
        if (strcmp (xdg_name, "DESKTOP") == 0)
        {
            if (asprintf (&path, "%s/Desktop", home) == -1)
                return NULL;
        }
        else
            path = strdup (home);
    }

    return path;
}


char *config_GetUserDir (vlc_userdir_t type)
{
    switch (type)
    {
        case VLC_HOME_DIR:
            break;
        case VLC_CONFIG_DIR:
            return config_GetAppDir ("CONFIG", ".config");
        case VLC_DATA_DIR:
            return config_GetAppDir ("DATA", ".local/share");
        case VLC_CACHE_DIR:
            return config_GetAppDir ("CACHE", ".cache");

        case VLC_DESKTOP_DIR:
            return config_GetTypeDir ("DESKTOP");
        case VLC_DOWNLOAD_DIR:
            return config_GetTypeDir ("DOWNLOAD");
        case VLC_TEMPLATES_DIR:
            return config_GetTypeDir ("TEMPLATES");
        case VLC_PUBLICSHARE_DIR:
            return config_GetTypeDir ("PUBLICSHARE");
        case VLC_DOCUMENTS_DIR:
            return config_GetTypeDir ("DOCUMENTS");
        case VLC_MUSIC_DIR:
            return config_GetTypeDir ("MUSIC");
        case VLC_PICTURES_DIR:
            return config_GetTypeDir ("PICTURES");
        case VLC_VIDEOS_DIR:
            return config_GetTypeDir ("VIDEOS");
    }
    return config_GetHomeDir ();
}
