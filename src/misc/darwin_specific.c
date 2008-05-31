/*****************************************************************************
 * darwin_specific.m: Darwin specific features
 *****************************************************************************
 * Copyright (C) 2001-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Sam Hocevar <sam@zoy.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Pierre d'Herbemont <pdherbemont@free.fr>
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
#include <dirent.h>                                                /* *dir() */

#include <CoreFoundation/CoreFoundation.h>

#ifdef HAVE_LOCALE_H
#   include <locale.h>
#endif
#ifdef HAVE_MACH_O_DYLD_H
#   include <mach-o/dyld.h>
#endif

#ifndef MAXPATHLEN
# define MAXPATHLEN 1024
#endif

/* CFLocaleCopyAvailableLocaleIdentifiers is present only on post-10.4 */
extern CFArrayRef CFLocaleCopyAvailableLocaleIdentifiers(void) __attribute__((weak_import));

/* emulate CFLocaleCopyAvailableLocaleIdentifiers on pre-10.4 */
static CFArrayRef copy_all_locale_indentifiers(void)
{
    CFMutableArrayRef available_locales;
    DIR * dir;
    struct dirent *file;

    dir = opendir( "/usr/share/locale" );
    available_locales = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

    while ( (file = readdir(dir)) )
    {
        /* we should probably filter out garbage */
        /* we can't use CFStringCreateWithFileSystemRepresentation as it is
         * supported only on post-10.4 (and this function is only for pre-10.4) */
        CFStringRef locale = CFStringCreateWithCString( kCFAllocatorDefault, file->d_name, kCFStringEncodingUTF8 );
        CFArrayAppendValue( available_locales, (void*)locale );
        CFRelease( locale );
    }

    closedir( dir );
    return available_locales;
}

/*****************************************************************************
 * system_Init: fill in program path & retrieve language
 *****************************************************************************/
void system_Init( libvlc_int_t *p_this, int *pi_argc, const char *ppsz_argv[] )
{
    VLC_UNUSED(p_this);
    char i_dummy;
    char *p_char = NULL;
    char *p_oldchar = &i_dummy;
    unsigned int i;
    (void)pi_argc;

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
    }

    if( !p_char )
    {
        char path[MAXPATHLEN+1];
        uint32_t path_len = MAXPATHLEN;
        if ( !_NSGetExecutablePath(path, &path_len) )
            p_char = strdup(path);
    }
    if( !p_char )
    {
        /* We are not linked to the VLC.framework, return the executable path */
        p_char = strdup( ppsz_argv[ 0 ] );
    }

    vlc_global()->psz_vlcpath = p_char;

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

    /* Check if $LANG is set. */
    if ( (p_char = getenv("LANG")) == NULL )
    {
        /*
           Retrieve the preferred language as chosen in  System Preferences.app
           (note that CFLocaleCopyCurrent() is not used because it returns the
            preferred locale not language)
        */
        CFArrayRef all_locales, preferred_locales;
        char psz_locale[50];

        if( CFLocaleCopyAvailableLocaleIdentifiers )
            all_locales = CFLocaleCopyAvailableLocaleIdentifiers();
        else
            all_locales = copy_all_locale_indentifiers();

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
}

/*****************************************************************************
 * system_Configure: check for system specific configuration options.
 *****************************************************************************/
void system_Configure( libvlc_int_t *p_this, int *pi_argc, const char *ppsz_argv[] )
{
    (void)p_this;
    (void)pi_argc;
    (void)ppsz_argv;
}

/*****************************************************************************
 * system_End: free the program path.
 *****************************************************************************/
void system_End( libvlc_int_t *p_this )
{
    (void)p_this;
    free( vlc_global()->psz_vlcpath );
}

