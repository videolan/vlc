/*****************************************************************************
 * qnx.c : QNX RTOS plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
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
#include <string.h>

#include <videolan/vlc.h>

#include "video.h"
#include "video_output.h"

/*****************************************************************************
 * Capabilities defined in the other files.
 ******************************************************************************/
void _M( aout_getfunctions )( function_list_t * p_function_list );
void _M( vout_getfunctions )( function_list_t * p_function_list );

/*****************************************************************************
 * Building configuration tree
 *****************************************************************************/
MODULE_CONFIG_START
ADD_WINDOW( "Configuration for QNX RTOS module" )
    ADD_COMMENT( "For now, the QNX RTOS module cannot be configured" )
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( "QNX RTOS module" )
    ADD_CAPABILITY( VOUT, 100 )
    ADD_CAPABILITY( AOUT, 50 )
    ADD_SHORTCUT( "qnx" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    _M( aout_getfunctions )( &p_module->p_functions->aout );
    _M( vout_getfunctions )( &p_module->p_functions->vout );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP
