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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>

#include "dummy.h"

static int OpenDummy(vlc_object_t *);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#ifdef WIN32
#define QUIET_TEXT N_("Do not open a DOS command box interface")
#define QUIET_LONGTEXT N_( \
    "By default the dummy interface plugin will start a DOS command box. " \
    "Enabling the quiet mode will not bring this command box but can also " \
    "be pretty annoying when you want to stop VLC and no video window is " \
    "open." )
#endif

vlc_module_begin ()
    set_shortname( N_("Dummy"))
    set_description( N_("Dummy interface function") )
    set_capability( "interface", 0 )
    set_callbacks( OpenIntf, NULL )
#ifdef WIN32
    set_section( N_( "Dummy Interface" ), NULL )
    add_category_hint( N_("Interface"), NULL, false )
    add_bool( "dummy-quiet", false, QUIET_TEXT, QUIET_LONGTEXT, false )
#endif
    add_submodule ()
        set_description( N_("Dummy font renderer function") )
        set_capability( "text renderer", 1 )
        set_callbacks( OpenRenderer, NULL )
    add_submodule ()
        set_description( N_("libc memcpy") )
        set_capability( "memcpy", 50 )
        set_callbacks( OpenDummy, NULL )
        add_shortcut( "c", "libc" )
vlc_module_end ()

static int OpenDummy( vlc_object_t *obj )
{
    (void) obj;
    return VLC_SUCCESS;
}
