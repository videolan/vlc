/*****************************************************************************
 * tests.c: several test functions needed by the plugins
 * Functions are prototyped in tests.h.
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: tests.c,v 1.10 2001/12/19 03:50:22 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@via.ecp.fr>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <string.h>                                    /* memcpy(), memset() */

#include "common.h"
#include "intf_msg.h"
#include "tests.h"

/*****************************************************************************
 * TestProgram: tests if the given string equals the program name
 *****************************************************************************/
int TestProgram( char * psz_program )
{
    return( !strcmp( psz_program, p_main->psz_arg0 ) );
}

/*****************************************************************************
 * TestMethod: tests if the given method was requested
 *****************************************************************************/
int TestMethod( char * psz_var, char * psz_method )
{
    int   i_len = strlen( psz_method );
    int   i_index = 0;
    char *psz_requested = main_GetPszVariable( psz_var, "" );

    while( psz_requested[i_index] && psz_requested[i_index] != ':' )
    {
        i_index++;
    }

    if( i_index != i_len )
    {
        return 0;
    }

    return !strncmp( psz_method, psz_requested, i_len );
}

/*****************************************************************************
 * TestCPU: tests if the processor has MMX support and other capabilities
 *****************************************************************************/
int TestCPU( int i_capabilities )
{
    return( (i_capabilities & p_main->i_cpu_capabilities) == i_capabilities );
}

