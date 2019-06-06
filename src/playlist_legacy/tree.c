/*****************************************************************************
 * tree.c : Playlist tree walking functions
 *****************************************************************************
 * Copyright (C) 1999-2007 VLC authors and VideoLAN
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

#include <vlc_common.h>
#include <assert.h>
#include "vlc_playlist_legacy.h"
#include "playlist_internal.h"

/************************************************************************
 * Local prototypes
 ************************************************************************/
playlist_item_t *GetNextUncle( playlist_t *p_playlist, playlist_item_t *p_item,
                               playlist_item_t *p_root );
playlist_item_t *GetPrevUncle( playlist_t *p_playlist, playlist_item_t *p_item,
                               playlist_item_t *p_root );

playlist_item_t *GetNextItem( playlist_t *p_playlist,
                              playlist_item_t *p_root,
                              playlist_item_t *p_item );
playlist_item_t *GetPrevItem( playlist_t *p_playlist,
                              playlist_item_t *p_item,
                              playlist_item_t *p_root );

/**
 * Create a playlist node
 *
 * \param p_playlist the playlist
 * \param psz_name the name of the node
 * \param p_parent the parent node to attach to or NULL if no attach
 * \param i_pos position of the node in the parent, PLAYLIST_END to append to end.
 * \param p_flags miscellaneous flags
 * \param p_input the input_item to attach to or NULL if it has to be created
 * \return the new node
 */
playlist_item_t * playlist_NodeCreate( playlist_t *p_playlist,
                                       const char *psz_name,
                                       playlist_item_t *p_parent, int i_pos,
                                       int i_flags )
{
    input_item_t *p_new_input;
    playlist_item_t *p_item;

    PL_ASSERT_LOCKED;
    if( !psz_name ) psz_name = _("Undefined");

    p_new_input = input_item_NewExt( NULL, psz_name, INPUT_DURATION_UNSET, ITEM_TYPE_NODE,
                                     ITEM_NET_UNKNOWN );
    if( !p_new_input )
        return NULL;
    p_item = playlist_ItemNewFromInput( p_playlist, p_new_input );
    input_item_Release( p_new_input );

    if( p_item == NULL )  return NULL;

    playlist_NodeInsert( p_parent, p_item, i_pos );
    playlist_SendAddNotify( p_playlist, p_item );

    p_item->i_flags |= i_flags;

    return p_item;
}

/**
 * Remove all the children of a node and removes the node
 *
 * \param p_playlist the playlist
 * \param p_root the node
 */
void playlist_NodeDelete( playlist_t *p_playlist, playlist_item_t *p_root )
{
    playlist_NodeDeleteExplicit( p_playlist, p_root,
        PLAYLIST_DELETE_STOP_IF_CURRENT );
}

void playlist_NodeDeleteExplicit( playlist_t *p_playlist,
    playlist_item_t *p_root, int flags )
{
    PL_ASSERT_LOCKED;

    /* Delete the node */
    if( p_root->i_flags & PLAYLIST_RO_FLAG &&
        !( flags & PLAYLIST_DELETE_FORCE ) )
        return;

    /* Delete the children */
    for( int i = p_root->i_children - 1 ; i >= 0; i-- )
        playlist_NodeDeleteExplicit( p_playlist,
            p_root->pp_children[i], flags | PLAYLIST_DELETE_FORCE );

    pl_priv(p_playlist)->b_reset_currently_playing = true;

    int i;
    var_SetAddress( p_playlist, "playlist-item-deleted", p_root );

    if( p_root->i_children == -1 ) {
        ARRAY_BSEARCH( p_playlist->items,->i_id, int, p_root->i_id, i );
        if( i != -1 )
            ARRAY_REMOVE( p_playlist->items, i );
    }

    if( get_current_status_item( p_playlist ) == p_root )
    {
        /* a deleted item cannot be currently playing */
        set_current_status_item( p_playlist, NULL );

        if( flags & PLAYLIST_DELETE_STOP_IF_CURRENT )
            playlist_Control( p_playlist, PLAYLIST_STOP, pl_Locked );
    }

    for( i = 0; i < p_playlist->current.i_size; i++ )
        if( p_playlist->current.p_elems[i] == p_root )
            ARRAY_REMOVE( p_playlist->current, i );
    for( i = 0; i < p_playlist->current.i_size; i++ )
        assert( p_playlist->current.p_elems[i] != p_root );

    PL_DEBUG( "deleting item `%s'", p_root->p_input->psz_name );

    /* Remove the item from its parent */
    playlist_item_t *p_parent = p_root->p_parent;
    if( p_parent != NULL )
        TAB_REMOVE(p_parent->i_children, p_parent->pp_children, p_root);

    playlist_ItemRelease( p_playlist, p_root );
}

int playlist_NodeInsert( playlist_item_t *p_parent, playlist_item_t *p_item,
                         int i_position )
{
    assert( p_parent && p_parent->i_children != -1 );
    if( i_position == -1 ) i_position = p_parent->i_children ;
    assert( i_position <= p_parent->i_children);

    TAB_INSERT(p_parent->i_children, p_parent->pp_children,
               p_item, i_position);
    p_item->p_parent = p_parent;

    /* Inherit special flags from parent (sd cases) */
    if( ( p_parent->i_flags & PLAYLIST_NO_INHERIT_FLAG ) == 0 )
        p_item->i_flags |= (p_parent->i_flags & PLAYLIST_RO_FLAG);

    return VLC_SUCCESS;
}

/**
 * Search a child of a node by its name
 *
 * \note The playlist must be locked, and the result is only valid until the
 * playlist is unlocked.
 *
 * \param p_node the node
 * \param psz_search the name of the child to search
 * \return the child item or NULL if not found or error
 */
playlist_item_t *playlist_ChildSearchName( playlist_item_t *p_node,
                                           const char *psz_search )
{
    int i;

    if( p_node->i_children < 0 )
    {
         return NULL;
    }
    for( i = 0 ; i< p_node->i_children; i++ )
    {
        if( !strcmp( p_node->pp_children[i]->p_input->psz_name, psz_search ) )
        {
            return p_node->pp_children[i];
        }
    }
    return NULL;
}

/**********************************************************************
 * Tree walking functions
 **********************************************************************/
/**
 * Finds the next item to play
 *
 * \param p_playlist the playlist
 * \param p_root the root node
 * \param p_item the previous item  (NULL if none )
 * \return the next item to play, or NULL if none found
 */
playlist_item_t *playlist_GetNextLeaf( playlist_t *p_playlist,
                                       playlist_item_t *p_root,
                                       playlist_item_t *p_item,
                                       bool b_ena, bool b_unplayed )
{
    PL_ASSERT_LOCKED;
    playlist_item_t *p_next;

    assert( p_root && p_root->i_children != -1 );

    PL_DEBUG2( "finding next of %s within %s",
               PLI_NAME( p_item ), PLI_NAME( p_root ) );

    /* Now, walk the tree until we find a suitable next item */
    p_next = p_item;
    while( 1 )
    {
        bool b_ena_ok = true, b_unplayed_ok = true;
        p_next = GetNextItem( p_playlist, p_root, p_next );
        if( !p_next || p_next == p_root )
            break;
        if( p_next->i_children == -1 )
        {
            if( b_ena && p_next->i_flags & PLAYLIST_DBL_FLAG )
                b_ena_ok = false;
            if( b_unplayed && p_next->i_nb_played != 0 )
                b_unplayed_ok = false;
            if( b_ena_ok && b_unplayed_ok ) break;
        }
    }
    if( p_next == NULL ) PL_DEBUG2( "at end of node" );
    return p_next;
}

/************************************************************************
 * Following functions are local
 ***********************************************************************/

/**
 * Get the next item in the tree
 * If p_item is NULL, return the first child of root
 **/
playlist_item_t *GetNextItem( playlist_t *p_playlist,
                              playlist_item_t *p_root,
                              playlist_item_t *p_item )
{
    /* If the item is NULL, return the firt child of root */
    if( p_item == NULL )
    {
        if( p_root->i_children > 0 )
            return p_root->pp_children[0];
        else
            return NULL;
    }

    /* Node with children, get the first one */
    if( p_item->i_children > 0 )
        return p_item->pp_children[0];

    playlist_item_t* p_parent = p_item->p_parent;
    for( int i = 0 ; i < p_parent->i_children ; i++ )
    {
        if( p_parent->pp_children[i] == p_item )
        {
            // Return the next children
            if( i + 1 < p_parent->i_children )
                return p_parent->pp_children[i+1];
            // We are the least one, so try to have uncles
            else
            {
                PL_DEBUG2( "Current item is the last of the node,"
                           "looking for uncle from %s",
                            p_parent->p_input->psz_name );
                if( p_parent == p_root )
                {
                    PL_DEBUG2( "already at root" );
                    return NULL;
                }
                else
                    return GetNextUncle( p_playlist, p_item, p_root );
            }
        }
    }
    return NULL;
}

playlist_item_t *GetNextUncle( playlist_t *p_playlist, playlist_item_t *p_item,
                               playlist_item_t *p_root )
{
    playlist_item_t *p_parent = p_item->p_parent;
    playlist_item_t *p_grandparent;
    bool b_found = false;

    (void)p_playlist;

    if( p_parent != NULL )
    {
        p_grandparent = p_parent->p_parent;
        while( p_grandparent )
        {
            int i;
            for( i = 0 ; i< p_grandparent->i_children ; i++ )
            {
                if( p_parent == p_grandparent->pp_children[i] )
                {
                    PL_DEBUG2( "parent %s found as child %i of grandparent %s",
                               p_parent->p_input->psz_name, i,
                               p_grandparent->p_input->psz_name );
                    b_found = true;
                    break;
                }
            }
            if( b_found && i + 1 < p_grandparent->i_children )
            {
                    return p_grandparent->pp_children[i+1];
            }
            /* Not found at root */
            if( p_grandparent == p_root )
            {
                return NULL;
            }
            else
            {
                p_parent = p_grandparent;
                p_grandparent = p_parent->p_parent;
            }
        }
    }
    /* We reached root */
    return NULL;
}

playlist_item_t *GetPrevUncle( playlist_t *p_playlist, playlist_item_t *p_item,
                               playlist_item_t *p_root )
{
    playlist_item_t *p_parent = p_item->p_parent;
    playlist_item_t *p_grandparent;
    bool b_found = false;

    (void)p_playlist;

    if( p_parent != NULL )
    {
        p_grandparent = p_parent->p_parent;
        while( 1 )
        {
            int i;
            for( i = p_grandparent->i_children -1 ; i >= 0; i-- )
            {
                if( p_parent == p_grandparent->pp_children[i] )
                {
                    b_found = true;
                    break;
                }
            }
            if( b_found && i - 1 > 0 )
            {
                return p_grandparent->pp_children[i-1];
            }
            /* Not found at root */
            if( p_grandparent == p_root )
            {
                return NULL;
            }
            else
            {
                p_parent = p_grandparent;
                p_grandparent = p_parent->p_parent;
            }
        }
    }
    /* We reached root */
    return NULL;
}


/* Recursively search the tree for previous item */
playlist_item_t *GetPrevItem( playlist_t *p_playlist,
                              playlist_item_t *p_root,
                              playlist_item_t *p_item )
{
    playlist_item_t *p_parent;
    int i;

    /* Node with children, get the last one */
    if( p_item && p_item->i_children > 0 )
        return p_item->pp_children[p_item->i_children - 1];

    /* Last child of its parent ? */
    if( p_item != NULL )
        p_parent = p_item->p_parent;
    else
    {
        msg_Err( p_playlist, "Get the last one" );
        abort();
    };

    for( i = p_parent->i_children -1 ; i >= 0 ;  i-- )
    {
        if( p_parent->pp_children[i] == p_item )
        {
            if( i-1 < 0 )
            {
               /* Was already the first sibling. Look for uncles */
                PL_DEBUG2( "current item is the first of its node,"
                           "looking for uncle from %s",
                           p_parent->p_input->psz_name );
                if( p_parent == p_root )
                {
                    PL_DEBUG2( "already at root" );
                    return NULL;
                }
                return GetPrevUncle( p_playlist, p_item, p_root );
            }
            else
            {
                return p_parent->pp_children[i-1];
            }
        }
    }
    return NULL;
}
