/*****************************************************************************
 * null.c : NULL module for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: null.c,v 1.3 2001/12/09 17:01:36 sam Exp $
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

#include "common.h"                                     /* boolean_t, byte_t */
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"

#include "modules.h"
#include "modules_inner.h"
#include "modules_export.h"

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
MODULE_CONFIG_STOP

MODULE_INIT_START
    p_module->i_capabilities = MODULE_CAPABILITY_NULL;
    p_module->psz_longname = "the Null module that does nothing";
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    /* Since the Null module can't do anything, there is no need to
     * fill the p_functions structure. */
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

