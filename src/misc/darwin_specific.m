/*****************************************************************************
 * darwin_specific.m: Darwin specific features
 *****************************************************************************
 * Copyright (C) 2001-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Sam Hocevar <sam@zoy.org>
 *          Christophe Massiot <massiot@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/
#include <string.h>                                              /* strdup() */
#include <stdlib.h>                                                /* free() */

#include <vlc/vlc.h>

#include <Cocoa/Cocoa.h>
#include <CoreFoundation/CFString.h>

#ifdef HAVE_LOCALE_H
#   include <locale.h>
#endif

/*****************************************************************************
 * system_Init: fill in program path & retrieve language
 *****************************************************************************/
static int FindLanguage( const char * psz_lang )
{
    const char ** ppsz_parser;
    const char * ppsz_all[] =
    {
        "Catalan", "ca",
        "Danish", "da",
        "German", "de",
        "British", "en_GB",
        "English", "en",
        "Spanish", "es",
        "French", "fr",
        "Italian", "it",
        "Japanese", "ja",
        "Korean", "ko",
        "Dutch", "nl",
        "Brazillian Portuguese", "pt_BR",
        "Romanian", "ro",
        "Russian", "ru",
        "Turkish", "tr",
        "Chinese Traditional", "zh_TW",
        NULL
    };

    for( ppsz_parser = ppsz_all ; ppsz_parser[0] ; ppsz_parser += 2 )
    {
        if( !strcmp( psz_lang, ppsz_parser[0] )
             || !strcmp( psz_lang, ppsz_parser[1] ) )
        {
            setenv( "LANG", ppsz_parser[1], 1 );
            return 1;
        }
    }

    return 0;
}

void system_Init( vlc_t *p_this, int *pi_argc, char *ppsz_argv[] )
{
    char i_dummy;
    char *p_char, *p_oldchar = &i_dummy;

    /* Get the full program path and name */
    p_char = p_this->p_libvlc->psz_vlcpath = strdup( ppsz_argv[ 0 ] );

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
        NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];

        /* Retrieve user's preferences. */
        NSUserDefaults * o_defs = [NSUserDefaults standardUserDefaults];
        NSArray * o_languages = [o_defs objectForKey:@"AppleLanguages"];
        NSEnumerator * o_enumerator = [o_languages objectEnumerator];
        NSString * o_lang;

        while ( (o_lang = [o_enumerator nextObject]) )
        {
            const char * psz_string = [o_lang lossyCString];
            if ( FindLanguage( psz_string ) )
            {
                break;
            }
        }

        [o_pool release];
    }

    vlc_mutex_init( p_this, &p_this->p_libvlc->iconv_lock );
    p_this->p_libvlc->iconv_macosx = vlc_iconv_open( "UTF-8", "UTF-8-MAC" );
}

/*****************************************************************************
 * system_Configure: check for system specific configuration options.
 *****************************************************************************/
void system_Configure( vlc_t *p_this, int *pi_argc, char *ppsz_argv[] )
{

}

/*****************************************************************************
 * system_End: free the program path.
 *****************************************************************************/
void system_End( vlc_t *p_this )
{
    free( p_this->p_libvlc->psz_vlcpath );

    if ( p_this->p_libvlc->iconv_macosx != (vlc_iconv_t)-1 )
        vlc_iconv_close( p_this->p_libvlc->iconv_macosx );
    vlc_mutex_destroy( &p_this->p_libvlc->iconv_lock );
}

