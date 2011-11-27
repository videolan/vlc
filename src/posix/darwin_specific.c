
/*****************************************************************************
 * darwin_specific.m: Darwin specific features
 *****************************************************************************
 * Copyright (C) 2001-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Sam Hocevar <sam@zoy.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Pierre d'Herbemont <pdherbemont@free.fr>
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
#include <dirent.h>                                                /* *dir() */
#include <libgen.h>
#include <dlfcn.h>
#include <CoreFoundation/CoreFoundation.h>
#include <mach-o/dyld.h>

#ifdef HAVE_LOCALE_H
#   include <locale.h>
#endif

#ifndef MAXPATHLEN
# define MAXPATHLEN 1024
#endif

/*****************************************************************************
 * system_Init: fill in program path & retrieve language
 *****************************************************************************/
void system_Init(void)
{
    char i_dummy;
    char *p_char = NULL;
    char *p_oldchar = &i_dummy;
    unsigned int i;

    /* Get the full program path and name */
    /* First try to see if we are linked to the framework */
    for (i = 0; i < _dyld_image_count(); i++)
    {
        const char * psz_img_name = _dyld_get_image_name(i);
        /* Check for "VLCKit.framework/Versions/Current/VLCKit",
         * as well as "VLCKit.framework/Versions/A/VLCKit" and
         * "VLC.framework/Versions/B/VLCKit" */
        if( (p_char = strstr( psz_img_name, "VLCKit.framework/Versions/" )) )
        {
            /* Look for the next forward slash */
            p_char += 26; /* p_char += strlen(" VLCKit.framework/Versions/" ) */
            while( *p_char != '\0' && *p_char != '/')
                p_char++;

            /* If the string ends with VLC then we've found a winner */
            if ( !strcmp( p_char, "/VLCKit" ) )
            {
                p_char = strdup( psz_img_name );
                break;
            }
            else
                p_char = NULL;
        }
        else
        {
            size_t len = strlen(psz_img_name);
            /* Do we end by "VLC"? If so we are the legacy VLC.app that doesn't
             * link to VLCKit. */
            if( !strcmp( psz_img_name + len - 3, "VLC") )
            {
                p_char = strdup( psz_img_name );
                break;
            }
        }
    }
    if ( !p_char )
    {
        /* We are not linked to the VLC.framework, let's use dladdr to figure
         * libvlc path */
        Dl_info info;
        if( dladdr(system_Init, &info) )
            p_char = strdup(dirname( info.dli_fname ));
    }
    if( !p_char )
    {
        char path[MAXPATHLEN+1];
        uint32_t path_len = MAXPATHLEN;
        if ( !_NSGetExecutablePath(path, &path_len) )
            p_char = strdup(path);
    }

    free(psz_vlcpath);
    psz_vlcpath = p_char;

    /* Remove trailing program name */
    for( ; *p_char ; )
    {
        if( *p_char == '/' )
        {
            *p_oldchar = '/';
            *p_char = '\0';
            p_oldchar = p_char;
        }
        p_char++;
    }

#ifdef ENABLE_NLS
    /* Check if $LANG is set. */
    if( NULL == getenv("LANG") )
    {
        /*
           Retrieve the preferred language as chosen in  System Preferences.app
           (note that CFLocaleCopyCurrent() is not used because it returns the
            preferred locale not language)
        */
        CFArrayRef all_locales, preferred_locales;
        char psz_locale[50];

        all_locales = CFLocaleCopyAvailableLocaleIdentifiers();

        preferred_locales = CFBundleCopyLocalizationsForPreferences( all_locales, NULL );

        if ( preferred_locales )
        {
            if ( CFArrayGetCount( preferred_locales ) )
            {
                CFStringRef user_language_string_ref = CFArrayGetValueAtIndex( preferred_locales, 0 );
                CFStringGetCString( user_language_string_ref, psz_locale, sizeof(psz_locale), kCFStringEncodingUTF8 );
                setenv( "LANG", psz_locale, 1 );
            }
            CFRelease( preferred_locales );
        }
        CFRelease( all_locales );
    }
#endif
}

/*****************************************************************************
 * system_Configure: check for system specific configuration options.
 *****************************************************************************/
void system_Configure( libvlc_int_t *p_this,
                       int i_argc, const char *const ppsz_argv[] )
{
    (void)p_this;
    (void)i_argc;
    (void)ppsz_argv;
}

/*****************************************************************************
 * system_End: free the program path.
 *****************************************************************************/
void system_End( void )
{
    free( psz_vlcpath );
    psz_vlcpath = NULL;
}

