/*****************************************************************************
 * motionmmxext.c : MMX EXT motion compensation module for vlc
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
 * $Id: motionmmxext.c,v 1.9 2001/07/11 02:01:05 sam Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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

#define MODULE_NAME motionmmxext
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>                                      /* malloc(), free() */

#include "config.h"
#include "common.h"                                     /* boolean_t, byte_t */
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "video.h"

#include "modules.h"
#include "modules_export.h"

/*****************************************************************************
 * Local and extern prototypes.
 *****************************************************************************/
void _M( motion_getfunctions )( function_list_t * p_function_list );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
ADD_WINDOW( "Configuration for motion compensation module" )
    ADD_COMMENT( "Ha, ha -- nothing to configure yet" )
MODULE_CONFIG_STOP

MODULE_INIT_START
    p_module->i_capabilities = MODULE_CAPABILITY_NULL
                                | MODULE_CAPABILITY_MOTION;
    p_module->psz_longname = "MMX EXT motion compensation module";
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    _M( motion_getfunctions )( &p_module->p_functions->motion );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * motion_Probe: tests probe the CPU and return a score
 *****************************************************************************/
int _M( motion_Probe )( probedata_t *p_data )
{
    if( !TestCPU( CPU_CAPABILITY_MMXEXT ) )
    {
        return( 0 );
    }

    if( TestMethod( MOTION_METHOD_VAR, "motionmmxext" )
         || TestMethod( MOTION_METHOD_VAR, "mmxext" ) )
    {
        return( 999 );
    }

    return( 200 );
}

