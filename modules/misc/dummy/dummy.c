/*****************************************************************************
 * dummy.c : dummy plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2001 the VideoLAN team
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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

#define SAVE_TEXT N_("Save raw codec data")
#define SAVE_LONGTEXT N_( \
    "This option allows you to save the raw codec data if you have " \
    "selected/forced the dummy decoder in the main options." )

#ifdef WIN32
#define QUIET_TEXT N_("Do not open a DOS command box interface")
#define QUIET_LONGTEXT N_( \
    "By default the dummy interface plugin will start a DOS command box. " \
    "Enabling the quiet mode will not bring this command box but can also " \
    "be pretty annoying when you want to stop VLC and no video window is " \
    "open." )
#endif

vlc_module_begin();
    set_shortname( _("Dummy"));
    set_description( _("Dummy interface function") );
    set_capability( "interface", 0 );
    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_GENERAL );
    add_shortcut( "vlc" );
    set_callbacks( E_(OpenIntf), NULL );
#ifdef WIN32
    set_section( N_( "Dummy Interface" ), NULL );
    add_category_hint( N_("Interface"), NULL, VLC_FALSE );
    add_bool( "dummy-quiet", 0, NULL, QUIET_TEXT, QUIET_LONGTEXT, VLC_FALSE );
#endif
    add_submodule();
        set_description( _("Dummy access function") );
        set_capability( "access2", 0 );
        set_callbacks( E_(OpenAccess), NULL );
    add_submodule();
        set_description( _("Dummy demux function") );
        set_capability( "demux2", 0 );
        set_callbacks( E_(OpenDemux), E_(CloseDemux) );
    add_submodule();
        set_section( N_( "Dummy decoder" ), NULL );
        set_description( _("Dummy decoder function") );
        set_capability( "decoder", 0 );
        set_callbacks( E_(OpenDecoder), E_(CloseDecoder) );
        add_bool( "dummy-save-es", 0, NULL, SAVE_TEXT, SAVE_LONGTEXT, VLC_TRUE );
    add_submodule();
        set_description( _("Dummy encoder function") );
        set_capability( "encoder", 0 );
        set_callbacks( E_(OpenEncoder), E_(CloseEncoder) );
    add_submodule();
        set_description( _("Dummy audio output function") );
        set_capability( "audio output", 1 );
        set_callbacks( E_(OpenAudio), NULL );
    add_submodule();
        set_description( _("Dummy video output function") );
        set_section( N_( "Dummy Video output" ), NULL );
        set_capability( "video output", 1 );
        set_callbacks( E_(OpenVideo), NULL );
        add_category_hint( N_("Video"), NULL, VLC_FALSE );
        add_string( "dummy-chroma", NULL, NULL, CHROMA_TEXT, CHROMA_LONGTEXT, VLC_TRUE );
    add_submodule();
        set_description( _("Dummy font renderer function") );
        set_capability( "text renderer", 1 );
        set_callbacks( E_(OpenRenderer), NULL );
vlc_module_end();

