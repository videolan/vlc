/*****************************************************************************
 * tree.c : Playlist tree walking functions
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

#define PLAYLIST_DEBUG 1

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
 * \paam psz_name the name of the node
 * \param p_parent the parent node to attach to or NULL if no attach
 * \return the new node
 */
playlist_item_t * playlist_NodeCreate( playlist_t *p_playlist, char *psz_name,
                                       playlist_item_t *p_parent )
{
    input_item_t *p_input;
    playlist_item_t *p_item;

    if( !psz_name ) psz_name = strdup( _("Undefined") );
    p_input = input_ItemNewWithType( VLC_OBJECT(p_playlist), NULL, psz_name,
                                     0, NULL, -1, ITEM_TYPE_NODE );
    p_input->i_id = ++p_playlist->i_last_input_id;
    p_item = playlist_ItemNewFromInput( VLC_OBJECT(p_playlist), p_input );
    p_item->i_children = 0;

    if( p_item == NULL )
    {
        return NULL;
    }
    INSERT_ELEM( p_playlist->pp_all_items,
                 p_playlist->i_all_size,
                 p_playlist->i_all_size,
                 p_item );

    if( p_parent != NULL )
    {
        playlist_NodeAppend( p_playlist, p_item, p_parent );
    }

    playlist_SendAddNotify( p_playlist, p_item->i_id,
                            p_parent ? p_parent->i_id : -1 );
    return p_item;
}

/**
 * Remove all the children of a node
 *
 * This function must be entered with the playlist lock
 *
 * \param p_playlist the playlist
 * \param p_root the node
 * \param b_delete_items do we have to delete the children items ?
 * \return VLC_SUCCESS or an error
 */
int playlist_NodeEmpty( playlist_t *p_playlist, playlist_item_t *p_root,
                        vlc_bool_t b_delete_items )
{
    int i;
    if( p_root->i_children == -1 )
    {
        return VLC_EGENERIC;
    }

    /* Delete the children */
    for( i =  p_root->i_children-1 ; i >= 0 ;i-- )
    {
        if( p_root->pp_children[i]->i_children > -1 )
        {
            playlist_NodeDelete( p_playlist, p_root->pp_children[i],
                                 b_delete_items , VLC_FALSE );
        }
        else if( b_delete_items )
        {
            /* Delete the item here */
            playlist_DeleteFromItemId( p_playlist,
                                       p_root->pp_children[i]->i_id );
        }
    }
    return VLC_SUCCESS;
}

/**
 * Remove all the children of a node and removes the node
 *
 * \param p_playlist the playlist
 * \param p_root the node
 * \param b_delete_items do we have to delete the children items ?
 * \return VLC_SUCCESS or an error
 */
int playlist_NodeDelete( playlist_t *p_playlist, playlist_item_t *p_root,
                         vlc_bool_t b_delete_items, vlc_bool_t b_force )
{
    int i, i_top, i_bottom;
    if( p_root->i_children == -1 )
    {
        return VLC_EGENERIC;
    }

    /* Delete the children */
    for( i =  p_root->i_children - 1 ; i >= 0; i-- )
    {
        if( p_root->pp_children[i]->i_children > -1 )
        {
            playlist_NodeDelete( p_playlist, p_root->pp_children[i],
                                 b_delete_items , b_force );
        }
        else if( b_delete_items )
        {
            playlist_DeleteFromItemId( p_playlist,
                                       p_root->pp_children[i]->i_id );
        }
    }
    /* Delete the node */
    if( p_root->i_flags & PLAYLIST_RO_FLAG && !b_force )
    {
    }
    else
    {
        var_SetInteger( p_playlist, "item-deleted", p_root->p_input->i_id );

        i_bottom = 0; i_top = p_playlist->i_all_size - 1;
        i = i_top / 2;
        while( p_playlist->pp_all_items[i]->p_input->i_id !=
                  p_root->p_input->i_id &&   i_top > i_bottom )
        {
            if( p_playlist->pp_all_items[i]->p_input->i_id <
                               p_root->p_input->i_id )
            {
                i_bottom = i + 1;
            }
            else
            {
                i_top = i - 1;
            }
            i = i_bottom + ( i_top - i_bottom ) / 2;
        }
        if( p_playlist->pp_all_items[i]->p_input->i_id ==
            p_root->p_input->i_id )
        {
            REMOVE_ELEM( p_playlist->pp_all_items, p_playlist->i_all_size, i );
        }
        playlist_ItemDelete( p_root );
    }
    return VLC_SUCCESS;
}


/**
 * Adds an item to the children of a node
 *
 * \param p_playlist the playlist
 * \param p_item the item to append
 * \param p_parent the parent node
 * \return VLC_SUCCESS or an error
 */
int playlist_NodeAppend( playlist_t *p_playlist,
                         playlist_item_t *p_item,
                         playlist_item_t *p_parent )
{
    return playlist_NodeInsert( p_playlist, p_item, p_parent, -1 );
}

int playlist_NodeInsert( playlist_t *p_playlist,
                         playlist_item_t *p_item,
                         playlist_item_t *p_parent,
                         int i_position )
{
   if( !p_parent || p_parent->i_children == -1 )
   {
        msg_Err( p_playlist, "invalid node" );
        return VLC_EGENERIC;
   }
   if( i_position == -1 ) i_position = p_parent->i_children ;

   INSERT_ELEM( p_parent->pp_children,
                p_parent->i_children,
                i_position,
                p_item );

   p_item->p_parent = p_parent;

   return VLC_SUCCESS;
}

/**
 * Deletes an item from the children of a node
 *
 * \param p_playlist the playlist
 * \param p_item the item to remove
 * \param p_parent the parent node
 * \return VLC_SUCCESS or an error
 */
int playlist_NodeRemoveItem( playlist_t *p_playlist,
                        playlist_item_t *p_item,
                        playlist_item_t *p_parent )
{
   int i;
   for( i= 0; i< p_parent->i_children ; i++ )
   {
       if( p_parent->pp_children[i] == p_item )
       {
           REMOVE_ELEM( p_parent->pp_children, p_parent->i_children, i );
       }
   }

   return VLC_SUCCESS;
}


/**
 * Count the children of a node
 *
 * \param p_playlist the playlist
 * \param p_node the node
 * \return the number of children
 */
int playlist_NodeChildrenCount( playlist_t *p_playlist, playlist_item_t*p_node)
{
    int i;
    int i_nb = 0;
    if( p_node->i_children == -1 )
        return 0;

    for( i=0 ; i< p_node->i_children;i++ )
    {
        if( p_node->pp_children[i]->i_children == -1 )
            i_nb++;
        else
            i_nb += playlist_NodeChildrenCount( p_playlist,
                                                p_node->pp_children[i] );
    }
    return i_nb;
}

/**
 * Search a child of a node by its name
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

playlist_item_t *playlist_GetLastLeaf(playlist_t *p_playlist,
                                      playlist_item_t *p_root )
{
    int i;
    playlist_item_t *p_item;
    for ( i = p_root->i_children - 1; i >= 0; i-- )
    {
        if( p_root->pp_children[i]->i_children == -1 )
            return p_root->pp_children[i];
        else if( p_root->pp_children[i]->i_children > 0)
        {
             p_item = playlist_GetLastLeaf( p_playlist,
                                            p_root->pp_children[i] );
            if ( p_item != NULL )
                return p_item;
        }
        else if( i == 0 )
            return NULL;
    }
    return NULL;
}

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
                                       playlist_item_t *p_item )
{
    playlist_item_t *p_next;

#ifdef PLAYLIST_DEBUG
    if( p_item != NULL )
        msg_Dbg( p_playlist, "finding next of %s within %s",
                        p_item->p_input->psz_name,  p_root->p_input->psz_name );
    else
        msg_Dbg( p_playlist, "finding something to play within %s",
                         p_root->p_input->psz_name );
#endif

    if( !p_root  || p_root->i_children == -1 )
    {
        msg_Err( p_playlist,"invalid arguments for GetNextLeaf" );
        return NULL;
    }

    /* Now, walk the tree until we find a suitable next item */
    p_next = p_item;
    do
    {
        p_next = GetNextItem( p_playlist, p_root, p_next );
    } while ( p_next && p_next != p_root && p_next->i_children != -1 );

#ifdef PLAYLIST_DEBUG
    if( p_next == NULL )
        msg_Dbg( p_playlist, "At end of node" );
#endif
    return p_next;
}

playlist_item_t *playlist_GetNextEnabledLeaf( playlist_t *p_playlist,
                                              playlist_item_t *p_root,
                                              playlist_item_t *p_item )
{
    playlist_item_t *p_next;

#ifdef PLAYLIST_DEBUG
    if( p_item != NULL )
        msg_Dbg( p_playlist, "finding next of %s within %s",
                        p_item->p_input->psz_name,  p_root->p_input->psz_name );
    else
        msg_Dbg( p_playlist, "finding something to play within %s",
                         p_root->p_input->psz_name );
#endif

    if( !p_root  || p_root->i_children == -1 )
    {
        msg_Err( p_playlist,"invalid arguments for GetNextEnabledLeaf" );
        return NULL;
    }

    /* Now, walk the tree until we find a suitable next item */
    p_next = p_item;
    do
    {
        p_next = GetNextItem( p_playlist, p_root, p_next );
    } while ( p_next && p_next != p_root &&
              !( p_next->i_children == -1 &&
              !(p_next->i_flags & PLAYLIST_DBL_FLAG) ) );

#ifdef PLAYLIST_DEBUG
    if( p_next == NULL )
        msg_Dbg( p_playlist, "At end of node" );
#endif
    return p_next;
}

/**
 * Finds the previous item to play
 *
 * \param p_playlist the playlist
 * \param p_root the root node
 * \param p_item the previous item  (NULL if none )
 * \return the next item to play, or NULL if none found
 */
playlist_item_t *playlist_GetPrevLeaf( playlist_t *p_playlist,
                                       playlist_item_t *p_root,
                                       playlist_item_t *p_item )
{
    playlist_item_t *p_prev;

#ifdef PLAYLIST_DEBUG
    if( p_item != NULL )
        msg_Dbg( p_playlist, "finding previous of %s within %s",
                        p_item->p_input->psz_name,  p_root->p_input->psz_name );
    else
        msg_Dbg( p_playlist, "finding previous to play within %s",
                         p_root->p_input->psz_name );
#endif

    if( !p_root || p_root->i_children == -1 )
    {
        msg_Err( p_playlist,"invalid arguments for GetPrevLeaf" );
        return NULL;
    }

    /* Now, walk the tree until we find a suitable previous item */
    p_prev = p_item;
    do
    {
        p_prev = GetPrevItem( p_playlist, p_root, p_prev );
    } while ( p_prev && p_prev != p_root && p_prev->i_children != -1 );

#ifdef PLAYLIST_DEBUG
    if( p_prev == NULL )
        msg_Dbg( p_playlist, "At beginning of node" );
#endif
    return p_prev;
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
    playlist_item_t *p_parent;
    int i;

    /* Node with children, get the first one */
    if( p_item && p_item->i_children > 0 )
        return p_item->pp_children[0];

    if( p_item != NULL )
        p_parent = p_item->p_parent;
    else
        p_parent = p_root;

    for( i= 0 ; i < p_parent->i_children ; i++ )
    {
        if( p_item == NULL || p_parent->pp_children[i] == p_item )
        {
            if( p_item == NULL )
                i = -1;

            if( i+1 >= p_parent->i_children )
            {
                /* Was already the last sibling. Look for uncles */
#ifdef PLAYLIST_DEBUG
                msg_Dbg( p_playlist, "Current item is the last of the node,"
                                     "looking for uncle from %s",
                                     p_parent->p_input->psz_name );
#endif
                if( p_parent == p_root )
                {
#ifdef PLAYLIST_DEBUG
                    msg_Dbg( p_playlist, "Already at root" );
                    return NULL;
#endif
                }
                return GetNextUncle( p_playlist, p_item, p_root );
            }
            else
            {
                return  p_parent->pp_children[i+1];
            }
        }
    }
    msg_Err( p_playlist, "I should not be here" );
    return NULL;
}

playlist_item_t *GetNextUncle( playlist_t *p_playlist, playlist_item_t *p_item,
                               playlist_item_t *p_root )
{
    playlist_item_t *p_parent = p_item->p_parent;
    playlist_item_t *p_grandparent;
    vlc_bool_t b_found = VLC_FALSE;

    if( p_parent != NULL )
    {
        p_grandparent = p_parent->p_parent;
        while( 1 )
        {
            int i;
            for( i = 0 ; i< p_grandparent->i_children ; i++ )
            {
                if( p_parent == p_grandparent->pp_children[i] )
                {
#ifdef PLAYLIST_DEBUG
                    msg_Dbg( p_playlist, "parent %s found as child %i of "
                                    "grandparent %s",
                                    p_parent->p_input->psz_name, i,
                                    p_grandparent->p_input->psz_name );
#endif
                    b_found = VLC_TRUE;
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
    vlc_bool_t b_found = VLC_FALSE;

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
                    b_found = VLC_TRUE;
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
#ifdef PLAYLIST_DEBUG
                msg_Dbg( p_playlist, "Current item is the first of the node,"
                                     "looking for uncle from %s",
                                     p_parent->p_input->psz_name );
#endif
                return GetPrevUncle( p_playlist, p_item, p_root );
            }
            else
            {
                return p_parent->pp_children[i-1];
            }
        }
    }
    msg_Err( p_playlist, "I should not be here" );
    return NULL;
}

/* Dump the contents of a node */
void playlist_NodeDump( playlist_t *p_playlist, playlist_item_t *p_item,
                        int i_level )
{
    char str[512];
    int i;

    if( i_level == 1 )
    {
        msg_Dbg( p_playlist, "%s (%i)",
                        p_item->p_input->psz_name, p_item->i_children );
    }

    if( p_item->i_children == -1 )
    {
        return;
    }

    for( i = 0; i< p_item->i_children; i++ )
    {
        memset( str, 32, 512 );
        sprintf( str + 2 * i_level , "%s (%i)",
                                p_item->pp_children[i]->p_input->psz_name,
                                p_item->pp_children[i]->i_children );
        msg_Dbg( p_playlist, "%s",str );
        if( p_item->pp_children[i]->i_children >= 0 )
        {
            playlist_NodeDump( p_playlist, p_item->pp_children[i],
                              i_level + 1 );
        }
    }
    return;
}
