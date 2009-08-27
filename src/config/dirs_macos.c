/*****************************************************************************
 * dirs_macos.c: MacOS directories configuration
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
#include <vlc_configuration.h>

#include <unistd.h>
#include <pwd.h>
#include <assert.h>
#include <limits.h>

const char *config_GetDataDir( void )
{
    static char path[PATH_MAX] = "";

#warning FIXME: thread-safety!
    if( *path == '\0' )
    {
        snprintf( path, sizeof( path ), "%s" DIR_SEP "share", psz_vlcpath );
        path[sizeof( path ) - 1] = '\0';
    }
    return path;
}

static const char *GetDir(void)
{
    /* FIXME: a full memory page here - quite a waste... */
    static char homedir[PATH_MAX] = "";

    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock (&lock);

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
    pthread_mutex_unlock (&lock);
    return homedir;
}

const char *config_GetConfDir( void )
{
    static char path[PATH_MAX] = "";

#warning FIXME: system config is not the same as shared app data...
    if( *path == '\0' )
    {
        snprintf( path, sizeof( path ), "%s" DIR_SEP "share", /* FIXME: Duh? */
                  psz_vlcpath );
        path[sizeof( path ) - 1] = '\0';
    }
    return path;
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
    /* 3/ Desperately try $TMP */
    if (home == NULL)
        home = getenv( "TMP" );
    /* 4/ Beyond hope, hard-code /tmp */
    if (home == NULL)
        home = "/tmp";

    return FromLocaleDup (home);
}

static char *config_GetAppDir (void)
{
    char *psz_dir;
    const char *psz_parent = GetDir ();

    if( asprintf( &psz_dir, "%s/Library/Preferences/VLC", psz_parent ) == -1 )
        psz_dir = NULL;
    return psz_dir;
}

char *config_GetUserDir (vlc_userdir_t type)
{
    switch (type)
    {
        case VLC_HOME_DIR:
            return config_GetHomeDir ();
        case VLC_CONFIG_DIR:
        case VLC_DATA_DIR:
        case VLC_CACHE_DIR:
            return config_GetAppDir ();

        case VLC_DESKTOP_DIR:
        case VLC_DOWNLOAD_DIR:
        case VLC_TEMPLATES_DIR:
        case VLC_PUBLICSHARE_DIR:
        case VLC_DOCUMENTS_DIR:
        case VLC_MUSIC_DIR:
        case VLC_PICTURES_DIR:
        case VLC_VIDEOS_DIR:
#warning FIXME not implemented
            return config_GetUserDir (VLC_HOME_DIR);;
    }
    assert (0);
}
