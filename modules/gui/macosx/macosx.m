/*****************************************************************************
 * macosx.m: MacOS X plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001-2003 VideoLAN
 * $Id: macosx.m,v 1.5 2003/03/30 18:14:38 gbazin Exp $
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

int  E_(OpenAudio)    ( vlc_object_t * );
void E_(CloseAudio)   ( vlc_object_t * );

int  E_(OpenVideo)    ( vlc_object_t * );
void E_(CloseVideo)   ( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ADEV_TEXT N_("audio device")
#define VDEV_TEXT N_("video device")

vlc_module_begin();
    set_description( _("MacOS X interface, sound and video") );
    add_submodule();
        set_capability( "interface", 100 );
        set_callbacks( E_(OpenIntf), E_(CloseIntf) );
    add_submodule();
        set_capability( "video output", 200 );
        set_callbacks( E_(OpenVideo), E_(CloseVideo) );
        add_category_hint( N_("Video"), NULL, VLC_FALSE );
        add_integer( "macosx-vdev", 0, NULL, VDEV_TEXT, VDEV_TEXT, VLC_FALSE );
    add_submodule();
        set_capability( "audio output", 100 );
        set_callbacks( E_(OpenAudio), E_(CloseAudio) );
        add_category_hint( N_("Audio"), NULL, VLC_FALSE );
        add_integer( "macosx-adev", -1, NULL, ADEV_TEXT, ADEV_TEXT, VLC_FALSE );
vlc_module_end();

