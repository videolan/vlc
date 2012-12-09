/*****************************************************************************
 * playlist.c: libvlc new API playlist handling functions
 *****************************************************************************
 * Copyright (C) 2005 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "libvlc_internal.h"
#include "../src/libvlc.h"

#include <vlc/libvlc_structures.h>
#include <vlc/libvlc.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_media_player.h>
#include <vlc/deprecated.h>

#include <vlc_playlist.h>

#include <assert.h>

void libvlc_playlist_play( libvlc_instance_t *p_instance, int i_id,
                           int i_options, char **ppsz_options )
{
    playlist_t *pl = pl_Get (p_instance->p_libvlc_int);
    VLC_UNUSED(i_id); VLC_UNUSED(i_options); VLC_UNUSED(ppsz_options);

    assert( pl );
    if( !var_GetBool( pl, "playlist-autostart" )
     || pl->items.i_size == 0 )
        return;
    playlist_Control( pl, PLAYLIST_PLAY, false );
}

int libvlc_add_intf( libvlc_instance_t *p_instance, const char *name )
{
    pl_Get (p_instance->p_libvlc_int);

    if( libvlc_InternalAddIntf( p_instance->p_libvlc_int, name ))
    {
        if( name != NULL )
            libvlc_printerr("interface \"%s\" initialization failed", name );
        else
            libvlc_printerr("default interface initialization failed");
        return -1;
    }
    return 0;
}
