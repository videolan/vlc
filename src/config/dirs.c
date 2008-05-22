/*****************************************************************************
 * dirs.c: directories configuration
 *****************************************************************************
 * Copyright (C) 2001-2007 the VideoLAN team
 * Copyright © 2007-2008 Rémi Denis-Courmont
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

#include <vlc/vlc.h>

#if defined( WIN32 )
# define _WIN32_IE IE5
# include <w32api.h>
# include <direct.h>
# include <shlobj.h>
#else
# include <unistd.h>
# include <pwd.h>
#endif

#include "../libvlc.h"
#include "configuration.h"
#include <vlc_charset.h>
#include <vlc_configuration.h>

#include <errno.h>                                                  /* errno */
#include <assert.h>
#include <limits.h>

/**
 * config_GetDataDir: find directory where shared data is installed
 *
 * @return a string (always succeeds).
 */
const char *config_GetDataDir( void )
{
#if defined (WIN32) || defined (UNDER_CE)
    return vlc_global()->psz_vlcpath;
#elif defined(__APPLE__) || defined (SYS_BEOS)
    static char path[PATH_MAX] = "";

    if( *path == '\0' )
    {
        snprintf( path, sizeof( path ), "%s/share",
                  vlc_global()->psz_vlcpath );
        path[sizeof( path ) - 1] = '\0';
    }
    return path;
#else
    return DATA_PATH;
#endif
}

/**
 * Determines the system configuration directory.
 *
 * @return a string (always succeeds).
 */
const char *config_GetConfDir( void )
{
#if defined (WIN32) || defined (UNDER_CE)
    return vlc_global()->psz_vlcpath;
#elif defined(__APPLE__) || defined (SYS_BEOS)
    static char path[PATH_MAX] = "";

    if( *path == '\0' )
    {
        snprintf( path, sizeof( path ), "%s/share", /* FIXME: Duh? */
                  vlc_global()->psz_vlcpath );
        path[sizeof( path ) - 1] = '\0';
    }
    return path;
#else
    return SYSCONFDIR;
#endif
}

static char *GetDir( bool b_appdata )
{
    const char *psz_localhome = NULL;

#if defined(WIN32) && !defined(UNDER_CE)
    wchar_t whomedir[MAX_PATH];
    /* Get the "Application Data" folder for the current user */
    if( S_OK == SHGetFolderPathW( NULL,
              (b_appdata ? CSIDL_APPDATA : CSIDL_PROFILE) | CSIDL_FLAG_CREATE,
                                  NULL, SHGFP_TYPE_CURRENT, whomedir ) )
        return FromWide( whomedir );

#elif defined(UNDER_CE)
    (void)b_appdata;
#ifndef CSIDL_APPDATA
#   define CSIDL_APPDATA 0x1A
#endif

    wchar_t whomedir[MAX_PATH];

    /* get the "Application Data" folder for the current user */
    if( SHGetSpecialFolderPath( NULL, whomedir, CSIDL_APPDATA, 1 ) )
        return FromWide( whomedir );
#else
    (void)b_appdata;
#endif

    psz_localhome = getenv( "HOME" );
#if defined(HAVE_GETPWUID_R)
    char buf[sysconf (_SC_GETPW_R_SIZE_MAX)];
    if( psz_localhome == NULL )
    {
        struct passwd pw, *res;

        if (!getpwuid_r (getuid (), &pw, buf, sizeof (buf), &res) && res)
            psz_localhome = pw.pw_dir;
    }
#endif
    if (psz_localhome == NULL)
        psz_localhome = getenv( "TMP" );
    if (psz_localhome == NULL)
        psz_localhome = "/tmp";

    return FromLocaleDup( psz_localhome );
}

/**
 * Get the user's home directory
 */
char *config_GetHomeDir( void )
{
    return GetDir( false );
}

static char *config_GetFooDir (const char *xdg_name, const char *xdg_default)
{
    char *psz_dir;
#if defined(WIN32) || defined(__APPLE__) || defined(SYS_BEOS)
    char *psz_parent = GetDir (true);

    if( asprintf( &psz_dir, "%s" DIR_SEP CONFIG_DIR, psz_parent ) == -1 )
        psz_dir = NULL;

    free (psz_parent);
    (void)xdg_name; (void)xdg_default;
#else
    char var[sizeof ("XDG__HOME") + strlen (xdg_name)];

    /* XDG Base Directory Specification - Version 0.6 */
    snprintf (var, sizeof (var), "XDG_%s_HOME", xdg_name);
    char *psz_home = getenv( var );
    psz_home = psz_home ? FromLocaleDup( psz_home ) : NULL;
    if( psz_home )
    {
        if( asprintf( &psz_dir, "%s/vlc", psz_home ) == -1 )
            psz_dir = NULL;
        goto out;
    }

    /* Try HOME, then fallback to non-XDG dirs */
    psz_home = config_GetHomeDir();
    if( asprintf( &psz_dir, "%s/%s/vlc", psz_home, xdg_default ) == -1 )
        psz_dir = NULL;

out:
    free (psz_home);
#endif
    return psz_dir;
}

/**
 * Get the user's VLC configuration directory
 */
char *config_GetUserConfDir( void )
{
    return config_GetFooDir ("CONFIG", ".config");
}

/**
 * Get the user's VLC data directory
 * (used for stuff like the skins, custom lua modules, ...)
 */
char *config_GetUserDataDir( void )
{
    return config_GetFooDir ("DATA", ".local/share");
}

/**
 * Get the user's VLC cache directory
 * (used for stuff like the modules cache, the album art cache, ...)
 */
char *config_GetCacheDir( void )
{
    return config_GetFooDir ("CACHE", ".cache");
}
