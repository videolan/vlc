/*****************************************************************************
 * memcpy3dn.c : 3D Now! memcpy module
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: memcpy3dn.c,v 1.1 2001/12/03 16:18:37 sam Exp $
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

#define MODULE_NAME memcpy3dn
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "common.h"
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "modules.h"
#include "modules_export.h"

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
static void memcpy_getfunctions( function_list_t * p_function_list );
static int  memcpy_Probe       ( probedata_t *p_data );
void *      _M( fast_memcpy )  ( void * to, const void * from, size_t len );

#undef HAVE_MMX
#undef HAVE_MMX2
#undef HAVE_SSE
#undef HAVE_SSE2
#define HAVE_3DNOW
#include "fastmemcpy.h"

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
ADD_WINDOW( "Configuration for 3D Now! memcpy module" )
    ADD_COMMENT( "Ha, ha -- nothing to configure yet" )
MODULE_CONFIG_STOP

MODULE_INIT_START
    p_module->i_capabilities = MODULE_CAPABILITY_NULL
                                | MODULE_CAPABILITY_MEMCPY;
    p_module->psz_longname = "3D Now! memcpy module";
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    memcpy_getfunctions( &p_module->p_functions->memcpy );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/* Following functions are local */

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static void memcpy_getfunctions( function_list_t * p_function_list )
{
    p_function_list->pf_probe = memcpy_Probe;
#define F p_function_list->functions.memcpy
    F.pf_fast_memcpy = _M( fast_memcpy );
#undef F
}

/*****************************************************************************
 * memcpy_Probe: returns a preference score
 *****************************************************************************/
static int memcpy_Probe( probedata_t *p_data )
{
    /* Test for 3D Now! support in the CPU */
    if( !TestCPU( CPU_CAPABILITY_3DNOW ) )
    {
        return( 0 );
    }

    if( TestMethod( MEMCPY_METHOD_VAR, "memcpy3dn" )
         || TestMethod( MEMCPY_METHOD_VAR, "3dn" ) )
    {
        return( 999 );
    }

    /* This plugin always works */
    return( 100 );
}

