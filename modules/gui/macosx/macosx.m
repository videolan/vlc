/*****************************************************************************
 * macosx.m: MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001-2003 VideoLAN
 * $Id: macosx.m,v 1.15 2003/06/17 16:09:16 gbazin Exp $
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *          Eugenio Jarosiewicz <ej0@cise.ufl.edu>
 *          Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
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

int  E_(OpenVideo)    ( vlc_object_t * );
void E_(CloseVideo)   ( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define VDEV_TEXT N_("Video device")
#define VDEV_LONGTEXT N_("Choose a number corresponding to " \
    "a screen in you video device selection menu and this screen " \
    "will be used by default as the screen for 'fullscreen'.")

#define OPAQUENESS_TEXT N_("Opaqueness")
#define OPAQUENESS_LONGTEXT N_( \
    "Set the transparency of the video output. 1 is non-transparent (default) " \
    "0 is fully transparent.")
    
#define FLOAT_TEXT N_("Always float on top")
#define FLOAT_LONGTEXT N_( \
    "Let the video window float on top of other windows.")

vlc_module_begin();
    set_description( _("MacOS X interface, sound and video") );
    set_capability( "interface", 100 );
    set_callbacks( E_(OpenIntf), E_(CloseIntf) );
    add_submodule();
        set_capability( "video output", 200 );
        set_callbacks( E_(OpenVideo), E_(CloseVideo) );
        add_category_hint( N_("Video"), NULL, VLC_FALSE );
        add_integer( "macosx-vdev", 0, NULL, VDEV_TEXT, VDEV_LONGTEXT, VLC_FALSE );
        add_float_with_range( "macosx-opaqueness", 1, 0, 1, NULL, OPAQUENESS_TEXT,
            OPAQUENESS_LONGTEXT, VLC_TRUE );
        add_bool( "macosx-float", 0, NULL, FLOAT_TEXT, FLOAT_LONGTEXT, VLC_FALSE );
vlc_module_end();

