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

#include <vlc_common.h>

#if defined( WIN32 )
# define _WIN32_IE IE5
# include <w32api.h>
#ifndef UNDER_CE
# include <direct.h>
#endif
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

#if defined( WIN32 )
# define DIR_SHARE ""
#else
# define DIR_SHARE "share"
#endif

/**
 * config_GetDataDir: find directory where shared data is installed
 *
 * @return a string (always succeeds).
 */
const char *config_GetDataDir( void )
{
#if defined (WIN32) || defined(__APPLE__) || defined (SYS_BEOS)
    static char path[PATH_MAX] = "";

    if( *path == '\0' )
    {
        snprintf( path, sizeof( path ), "%s" DIR_SEP DIR_SHARE, psz_vlcpath );
        path[sizeof( path ) - 1] = '\0';
    }
    return path;
#else
    return DATA_PATH;
#endif
}

static const char *GetDir( bool b_appdata, bool b_common_appdata )
{
    /* FIXME: a full memory page here - quite a waste... */
    static char homedir[PATH_MAX] = "";

#if defined (WIN32)
    wchar_t wdir[MAX_PATH];

# if defined (UNDER_CE)
    /*There are some errors in cegcc headers*/
#undef SHGetSpecialFolderPath
    BOOL WINAPI SHGetSpecialFolderPath(HWND,LPWSTR,int,BOOL);
    if( SHGetSpecialFolderPath( NULL, wdir, CSIDL_APPDATA, 1 ) )
# else
    /* Get the "Application Data" folder for the current user */
    if( S_OK == SHGetFolderPathW( NULL,
              ( b_appdata ? CSIDL_APPDATA :
               ( b_common_appdata ? CSIDL_COMMON_APPDATA: CSIDL_PERSONAL ) )
              | CSIDL_FLAG_CREATE,
                                  NULL, SHGFP_TYPE_CURRENT, wdir ) )
# endif
    {
        static char appdir[PATH_MAX] = "";
        static char comappdir[PATH_MAX] = "";
        WideCharToMultiByte (CP_UTF8, 0, wdir, -1,
                             b_appdata ? appdir :
                             (b_common_appdata ? comappdir :homedir),
                              PATH_MAX, NULL, NULL);
        return b_appdata ? appdir : (b_common_appdata ? comappdir :homedir);
    }
#else
    (void)b_appdata;
    (void)b_common_appdata;
#endif

#ifdef LIBVLC_USE_PTHREAD
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock (&lock);
#endif

    if (!*homedir)
    {
        const char *psz_localhome = getenv( "HOME" );
#if defined(HAVE_GETPWUID_R)
        char buf[sysconf (_SC_GETPW_R_SIZE_MAX)];
        if (psz_localhome == NULL)
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

        const char *uhomedir = FromLocale (psz_localhome);
        strncpy (homedir, uhomedir, sizeof (homedir) - 1);
        homedir[sizeof (homedir) - 1] = '\0';
        LocaleFree (uhomedir);
    }
#ifdef LIBVLC_USE_PTHREAD
    pthread_mutex_unlock (&lock);
#endif
    return homedir;
}

/**
 * Determines the system configuration directory.
 *
 * @return a string (always succeeds).
 */
const char *config_GetConfDir( void )
{
#if defined (WIN32)
    return GetDir( false, true );
#elif defined(__APPLE__) || defined (SYS_BEOS)
    static char path[PATH_MAX] = "";

    if( *path == '\0' )
    {
        snprintf( path, sizeof( path ), "%s"DIR_SEP DIR_SHARE, /* FIXME: Duh? */
                  psz_vlcpath );
        path[sizeof( path ) - 1] = '\0';
    }
    return path;
#else
    return SYSCONFDIR;
#endif
}

/**
 * Get the user's home directory
 */
const char *config_GetHomeDir( void )
{
    return GetDir (false, false);
}

static char *config_GetFooDir (const char *xdg_name, const char *xdg_default)
{
    char *psz_dir;
#if defined(WIN32) || defined(__APPLE__) || defined(SYS_BEOS)
    const char *psz_parent = GetDir (true, false);

    if( asprintf( &psz_dir, "%s" DIR_SEP CONFIG_DIR, psz_parent ) == -1 )
        psz_dir = NULL;

    (void)xdg_name; (void)xdg_default;
#else
    char var[sizeof ("XDG__HOME") + strlen (xdg_name)];
    /* XDG Base Directory Specification - Version 0.6 */
    snprintf (var, sizeof (var), "XDG_%s_HOME", xdg_name);

    const char *psz_home = getenv (var);
    psz_home = psz_home ? FromLocale (psz_home) : NULL;
    if( psz_home )
    {
        if( asprintf( &psz_dir, "%s/vlc", psz_home ) == -1 )
            psz_dir = NULL;
        LocaleFree (psz_home);
        return psz_dir;
    }

    /* Try HOME, then fallback to non-XDG dirs */
    psz_home = config_GetHomeDir();
    if( asprintf( &psz_dir, "%s/%s/vlc", psz_home, xdg_default ) == -1 )
        psz_dir = NULL;
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
