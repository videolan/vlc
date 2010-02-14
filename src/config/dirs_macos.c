/*****************************************************************************
 * dirs_macos.c: Mac OS X directories configuration
 *****************************************************************************
 * Copyright (C) 2001-2009 the VideoLAN team
 * Copyright © 2007-2009 Rémi Denis-Courmont
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
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

#include <CoreFoundation/CoreFoundation.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include "../libvlc.h"
#include <vlc_charset.h>
#include <vlc_configuration.h>
#include "configuration.h"

static char *configdir = NULL;

static pthread_once_t once = PTHREAD_ONCE_INIT;

static void init_dirs( void )
{
    configdir = config_GetUserDir(VLC_CONFIG_DIR);
}

const char *config_GetConfDir( void )
{
    pthread_once(&once, init_dirs);
    return configdir;
}

char *config_GetDataDirDefault (void)
{
    char *datadir;

    if (asprintf (&datadir, "%s/share", psz_vlcpath) == -1)
        return NULL;
    return datadir;
}

const char *config_GetLibDir (void)
{
    abort ();
}

static char *config_GetHomeDir (void)
{
    const char *home = getenv ("HOME");

    if (home == NULL)
        home = "/tmp";

    return FromLocaleDup (home);
}

static char *getAppDependentDir(vlc_userdir_t type)
{
    const char *psz_path;
    switch (type)
    {
        case VLC_CONFIG_DIR:
            psz_path = "%s/Library/Preferences/%s";
            break;
        case VLC_TEMPLATES_DIR:
        case VLC_DATA_DIR:
            psz_path = "%s/Library/Application Support/%s";
            break;
        case VLC_CACHE_DIR:
            psz_path = "%s/Library/Caches/%s";
            break;
        default:
            assert(0);
            break;
    }

    // Default fallback
    const char *name = "org.videolan.vlc";

    CFBundleRef mainBundle = CFBundleGetMainBundle();
    if (mainBundle)
    {
        CFStringRef identifierAsNS = CFBundleGetIdentifier(mainBundle);
        if (identifierAsNS)
        {
            char identifier[256];
            Boolean ret = CFStringGetCString(identifierAsNS, identifier, sizeof(identifier), kCFStringEncodingUTF8);
            if (ret)
                name = identifier;            
        }
    }

    char *psz_parent = config_GetHomeDir ();
    char *psz_dir;
    if( asprintf( &psz_dir, psz_path, psz_parent, name) == -1 )
        psz_dir = NULL;
    free(psz_parent);

    return psz_dir;    
}

char *config_GetUserDir (vlc_userdir_t type)
{
    const char *psz_path;
    switch (type)
    {
        case VLC_CONFIG_DIR:
        case VLC_TEMPLATES_DIR:
        case VLC_DATA_DIR:
        case VLC_CACHE_DIR:
            return getAppDependentDir(type);

        case VLC_DESKTOP_DIR:
            psz_path = "%s/Desktop";
            break;
        case VLC_DOWNLOAD_DIR:
            psz_path = "%s/Downloads";
            break;
        case VLC_DOCUMENTS_DIR:
            psz_path = "%s/Documents";
            break;
        case VLC_MUSIC_DIR:
            psz_path = "%s/Music";
            break;
        case VLC_PICTURES_DIR:
            psz_path = "%s/Pictures";
            break;
        case VLC_VIDEOS_DIR:
            psz_path = "%s/Movies";
            break;
        case VLC_PUBLICSHARE_DIR:
            psz_path = "%s/Public";
            break;
        case VLC_HOME_DIR:
        default:
            psz_path = "%s";
    }
    char *psz_parent = config_GetHomeDir ();
    char *psz_dir;
    if( asprintf( &psz_dir, psz_path, psz_parent ) == -1 )
        psz_dir = NULL;
    free(psz_parent);
    return psz_dir;
}
