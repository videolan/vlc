/*****************************************************************************
 * kde.cpp : KDE plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: kde.cpp,v 1.4 2001/11/28 15:08:05 massiot Exp $
 *
 * Authors: Andres Krapf <dae@chez.com> Sun Mar 25 2001
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
#define MODULE_NAME kde 
#include "intf_plugin.h"

extern "C"
{

/*****************************************************************************
 * Capabilities defined in the other files.
 *****************************************************************************/
void _M( intf_getfunctions )( function_list_t * p_function_list );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
ADD_WINDOW( "Configuration for KDE module" )
    ADD_COMMENT( "Ha, ha -- nothing to configure yet" )
MODULE_CONFIG_STOP

MODULE_INIT_START
    p_module->i_capabilities = MODULE_CAPABILITY_NULL
                                | MODULE_CAPABILITY_INTF;
    p_module->psz_longname = "KDE interface module";
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    _M( intf_getfunctions )( &p_module->p_functions->intf );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

} /* extern "C" */

