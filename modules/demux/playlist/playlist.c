/*****************************************************************************
 * playlist.c :  Playlist import module
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id: playlist.c,v 1.1 2004/01/11 00:45:06 zorglub Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>

/***************************************************************************
 * Prototypes
 ***************************************************************************/
int Import_Old ( vlc_object_t * );
int Import_M3U ( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();

    add_shortcut( "playlist" );

    set_description( _("Old playlist open") );
    add_shortcut( "old-open" );
    set_capability( "demux2" , 10 );
    set_callbacks( Import_Old , NULL );

    add_submodule();
        set_description( _("M3U playlist import") );
        add_shortcut( "m3u-open" );
        set_capability( "demux2" , 10 );
        set_callbacks( Import_M3U , NULL );
vlc_module_end();
