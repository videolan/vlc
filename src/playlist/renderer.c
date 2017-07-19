/*****************************************************************************
 * renderer.c : Manage renderer modules
 *****************************************************************************
 * Copyright (C) 1999-2017 VLC authors, VideoLAN and VideoLabs
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
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

#include <vlc_common.h>
#include <vlc_playlist.h>
#include <vlc_renderer_discovery.h>

#include "playlist/playlist_internal.h"

int playlist_SetRenderer( playlist_t* p_playlist, vlc_renderer_item_t* p_item )
{
    if( p_item )
        vlc_renderer_item_hold( p_item );

    PL_LOCK;

    playlist_private_t *p_priv = pl_priv( p_playlist );
    vlc_renderer_item_t *p_prev_renderer = p_priv->p_renderer;
    p_priv->p_renderer = p_item;
    if( p_priv->p_input )
        input_Control( p_priv->p_input, INPUT_SET_RENDERER, p_item );

    PL_UNLOCK;

    if( p_prev_renderer )
        vlc_renderer_item_release( p_prev_renderer );
    return VLC_SUCCESS;
}
