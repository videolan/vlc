/*****************************************************************************
 * dummy.c : dummy plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: dummy.c,v 1.7 2003/05/15 22:27:37 massiot Exp $
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
#include <string.h>

#include <vlc/vlc.h>

#include "dummy.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define CHROMA_TEXT N_("Dummy image chroma format")
#define CHROMA_LONGTEXT N_( \
    "Force the dummy video output to create images using a specific chroma " \
    "format instead of trying to improve performances by using the most " \
    "efficient one.")

#ifdef WIN32
#define QUIET_TEXT N_("Don't open a dos command box interface")
#define QUIET_LONGTEXT N_( \
    "By default the dummy interface plugin will start a dos command box. " \
    "Enabling the quiet mode will not bring this command box but can also " \
    "be pretty annoying when you want to stop vlc and no video window is " \
    "opened." )
#endif

vlc_module_begin();
    set_description( _("dummy functions") );
    add_shortcut( "vlc" );
    add_submodule();
        set_description( _("dummy interface function") );
        set_capability( "interface", 0 );
        set_callbacks( E_(OpenIntf), NULL );
#ifdef WIN32
        add_category_hint( N_("Interface"), NULL, VLC_FALSE );
        add_bool( "dummy-quiet", 0, NULL, QUIET_TEXT, QUIET_LONGTEXT, VLC_FALSE );
#endif
    add_submodule();
        set_description( _("dummy access function") );
        set_capability( "access", 0 );
        set_callbacks( E_(OpenAccess), NULL );
    add_submodule();
        set_description( _("dummy demux function") );
        set_capability( "demux", 0 );
        set_callbacks( E_(OpenDemux), E_(CloseDemux) );
    add_submodule();
        set_description( _("dummy decoder function") );
        set_capability( "decoder", 0 );
        set_callbacks( E_(OpenDecoder), NULL );
    add_submodule();
        set_description( _("dummy audio output function") );
        set_capability( "audio output", 1 );
        set_callbacks( E_(OpenAudio), NULL );
    add_submodule();
        set_description( _("dummy video output function") );
        set_capability( "video output", 1 );
        set_callbacks( E_(OpenVideo), NULL );
        add_category_hint( N_("Video"), NULL, VLC_FALSE );
        add_string( "dummy-chroma", NULL, NULL, CHROMA_TEXT, CHROMA_LONGTEXT, VLC_FALSE );
vlc_module_end();

