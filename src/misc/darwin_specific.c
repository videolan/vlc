/*****************************************************************************
 * darwin_specific.c: Darwin specific features 
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: darwin_specific.c,v 1.9 2002/05/19 23:51:37 massiot Exp $
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
#include <string.h>                                              /* strdup() */
#include <stdlib.h>                                                /* free() */

#include <videolan/vlc.h>

/*****************************************************************************
 * Static vars
 *****************************************************************************/
static char * psz_program_path;

/*****************************************************************************
 * system_Init: fill in program path.
 *****************************************************************************/
void system_Init( int *pi_argc, char *ppsz_argv[], char *ppsz_env[] )
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

    /* Run the interface with a real-time priority too */
    {
        struct sched_param param;
        param.sched_priority = 10;
        if (pthread_setschedparam(pthread_self(), SCHED_RR, &param))  
        {
            intf_ErrMsg("pthread_setschedparam failed");
        }
    }
}

/*****************************************************************************
 * system_Configure: check for system specific configuration options.
 *****************************************************************************/
void system_Configure( void )
{

}

/*****************************************************************************
 * system_End: free the program path.
 *****************************************************************************/
void system_End( void )
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

