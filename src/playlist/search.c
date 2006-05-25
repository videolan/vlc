/*****************************************************************************
 * search.c : Search functions
 *****************************************************************************
 * Copyright (C) 1999-2004 the VideoLAN team
 * $Id$
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
#include <vlc/vlc.h>
#include <vlc/input.h>

#include "vlc_playlist.h"

/***************************************************************************
 * Item search functions
 ***************************************************************************/

/**
 * Search a playlist item by its playlist_item id
 *
 * \param p_playlist the playlist
 * \param i_id the id to find
 * \return the item or NULL on failure
 */
playlist_item_t * playlist_ItemGetById( playlist_t * p_playlist , int i_id )
{
    int i, i_top, i_bottom;
    i_bottom = 0; i_top = p_playlist->i_all_size - 1;
    i = i_top / 2;
    while( p_playlist->pp_all_items[i]->i_id != i_id &&
           i_top > i_bottom )
    {
        if( p_playlist->pp_all_items[i]->i_id < i_id )
            i_bottom = i + 1;
        else
            i_top = i - 1;
        i = i_bottom + ( i_top - i_bottom ) / 2;
    }
    if( p_playlist->pp_all_items[i]->i_id == i_id )
    {
        return p_playlist->pp_all_items[i];
    }
    return NULL;
}

/**
 * Search an item by its input_item_t
 *
 * \param p_playlist the playlist
 * \param p_item the input_item_t to find
 * \return the item, or NULL on failure
 */
playlist_item_t * playlist_ItemGetByInput( playlist_t * p_playlist ,
                                           input_item_t *p_item )
{
    int i;
    if( p_playlist->status.p_item && p_playlist->status.p_item->p_input == p_item )
    {
        return p_playlist->status.p_item;
    }

    for( i =  0 ; i < p_playlist->i_all_size; i++ )
    {
msg_Err( p_playlist, "%p, %p", p_item, p_playlist->pp_all_items[i]->p_input );
        if( p_playlist->pp_all_items[i]->p_input == p_item )
        {
            return p_playlist->pp_all_items[i];
        }
    }
    return NULL;
}

/***************************************************************************
 * Live search handling
 ***************************************************************************/

int playlist_LiveSearchUpdate( playlist_t *p_playlist, playlist_item_t *p_root,
                               const char *psz_string )
{
   int i;
   for( i = 0 ; i< p_root->i_children ; i ++ )
   {
        playlist_item_t *p_item = p_root->pp_children[i];
        if( p_item->i_children > -1 )
        {
            playlist_LiveSearchUpdate( p_playlist, p_item, psz_string );
        }
#define META_MATCHES( field ) ( p_item->p_input->p_meta && \
                                p_item->p_input->p_meta->psz_##field && \
                                strcasestr( p_item->p_input->p_meta->psz_##field, psz_string ) )
        /* Todo: Filter on all fields */
        if( strcasestr( p_item->p_input->psz_name, psz_string ) ||
            META_MATCHES( artist ) || META_MATCHES( album ) )
            p_item->i_flags &= ~PLAYLIST_DBL_FLAG;
        else
            p_item->i_flags |= PLAYLIST_DBL_FLAG;
   }
   return VLC_SUCCESS;
}
