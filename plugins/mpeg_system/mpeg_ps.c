/*****************************************************************************
 * mpeg_ps.c : Program Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: mpeg_ps.c,v 1.3 2002/02/15 13:32:53 sam Exp $
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */

#include <videolan/vlc.h>

/*****************************************************************************
 * Capabilities defined in the other files.
 *****************************************************************************/
void _M( input_getfunctions )( function_list_t * p_function_list );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( "ISO 13818-1 MPEG Program Stream input" )
    ADD_CAPABILITY( INPUT, 10 )
    ADD_SHORTCUT( "ps" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    _M( input_getfunctions )( &p_module->p_functions->input );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

