/*****************************************************************************
 * search.c : Search functions
 *****************************************************************************
 * Copyright (C) 1999-2009 VLC authors and VideoLAN
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
#include <assert.h>

#include <vlc_common.h>
#include <vlc_playlist_legacy.h>
#include <vlc_charset.h>
#include "playlist_internal.h"

/***************************************************************************
 * Item search functions
 ***************************************************************************/

/***************************************************************************
 * Live search handling
 ***************************************************************************/

/**
 * Enable all items in the playlist
 * @param p_root: the current root item
 */
static void playlist_LiveSearchClean( playlist_item_t *p_root )
{
    for( int i = 0; i < p_root->i_children; i++ )
    {
        playlist_item_t *p_item = p_root->pp_children[i];
        if( p_item->i_children >= 0 )
            playlist_LiveSearchClean( p_item );
        p_item->i_flags &= ~PLAYLIST_DBL_FLAG;
    }
}


/**
 * Enable/Disable items in the playlist according to the search argument
 * @param p_root: the current root item
 * @param psz_string: the string to search
 * @return true if an item match
 */
static bool playlist_LiveSearchUpdateInternal( playlist_item_t *p_root,
                                               const char *psz_string, bool b_recursive )
{
    int i;
    bool b_match = false;
    for( i = 0 ; i < p_root->i_children ; i ++ )
    {
        bool b_enable = false;
        playlist_item_t *p_item = p_root->pp_children[i];
        // Go recurssively if their is some children
        if( b_recursive && p_item->i_children >= 0 &&
            playlist_LiveSearchUpdateInternal( p_item, psz_string, true ) )
        {
            b_enable = true;
        }

        if( !b_enable )
        {
            vlc_mutex_lock( &p_item->p_input->lock );
            // Do we have some meta ?
            if( p_item->p_input->p_meta )
            {
                // Use Title or fall back to psz_name
                const char *psz_title = vlc_meta_Get( p_item->p_input->p_meta, vlc_meta_Title );
                if( !psz_title )
                    psz_title = p_item->p_input->psz_name;
                const char *psz_album = vlc_meta_Get( p_item->p_input->p_meta, vlc_meta_Album );
                const char *psz_artist = vlc_meta_Get( p_item->p_input->p_meta, vlc_meta_Artist );
                b_enable = ( psz_title && vlc_strcasestr( psz_title, psz_string ) ) ||
                           ( psz_album && vlc_strcasestr( psz_album, psz_string ) ) ||
                           ( psz_artist && vlc_strcasestr( psz_artist, psz_string ) );
            }
            else
                b_enable = p_item->p_input->psz_name && vlc_strcasestr( p_item->p_input->psz_name, psz_string );
            vlc_mutex_unlock( &p_item->p_input->lock );
        }

        if( b_enable )
            p_item->i_flags &= ~PLAYLIST_DBL_FLAG;
        else
            p_item->i_flags |= PLAYLIST_DBL_FLAG;

        b_match |= b_enable;
   }
   return b_match;
}



/**
 * Launch the recursive search in the playlist
 * @param p_playlist: the playlist
 * @param p_root: the current root item
 * @param psz_string: the string to find
 * @return VLC_SUCCESS
 */
int playlist_LiveSearchUpdate( playlist_t *p_playlist, playlist_item_t *p_root,
                               const char *psz_string, bool b_recursive )
{
    PL_ASSERT_LOCKED;
    pl_priv(p_playlist)->b_reset_currently_playing = true;
    if( *psz_string )
        playlist_LiveSearchUpdateInternal( p_root, psz_string, b_recursive );
    else
        playlist_LiveSearchClean( p_root );
    vlc_cond_signal( &pl_priv(p_playlist)->signal );
    return VLC_SUCCESS;
}

