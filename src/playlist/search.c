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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <assert.h>

#include <vlc/vlc.h>
#include "vlc_playlist.h"
#include "playlist_internal.h"

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
playlist_item_t * playlist_ItemGetById( playlist_t * p_playlist , int i_id,
                                        vlc_bool_t b_locked )
{
    int i;
    if( !b_locked ) PL_LOCK;
    ARRAY_BSEARCH( p_playlist->all_items,->i_id, int, i_id, i );
    if( i != -1 )
    {
        if( !b_locked ) PL_UNLOCK;
        return ARRAY_VAL( p_playlist->all_items, i );
    }
    if( !b_locked ) PL_UNLOCK;
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
                                           input_item_t *p_item,
                                           vlc_bool_t b_locked )
{
    int i;
    if( !b_locked ) PL_LOCK;
    if( p_playlist->status.p_item &&
        p_playlist->status.p_item->p_input == p_item )
    {
        if( !b_locked ) PL_UNLOCK;
        return p_playlist->status.p_item;
    }
    /** \todo Check if this is always incremental and whether we can bsearch */
    for( i =  0 ; i < p_playlist->all_items.i_size; i++ )
    {
        if( ARRAY_VAL(p_playlist->all_items, i)->p_input->i_id == p_item->i_id )
        {
            if( !b_locked ) PL_UNLOCK;
            return ARRAY_VAL(p_playlist->all_items, i);
        }
    }
    if( !b_locked ) PL_UNLOCK;
    return NULL;
}

/**
 * Get input by item id
 *
 * Find the playlist item matching the input id under the given node
 * \param p_playlist the playlist
 * \param i_input_id the id of the input to find
 * \param p_root the root node of the search
 * \return the playlist item or NULL on failure
 */
playlist_item_t * playlist_ItemGetByInputId( playlist_t *p_playlist,
                                             int i_input_id,
                                             playlist_item_t *p_root )
{
    int i;
    assert( p_root != NULL );
    for( i = 0 ; i< p_root->i_children ; i++ )
    {
        if( p_root->pp_children[i]->p_input &&
            p_root->pp_children[i]->p_input->i_id == i_input_id )
        {
            return p_root->pp_children[i];
        }
        else if( p_root->pp_children[i]->i_children >= 0 )
        {
            return playlist_ItemGetByInputId( p_playlist, i_input_id,
                                              p_root->pp_children[i] );
        }
    }
    return NULL;
}

/***************************************************************************
 * Live search handling
 ***************************************************************************/

static vlc_bool_t playlist_LiveSearchUpdateInternal( playlist_t *p_playlist,
                                                     playlist_item_t *p_root,
                                                     const char *psz_string )
{
   int i;
   vlc_bool_t b_match = VLC_FALSE;
   for( i = 0 ; i < p_root->i_children ; i ++ )
   {
        playlist_item_t *p_item = p_root->pp_children[i];
        if( p_item->i_children > -1 )
        {
            if( playlist_LiveSearchUpdateInternal( p_playlist, p_item, psz_string ) ||
                strcasestr( p_item->p_input->psz_name, psz_string ) )
            {
                p_item->i_flags &= ~PLAYLIST_DBL_FLAG;
                b_match = VLC_TRUE;
            }
            else
            {
                p_item->i_flags |= PLAYLIST_DBL_FLAG;
            }
        }
        else
        {
            if( strcasestr( p_item->p_input->psz_name, psz_string ) || /* Soon to be replaced by vlc_meta_Title */
                input_item_MetaMatch( p_item->p_input, vlc_meta_Album, psz_string ) ||
                input_item_MetaMatch( p_item->p_input, vlc_meta_Artist, psz_string ) )
            {
                p_item->i_flags &= ~PLAYLIST_DBL_FLAG;
                b_match = VLC_TRUE;
            }
            else
            {
                p_item->i_flags |= PLAYLIST_DBL_FLAG;
            }
        }
   }
   return b_match;
}

int playlist_LiveSearchUpdate( playlist_t *p_playlist, playlist_item_t *p_root,
                               const char *psz_string )
{
    p_playlist->b_reset_currently_playing = VLC_TRUE;
    playlist_LiveSearchUpdateInternal( p_playlist, p_root, psz_string );
    vlc_cond_signal( &p_playlist->object_wait );
    return VLC_SUCCESS;
}
