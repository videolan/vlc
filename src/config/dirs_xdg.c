/*****************************************************************************
 * dirs_xdg.c: XDG directories configuration
 *****************************************************************************
 * Copyright (C) 2001-2007 the VideoLAN team
 * Copyright © 2007-2009 Rémi Denis-Courmont
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
#include <vlc_charset.h>
#include "config/configuration.h"

#include <unistd.h>
#include <pwd.h>
#include <assert.h>
#include <limits.h>

/**
 * Determines the shared data directory
 *
 * @return a nul-terminated string or NULL. Use free() to release it.
 */
char *config_GetDataDirDefault (void)
{
    return strdup (DATA_PATH);
}

/**
 * Determines the architecture-dependent data directory
 *
 * @return a string (always succeeds).
 */
const char *config_GetLibDir (void)
{
    return PKGLIBDIR;
}

/**
 * Determines the system configuration directory.
 *
 * @return a string (always succeeds).
 */
const char *config_GetConfDir( void )
{
    return SYSCONFDIR;
}

static char *config_GetHomeDir (void)
{
    /* 1/ Try $HOME  */
    const char *home = getenv ("HOME");
#if defined(HAVE_GETPWUID_R)
    /* 2/ Try /etc/passwd */
    char buf[sysconf (_SC_GETPW_R_SIZE_MAX)];
    if (home == NULL)
    {
        struct passwd pw, *res;

        if (!getpwuid_r (getuid (), &pw, buf, sizeof (buf), &res) && res)
            home = pw.pw_dir;
    }
#endif

    return FromLocaleDup (home);
}

static char *config_GetAppDir (const char *xdg_name, const char *xdg_default)
{
    char *psz_dir;
    char var[sizeof ("XDG__HOME") + strlen (xdg_name)];

    /* XDG Base Directory Specification - Version 0.6 */
    snprintf (var, sizeof (var), "XDG_%s_HOME", xdg_name);

    char *psz_home = FromLocale (getenv (var));
    if( psz_home )
    {
        if( asprintf( &psz_dir, "%s/vlc", psz_home ) == -1 )
            psz_dir = NULL;
        LocaleFree (psz_home);
        return psz_dir;
    }

    psz_home = config_GetHomeDir ();
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
    const size_t homelen = strlen (home);
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
                    continue;
                }
                *(out++) = *(ptr++);
            }
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
                path = NULL;
        }
        else
            path = strdup (home);
    }

    char *ret = FromLocaleDup (path);
    free (path);
    return ret;
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
