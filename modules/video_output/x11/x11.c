/*****************************************************************************
 * x11.c : X11 plugin for vlc
 *****************************************************************************
 * Copyright (C) 1998-2001 the VideoLAN team
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          David Kennedy <dkennedy@tinytoad.com>
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

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
extern int  Activate   ( vlc_object_t * );
extern void Deactivate ( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define DISPLAY_TEXT N_("X11 display")
#define DISPLAY_LONGTEXT N_( \
    "X11 hardware display to use. By default VLC will " \
    "use the value of the DISPLAY environment variable.")

#define SHM_TEXT N_("Use shared memory")
#define SHM_LONGTEXT N_( \
    "Use shared memory to communicate between VLC and the X server.")

vlc_module_begin ()
    set_shortname( "X11" )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VOUT )
    add_string( "x11-display", NULL, NULL, DISPLAY_TEXT, DISPLAY_LONGTEXT, true )
#ifdef HAVE_SYS_SHM_H
    add_bool( "x11-shm", 1, NULL, SHM_TEXT, SHM_LONGTEXT, true )
#endif
    set_description( N_("X11 video output") )
    set_capability( "video output", 70 )
    set_callbacks( Activate, Deactivate )
vlc_module_end ()

