/*****************************************************************************
 * dvdread.c : DvdRead input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: dvdread.c,v 1.13 2002/03/06 16:39:37 stef Exp $
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
 * Capabilities defined in the other files.
 *****************************************************************************/
void _M( access_getfunctions)( function_list_t * p_function_list );
void _M( demux_getfunctions)( function_list_t * p_function_list );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
ADD_CATEGORY_HINT( "[dvdread:][device][@[title][,[chapter][,angle]]]", NULL )
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( "DVDRead input module" )
    ADD_CAPABILITY( DEMUX, 0 )
    ADD_CAPABILITY( ACCESS, 110 )
    ADD_SHORTCUT( "dvdread" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    _M( access_getfunctions)( &p_module->p_functions->access );
    _M( demux_getfunctions)( &p_module->p_functions->demux );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP
