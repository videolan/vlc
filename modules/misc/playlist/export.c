/*****************************************************************************
 * export.c :  Playlist export module
 *****************************************************************************
 * Copyright (C) 2004-2009 the VideoLAN team
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#define VLC_MODULE_LICENSE VLC_LICENSE_GPL_2_PLUS
#include <vlc_common.h>
#include <vlc_plugin.h>

/***************************************************************************
 * Prototypes
 ***************************************************************************/
int Export_M3U    ( vlc_object_t *p_intf );
int Export_M3U8   ( vlc_object_t *p_intf );
int Export_HTML   ( vlc_object_t *p_intf );
int xspf_export_playlist( vlc_object_t *p_intf );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()

    set_category( CAT_PLAYLIST )
    set_subcategory( SUBCAT_PLAYLIST_EXPORT )
    add_submodule ()
        set_description( N_("M3U playlist export") )
        add_shortcut( "export-m3u" )
        set_capability( "playlist export" , 0 )
        set_callback( Export_M3U  )

    add_submodule ()
        set_description( N_("M3U8 playlist export") )
        add_shortcut( "export-m3u8" )
        set_capability( "playlist export" , 0 )
        set_callback( Export_M3U8 )

    add_submodule ()
        set_description( N_("XSPF playlist export") )
        add_shortcut( "export-xspf" )
        set_capability( "playlist export" , 0 )
        set_callback( xspf_export_playlist  )

    add_submodule ()
        set_description( N_("HTML playlist export") )
        add_shortcut( "export-html" )
        set_capability( "playlist export", 0 )
        set_callback( Export_HTML )

vlc_module_end ()
