/*****************************************************************************
 * darwin_specific.c: Darwin specific features 
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: darwin_specific.c,v 1.2 2001/04/13 14:33:22 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
#include "defs.h"

#include <string.h>                                              /* strdup() */
#include <stdlib.h>                                                /* free() */

#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "darwin_specific.h"

/*****************************************************************************
 * Static vars
 *****************************************************************************/
static char * psz_program_path;

void system_Create( int *pi_argc, char *ppsz_argv[], char *ppsz_env[] )
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
    
    return;
}

void system_Destroy( void )
{
    free( psz_program_path );
}

char * system_GetProgramPath( void )
{
    return( psz_program_path );
}

