/*****************************************************************************
 * macosx.c: minimal Mac OS X module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2012 VLC authors and VideoLAN
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *          Eugenio Jarosiewicz <ej0@cise.ufl.edu>
 *          Pierre d'Herbemont <pdherbemont # videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_window.h>

/*****************************************************************************
 * External prototypes
 *****************************************************************************/
int  OpenIntf     ( vlc_object_t * );
void CloseIntf    ( vlc_object_t * );

int  WindowOpen   ( vout_window_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin ()
    /* Minimal interface. see intf.m */
    set_shortname( "Minimal Macosx" )
    add_shortcut( "minimal_macosx", "miosx" )
    set_description( N_("Minimal Mac OS X interface") )
    set_capability( "interface", 50 )
    set_callbacks( OpenIntf, CloseIntf )
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_MAIN )

    add_submodule ()
    /* Will be loaded even without interface module. see voutgl.m */
        set_description( "Minimal Mac OS X Video Output Provider" )
        set_capability( "vout window", 50 )
        set_callback( WindowOpen )
vlc_module_end ()

