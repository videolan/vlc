/*****************************************************************************
 * dvd.c : dvdplay module for vlc
 *****************************************************************************
 * This plugins should handle all the known specificities of the DVD format,
 * especially the 2048 bytes logical block size.
 * It depends on: libdvdplay for ifo files and block reading.
 *****************************************************************************
 *    
 * Copyright (C) 2001 VideoLAN
 * $Id: dvd.c,v 1.6 2003/06/17 16:09:16 gbazin Exp $
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

#include <vlc/vlc.h>

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
int  E_(OpenDVD)   ( vlc_object_t * );
void E_(CloseDVD)  ( vlc_object_t * );
int  E_(InitDVD)   ( vlc_object_t * );
void E_(EndDVD)    ( vlc_object_t * );
int  E_(OpenIntf)  ( vlc_object_t * );
void E_(CloseIntf) ( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    add_usage_hint( N_("[dvdplay:][device][@[title][,[chapter][,angle]]]") );
    set_description( _("DVD input with menus support") );
    set_capability( "access", 120 );
    set_callbacks( E_(OpenDVD), E_(CloseDVD) );
    add_shortcut( "dvd" );
    add_submodule();
        set_capability( "demux", 0 );
        set_callbacks( E_(InitDVD), E_(EndDVD) );
    add_submodule();
        set_capability( "interface", 0 );
        set_callbacks( E_(OpenIntf), E_(CloseIntf) );
vlc_module_end();

