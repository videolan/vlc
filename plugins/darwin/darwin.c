/*****************************************************************************
 * darwin.c : Darwin plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: darwin.c,v 1.5 2001/05/30 17:03:12 sam Exp $
 *
 * Authors: Colin Delacroix <colin@zoy.org>
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

#define MODULE_NAME darwin
#include "modules_inner.h"

/* CD (2001/04/06):
 * This module was written to handle audio output when we thought that 
 * CoreAudio was in Darwin. It currently does nothing.
 * All the audio output code has been moved to the macosx plugin.
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>                                      /* malloc(), free() */

#include "config.h"
#include "common.h"                                     /* boolean_t, byte_t */
#include "threads.h"
#include "mtime.h"

#include "modules.h"

/*****************************************************************************
 * Capabilities defined in the other files.
 *****************************************************************************/
// void _M( aout_getfunctions )( function_list_t * p_function_list );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
ADD_WINDOW( "Configuration for Darwin module" )
    ADD_COMMENT( "Ha, ha -- nothing to configure yet" )
MODULE_CONFIG_STOP

MODULE_INIT_START
    p_module->i_capabilities = MODULE_CAPABILITY_NULL;
    p_module->psz_longname = "Darwin support module";
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    // _M( aout_getfunctions )( &p_module->p_functions->aout );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

