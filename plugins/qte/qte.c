/*****************************************************************************
 * qte.c : Qt Embedded video output
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: qte.c,v 1.1.2.1 2002/09/30 20:32:46 jpsaman Exp $
 *
 * Authors: Gerald Hansink <gerald.hansink@ordina.nl>
 *          Jean-Paul Saman <jpsaman@wxs.nl>
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
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                                /* strerror() */

#include <videolan/vlc.h>

void _M( vout_getfunctions )( function_list_t * p_function_list );

MODULE_CONFIG_START
ADD_CATEGORY_HINT( N_("Miscellaneous"), NULL )
ADD_STRING  ( "qte-display", NULL, NULL, NULL, NULL )
ADD_BOOL    ( "qte-altfullscreen", NULL, NULL, NULL, NULL )
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( _("Qt Embedded video output module") )
    ADD_CAPABILITY( VOUT, 150 )
    ADD_SHORTCUT( "qtevlc" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    _M( vout_getfunctions )( &p_module->p_functions->vout );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

