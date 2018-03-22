/*****************************************************************************
 * dirs.c: OS/2 directories configuration
 *****************************************************************************
 * Copyright (C) 2010 VLC authors and VideoLAN
 *
 * Authors: KO Myung-Hun <komh@chollian.net>
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
#include <vlc_charset.h>
#include "config/configuration.h"

#include <assert.h>

static const char *config_GetRelDir( const char *dir )
{
    static int prefixlen = -1;

    if( prefixlen == -1 )
    {
        prefixlen = 0;
        while( LIBDIR[ prefixlen ] && SYSDATADIR[ prefixlen ]
               && LIBDIR[ prefixlen ] == SYSDATADIR[ prefixlen])
            prefixlen++;
    }

    return dir + prefixlen;
}

static const char *config_GetBaseDir( void )
{
    static CHAR basedir[ CCHMAXPATH + 4 ] = "";

    if( basedir[ 0 ] == '\0')
    {
        HMODULE hmod;

        DosQueryModFromEIP( &hmod, NULL, 0, NULL, NULL, ( ULONG )system_Init );
        DosQueryModuleName( hmod, sizeof( basedir ), basedir );

        /* remove \ + the DLL name */
        char *slash = strrchr( basedir, '\\');
        *slash = '\0';

        /* remove lib dir name */
        slash = strrchr( basedir, '\\');
        if( slash == NULL )
            abort();
        slash[ 1 ] = '\0';
    }

    return basedir;
}

static char *config_GetRealDir( const char *dir )
{
    char realdir[ CCHMAXPATH + 4 ];

    snprintf( realdir, sizeof( realdir ), "%s%s",
              config_GetBaseDir(), config_GetRelDir( dir ));

    return FromLocaleDup( realdir );
}

char *config_GetLibDir( void )
{
    const char *path = getenv ("VLC_LIB_PATH");
    if (path)
        return FromLocaleDup( path );

    return config_GetRealDir( PKGLIBDIR );
}

static char *config_GetLibExecDir(void)
{
    const char *path = getenv ("VLC_LIB_PATH");
    if (path)
        return FromLocaleDup( path );

    return config_GetRealDir( PKGLIBEXECDIR );
}

/**
 * Determines the shared data directory
 *
 * @return a nul-terminated string or NULL. Use free() to release it.
 */
static char *config_GetDataDir(void)
{
    const char *path = getenv ("VLC_DATA_PATH");
    if (path)
        return FromLocaleDup( path );

    return config_GetRealDir( PKGDATADIR );
}

char *config_GetSysPath(vlc_sysdir_t type, const char *filename)
{
    char *dir = NULL;

    switch (type)
    {
        case VLC_PKG_DATA_DIR:
            dir = config_GetDataDir();
            break;
        case VLC_PKG_LIB_DIR:
            dir = config_GetLibDir();
            break;
        case VLC_PKG_LIBEXEC_DIR:
            dir = config_GetLibExecDir();
            break;
        case VLC_SYSDATA_DIR:
            dir = config_GetRealDir( SYSDATADIR );
            break;
        case VLC_LOCALE_DIR:
            dir = config_GetRealDir( LOCALEDIR );
            break;
        default:
            vlc_assert_unreachable();
    }

    if (filename == NULL || unlikely(dir == NULL))
        return dir;

    char *path;
    asprintf(&path, "%s/%s", dir, filename);
    free(dir);
    return path;
}

static char *config_GetHomeDir (void)
{
    const char *home = getenv ("HOME");
    if (home != NULL)
        return FromLocaleDup (home);

    return config_GetLibDir();
}

char *config_GetUserDir (vlc_userdir_t type)
{
    switch (type)
    {
        case VLC_HOME_DIR:
        case VLC_CONFIG_DIR:
        case VLC_USERDATA_DIR:
        case VLC_CACHE_DIR:
        case VLC_DESKTOP_DIR:
        case VLC_DOWNLOAD_DIR:
        case VLC_TEMPLATES_DIR:
        case VLC_PUBLICSHARE_DIR:
        case VLC_DOCUMENTS_DIR:
        case VLC_MUSIC_DIR:
        case VLC_PICTURES_DIR:
        case VLC_VIDEOS_DIR:
            break;
    }
    return config_GetHomeDir ();
}
