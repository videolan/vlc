/*****************************************************************************
 * item.c : Playlist item creation/deletion/add/removal functions
 *****************************************************************************
 * Copyright (C) 1999-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Clément Stenac <zorglub@videolan.org>
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

void AddItem( playlist_t *p_playlist, playlist_item_t *p_item,
              playlist_item_t *p_node, int i_pos );
void GoAndPreparse( playlist_t *p_playlist, int i_mode,
                    playlist_item_t *p_item );
void ChangeToNode( playlist_t *p_playlist, playlist_item_t *p_item );
int DeleteInner( playlist_t * p_playlist, playlist_item_t *p_item,
                          vlc_bool_t b_stop );

/*****************************************************************************
 * Playlist item creation
 *****************************************************************************/
/**
 * Create a new item, without adding it to the playlist
 *
 * \param p_obj a vlc object (anyone will do)
 * \param psz_uri the mrl of the item
 * \param psz_name a text giving a name or description of the item
 * \return the new item or NULL on failure
 */
playlist_item_t * __playlist_ItemNew( vlc_object_t *p_obj,
                                      const char *psz_uri,
                                      const char *psz_name )
{
    return playlist_ItemNewWithType( p_obj, psz_uri,
                                     psz_name, 0, NULL, -1,
                                     ITEM_TYPE_UNKNOWN );
}

playlist_item_t * playlist_ItemNewWithType( vlc_object_t *p_obj,
                                            const char *psz_uri,
                                            const char *psz_name,
                                            int i_options,
                                            const char **ppsz_options,
                                            int i_duration,
                                            int i_type )
{
    if( psz_uri == NULL ) return NULL;
    input_item_t *p_input = input_ItemNewWithType( p_obj, psz_uri,
                                        psz_name, i_options, ppsz_options,
                                        i_duration, i_type );
    return playlist_ItemNewFromInput( p_obj, p_input );
}

playlist_item_t *__playlist_ItemNewFromInput( vlc_object_t *p_obj,
                                            input_item_t *p_input )
{
    /** FIXME !!!!! don't find playlist each time */
    playlist_t *p_playlist = (playlist_t *)vlc_object_find( p_obj, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    playlist_item_t * p_item;
    p_item = malloc( sizeof( playlist_item_t ) );
    if( p_item == NULL ) return NULL;

    p_item->p_input = p_input;
    vlc_gc_incref( p_item->p_input );

    p_item->i_id = ++p_playlist->i_last_playlist_id;

    p_item->p_parent = NULL;
    p_item->i_children = -1;
    p_item->pp_children = NULL;
    p_item->i_flags = 0;

    vlc_object_release( p_playlist );

    return p_item;
}

/**
 * Copy a playlist item - FIXME: Rewrite FIXME
 *
 * Creates a new item with name, mrl and meta infor like the
 * source. Does not copy children for node type items.
 * \param p_obj any vlc object, needed for mutex init
 * \param p_item the item to copy
 * \return pointer to the new item, or NULL on error
 * \note function takes the lock on p_item
 */
playlist_item_t *__playlist_ItemCopy( vlc_object_t *p_obj,
                                      playlist_item_t *p_item )
{
    playlist_item_t *p_res;
    int i;
    vlc_mutex_lock( &p_item->p_input->lock );

    p_res = malloc( sizeof( playlist_item_t ) );
    if( p_res == NULL )
    {
        vlc_mutex_unlock( &p_item->p_input->lock );
        return NULL;
    }

    *p_res = *p_item;
    vlc_mutex_init( p_obj, &p_res->p_input->lock );

    if( p_item->p_input->i_options )
        p_res->p_input->ppsz_options =
            malloc( p_item->p_input->i_options * sizeof(char*) );
    for( i = 0; i < p_item->p_input->i_options; i++ )
    {
        p_res->p_input->ppsz_options[i] = strdup( p_item->p_input->ppsz_options[i] );
    }

    if( p_item->i_children != -1 )
    {
        msg_Warn( p_obj, "not copying playlist-item's children" );
        p_res->i_children = -1;
        p_res->pp_children = NULL;
    }
    p_res->p_parent = NULL;

    if( p_item->p_input->psz_name )
        p_res->p_input->psz_name = strdup( p_item->p_input->psz_name );
    if( p_item->p_input->psz_uri )
        p_res->p_input->psz_uri = strdup( p_item->p_input->psz_uri );

    if( p_item->p_input->i_es )
    {
        p_res->p_input->es =
            (es_format_t**)malloc( p_item->p_input->i_es * sizeof(es_format_t*));
        for( i = 0; i < p_item->p_input->i_es; i++ )
        {
            p_res->p_input->es[ i ] = (es_format_t*)malloc(sizeof(es_format_t*));
            es_format_Copy( p_res->p_input->es[ i ],
                         p_item->p_input->es[ i ] );
        }
    }
    if( p_item->p_input->i_categories )
    {
        p_res->p_input->pp_categories = NULL;
        p_res->p_input->i_categories = 0;
        for( i = 0; i < p_item->p_input->i_categories; i++ )
        {
            info_category_t *p_incat;
            p_incat = p_item->p_input->pp_categories[i];
            if( p_incat->i_infos )
            {
                int j;
                for( j = 0; j < p_incat->i_infos; j++ )
                {
                    vlc_input_item_AddInfo( p_res->p_input, p_incat->psz_name,
                                            p_incat->pp_infos[j]->psz_name,
                                            "%s",
                                            p_incat->pp_infos[j]->psz_value );
                }
            }
        }
    }

    vlc_mutex_unlock( &p_item->p_input->lock );
    return p_res;
}

/***************************************************************************
 * Playlist item destruction
 ***************************************************************************/

/** Delete a playlist item and detach its input item */
int playlist_ItemDelete( playlist_item_t *p_item )
{
    vlc_gc_decref( p_item->p_input );
    free( p_item );
    return VLC_SUCCESS;
}

/** Remove an input item from ONELEVEL and CATEGORY */
int playlist_DeleteAllFromInput( playlist_t *p_playlist, int i_input_id )
{
    playlist_DeleteFromInput( p_playlist, i_input_id,
                              p_playlist->p_root_category, VLC_TRUE );
    playlist_DeleteFromInput( p_playlist, i_input_id,
                              p_playlist->p_root_onelevel, VLC_TRUE );
    return VLC_SUCCESS;
}

/** Remove an input item from ONELEVEL and CATEGORY.
 * This function must be entered without the playlist lock */
int playlist_LockDeleteAllFromInput( playlist_t * p_playlist, int i_id )
{
    int i_ret;
    vlc_mutex_lock( &p_playlist->object_lock );
    i_ret = playlist_DeleteAllFromInput( p_playlist, i_id );
    vlc_mutex_unlock( &p_playlist->object_lock );
    return i_ret;
}

/** Remove an input item when it appears from a root playlist item */
int playlist_DeleteFromInput( playlist_t *p_playlist, int i_input_id,
                              playlist_item_t *p_root, vlc_bool_t b_do_stop )
{
    int i;
    for( i = 0 ; i< p_root->i_children ; i++ )
    {
        if( p_root->pp_children[i]->i_children == -1 &&
            p_root->pp_children[i]->p_input->i_id == i_input_id )
        {
            DeleteInner( p_playlist, p_root->pp_children[i], b_do_stop );
        }
        else if( p_root->pp_children[i]->i_children >= 0 )
        {
            return playlist_DeleteFromInput( p_playlist, i_input_id,
                                        p_root->pp_children[i], b_do_stop );
        }
    }
}

/** Remove a playlist item from the playlist, given its id */
int playlist_DeleteFromItemId( playlist_t *p_playlist, int i_id )
{
    playlist_item_t *p_item = playlist_ItemGetById( p_playlist, i_id );
    if( !p_item ) return VLC_EGENERIC;
    return DeleteInner( p_playlist, p_item, VLC_TRUE );
}

/** Remove a playlist item from the playlist, given its id
 * This function should be entered without the playlist lock */
int playlist_LockDelete( playlist_t * p_playlist, int i_id )
{
    int i_ret;
    vlc_mutex_lock( &p_playlist->object_lock );
    i_ret = playlist_DeleteFromItemId( p_playlist, i_id );
    vlc_mutex_unlock( &p_playlist->object_lock );
    return i_ret;
}

/** Clear the playlist */
void playlist_Clear( playlist_t * p_playlist )
{
    playlist_NodeEmpty( p_playlist, p_playlist->p_root_category, VLC_TRUE );
    playlist_NodeEmpty( p_playlist, p_playlist->p_root_onelevel, VLC_TRUE );
}
/** Clear the playlist. This function must be entered without the lock */
void playlist_LockClear( playlist_t *p_playlist )
{
    vlc_mutex_lock( &p_playlist->object_lock );
    playlist_Clear( p_playlist );
    vlc_mutex_unlock( &p_playlist->object_lock );
}

/***************************************************************************
 * Playlist item addition
 ***************************************************************************/

/**
 * Add a MRL into the playlist.
 *
 * \param p_playlist the playlist to add into
 * \param psz_uri the mrl to add to the playlist
 * \param psz_name a text giving a name or description of this item
 * \param i_mode the mode used when adding
 * \param i_pos the position in the playlist where to add. If this is
 *        PLAYLIST_END the item will be added at the end of the playlist
 *        regardless of it's size
 * \return The id of the playlist item
 */
int playlist_PlaylistAdd( playlist_t *p_playlist, const char *psz_uri,
                          const char *psz_name, int i_mode, int i_pos )
{
    return playlist_PlaylistAddExt( p_playlist, psz_uri, psz_name,
                                    i_mode, i_pos, -1, NULL, 0 );
}

/**
 * Add a MRL into the playlist, duration and options given
 *
 * \param p_playlist the playlist to add into
 * \param psz_uri the mrl to add to the playlist
 * \param psz_name a text giving a name or description of this item
 * \param i_mode the mode used when adding
 * \param i_pos the position in the playlist where to add. If this is
 *        PLAYLIST_END the item will be added at the end of the playlist
 *        regardless of it's size
 * \param i_duration length of the item in milliseconds.
 * \param ppsz_options an array of options
 * \param i_options the number of options
 * \return The id of the playlist item
*/
int playlist_PlaylistAddExt( playlist_t *p_playlist, const char * psz_uri,
                             const char *psz_name, int i_mode, int i_pos,
                             mtime_t i_duration, const char **ppsz_options,
                             int i_options )
{
    input_item_t *p_input = input_ItemNewExt( p_playlist, psz_uri, psz_name,
                                              i_options, ppsz_options,
                                              i_duration );

    return playlist_PlaylistAddInput( p_playlist, p_input, i_mode, i_pos );
}

/** Add an input item to the playlist node */
int playlist_PlaylistAddInput( playlist_t* p_playlist, input_item_t *p_input,
                               int i_mode, int i_pos )
{
    playlist_item_t *p_item;
    p_input->i_id = ++p_playlist->i_last_input_id;

    msg_Dbg( p_playlist, "adding playlist item `%s' ( %s )",
             p_input->psz_name, p_input->psz_uri );

    vlc_mutex_lock( &p_playlist->object_lock );

    /* Add to ONELEVEL */
    p_item = playlist_ItemNewFromInput( p_playlist, p_input );
    if( p_item == NULL ) return VLC_EGENERIC;
    AddItem( p_playlist, p_item,p_playlist->p_local_onelevel, i_pos );

    /* Add to CATEGORY */
    p_item = playlist_ItemNewFromInput( p_playlist, p_input );
    if( p_item == NULL ) return VLC_EGENERIC;
    AddItem( p_playlist, p_item, p_playlist->p_local_category, i_pos );

    GoAndPreparse( p_playlist, i_mode, p_item );

    vlc_mutex_unlock( &p_playlist->object_lock );
    return VLC_SUCCESS;
}

/** Add an input item to p_direct_parent in the category tree, and to the
 *  matching top category in onelevel **/
int playlist_BothAddInput( playlist_t *p_playlist,
                           input_item_t *p_input,
                           playlist_item_t *p_direct_parent,
                           int i_mode, int i_pos )
{
    playlist_item_t *p_item, *p_up;
    int i_top;
    vlc_mutex_lock( & p_playlist->object_lock );

    /* Add to category */
    p_item = playlist_ItemNewFromInput( p_playlist, p_input );
    if( p_item == NULL ) return VLC_EGENERIC;
    fprintf( stderr, "Adding to CATEGORY\n");
    AddItem( p_playlist, p_item, p_direct_parent, i_pos );

    /* Add to onelevel */
    p_item = playlist_ItemNewFromInput( p_playlist, p_input );
    if( p_item == NULL ) return VLC_EGENERIC;
    fprintf( stderr, "Adding to ONE\n");

    p_up = p_direct_parent;
    while( p_up->p_parent != p_playlist->p_root_category )
    {
        p_up = p_up->p_parent;
    }
    for( i_top = 0 ; i_top < p_playlist->p_root_onelevel->i_children; i_top++ )
    {
        if( p_playlist->p_root_onelevel->pp_children[i_top]->p_input->i_id == p_up->p_input->i_id )
        {
            AddItem( p_playlist, p_item,
                     p_playlist->p_root_onelevel->pp_children[i_top], i_pos );
            break;
        }
    }
    GoAndPreparse( p_playlist, i_mode, p_item );

    vlc_mutex_unlock( &p_playlist->object_lock );
    return VLC_SUCCESS;
}

/**
 * Add an item where it should be added, when adding from a node
 * (ex: directory access, playlist demuxers, services discovery, ... )
 * \param p_playlist the playlist
 * \param p_input the input to add
 * \param p_parent the direct node
 * \param p_item_in_category the item within category root (as returned by playlist_ItemToNode)
 * \param b_forced_parent Are we forced to add only to p_parent ?
 */
void playlist_AddWhereverNeeded( playlist_t *p_playlist, input_item_t *p_input,
                                 playlist_item_t *p_parent,
                                 playlist_item_t *p_item_in_category,
                                 vlc_bool_t b_forced_parent, int i_mode )
{
    /* If we have forced a parent :
     *   - Just add the input to the forced parent (which should be p_parent)
     * Else
     *    - If we have item in category, add to it, and to onelevel (bothadd)
     *    - If we don't, just add to p_parent
     */
    if( b_forced_parent == VLC_TRUE || !p_item_in_category  )
    {
        playlist_NodeAddInput( p_playlist, p_input, p_parent, i_mode,
                               PLAYLIST_END );
    }
    else
    {
        playlist_BothAddInput( p_playlist, p_input, p_item_in_category,
                               i_mode, PLAYLIST_END );
    }
}


/** Add an input item to a given node */
playlist_item_t * playlist_NodeAddInput( playlist_t *p_playlist,
                                         input_item_t *p_input,
                                         playlist_item_t *p_parent,
                                         int i_mode, int i_pos )
{
    playlist_item_t *p_item;

    /* Sanity checks */
    if( !p_parent || p_parent->i_children == -1 )
    {
        msg_Err( p_playlist, "invalid node" );
    }

    vlc_mutex_lock( &p_playlist->object_lock );

    p_item = playlist_ItemNewFromInput( p_playlist, p_input );
    if( p_item == NULL ) return NULL;
    AddItem( p_playlist, p_item, p_parent, i_pos );

    vlc_mutex_unlock( &p_playlist->object_lock );

    return p_item;
}

/** Add a playlist item to a given node */
void playlist_NodeAddItem( playlist_t *p_playlist, playlist_item_t *p_item,
                           playlist_item_t *p_parent, int i_mode, int i_pos )
{
    vlc_mutex_lock( &p_playlist->object_lock );
    AddItem( p_playlist, p_item, p_parent, i_pos );
    vlc_mutex_unlock( &p_playlist->object_lock );
}

/*****************************************************************************
 * Playlist item misc operations
 *****************************************************************************/

/**
 * Transform an item to a node. Return the node in the category tree, or NULL
 * if not found there
 * This function must be entered without the playlist lock
 */
playlist_item_t *playlist_ItemToNode( playlist_t *p_playlist,
                                      playlist_item_t *p_item )
{
    /* What we do
     * Find the input in CATEGORY.
     *  - If we find it
     *    - change it to node
     *    - we'll return it at the end
     *    - Delete the input from ONELEVEL
     *  - If we don't find it, just change to node (we are probably in VLM)
     *    and return NULL
     *
     * If we were in ONELEVEL, we thus retrieve the node in CATEGORY (will be
     * useful for later BothAddInput )
     */

    /** \todo First look if we don't already have it */
    playlist_item_t *p_item_in_category = playlist_ItemFindFromInputAndRoot(
                                            p_playlist, p_item->p_input->i_id,
                                            p_playlist->p_root_category );

    if( p_item_in_category )
    {
        ChangeToNode( p_playlist, p_item_in_category );
        playlist_DeleteFromInput( p_playlist, p_item->p_input->i_id,
                                  p_playlist->p_root_onelevel, VLC_FALSE );
        var_SetInteger( p_playlist, "item-change", p_item->p_input->i_id );
        return p_item_in_category;
    }
    else
    {
        ChangeToNode( p_playlist, p_item );
        return NULL;
    }
}

/** Transform an item to a node
 *  This function must be entered without the playlist lock
 *  \see playlist_ItemToNode
 */
playlist_item_t * playlist_LockItemToNode( playlist_t *p_playlist,
                                           playlist_item_t *p_item )
{
    playlist_item_t *p_ret;
    vlc_mutex_lock( &p_playlist->object_lock );
    p_ret = playlist_ItemToNode( p_playlist, p_item );
    vlc_mutex_unlock( &p_playlist->object_lock );
    return p_ret;
}

/** Find an item within a root, given its input id.
 * \return the first found item, or NULL if not found
 */
playlist_item_t *playlist_ItemFindFromInputAndRoot( playlist_t *p_playlist,
                                                    int i_input_id,
                                                    playlist_item_t *p_root )
{
    int i;
    for( i = 0 ; i< p_root->i_children ; i++ )
    {
        if( p_root->pp_children[i]->i_children == -1 &&
            p_root->pp_children[i]->p_input->i_id == i_input_id )
        {
            return p_root->pp_children[i];
        }
        else if( p_root->pp_children[i]->i_children >= 0 )
        {
            playlist_item_t *p_search =
                 playlist_ItemFindFromInputAndRoot( p_playlist, i_input_id,
                                                    p_root->pp_children[i] );
            if( p_search ) return p_search;
        }
    }
    return NULL;
}


/**
 * Moves an item
 *
 * This function must be entered with the playlist lock
 *
 * \param p_playlist the playlist
 * \param p_item the item to move
 * \param p_node the new parent of the item
 * \param i_newpos the new position under this new parent
 * \return VLC_SUCCESS or an error
 */
int playlist_TreeMove( playlist_t * p_playlist, playlist_item_t *p_item,
                       playlist_item_t *p_node, int i_newpos )
{
    int j;
    playlist_item_t *p_detach = NULL;

    if( p_node->i_children == -1 ) return VLC_EGENERIC;

    p_detach = p_item->p_parent;
    for( j = 0; j < p_detach->i_children; j++ )
    {
         if( p_detach->pp_children[j] == p_item ) break;
    }
    REMOVE_ELEM( p_detach->pp_children, p_detach->i_children, j );

    /* Attach to new parent */
    INSERT_ELEM( p_node->pp_children, p_node->i_children, i_newpos, p_item );

    return VLC_SUCCESS;
}

/** Send a notification that an item has been added to a node */
void playlist_SendAddNotify( playlist_t *p_playlist, int i_item_id,
                             int i_node_id )
{
    vlc_value_t val;
    playlist_add_t *p_add = (playlist_add_t *)malloc(sizeof( playlist_add_t));
    p_add->i_item = i_item_id;
    p_add->i_node = i_node_id;
    val.p_address = p_add;
    var_Set( p_playlist, "item-append", val );
    free( p_add );
}

/*****************************************************************************
 * Playlist item accessors
 *****************************************************************************/

/** Set the name of a playlist item */
int playlist_ItemSetName( playlist_item_t *p_item, char *psz_name )
{
    if( psz_name && p_item )
    {
        if( p_item->p_input->psz_name ) free( p_item->p_input->psz_name );
        p_item->p_input->psz_name = strdup( psz_name );
        return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

/** Set the duration of a playlist item
 * \param i_duration the new duration in microseconds
 */
int playlist_ItemSetDuration( playlist_item_t *p_item, mtime_t i_duration )
{
    char psz_buffer[MSTRTIME_MAX_SIZE];
    if( p_item )
    {
        p_item->p_input->i_duration = i_duration;
        if( i_duration != -1 )
        {
            secstotimestr( psz_buffer, (int)(i_duration/1000000) );
        }
        else
        {
            memcpy( psz_buffer, "--:--:--", sizeof("--:--:--") );
        }
        vlc_input_item_AddInfo( p_item->p_input, _("General") , _("Duration"),
                                "%s", psz_buffer );

        return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

/** Add an option to a playlist item */
void playlist_ItemAddOption( playlist_item_t *p_item,
                             const char *psz_option)
{
    vlc_input_item_AddOption( p_item->p_input, psz_option );
}

/***************************************************************************
 * The following functions are local
 ***************************************************************************/

/* Enqueue an item for preparsing, and play it, if needed */
void GoAndPreparse( playlist_t *p_playlist, int i_mode,
                    playlist_item_t *p_item )
{
#if 0
    if( (i_mode & PLAYLIST_GO ) && p_view )
    {
        p_playlist->request.b_request = VLC_TRUE;
        /* FIXME ... */
        p_playlist->request.i_view = VIEW_CATEGORY;
        p_playlist->request.p_node = p_view->p_root;
        p_playlist->request.p_item = p_item;

        if( p_playlist->p_input )
        {
            input_StopThread( p_playlist->p_input );
        }
        p_playlist->status.i_status = PLAYLIST_RUNNING;
    }
#endif
    if( i_mode & PLAYLIST_PREPARSE &&
        var_CreateGetBool( p_playlist, "auto-preparse" ) )
    {
        playlist_PreparseEnqueue( p_playlist, p_item->p_input );
    }
}

/* Add the playlist item to the requested node and fire a notification */
void AddItem( playlist_t *p_playlist, playlist_item_t *p_item,
              playlist_item_t *p_node, int i_pos )
{
    INSERT_ELEM( p_playlist->pp_items, p_playlist->i_size,
                 p_playlist->i_size, p_item );
#if 0
    fprintf( stderr, "Adding input %s (id %i) - playlist item id %i "
                     "to node %s (id %i)\n",
                     p_item->p_input->psz_name, p_item->p_input->i_id,
                     p_item->i_id, p_node->p_input->psz_name,
                     p_node->i_id );
#endif
    INSERT_ELEM( p_playlist->pp_all_items, p_playlist->i_all_size,
                 p_playlist->i_all_size, p_item );
    p_playlist->i_enabled ++;

    if( i_pos == PLAYLIST_END )
    {
        playlist_NodeAppend( p_playlist, p_item, p_node );
    }
    else
    {
        playlist_NodeInsert( p_playlist, p_item, p_node, i_pos );
    }

    playlist_SendAddNotify( p_playlist, p_item->i_id, p_node->i_id );
}

/* Actually convert an item to a node */
void ChangeToNode( playlist_t *p_playlist, playlist_item_t *p_item )
{
    int i;
    if( p_item->i_children == -1 )
        p_item->i_children = 0;

    /* Remove it from the array of available items */
    for( i = 0 ; i < p_playlist->i_size ; i++ )
    {
        if( p_item == p_playlist->pp_items[i] )
        {
            REMOVE_ELEM( p_playlist->pp_items, p_playlist->i_size, i );
        }
    }
}

/* Do the actual removal */
int DeleteInner( playlist_t * p_playlist, playlist_item_t *p_item,
                vlc_bool_t b_stop )
{
    int i, i_top, i_bottom;
    int i_id = p_item->i_id;
    vlc_bool_t b_flag = VLC_FALSE;

    //fprintf( stderr, "Deleting item %i - %s\n", i_id,
    //                                            p_item->p_input->psz_name );

    if( p_item->i_children > -1 )
    {
        return playlist_NodeDelete( p_playlist, p_item, VLC_TRUE, VLC_FALSE );
    }
    var_SetInteger( p_playlist, "item-deleted", i_id );

    /* Remove the item from the bank */
    i_bottom = 0; i_top = p_playlist->i_all_size - 1;
    i = i_top / 2;
    while( p_playlist->pp_all_items[i]->i_id != i_id &&
           i_top > i_bottom )
    {
        if( p_playlist->pp_all_items[i]->i_id < i_id )
        {
            i_bottom = i + 1;
        }
        else
        {
            i_top = i - 1;
        }
        i = i_bottom + ( i_top - i_bottom ) / 2;
    }
    if( p_playlist->pp_all_items[i]->i_id == i_id )
    {
        REMOVE_ELEM( p_playlist->pp_all_items, p_playlist->i_all_size, i );
    }

    /* Check if it is the current item */
    if( p_playlist->status.p_item == p_item )
    {
        /* Hack we don't call playlist_Control for lock reasons */
        if( b_stop )
        {
            p_playlist->status.i_status = PLAYLIST_STOPPED;
            p_playlist->request.b_request = VLC_TRUE;
            p_playlist->request.p_item = NULL;
            msg_Info( p_playlist, "stopping playback" );
        }
        b_flag = VLC_TRUE;
    }

    msg_Dbg( p_playlist, "deleting playlist item `%s'",
                          p_item->p_input->psz_name );

    /* Remove the item from its parent */
    playlist_NodeRemoveItem( p_playlist, p_item, p_item->p_parent );

    if( b_flag == VLC_FALSE )
        playlist_ItemDelete( p_item );
    else
        p_item->i_flags |= PLAYLIST_REMOVE_FLAG;

    return VLC_SUCCESS;
}
