/*****************************************************************************
 * vcd.c : VCD input module for vlc
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
 * $Id: vcd.c,v 1.3 2001/12/09 17:01:37 sam Exp $
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

#define MODULE_NAME vcd
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */

#include "common.h"                                     /* boolean_t, byte_t */
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"

#include "modules.h"
#include "modules_export.h"

/*****************************************************************************
 * Capabilities defined in the other files.
 *****************************************************************************/
void _M( input_getfunctions )( function_list_t * p_function_list );

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
ADD_WINDOW( "Configuration for VCD module" )
    ADD_COMMENT( "foobar !" )
MODULE_CONFIG_STOP

MODULE_INIT_START
    p_module->i_capabilities = MODULE_CAPABILITY_NULL
                                | MODULE_CAPABILITY_INPUT;
    p_module->psz_longname = "basic VCD input module";
    
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    _M( input_getfunctions )( &p_module->p_functions->input );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

