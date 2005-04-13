/*****************************************************************************
 * view.c : Playlist views functions
 *****************************************************************************
 * Copyright (C) 1999-2004 VideoLAN
 * $Id: item.c 7997 2004-06-18 11:35:45Z sigmunau $
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
#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "vlc_playlist.h"

#undef PLAYLIST_DEBUG

/************************************************************************
 * Local prototypes
 ************************************************************************/

/* TODO: inline */
playlist_item_t *playlist_FindDirectParent( playlist_t *p_playlist,
                                        playlist_item_t *, int );

playlist_item_t *playlist_RecursiveFindNext( playlist_t *p_playlist,
                int i_view,
                playlist_item_t *p_root,
                playlist_item_t *p_item,
                playlist_item_t *p_parent );

playlist_item_t *playlist_RecursiveFindPrev( playlist_t *p_playlist,
                int i_view,
                playlist_item_t *p_root,
                playlist_item_t *p_item,
                playlist_item_t *p_parent );

void playlist_NodeDump( playlist_t *p_playlist, playlist_item_t *p_item,
                        int i_level );

/**********************************************************************
 * Exported View management functions
 **********************************************************************/

/**
 * Create a new view
 *
 * \param p_playlist a playlist object
 * \param i_id the view identifier
 * \return the new view or NULL on failure
 */
playlist_view_t * playlist_ViewCreate( playlist_t *p_playlist, int i_id,
                                       char *psz_name )
{
    playlist_view_t * p_view;

    p_view = malloc( sizeof( playlist_view_t ) );

    memset( p_view, 0, sizeof( playlist_view_t ) );

    p_view->p_root = playlist_NodeCreate( p_playlist, i_id, NULL, NULL );
    p_view->i_id = i_id;
    p_view->psz_name = psz_name ? strdup( psz_name ) : strdup(_("Undefined") );

    return p_view;
}

/**
 * Creates a new view and add it to the list
 *
 * This function must be entered without the playlist lock
 *
 * \param p_playlist a playlist object
 * \param i_id the view identifier
 * \return VLC_SUCCESS or an error
 */
int playlist_ViewInsert( playlist_t *p_playlist, int i_id, char *psz_name )
{
    playlist_view_t *p_view =
        playlist_ViewCreate( p_playlist, i_id , psz_name );
    if( !p_view )
    {
        msg_Err( p_playlist, "Creation failed" );
        return VLC_EGENERIC;
    }

    vlc_mutex_lock( &p_playlist->object_lock );

    INSERT_ELEM( p_playlist->pp_views, p_playlist->i_views,
                 p_playlist->i_views, p_view );

    vlc_mutex_unlock( &p_playlist->object_lock );
    return VLC_SUCCESS;
}


/**
 * Deletes a view
 *
 * This function must be entered wit the playlist lock
 *
 * \param p_view the view to delete
 * \return nothing
 */
int playlist_ViewDelete( playlist_t *p_playlist,playlist_view_t *p_view )
{
    playlist_NodeDelete( p_playlist, p_view->p_root, VLC_TRUE, VLC_TRUE );
    REMOVE_ELEM( p_playlist->pp_views, p_playlist->i_views, 0 );
    return VLC_SUCCESS;
}


/**
 * Dumps the content of a view
 *
 * \param p_playlist the playlist
 * \param p_view the view to dump
 * \return nothing
 */
int playlist_ViewDump( playlist_t *p_playlist, playlist_view_t *p_view )
{
#ifdef PLAYLIST_DEBUG
    msg_Dbg( p_playlist, "dumping view %i",p_view->i_id );
    playlist_NodeDump( p_playlist,p_view->p_root, 1 );
#endif
    return VLC_SUCCESS;
}

/**
 * Counts the items of a view
 *
 * \param p_playlist the playlist
 * \param p_view the view to count
 * \return the number of items
 */
int playlist_ViewItemCount( playlist_t *p_playlist,
                            playlist_view_t *p_view )
{
    return playlist_NodeChildrenCount( p_playlist, p_view->p_root );
}


/**
 * Updates a view. Only make sense for "sorted" and "ALL" views
 *
 * \param p_playlist the playlist
 * \param i_view the view to update
 * \return nothing
 */
int playlist_ViewUpdate( playlist_t *p_playlist, int i_view)
{
    playlist_view_t *p_view = playlist_ViewFind( p_playlist, i_view );

    if( p_view == NULL )
    {
        return VLC_EGENERIC;
    }

    if( i_view == VIEW_ALL )
    {
        p_view->p_root->i_children = p_playlist->i_size;
        p_view->p_root->pp_children = p_playlist->pp_items;
    }

    /* Handle update of sorted views here */
    if( i_view >= VIEW_FIRST_SORTED )
    {
        int i_sort_type;
        playlist_ViewEmpty( p_playlist, i_view, VLC_FALSE );

        switch( i_view )
        {
            case VIEW_S_AUTHOR: i_sort_type = SORT_AUTHOR;break;
            case VIEW_S_GENRE: i_sort_type = SORT_GENRE;break;
            default: i_sort_type = SORT_AUTHOR;
        }
        playlist_NodeGroup( p_playlist, i_view, p_view->p_root,
                            p_playlist->pp_items,p_playlist->i_size,
                            i_sort_type, ORDER_NORMAL );
    }


    return VLC_SUCCESS;
}


/**
 * Find a view
 *
 * \param p_playlist the playlist
 * \param i_id the id to find
 * \return the found view or NULL if not found
 */
playlist_view_t *playlist_ViewFind( playlist_t *p_playlist, int i_id )
{
    int i;
    for( i=0 ; i< p_playlist->i_views ; i++ )
    {
        if( p_playlist->pp_views[i]->i_id == i_id )
        {
            return p_playlist->pp_views[i];
        }
    }
    return NULL;
}


int playlist_ViewEmpty( playlist_t *p_playlist, int i_view,
                        vlc_bool_t b_delete_items )
{
    playlist_view_t *p_view = playlist_ViewFind( p_playlist, i_view );

    if( p_view == NULL )
    {
        return VLC_EGENERIC;
    }

    return playlist_NodeEmpty( p_playlist, p_view->p_root, b_delete_items );
}

/**********************************************************************
 * Exported Nodes management functions
 **********************************************************************/



/**
 * Create a playlist node
 *
 * \param p_playlist the playlist
 * \paam psz_name the name of the node
 * \param p_parent the parent node to attach to or NULL if no attach
 * \return the new node
 */
playlist_item_t * playlist_NodeCreate( playlist_t *p_playlist, int i_view,
                                       char *psz_name,
                                       playlist_item_t *p_parent )
{
    /* Create the item */
    playlist_item_t *p_item = (playlist_item_t *)malloc(
                                        sizeof( playlist_item_t ) );
    vlc_value_t val;
    playlist_add_t *p_add = (playlist_add_t*)malloc( sizeof(playlist_add_t));

    if( p_item == NULL )
    {
        return NULL;
    }
    vlc_input_item_Init( VLC_OBJECT(p_playlist), &p_item->input );

    if( psz_name != NULL )
    {
        p_item->input.psz_name = strdup( psz_name );
    }
    else
    {
        p_item->input.psz_name = strdup( _("Undefined") );
    }

    p_item->input.psz_uri = NULL;

    p_item->b_enabled = VLC_TRUE;
    p_item->i_nb_played = 0;

    p_item->i_flags = 0;

    p_item->i_children = 0;
    p_item->pp_children = NULL;

    p_item->input.i_duration = -1;
    p_item->input.ppsz_options = NULL;
    p_item->input.i_options = 0;
    p_item->input.i_categories = 0;
    p_item->input.pp_categories = NULL;
    p_item->input.i_id = ++p_playlist->i_last_id;

    p_item->input.i_type = ITEM_TYPE_NODE;

    p_item->pp_parents = NULL;
    p_item->i_parents = 0;

    p_item->i_flags |= PLAYLIST_SKIP_FLAG; /* Default behaviour */

    vlc_mutex_init( p_playlist, &p_item->input.lock );

    INSERT_ELEM( p_playlist->pp_all_items,
                 p_playlist->i_all_size,
                 p_playlist->i_all_size,
                 p_item );

    if( p_parent != NULL )
    {
        playlist_NodeAppend( p_playlist, i_view, p_item, p_parent );
    }

    p_add->i_node = p_parent ? p_parent->input.i_id : -1;
    p_add->i_item = p_item->input.i_id;
    p_add->i_view = i_view;
    val.p_address = p_add;
    var_Set( p_playlist, "item-append", val);

    free( p_add );

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
            playlist_Delete( p_playlist, p_root->pp_children[i]->input.i_id );
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
            /* Delete the item here */
            playlist_Delete( p_playlist, p_root->pp_children[i]->input.i_id );
        }
    }
    /* Delete the node */
    if( p_root->i_flags & PLAYLIST_RO_FLAG && !b_force )
    {
    }
    else
    {
        for( i = 0 ; i< p_root->i_parents; i++ )
        {
            playlist_NodeRemoveItem( p_playlist, p_root,
                                     p_root->pp_parents[i]->p_parent );
        }
        var_SetInteger( p_playlist, "item-deleted", p_root->input.i_id );

        i_bottom = 0; i_top = p_playlist->i_all_size;
        i = i_top / 2;
        while( p_playlist->pp_all_items[i]->input.i_id != p_root->input.i_id &&
               i_top > i_bottom )
        {
            if( p_playlist->pp_all_items[i]->input.i_id < p_root->input.i_id )
            {
                i_bottom = i + 1;
            }
            else
            {
                i_top = i - 1;
            }
            i = i_bottom + ( i_top - i_bottom ) / 2;
        }
        if( p_playlist->pp_all_items[i]->input.i_id == p_root->input.i_id )
        {
            REMOVE_ELEM( p_playlist->pp_all_items, p_playlist->i_all_size, i );
        }
        playlist_ItemDelete( p_root );
    }
    return VLC_SUCCESS;
}


/**
 * Adds an item to the childs of a node
 *
 * \param p_playlist the playlist
 * \param i_view the view of the node ( needed for parent search )
 * \param p_item the item to append
 * \param p_parent the parent node
 * \return VLC_SUCCESS or an error
 */
int playlist_NodeAppend( playlist_t *p_playlist,
                         int i_view,
                         playlist_item_t *p_item,
                         playlist_item_t *p_parent )
{
    return playlist_NodeInsert( p_playlist, i_view, p_item, p_parent, -1 );
}

int playlist_NodeInsert( playlist_t *p_playlist,
                         int i_view,
                         playlist_item_t *p_item,
                         playlist_item_t *p_parent,
                         int i_position )
{
   int i;
   vlc_bool_t b_found = VLC_FALSE;
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

   /* Add the parent to the array */
   for( i= 0; i< p_item->i_parents ; i++ )
   {
       if( p_item->pp_parents[i]->i_view == i_view )
       {
           b_found = VLC_TRUE;
           break;
       }
   }
   if( b_found == VLC_FALSE )
   {
        struct item_parent_t *p_ip = (struct item_parent_t *)
                                     malloc(sizeof(struct item_parent_t) );
        p_ip->i_view = i_view;
        p_ip->p_parent = p_parent;

        INSERT_ELEM( p_item->pp_parents,
                     p_item->i_parents, p_item->i_parents,
                     p_ip );
   }

   /* Let the interface know this has been updated */
   p_parent->i_serial++;
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
   if( !p_parent || p_parent->i_children == -1 )
   {
        msg_Err( p_playlist, "invalid node" );
   }

   for( i= 0; i< p_parent->i_children ; i++ )
   {
       if( p_parent->pp_children[i] == p_item )
       {
           REMOVE_ELEM( p_parent->pp_children, p_parent->i_children, i );
       }
   }

   /* Let the interface know this has been updated */
   p_parent->i_serial++;

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
    {
        return 0;
    }

    for( i=0 ; i< p_node->i_children;i++ )
    {
        if( p_node->pp_children[i]->i_children == -1 )
        {
            i_nb++;
        }
        else
        {
            i_nb += playlist_NodeChildrenCount( p_playlist, 
                                                p_node->pp_children[i] );
        }
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
        if( !strcmp( p_node->pp_children[i]->input.psz_name, psz_search ) )
        {
            return p_node->pp_children[i];
        }
    }
    return NULL;
}


/**********************************************************************
 * Tree functions
 **********************************************************************/

/**
 * Finds the next item to play
 *
 * \param p_playlist the playlist
 * \param i_view the view
 * \param p_root the root node
 * \param p_node the node we are playing from
 * \param p_item the item we were playing (NULL if none )
 * \return the next item to play, or NULL if none found
 */
playlist_item_t *playlist_FindNextFromParent( playlist_t *p_playlist,
                                        int i_view, /* FIXME: useless */
                                        playlist_item_t *p_root,
                                        playlist_item_t *p_node,
                                        playlist_item_t *p_item )
{
    playlist_item_t *p_search, *p_next;

#ifdef PLAYLIST_DEBUG
    if( p_item != NULL )
    {
        msg_Dbg( p_playlist, "finding next of %s within %s",
                        p_item->input.psz_name, p_node->input.psz_name );
    }
    else
    {
        msg_Dbg( p_playlist, "finding something to play within %s",
                                 p_node->input.psz_name );

    }
#endif

    if( !p_node  || p_node->i_children == -1 )
    {
        msg_Err( p_playlist,"invalid arguments for FindNextFromParent" );
        return NULL;
    }

    /* Find the parent node of the item */
    if( p_item != NULL )
    {
        p_search = playlist_FindDirectParent( p_playlist, p_item, i_view );
        if( p_search == NULL )
        {
            msg_Err( p_playlist, "parent node not found" );
            return NULL;
        }
    }
    else
    {
        p_search = p_node;
    }

    /* Now, go up the tree until we find a suitable next item */
    p_next = playlist_RecursiveFindNext( p_playlist,i_view,
                                         p_node, p_item, p_search );

    /* Not found, do we go past p_node ? */
    if( p_next == NULL )
    {
        if( p_playlist->b_go_next )
        {
            p_next = playlist_RecursiveFindNext( p_playlist, i_view,
                                p_root, p_item, p_search );
            if( p_next == NULL )
            {
                return NULL;
            }
            /* OK, we could continue, so set our current node to the root */
            p_playlist->status.p_node = p_root;
        }
        else
        {
            return NULL;
        }
    }
    return p_next;
}

/**
 * Finds the previous item to play
 *
 * \param p_playlist the playlist
 * \param i_view the view
 * \param p_root the root node
 * \param p_node the node we are playing from
 * \param p_item the item we were playing (NULL if none )
 * \return the next item to play, or NULL if none found
 */
playlist_item_t *playlist_FindPrevFromParent( playlist_t *p_playlist,
                                        int i_view,
                                        playlist_item_t *p_root,
                                        playlist_item_t *p_node,
                                        playlist_item_t *p_item )
{
    playlist_item_t *p_search, *p_next;

#ifdef PLAYLIST_DEBUG
    if( p_item != NULL )
    {
        msg_Dbg( p_playlist, "Finding prev of %s within %s",
                        p_item->input.psz_name, p_node->input.psz_name );
    }
    else
    {
        msg_Dbg( p_playlist, "Finding prev from %s",p_node->input.psz_name );
    }
#endif

    if( !p_node  || p_node->i_children == -1 )
    {
        msg_Err( p_playlist,"invalid arguments for FindPrevFromParent" );
        return NULL;
    }

    /* Find the parent node of the item */
    if( p_item != NULL )
    {
        p_search = playlist_FindDirectParent( p_playlist, p_item, i_view );
        if( p_search == NULL )
        {
            msg_Err( p_playlist, "parent node not found" );
            return NULL;
        }
    }
    else
    {
        p_search = p_node;
    }

    /* Now, go up the tree until we find a suitable next item */
    p_next = playlist_RecursiveFindPrev( p_playlist,i_view,
                                         p_node, p_item, p_search );

    if( p_next == NULL )
    {
        if( p_playlist->b_go_next )
        {
            p_next = playlist_RecursiveFindPrev( p_playlist, i_view,
                                p_root, p_item, p_search );
            if( p_next == NULL )
            {
                return NULL;
            }
            /* OK, we could continue, so set our current node to the root */
            p_playlist->status.p_node = p_root;
        }
        else
        {
            return NULL;
        }
    }
    return p_next;
}

/************************************************************************
 * Following functions are local
 ***********************************************************************/


/* Recursively search the tree for next item */
playlist_item_t *playlist_RecursiveFindNext( playlist_t *p_playlist,
                int i_view,
                playlist_item_t *p_root,
                playlist_item_t *p_item,
                playlist_item_t *p_parent )
{
    int i;
    playlist_item_t *p_parent_parent;

    for( i= 0 ; i < p_parent->i_children ; i++ )
    {
        if( p_parent->pp_children[i] == p_item || p_item == NULL )
        {
            if( p_item == NULL )
            {
                i = -1;
            }
#ifdef PLAYLIST_DEBUG
            msg_Dbg( p_playlist,"Current item found, child %i of %s",
                                i , p_parent->input.psz_name );
#endif
            /* We found our item */
            if( i+1 >= p_parent->i_children )
            {
                /* Too far... */
#ifdef PLAYLIST_DEBUG
                msg_Dbg( p_playlist, "Going up the tree,at parent of %s",
                                p_parent->input.psz_name );
#endif
                if( p_parent == p_root )
                {
                    /* Hmm, seems it's the end for you, guy ! */
                    return NULL;
                }

                /* Go up one level */
                p_parent_parent = playlist_FindDirectParent( p_playlist,
                                                             p_parent, i_view );
                if( p_parent_parent == NULL )
                {
                    msg_Warn( p_playlist, "Unable to find parent !");
                    return NULL;
                }
                return playlist_RecursiveFindNext( p_playlist, i_view,p_root,
                                                   p_parent, p_parent_parent );
            }
            else
            {
                if( p_parent->pp_children[i+1]->i_children == -1 )
                {
                    /* Cool, we have found a real item to play */
#ifdef PLAYLIST_DEBUG
                    msg_Dbg( p_playlist, "Playing child %i of %s",
                                     i+1 , p_parent->input.psz_name );
#endif
                    return p_parent->pp_children[i+1];
                }
                else if( p_parent->pp_children[i+1]->i_children > 0 )
                {
                    /* Select the first child of this node */
#ifdef PLAYLIST_DEBUG
                    msg_Dbg( p_playlist, "%s is a node with children, "
                                 "playing the first",
                                  p_parent->pp_children[i+1]->input.psz_name);
#endif
                    if( p_parent->pp_children[i+1]->pp_children[0]
                                    ->i_children >= 0 )
                    {
                        /* first child is a node ! */
                        return playlist_RecursiveFindNext( p_playlist, i_view,
                                   p_root, NULL ,
                                   p_parent->pp_children[i+1]->pp_children[0]);
                    }
                    return p_parent->pp_children[i+1]->pp_children[0];
                }
                else
                {
                    /* This node has no child... We must continue */
#ifdef PLAYLIST_DEBUG
                    msg_Dbg( p_playlist, "%s is a node with no children",
                                 p_parent->pp_children[i+1]->input.psz_name);
#endif
                    p_item = p_parent->pp_children[i+1];
                }
            }
        }
    }
    /* Just in case :) */
    return NULL;
}

/* Recursively search the tree for previous item */
playlist_item_t *playlist_RecursiveFindPrev( playlist_t *p_playlist,
                int i_view,
                playlist_item_t *p_root,
                playlist_item_t *p_item,
                playlist_item_t *p_parent )
{
    int i;
    playlist_item_t *p_parent_parent;

    for( i= p_parent->i_children - 1 ; i >= 0 ; i-- )
    {
        if( p_parent->pp_children[i] == p_item || p_item == NULL )
        {
            if( p_item == NULL )
            {
                i = -1;
            }
#ifdef PLAYLIST_DEBUG
            msg_Dbg( p_playlist,"Current item found, child %i of %s",
                             i , p_parent->input.psz_name );
#endif
            /* We found our item */
            if( i < 1 )
            {
                /* Too far... */
#ifdef PLAYLIST_DEBUG
                msg_Dbg( p_playlist, "Going up the tree,at parent of %s",
                                     p_parent->input.psz_name );
#endif
                if( p_parent == p_root )
                {
                    /* Hmm, seems it's the end for you, guy ! */
                    return NULL;
                }
                /* Go up one level */
                p_parent_parent = playlist_FindDirectParent( p_playlist,
                                            p_parent, i_view );
                return playlist_RecursiveFindPrev( p_playlist, i_view,p_root,
                                            p_parent, p_parent_parent );
            }
            else
            {
                if( p_parent->pp_children[i-1]->i_children == -1 )
                {
                    /* Cool, we have found a real item to play */
#ifdef PLAYLIST_DEBUG
                    msg_Dbg( p_playlist, "Playing child %i of %s",
                                     i-1, p_parent->input.psz_name );
#endif
                    return p_parent->pp_children[i-1];
                }
                else if( p_parent->pp_children[i-1]->i_children > 0 )
                {
                    /* Select the last child of this node */
#ifdef PLAYLIST_DEBUG
                    msg_Dbg( p_playlist, "%s is a node with children,"
                                   " playing the last",
                                   p_parent->pp_children[i-1]->input.psz_name);
#endif
                    if( p_parent->pp_children[i-1]->pp_children[p_parent->
                            pp_children[i-1]->i_children-1]->i_children >= 0 )
                    {
                        /* Last child is a node */
                        return playlist_RecursiveFindPrev( p_playlist, i_view,
                                    p_root,NULL,
                                    p_parent->pp_children[i-1]->pp_children[
                                    p_parent->pp_children[i-1]->i_children-1]);
                    }
                    return p_parent->pp_children[i-1]->pp_children[
                                 p_parent->pp_children[i-1]->i_children-1];
                }
                else
                {
                    /* This node has no child... We must continue */
#ifdef PLAYLIST_DEBUG
                    msg_Dbg( p_playlist, "%s is a node with no children",
                                p_parent->pp_children[i-1]->input.psz_name);
#endif
                    p_item = p_parent->pp_children[i-1];
                }
            }
        }
    }
    return NULL;
}

/* This function returns the parent of an item in a view */
playlist_item_t *playlist_FindDirectParent( playlist_t *p_playlist,
                                         playlist_item_t *p_item,
                                         int i_view )
{
        int i = 0;
        for( i= 0; i< p_item->i_parents ; i++ )
        {
            if( p_item->pp_parents[i]->i_view == i_view )
            {
                return p_item->pp_parents[i]->p_parent;
            }
        }
        return NULL;
}


#ifdef PLAYLIST_DEBUG
/* This function dumps a node : to be used only for debug*/
void playlist_NodeDump( playlist_t *p_playlist, playlist_item_t *p_item,
                        int i_level )
{
    char str[512];
    int i;

    if( i_level == 1 )
    {
        msg_Dbg( p_playlist, "%s (%i)",
                        p_item->input.psz_name, p_item->i_children );
    }

    if( p_item->i_children == -1 )
    {
        return;
    }

    for( i = 0; i< p_item->i_children; i++ )
    {
        memset( str, 32, 512 );
        sprintf( str + 2 * i_level , "%s (%i)",
                                p_item->pp_children[i]->input.psz_name,
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
#endif
