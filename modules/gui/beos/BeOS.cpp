/*****************************************************************************
 * beos.cpp : BeOS plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: BeOS.cpp,v 1.4 2003/01/25 20:15:41 titer Exp $
 *
 * Authors: Jean-Marc Dressler <polux@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
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

/*****************************************************************************
 * External prototypes
 *****************************************************************************/
int  E_(OpenIntf)     ( vlc_object_t * );
void E_(CloseIntf)    ( vlc_object_t * );

int  E_(OpenAudio)    ( vlc_object_t * );
void E_(CloseAudio)   ( vlc_object_t * );

int  E_(OpenVideo)    ( vlc_object_t * );
void E_(CloseVideo)   ( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("BeOS standard API module") );
    add_submodule();
        set_capability( "interface", 100 );
        set_callbacks( E_(OpenIntf), E_(CloseIntf) );
        add_integer( "beos-intf-width", 0, NULL, "", "" );
        add_integer( "beos-intf-height", 0, NULL, "", "" );
        add_integer( "beos-intf-xpos", 0, NULL, "", "" );
        add_integer( "beos-intf-ypos", 0, NULL, "", "" );
        add_integer( "beos-playlist-width", 0, NULL, "", "" );
        add_integer( "beos-playlist-height", 0, NULL, "", "" );
        add_integer( "beos-playlist-xpos", 0, NULL, "", "" );
        add_integer( "beos-playlist-ypos", 0, NULL, "", "" );
        add_bool( "beos-playlist-show", 0, NULL, "", "" );
        add_bool( "beos-messages-show", 0, NULL, "", "" );
    add_submodule();                                     
        set_capability( "video output", 100 );
        set_callbacks( E_(OpenVideo), E_(CloseVideo) );
    add_submodule();
        set_capability( "audio output", 100 );
        set_callbacks( E_(OpenAudio), E_(CloseAudio) );
vlc_module_end();
