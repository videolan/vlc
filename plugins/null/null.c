/*****************************************************************************
 * null.c : NULL module for vlc
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
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

#define MODULE_NAME null

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */

#include "config.h"
#include "common.h"                                     /* boolean_t, byte_t */
#include "threads.h"
#include "mtime.h"

#include "modules.h"
#include "modules_inner.h"

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
ADD_WINDOW( "Configuration for null module" )
    ADD_PANE( "First pane" )
        ADD_FRAME( "First frame" )
            ADD_COMMENT( "You can put whatever you want here." )
            ADD_STRING( "Random text: ", MODULE_VAR(text), NULL )
        ADD_FRAME( "Second frame" )
            ADD_COMMENT( "The file below is not used." )
            ADD_FILE( "Select file: ", MODULE_VAR(file), NULL )
        ADD_FRAME( "Third frame" )
            ADD_COMMENT( "This space intentionally left blank." )
    ADD_PANE( "Second pane" )
        ADD_FRAME( "Frame" )
            ADD_COMMENT( "There is nothing in this frame." )
MODULE_CONFIG_END

/*****************************************************************************
 * InitModule: get the module structure and configuration.
 *****************************************************************************
 * We have to fill psz_name, psz_longname and psz_version. These variables
 * will be strdup()ed later by the main application because the module can
 * be unloaded later to save memory, and we want to be able to access this
 * data even after the module has been unloaded.
 *****************************************************************************/
MODULE_INIT
{
    p_module->psz_name = MODULE_STRING;
    p_module->psz_longname = "the Null module that does nothing";
    p_module->psz_version = VERSION;

    p_module->i_capabilities = MODULE_CAPABILITY_NULL;

    return( 0 );
}

/*****************************************************************************
 * ActivateModule: set the module to an usable state.
 *****************************************************************************
 * This function fills the capability functions and the configuration
 * structure. Once ActivateModule() has been called, the i_usage can
 * be set to 0 and calls to NeedModule() be made to increment it. To unload
 * the module, one has to wait until i_usage == 0 and call DeactivateModule().
 *****************************************************************************/
MODULE_ACTIVATE
{
    /* Since the Null module can't do anything, there is no need to
     * fill the p_functions structure. */
    p_module->p_functions = NULL;
    p_module->p_config = p_config;

    return( 0 );
}

/*****************************************************************************
 * DeactivateModule: make sure the module can be unloaded.
 *****************************************************************************
 * This function must only be called when i_usage == 0. If it successfully
 * returns, i_usage can be set to -1 and the module unloaded. Be careful to
 * lock usage_lock during the whole process.
 *****************************************************************************/
MODULE_DEACTIVATE
{
    /* We didn't allocate p_functions - so we don't have to free it */
    return( 0 );
}

