/*****************************************************************************
 * tests.c: several test functions needed by the plugins
 * Functions are prototyped in tests.h.
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
 * $Id: tests.c,v 1.7 2001/05/30 17:03:12 sam Exp $
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

#include "config.h"
#include "common.h"

#include "intf_msg.h"
#include "tests.h"

#include "main.h"

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
    return( !strcmp( psz_method, main_GetPszVariable( psz_var, "" ) ) );
}

/*****************************************************************************
 * TestCPU: tests if the processor has MMX support and other capabilities
 *****************************************************************************/
int TestCPU( int i_capabilities )
{
    return( (i_capabilities & p_main->i_cpu_capabilities) == i_capabilities );
}

