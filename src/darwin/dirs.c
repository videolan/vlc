/*****************************************************************************
 * darwin_dirs.c: Darwin directories configuration
 *****************************************************************************
 * Copyright (C) 2001-2016 VLC authors and VideoLAN
 * Copyright (C) 2007-2012 Rémi Denis-Courmont
 *
 * Authors: Rémi Denis-Courmont
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          Pierre d'Herbemont <pdherbemont # videolan org>
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

#include <libgen.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>

#include <CoreFoundation/CoreFoundation.h>

char *config_GetLibDir (void)
{
    /* Get the full program path and name */
    /* First try to see if we are linked to the framework */
    for (unsigned i = 0; i < _dyld_image_count(); i++)
    {
        const char *psz_img_name = _dyld_get_image_name(i);
        const char *p = strstr( psz_img_name, "VLCKit.framework/Versions/" );

        /* Check for "VLCKit.framework/Versions/Current/VLCKit",
         * as well as "VLCKit.framework/Versions/A/VLCKit" and
         * "VLC.framework/Versions/B/VLCKit" */
        if (p != NULL) {
            /* Look for the next forward slash */
            p += 26; /* p_char += strlen(" VLCKit.framework/Versions/" ) */
            p += strcspn( p, "/" );

            /* If the string ends with VLCKit then we've found a winner */
            if (!strcmp( p, "/VLCKit"))
                return strdup( dirname(psz_img_name) );
        }

        /* Do we end by "VLC"? If so we are the legacy VLC.app that doesn't
         * link to VLCKit. */
        size_t len = strlen(psz_img_name);
        if (len >= 3 && !strcmp( psz_img_name + len - 3, "VLC"))
            return strdup( dirname(psz_img_name) );

        /* Do we end by "VLC-Plugin"? oh, we must be the NPAPI plugin */
        if (len >= 10 && !strcmp( psz_img_name + len - 10, "VLC-Plugin"))
            return strdup( dirname(psz_img_name) );

        /* Do we end by "VLC for iOS"? so we are the iOS app */
        if (len >= 11 && !strcmp( psz_img_name + len - 11, "VLC for iOS"))
            return strdup( dirname(psz_img_name) );

        /* Do we end by "VLC-TV"? so we are the tvOS app */
        if (len >= 6 && !strcmp( psz_img_name + len - 6, "VLC-TV"))
            return strdup( dirname(psz_img_name) );
    }

    /* we are not part of any Mac-style package but were installed
     * the UNIX way. let's trick-around a bit */
    Dl_info info;
    if (dladdr(system_Init, &info)) {
        char *incompletepath = strdup(dirname( (char *)info.dli_fname ));
        char *path = NULL;
        asprintf(&path, "%s/"PACKAGE, incompletepath);
        free(incompletepath);
        return path;
    }

    /* should never happen */
    abort ();
}

char *config_GetDataDir (void)
{
    const char *path = getenv ("VLC_DATA_PATH");
    if (path)
        return strdup (path);

    char *vlcpath = config_GetLibDir ();
    char *datadir;

    if (asprintf (&datadir, "%s/share", vlcpath) == -1)
        datadir = NULL;

    free (vlcpath);
    return datadir;
}

static char *config_GetHomeDir (void)
{
    const char *home = getenv ("HOME");

    if (home == NULL)
        home = "/tmp";

    return strdup (home);
}

static char *getAppDependentDir(vlc_userdir_t type)
{
    const char *psz_path;
    switch (type) {
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
            vlc_assert_unreachable();
            break;
    }

    // Default fallback
    const char *fallback = "org.videolan.vlc";
    char *name = NULL;

    CFBundleRef mainBundle = CFBundleGetMainBundle();
    if (mainBundle) {
        CFStringRef identifierAsNS = CFBundleGetIdentifier(mainBundle);
        if (identifierAsNS) {
            CFIndex len = CFStringGetLength(identifierAsNS);
            CFIndex size = CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF8);
            char *identifier = calloc(len + 1, sizeof(char));
            if (identifier != NULL) {
                Boolean ret = CFStringGetCString(identifierAsNS, identifier, size, kCFStringEncodingUTF8);
                if (ret)
                    name = identifier;
            }
        }
    }

    char *psz_parent = config_GetHomeDir ();
    char *psz_dir;
    if ( asprintf( &psz_dir, psz_path, psz_parent, (name) ? name : fallback) == -1 )
        psz_dir = NULL;
    free(psz_parent);
    free(name);

    return psz_dir;
}

char *config_GetUserDir (vlc_userdir_t type)
{
    const char *psz_path;
    switch (type) {
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
    char *psz_parent = config_GetHomeDir();
    char *psz_dir;
    if (asprintf( &psz_dir, psz_path, psz_parent ) == -1)
        psz_dir = NULL;
    free(psz_parent);
    return psz_dir;
}
