/*****************************************************************************
 * item.c : Playlist item creation/deletion/add/removal functions
 *****************************************************************************
 * Copyright (C) 1999-2007 the VideoLAN team
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <assert.h>
#include <vlc_playlist.h>
#include "playlist_internal.h"

static void AddItem( playlist_t *p_playlist, playlist_item_t *p_item,
                     playlist_item_t *p_node, int i_mode, int i_pos );
static void GoAndPreparse( playlist_t *p_playlist, int i_mode,
                           playlist_item_t *, playlist_item_t * );
static void ChangeToNode( playlist_t *p_playlist, playlist_item_t *p_item );
static int DeleteInner( playlist_t * p_playlist, playlist_item_t *p_item,
                        bool b_stop );

static playlist_item_t *ItemToNode( playlist_t *, playlist_item_t *, bool );

/*****************************************************************************
 * An input item has gained a subitem (Event Callback)
 *****************************************************************************/
static void input_item_subitem_added( const vlc_event_t * p_event,
                                      void * user_data )
{
    playlist_item_t *p_parent_playlist_item = user_data;
    playlist_t * p_playlist = p_parent_playlist_item->p_playlist;
    input_item_t * p_parent, * p_child;
    playlist_item_t * p_child_in_category;
    playlist_item_t * p_item_in_category;
    bool b_play;

    p_parent = p_event->p_obj;
    p_child = p_event->u.input_item_subitem_added.p_new_child;

    PL_LOCK;
    b_play = var_CreateGetBool( p_playlist, "playlist-autostart" );

    /* This part is really hakish, but this playlist system isn't simple */
    /* First check if we haven't already added the item as we are
     * listening using the onelevel and the category representent
     * (Because of the playlist design) */
    p_child_in_category = playlist_ItemFindFromInputAndRoot(
                            p_playlist, p_child,
                            p_playlist->p_root_category,
                            false /* Only non-node */ );

    if( !p_child_in_category )
    {
        /* Then, transform to a node if needed */
        p_item_in_category = playlist_ItemFindFromInputAndRoot(
                                p_playlist, p_parent,
                                p_playlist->p_root_category,
                                false /* Only non-node */ );
        if( !p_item_in_category )
        {
            /* Item may have been removed */
            PL_UNLOCK;
            return;
        }

        b_play = b_play &&
            p_item_in_category == get_current_status_item( p_playlist );

        /* If this item is already a node don't transform it */
        if( p_item_in_category->i_children == -1 )
        {
            p_item_in_category = ItemToNode( p_playlist,
                    p_item_in_category, pl_Locked );
            p_item_in_category->p_input->i_type = ITEM_TYPE_PLAYLIST;
        }

        int i_ret = playlist_BothAddInput( p_playlist, p_child,
                p_item_in_category,
                PLAYLIST_APPEND | PLAYLIST_SPREPARSE , PLAYLIST_END,
                NULL, NULL, pl_Locked );

        if( i_ret == VLC_SUCCESS && b_play )
        {
            playlist_Control( p_playlist, PLAYLIST_VIEWPLAY,
                          pl_Locked, p_item_in_category, NULL );
        }
    }

    PL_UNLOCK;

}

/*****************************************************************************
 * An input item's meta or duration has changed (Event Callback)
 *****************************************************************************/
static void input_item_changed( const vlc_event_t * p_event,
                                void * user_data )
{
    playlist_item_t *p_item = user_data;
    VLC_UNUSED( p_event );
    var_SetAddress( p_item->p_playlist, "item-change", p_item->p_input );
}

/*****************************************************************************
 * Listen to vlc_InputItemAddSubItem event
 *****************************************************************************/
static void install_input_item_observer( playlist_item_t * p_item )
{
    vlc_event_manager_t * p_em = &p_item->p_input->event_manager;
    vlc_event_attach( p_em, vlc_InputItemSubItemAdded,
                      input_item_subitem_added, p_item );
    vlc_event_attach( p_em, vlc_InputItemDurationChanged,
                      input_item_changed, p_item );
    vlc_event_attach( p_em, vlc_InputItemMetaChanged,
                      input_item_changed, p_item );
    vlc_event_attach( p_em, vlc_InputItemNameChanged,
                      input_item_changed, p_item );
    vlc_event_attach( p_em, vlc_InputItemInfoChanged,
                      input_item_changed, p_item );
    vlc_event_attach( p_em, vlc_InputItemErrorWhenReadingChanged,
                      input_item_changed, p_item );
}

static void uninstall_input_item_observer( playlist_item_t * p_item )
{
    vlc_event_manager_t * p_em = &p_item->p_input->event_manager;
    vlc_event_detach( p_em, vlc_InputItemSubItemAdded,
                      input_item_subitem_added, p_item );
    vlc_event_detach( p_em, vlc_InputItemMetaChanged,
                      input_item_changed, p_item );
    vlc_event_detach( p_em, vlc_InputItemDurationChanged,
                      input_item_changed, p_item );
    vlc_event_detach( p_em, vlc_InputItemNameChanged,
                      input_item_changed, p_item );
    vlc_event_detach( p_em, vlc_InputItemInfoChanged,
                      input_item_changed, p_item );
    vlc_event_detach( p_em, vlc_InputItemErrorWhenReadingChanged,
                      input_item_changed, p_item );
}

/*****************************************************************************
 * Playlist item creation
 *****************************************************************************/
playlist_item_t *playlist_ItemNewFromInput( playlist_t *p_playlist,
                                              input_item_t *p_input )
{
    playlist_item_t* p_item = malloc( sizeof( playlist_item_t ) );
    if( !p_item )
        return NULL;

    assert( p_input );

    p_item->p_input = p_input;
    vlc_gc_incref( p_item->p_input );

    p_item->i_id = ++pl_priv(p_playlist)->i_last_playlist_id;

    p_item->p_parent = NULL;
    p_item->i_children = -1;
    p_item->pp_children = NULL;
    p_item->i_flags = 0;
    p_item->p_playlist = p_playlist;

    install_input_item_observer( p_item );

    return p_item;
}

/***************************************************************************
 * Playlist item destruction
 ***************************************************************************/

/**
 * Release an item
 *
 * \param p_item item to delete
 * \return VLC_SUCCESS
*/
int playlist_ItemRelease( playlist_item_t *p_item )
{
    /* For the assert */
    playlist_t *p_playlist = p_item->p_playlist;
    PL_ASSERT_LOCKED;

    /* Surprise, we can't actually do more because we
     * don't do refcounting, or eauivalent.
     * Because item are not only accessed by their id
     * using playlist_item outside the PL_LOCK isn't safe.
     * Most of the modules does that.
     *
     * Who wants to add proper memory management? */
    uninstall_input_item_observer( p_item );
    ARRAY_APPEND( pl_priv(p_playlist)->items_to_delete, p_item);
    return VLC_SUCCESS;
}

/**
 * Delete input item
 *
 * Remove an input item when it appears from a root playlist item
 * \param p_playlist playlist object
 * \param p_input the input to delete
 * \param p_root root playlist item
 * \param b_do_stop must stop or not the playlist
 * \return VLC_SUCCESS or VLC_EGENERIC
*/
static int DeleteFromInput( playlist_t *p_playlist, input_item_t *p_input,
                            playlist_item_t *p_root, bool b_do_stop )
{
    int i;
    PL_ASSERT_LOCKED;
    for( i = 0 ; i< p_root->i_children ; i++ )
    {
        if( p_root->pp_children[i]->i_children == -1 &&
            p_root->pp_children[i]->p_input == p_input )
        {
            DeleteInner( p_playlist, p_root->pp_children[i], b_do_stop );
            return VLC_SUCCESS;
        }
        else if( p_root->pp_children[i]->i_children >= 0 )
        {
            int i_ret = DeleteFromInput( p_playlist, p_input,
                                         p_root->pp_children[i], b_do_stop );
            if( i_ret == VLC_SUCCESS ) return VLC_SUCCESS;
        }
    }
    return VLC_EGENERIC;
}

/**
 * Delete input item
 *
 * Remove an input item when it appears from a root playlist item
 * \param p_playlist playlist object
 * \param p_input the input to delete
 * \param p_root root playlist item
 * \param b_locked TRUE if the playlist is locked
 * \return VLC_SUCCESS or VLC_EGENERIC
 */
int playlist_DeleteFromInputInParent( playlist_t *p_playlist,
                                      input_item_t *p_item,
                                      playlist_item_t *p_root, bool b_locked )
{
    int i_ret;
    PL_LOCK_IF( !b_locked );
    i_ret = DeleteFromInput( p_playlist, p_item, p_root, true );
    PL_UNLOCK_IF( !b_locked );
    return i_ret;
}

/**
 * Delete from input
 *
 * Remove an input item from ONELEVEL and CATEGORY
 * \param p_playlist playlist object
 * \param p_input the input to delete
 * \param b_locked TRUE if the playlist is locked
 * \return VLC_SUCCESS or VLC_ENOITEM
 */
int playlist_DeleteFromInput( playlist_t *p_playlist, input_item_t *p_input,
                              bool b_locked )
{
    int i_ret1, i_ret2;
    PL_LOCK_IF( !b_locked );
    i_ret1 = DeleteFromInput( p_playlist, p_input,
                             p_playlist->p_root_category, true );
    i_ret2 = DeleteFromInput( p_playlist, p_input,
                     p_playlist->p_root_onelevel, true );
    PL_UNLOCK_IF( !b_locked );
    return ( i_ret1 == VLC_SUCCESS || i_ret2 == VLC_SUCCESS ) ?
                            VLC_SUCCESS : VLC_ENOITEM;
}

/**
 * Clear the playlist
 *
 * \param p_playlist playlist object
 * \param b_locked TRUE if the playlist is locked
 * \return nothing
 */
void playlist_Clear( playlist_t * p_playlist, bool b_locked )
{
    PL_LOCK_IF( !b_locked );
    playlist_NodeEmpty( p_playlist, p_playlist->p_local_category, true );
    playlist_NodeEmpty( p_playlist, p_playlist->p_local_onelevel, true );
    PL_UNLOCK_IF( !b_locked );
}

/**
 * Delete playlist item
 *
 * Remove a playlist item from the playlist, given its id
 * This function is to be used only by the playlist
 * \param p_playlist playlist object
 * \param i_id id of the item do delete
 * \return VLC_SUCCESS or an error
 */
int playlist_DeleteFromItemId( playlist_t *p_playlist, int i_id )
{
    PL_ASSERT_LOCKED;
    playlist_item_t *p_item = playlist_ItemGetById( p_playlist, i_id );
    if( !p_item ) return VLC_EGENERIC;
    return DeleteInner( p_playlist, p_item, true );
}

/***************************************************************************
 * Playlist item addition
 ***************************************************************************/
/**
 * Playlist add
 *
 * Add an item to the playlist or the media library
 * \param p_playlist the playlist to add into
 * \param psz_uri the mrl to add to the playlist
 * \param psz_name a text giving a name or description of this item
 * \param i_mode the mode used when adding
 * \param i_pos the position in the playlist where to add. If this is
 *        PLAYLIST_END the item will be added at the end of the playlist
 *        regardless of its size
 * \param b_playlist TRUE for playlist, FALSE for media library
 * \param b_locked TRUE if the playlist is locked
 * \return VLC_SUCCESS or a VLC error code
 */
int playlist_Add( playlist_t *p_playlist, const char *psz_uri,
                  const char *psz_name, int i_mode, int i_pos,
                  bool b_playlist, bool b_locked )
{
    return playlist_AddExt( p_playlist, psz_uri, psz_name,
                            i_mode, i_pos, -1, 0, NULL, 0, b_playlist, b_locked );
}

/**
 * Add a MRL into the playlist or the media library, duration and options given
 *
 * \param p_playlist the playlist to add into
 * \param psz_uri the mrl to add to the playlist
 * \param psz_name a text giving a name or description of this item
 * \param i_mode the mode used when adding
 * \param i_pos the position in the playlist where to add. If this is
 *        PLAYLIST_END the item will be added at the end of the playlist
 *        regardless of its size
 * \param i_duration length of the item in milliseconds.
 * \param i_options the number of options
 * \param ppsz_options an array of options
 * \param i_option_flags options flags
 * \param b_playlist TRUE for playlist, FALSE for media library
 * \param b_locked TRUE if the playlist is locked
 * \return VLC_SUCCESS or a VLC error code
*/
int playlist_AddExt( playlist_t *p_playlist, const char * psz_uri,
                     const char *psz_name, int i_mode, int i_pos,
                     mtime_t i_duration,
                     int i_options, const char *const *ppsz_options,
                     unsigned i_option_flags,
                     bool b_playlist, bool b_locked )
{
    int i_ret;
    input_item_t *p_input;

    p_input = input_item_NewExt( p_playlist, psz_uri, psz_name,
                                 i_options, ppsz_options, i_option_flags,
                                 i_duration );
    if( p_input == NULL )
        return VLC_ENOMEM;
    i_ret = playlist_AddInput( p_playlist, p_input, i_mode, i_pos, b_playlist,
                               b_locked );
    vlc_gc_decref( p_input );
    return i_ret;
}

/**
 * Add an input item to the playlist node
 *
 * \param p_playlist the playlist to add into
 * \param p_input the input item to add
 * \param i_mode the mode used when adding
 * \param i_pos the position in the playlist where to add. If this is
 *        PLAYLIST_END the item will be added at the end of the playlist
 *        regardless of its size
 * \param b_playlist TRUE for playlist, FALSE for media library
 * \param b_locked TRUE if the playlist is locked
 * \return VLC_SUCCESS or VLC_ENOMEM or VLC_EGENERIC
*/
int playlist_AddInput( playlist_t* p_playlist, input_item_t *p_input,
                       int i_mode, int i_pos, bool b_playlist,
                       bool b_locked )
{
    playlist_item_t *p_item_cat, *p_item_one;
    if( p_playlist->b_die ) return VLC_EGENERIC;
    if( !pl_priv(p_playlist)->b_doing_ml )
        PL_DEBUG( "adding item `%s' ( %s )", p_input->psz_name,
                                             p_input->psz_uri );

    PL_LOCK_IF( !b_locked );

    /* Add to ONELEVEL */
    p_item_one = playlist_ItemNewFromInput( p_playlist, p_input );
    if( p_item_one == NULL ) return VLC_ENOMEM;
    AddItem( p_playlist, p_item_one,
             b_playlist ? p_playlist->p_local_onelevel :
                          p_playlist->p_ml_onelevel , i_mode, i_pos );

    /* Add to CATEGORY */
    p_item_cat = playlist_ItemNewFromInput( p_playlist, p_input );
    if( p_item_cat == NULL ) return VLC_ENOMEM;
    AddItem( p_playlist, p_item_cat,
             b_playlist ? p_playlist->p_local_category :
                          p_playlist->p_ml_category , i_mode, i_pos );

    GoAndPreparse( p_playlist, i_mode, p_item_cat, p_item_one );

    PL_UNLOCK_IF( !b_locked );
    return VLC_SUCCESS;
}

/**
 * Add input
 *
 * Add an input item to p_direct_parent in the category tree, and to the
 * matching top category in onelevel
 * \param p_playlist the playlist to add into
 * \param p_input the input item to add
 * \param p_direct_parent the parent item to add into
 * \param i_mode the mode used when adding
 * \param i_pos the position in the playlist where to add. If this is
 *        PLAYLIST_END the item will be added at the end of the playlist
 *        regardless of its size
 * \param i_cat id of the items category
 * \param i_one id of the item onelevel category
 * \param b_locked TRUE if the playlist is locked
 * \return VLC_SUCCESS if success, VLC_EGENERIC if fail, VLC_ENOMEM if OOM
 */
int playlist_BothAddInput( playlist_t *p_playlist,
                           input_item_t *p_input,
                           playlist_item_t *p_direct_parent,
                           int i_mode, int i_pos,
                           int *i_cat, int *i_one, bool b_locked )
{
    playlist_item_t *p_item_cat, *p_item_one, *p_up;
    int i_top;
    assert( p_input );

    if( !vlc_object_alive( p_playlist ) )
        return VLC_EGENERIC;

    PL_LOCK_IF( !b_locked );

    /* Add to category */
    p_item_cat = playlist_ItemNewFromInput( p_playlist, p_input );
    if( p_item_cat == NULL ) return VLC_ENOMEM;
    AddItem( p_playlist, p_item_cat, p_direct_parent, i_mode, i_pos );

    /* Add to onelevel */
    /** \todo make a faster case for ml import */
    p_item_one = playlist_ItemNewFromInput( p_playlist, p_input );
    if( p_item_one == NULL ) return VLC_ENOMEM;

    p_up = p_direct_parent;
    while( p_up->p_parent != p_playlist->p_root_category )
    {
        p_up = p_up->p_parent;
    }
    for( i_top = 0 ; i_top < p_playlist->p_root_onelevel->i_children; i_top++ )
    {
        if( p_playlist->p_root_onelevel->pp_children[i_top]->p_input ==
                             p_up->p_input )
        {
            AddItem( p_playlist, p_item_one,
                     p_playlist->p_root_onelevel->pp_children[i_top],
                     i_mode, i_pos );
            break;
        }
    }
    GoAndPreparse( p_playlist, i_mode, p_item_cat, p_item_one );

    if( i_cat ) *i_cat = p_item_cat->i_id;
    if( i_one ) *i_one = p_item_one->i_id;

    PL_UNLOCK_IF( !b_locked );
    return VLC_SUCCESS;
}

/**
 * Add an input item to a given node
 *
 * \param p_playlist the playlist to add into
 * \param p_input the input item to add
 * \param p_parent the parent item to add into
 * \param i_mode the mode used when addin
 * \param i_pos the position in the playlist where to add. If this is
 *        PLAYLIST_END the item will be added at the end of the playlist
 *        regardless of its size
 * \param b_locked TRUE if the playlist is locked
 * \return the new playlist item
 */
playlist_item_t * playlist_NodeAddInput( playlist_t *p_playlist,
                                         input_item_t *p_input,
                                         playlist_item_t *p_parent,
                                         int i_mode, int i_pos,
                                         bool b_locked )
{
    playlist_item_t *p_item;
    assert( p_input );
    assert( p_parent && p_parent->i_children != -1 );

    if( p_playlist->b_die )
        return NULL;
    PL_LOCK_IF( !b_locked );

    p_item = playlist_ItemNewFromInput( p_playlist, p_input );
    if( p_item == NULL ) return NULL;
    AddItem( p_playlist, p_item, p_parent, i_mode, i_pos );

    PL_UNLOCK_IF( !b_locked );

    return p_item;
}

/*****************************************************************************
 * Playlist item misc operations
 *****************************************************************************/

/**
 * Item to node
 *
 * Transform an item to a node. Return the node in the category tree, or NULL
 * if not found there
 * This function must be entered without the playlist lock
 * \param p_playlist the playlist object
 * \param p_item the item to transform
 * \param b_locked TRUE if the playlist is locked
 * \return the item transform in a node
 */
static playlist_item_t *ItemToNode( playlist_t *p_playlist,
                                    playlist_item_t *p_item,
                                    bool b_locked )
{

    playlist_item_t *p_item_in_category;
    /* What we do
     * Find the input in CATEGORY.
     *  - If we find it
     *    - change it to node
     *    - we'll return it at the end
     *    - If we are a direct child of onelevel root, change to node, else
     *      delete the input from ONELEVEL
     *  - If we don't find it, just change to node (we are probably in VLM)
     *    and return NULL
     *
     * If we were in ONELEVEL, we thus retrieve the node in CATEGORY (will be
     * useful for later BothAddInput )
     */

    PL_LOCK_IF( !b_locked );

    /* Fast track the media library, no time to loose */
    if( p_item == p_playlist->p_ml_category ) {
        PL_UNLOCK_IF( !b_locked );
        return p_item;
    }

    /** \todo First look if we don't already have it */
    p_item_in_category = playlist_ItemFindFromInputAndRoot(
                                            p_playlist, p_item->p_input,
                                            p_playlist->p_root_category,
                                            true );

    if( p_item_in_category )
    {
        playlist_item_t *p_item_in_one = playlist_ItemFindFromInputAndRoot(
                                            p_playlist, p_item->p_input,
                                            p_playlist->p_root_onelevel,
                                            true );
        assert( p_item_in_one );

        /* We already have it, and there is nothing more to do */
        ChangeToNode( p_playlist, p_item_in_category );

        /* Item in one is a root, change it to node */
        if( p_item_in_one->p_parent == p_playlist->p_root_onelevel )
            ChangeToNode( p_playlist, p_item_in_one );
        else
        {
            playlist_item_t *p_status_item = get_current_status_item( p_playlist );
            playlist_item_t *p_status_node = get_current_status_node( p_playlist );
            if( p_item_in_one == p_status_item )
            {
                /* We're deleting the current playlist item. Update
                 * the playlist object to point at the previous item
                 * so the playlist won't be restarted */
                playlist_item_t *p_prev_status_item = NULL;
                int i = 0;
                while( i < p_status_node->i_children &&
                       p_status_node->pp_children[i] != p_status_item )
                {
                    p_prev_status_item = p_status_node->pp_children[i];
                    i++;
                }
                if( i == p_status_node->i_children )
                    p_prev_status_item = NULL;
                if( p_prev_status_item )
                    set_current_status_item( p_playlist, p_prev_status_item );
            }
            DeleteFromInput( p_playlist, p_item_in_one->p_input,
                             p_playlist->p_root_onelevel, false );
        }
        pl_priv(p_playlist)->b_reset_currently_playing = true;
        vlc_cond_signal( &pl_priv(p_playlist)->signal );
        var_SetAddress( p_playlist, "item-change", p_item_in_category->p_input );
        var_SetAddress( p_playlist, "leaf-to-parent", p_item_in_category->p_input );
        PL_UNLOCK_IF( !b_locked );
        return p_item_in_category;
    }
    else
    {
        ChangeToNode( p_playlist, p_item );
        PL_UNLOCK_IF( !b_locked );
        return p_item;
    }
}

/**
 * Find an item within a root, given its input id.
 *
 * \param p_playlist the playlist object
 * \param p_item the input item
 * \param p_root root playlist item
 * \param b_items_only TRUE if we want the item himself
 * \return the first found item, or NULL if not found
 */
playlist_item_t *playlist_ItemFindFromInputAndRoot( playlist_t *p_playlist,
                                                    input_item_t *p_item,
                                                    playlist_item_t *p_root,
                                                    bool b_items_only )
{
    int i;
    for( i = 0 ; i< p_root->i_children ; i++ )
    {
        if( ( b_items_only ? p_root->pp_children[i]->i_children == -1 : 1 ) &&
            p_root->pp_children[i]->p_input == p_item )
        {
            return p_root->pp_children[i];
        }
        else if( p_root->pp_children[i]->i_children >= 0 )
        {
            playlist_item_t *p_search =
                 playlist_ItemFindFromInputAndRoot( p_playlist, p_item,
                                                    p_root->pp_children[i],
                                                    b_items_only );
            if( p_search ) return p_search;
        }
    }
    return NULL;
}


static int ItemIndex ( playlist_item_t *p_item )
{
    for( int i = 0; i < p_item->p_parent->i_children; i++ )
        if( p_item->p_parent->pp_children[i] == p_item ) return i;
    return -1;
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
    PL_ASSERT_LOCKED;

    if( p_node->i_children == -1 ) return VLC_EGENERIC;

    playlist_item_t *p_detach = p_item->p_parent;
    int i_index = ItemIndex( p_item );

    REMOVE_ELEM( p_detach->pp_children, p_detach->i_children, i_index );

    if( p_detach == p_node && i_index < i_newpos )
        i_newpos--;

    INSERT_ELEM( p_node->pp_children, p_node->i_children, i_newpos, p_item );
    p_item->p_parent = p_node;

    pl_priv( p_playlist )->b_reset_currently_playing = true;
    vlc_cond_signal( &pl_priv( p_playlist )->signal );
    return VLC_SUCCESS;
}

/**
 * Moves an array of items
 *
 * This function must be entered with the playlist lock
 *
 * \param p_playlist the playlist
 * \param i_items the number of indexes to move
 * \param pp_items the array of indexes to move
 * \param p_node the target node
 * \param i_newpos the target position under this node
 * \return VLC_SUCCESS or an error
 */
int playlist_TreeMoveMany( playlist_t *p_playlist,
                            int i_items, playlist_item_t **pp_items,
                            playlist_item_t *p_node, int i_newpos )
{
    PL_ASSERT_LOCKED;

    if ( p_node->i_children == -1 ) return VLC_EGENERIC;

    int i;
    for( i = 0; i < i_items; i++ )
    {
        playlist_item_t *p_item = pp_items[i];
        int i_index = ItemIndex( p_item );
        playlist_item_t *p_parent = p_item->p_parent;
        REMOVE_ELEM( p_parent->pp_children, p_parent->i_children, i_index );
        if ( p_parent == p_node && i_index < i_newpos ) i_newpos--;
    }
    for( i = i_items - 1; i >= 0; i-- )
    {
        playlist_item_t *p_item = pp_items[i];
        INSERT_ELEM( p_node->pp_children, p_node->i_children, i_newpos, p_item );
        p_item->p_parent = p_node;
    }

    pl_priv( p_playlist )->b_reset_currently_playing = true;
    vlc_cond_signal( &pl_priv( p_playlist )->signal );
    return VLC_SUCCESS;
}

/**
 * Send a notification that an item has been added to a node
 *
 * \param p_playlist the playlist object
 * \param i_item_id id of the item added
 * \param i_node_id id of the node in wich the item was added
 * \param b_signal TRUE if the function must send a signal
 * \return nothing
 */
void playlist_SendAddNotify( playlist_t *p_playlist, int i_item_id,
                             int i_node_id, bool b_signal )
{
    playlist_private_t *p_sys = pl_priv(p_playlist);
    PL_ASSERT_LOCKED;

    p_sys->b_reset_currently_playing = true;
    if( b_signal )
        vlc_cond_signal( &p_sys->signal );

    playlist_add_t add;
    add.i_item = i_item_id;
    add.i_node = i_node_id;

    vlc_value_t val;
    val.p_address = &add;

    var_Set( p_playlist, "playlist-item-append", val );
}

/***************************************************************************
 * The following functions are local
 ***************************************************************************/

/* Enqueue an item for preparsing, and play it, if needed */
static void GoAndPreparse( playlist_t *p_playlist, int i_mode,
                           playlist_item_t *p_item_cat,
                           playlist_item_t *p_item_one )
{
    PL_ASSERT_LOCKED;
    if( (i_mode & PLAYLIST_GO ) )
    {
        playlist_item_t *p_parent = p_item_one;
        playlist_item_t *p_toplay = NULL;
        while( p_parent )
        {
            if( p_parent == p_playlist->p_root_category )
            {
                p_toplay = p_item_cat; break;
            }
            else if( p_parent == p_playlist->p_root_onelevel )
            {
                p_toplay = p_item_one; break;
            }
            p_parent = p_parent->p_parent;
        }
        assert( p_toplay );
        pl_priv(p_playlist)->request.b_request = true;
        pl_priv(p_playlist)->request.i_skip = 0;
        pl_priv(p_playlist)->request.p_item = p_toplay;
        if( pl_priv(p_playlist)->p_input )
            input_Stop( pl_priv(p_playlist)->p_input, true );
        pl_priv(p_playlist)->request.i_status = PLAYLIST_RUNNING;
        vlc_cond_signal( &pl_priv(p_playlist)->signal );
    }
    /* Preparse if PREPARSE or SPREPARSE & not enough meta */
    char *psz_artist = input_item_GetArtist( p_item_cat->p_input );
    char *psz_album = input_item_GetAlbum( p_item_cat->p_input );
    if( pl_priv(p_playlist)->b_auto_preparse &&
          (i_mode & PLAYLIST_PREPARSE ||
          ( i_mode & PLAYLIST_SPREPARSE &&
            ( EMPTY_STR( psz_artist ) || ( EMPTY_STR( psz_album ) ) )
          ) ) )
        playlist_PreparseEnqueue( p_playlist, p_item_cat->p_input, pl_Locked );
    /* If we already have it, signal it */
    else if( !EMPTY_STR( psz_artist ) && !EMPTY_STR( psz_album ) )
        input_item_SetPreparsed( p_item_cat->p_input, true );
    free( psz_artist );
    free( psz_album );
}

/* Add the playlist item to the requested node and fire a notification */
static void AddItem( playlist_t *p_playlist, playlist_item_t *p_item,
                     playlist_item_t *p_node, int i_mode, int i_pos )
{
    PL_ASSERT_LOCKED;
    ARRAY_APPEND(p_playlist->items, p_item);
    ARRAY_APPEND(p_playlist->all_items, p_item);

    if( i_pos == PLAYLIST_END )
        playlist_NodeAppend( p_playlist, p_item, p_node );
    else
        playlist_NodeInsert( p_playlist, p_item, p_node, i_pos );

    if( !pl_priv(p_playlist)->b_doing_ml )
        playlist_SendAddNotify( p_playlist, p_item->i_id, p_node->i_id,
                                 !( i_mode & PLAYLIST_NO_REBUILD ) );
}

/* Actually convert an item to a node */
static void ChangeToNode( playlist_t *p_playlist, playlist_item_t *p_item )
{
    int i;
    if( p_item->i_children == -1 )
        p_item->i_children = 0;

    /* Remove it from the array of available items */
    ARRAY_BSEARCH( p_playlist->items,->i_id, int, p_item->i_id, i );
    if( i != -1 )
        ARRAY_REMOVE( p_playlist->items, i );
}

/* Do the actual removal */
static int DeleteInner( playlist_t * p_playlist, playlist_item_t *p_item,
                        bool b_stop )
{
    int i;
    int i_id = p_item->i_id;
    PL_ASSERT_LOCKED;

    if( p_item->i_children > -1 )
    {
        return playlist_NodeDelete( p_playlist, p_item, true, false );
    }
    pl_priv(p_playlist)->b_reset_currently_playing = true;
    var_SetInteger( p_playlist, "playlist-item-deleted", i_id );

    /* Remove the item from the bank */
    ARRAY_BSEARCH( p_playlist->all_items,->i_id, int, i_id, i );
    if( i != -1 )
        ARRAY_REMOVE( p_playlist->all_items, i );

    ARRAY_BSEARCH( p_playlist->items,->i_id, int, i_id, i );
    if( i != -1 )
        ARRAY_REMOVE( p_playlist->items, i );

    /* Check if it is the current item */
    if( get_current_status_item( p_playlist ) == p_item )
    {
        /* Stop it if we have to */
        if( b_stop )
        {
            playlist_Control( p_playlist, PLAYLIST_STOP, pl_Locked );
            msg_Info( p_playlist, "stopping playback" );
        }
        /* In any case, this item can't be the next one to be played ! */
        set_current_status_item( p_playlist, NULL );
    }

    ARRAY_BSEARCH( p_playlist->current,->i_id, int, i_id, i );
    if( i != -1 )
        ARRAY_REMOVE( p_playlist->current, i );

    PL_DEBUG( "deleting item `%s'", p_item->p_input->psz_name );

    /* Remove the item from its parent */
    playlist_NodeRemoveItem( p_playlist, p_item, p_item->p_parent );

    playlist_ItemRelease( p_item );

    return VLC_SUCCESS;
}
