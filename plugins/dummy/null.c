/*****************************************************************************
 * null.c : NULL module for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: null.c,v 1.4 2001/12/30 07:09:55 sam Exp $
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */

#include <videolan/vlc.h>

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
    SET_DESCRIPTION( "the Null module that does nothing" )
    ADD_SHORTCUT( "null" )
MODULE_INIT_STOP


MODULE_ACTIVATE_START
    /* Since the Null module can't do anything, there is no need to
     * fill the p_functions structure. */
MODULE_ACTIVATE_STOP


MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

