/*****************************************************************************
 * darwin_specific.m: Darwin specific features 
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: darwin_specific.m,v 1.6 2003/01/16 14:40:04 massiot Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

/*****************************************************************************
 * Static vars
 *****************************************************************************/
static char * psz_program_path;

/*****************************************************************************
 * system_Init: fill in program path & retrieve language
 *****************************************************************************/
static int FindLanguage( const char * psz_lang )
{
    const char * psz_short = NULL;

    if ( !strcmp(psz_lang, "German") )
    {
        psz_short = "de";
    }
    else if ( !strcmp(psz_lang, "British") )
    {
        psz_short = "en_GB";
    }
    else if ( !strcmp(psz_lang, "French") )
    {
        psz_short = "fr";
    }
    else if ( !strcmp(psz_lang, "Italian") )
    {
        psz_short = "it";
    }
    else if ( !strcmp(psz_lang, "Japanese") )
    {
        psz_short = "ja";
    }
    else if ( !strcmp(psz_lang, "Dutch") )
    {
        psz_short = "nl";
    }
    else if ( !strcmp(psz_lang, "no") )
    {
        psz_short = "no";
    }
    else if ( !strcmp(psz_lang, "pl") )
    {
        psz_short = "pl";
    }
    else if ( !strcmp(psz_lang, "ru") )
    {
        psz_short = "ru";
    }
    else if ( !strcmp(psz_lang, "sv") )
    {
        psz_short = "sv";
    }
    else if ( !strcmp(psz_lang, "English") )
    {
        psz_short = "C";
    }

    if ( psz_short != NULL )
    {
        setenv("LANG", psz_short, 1);
        return 1;
    }

    return 0;
}

void system_Init( vlc_t *p_this, int *pi_argc, char *ppsz_argv[] )
{
    char i_dummy;
    char *p_char, *p_oldchar = &i_dummy;

    /* Get the full program path and name */
    p_char = psz_program_path = strdup( ppsz_argv[ 0 ] );

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
        vlc_bool_t b_found = 0;
        NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];

        /* Retrieve user's preferences. */
        NSUserDefaults * o_defs = [NSUserDefaults standardUserDefaults]; 
        NSArray * o_languages = [o_defs objectForKey:@"AppleLanguages"]; 
        NSEnumerator * o_enumerator = [o_languages objectEnumerator]; 
        NSString * o_lang;

        while ( (o_lang = [o_enumerator nextObject]) )
        {
            if( !b_found )
            { 
                const char * psz_string = [o_lang lossyCString];
                if ( FindLanguage( psz_string ) )
                {
                    b_found = 1;
                }
            }
        }

        [o_languages release];
        [o_pool release];
    }
}

/*****************************************************************************
 * system_Configure: check for system specific configuration options.
 *****************************************************************************/
void system_Configure( vlc_t *p_this )
{

}

/*****************************************************************************
 * system_End: free the program path.
 *****************************************************************************/
void system_End( vlc_t *p_this )
{
    free( psz_program_path );
}

/*****************************************************************************
 * system_GetProgramPath: get the full path to the program.
 *****************************************************************************/
char * system_GetProgramPath( void )
{
    return( psz_program_path );
}

