/*****************************************************************************
 * win32.cpp : Win32 interface plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr> 
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

#include <vlc/vlc.h>

#include "win32.h"                                       /* Borland specific */

/*****************************************************************************
 * External prototypes
 *****************************************************************************/
int  E_(Open)  ( vlc_object_t * );
void E_(Close) ( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define MAX_LINES_TEXT N_("maximum number of lines in the log window")
#define MAX_LINES_LONGTEXT N_( \
    "You can set the maximum number of lines that the log window will display."\
    " Enter -1 if you want to keep all messages." )

vlc_module_begin();
    add_category_hint( N_("Miscellaneous"), NULL );
    add_integer( "intfwin-max-lines", 500, NULL, MAX_LINES_TEXT, MAX_LINES_LONGTEXT );
    set_description( _("Win32 interface module") );
    set_capability( "interface", 100 );
    set_callbacks( E_(Open), E_(Close) );
    add_shortcut( "win" );
    add_shortcut( "win32" );
vlc_module_end();

