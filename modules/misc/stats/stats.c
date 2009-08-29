/*****************************************************************************
 * stats.c : stats plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2008 the VideoLAN team
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Pierre d'Herbemont <pdherbemont@videolan.org>
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

#include "stats.h"

/* Example usage:
 *  $ vlc movie.avi --sout="#transcode{aenc=dummy,venc=stats}:\
 *                          std{access=http,mux=dummy,dst=0.0.0.0:8081}"
 *  $ vlc -vvv http://127.0.0.1:8081 --demux=stats --vout=stats --codec=stats
 */

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin ()
    set_shortname( N_("Stats"))
    set_description( N_("Stats encoder function") )
    set_capability( "encoder", 0 )
    add_shortcut( "stats" )
    set_callbacks( OpenEncoder, CloseEncoder )
    add_submodule ()
        set_section( N_( "Stats decoder" ), NULL )
        set_description( N_("Stats decoder function") )
        set_capability( "decoder", 0 )
        add_shortcut( "stats" )
        set_callbacks( OpenDecoder, CloseDecoder )
    add_submodule ()
        set_section( N_( "Stats demux" ), NULL )
        set_description( N_("Stats demux function") )
        set_capability( "demux", 0 )
        add_shortcut( "stats" )
        set_callbacks( OpenDemux, CloseDemux )
vlc_module_end ()

