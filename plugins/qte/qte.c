/*****************************************************************************
 * qte.c : Qt Embedded video output
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: qte.c,v 1.1.2.2 2002/11/26 19:54:12 jpsaman Exp $
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
#include <errno.h>                                             /* ENOMEM */
#include <stdlib.h>                                            /* free() */
#include <string.h>                                            /* strerror() */

#include <videolan/vlc.h>
#include "video.h"
#include "video_output.h"

#define WINDOW_TEXT N_("Qte video output size (video,fullscreen)")
#define WINDOW_LONGTEXT N_("Specify size output of the window to use.\nBy default the " \
     "video mode will be used ")
#define DISPLAY_TEXT N_("Qte video output display orientatione (portrait,landscape)")
#define DISPLAY_LONGTEXT N_( \
    "Specify the orientation for output display to use.\nBy default Qte video output will " \
    "use the portrait orientation.")

void _M( vout_getfunctions )( function_list_t * p_function_list );

MODULE_CONFIG_START
  ADD_CATEGORY_HINT( N_("Miscellaneous"), NULL )
  ADD_STRING  ( N_("qte-display"), "portrait", NULL, DISPLAY_TEXT, DISPLAY_LONGTEXT )
  ADD_STRING  ( N_("qte-window"), "video", NULL, WINDOW_TEXT, WINDOW_LONGTEXT )
MODULE_CONFIG_STOP

MODULE_INIT_START
  SET_DESCRIPTION( _("Qt Embedded video output module") )
  ADD_CAPABILITY( VOUT, 150 )
  ADD_SHORTCUT( "qte-vlc" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
  _M( vout_getfunctions )( &p_module->p_functions->vout );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

