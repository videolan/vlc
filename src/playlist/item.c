/*****************************************************************************
 * item.c : Playlist item creation/deletion/add/removal functions
 *****************************************************************************
 * Copyright (C) 1999-2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Clément Stenac <zorglub@videolan.org>
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
#include <vlc_playlist.h>
#include <vlc_rand.h>
#include "playlist_internal.h"

static void AddItem( playlist_t *p_playlist, playlist_item_t *p_item,
                     playlist_item_t *p_node, int i_mode, int i_pos );
static void GoAndPreparse( playlist_t *p_playlist, int i_mode,
                           playlist_item_t * );
static void ChangeToNode( playlist_t *p_playlist, playlist_item_t *p_item );

static int RecursiveAddIntoParent (
                playlist_t *p_playlist, playlist_item_t *p_parent,
                input_item_node_t *p_node, int i_pos, bool b_flat,
                playlist_item_t **pp_first_leaf );
static int RecursiveInsertCopy (
                playlist_t *p_playlist, playlist_item_t *p_item,
                playlist_item_t *p_parent, int i_pos, bool b_flat );

/*****************************************************************************
 * An input item has gained subitems (Event Callback)
 *****************************************************************************/

static void input_item_add_subitem_tree ( const vlc_event_t * p_event,
                                          void * user_data )
{
    input_item_t *p_input = p_event->p_obj;
    playlist_t *p_playlist = (( playlist_item_t* ) user_data)->p_playlist;
    input_item_node_t *p_new_root = p_event->u.input_item_subitem_tree_added.p_root;

    PL_LOCK;

    playlist_item_t *p_item =
        playlist_ItemGetByInput( p_playlist, p_input );

    assert( p_item != NULL );

    bool b_current = get_current_status_item( p_playlist ) == p_item;
    bool b_autostart = var_GetBool( p_playlist, "playlist-autostart" );
    bool b_stop = p_item->i_flags & PLAYLIST_SUBITEM_STOP_FLAG;
    bool b_flat = false;

    p_item->i_flags &= ~PLAYLIST_SUBITEM_STOP_FLAG;

    /* We will have to flatten the tree out if we are in "the playlist" node and
    the user setting demands flat playlist */

    if( !pl_priv(p_playlist)->b_tree ) {
        playlist_item_t *p_up = p_item;
        while( p_up->p_parent )
        {
            if( p_up->p_parent == p_playlist->p_playing )
            {
                b_flat = true;
                break;
            }
            p_up = p_up->p_parent;
        }
    }

    int pos = 0;

    /* If we have to flatten out, then take the item's position in the parent as
    insertion point and delete the item */

    if( b_flat )
    {
        playlist_item_t *p_parent = p_item->p_parent;
        assert( p_parent != NULL );

        int i;
        for( i = 0; i < p_parent->i_children; i++ )
        {
            if( p_parent->pp_children[i] == p_item )
            {
                pos = i;
                break;
            }
        }
        assert( i < p_parent->i_children );

        playlist_DeleteItem( p_playlist, p_item, true );

        p_item = p_parent;
    }
    else
    {
        pos = p_item->i_children >= 0 ? p_item->i_children : 0;
    }

    /* At this point:
    "p_item" is the node where sub-items should be inserted,
    "pos" is the insertion position in that node */

    int last_pos = playlist_InsertInputItemTree( p_playlist,
                                                 p_item,
                                                 p_new_root,
                                                 pos,
                                                 b_flat );

    if( !b_flat ) var_SetInteger( p_playlist, "leaf-to-parent", p_item->i_id );

    //control playback only if it was the current playing item that got subitems
    if( b_current )
    {
        if( last_pos == pos || ( b_stop && !b_flat ) || !b_autostart )
        {
            /* We stop, either because no sub-item was actually created, or some
            flags/settings want us to do so at this point */
            PL_UNLOCK;
            playlist_Stop( p_playlist );
            return;
        }
        else
        {
            /* Continue to play, either random or the first new item */
            playlist_item_t *p_play_item;

            if( var_GetBool( p_playlist, "random" ) )
            {
                unsigned rand_pos =
                    ((unsigned)vlc_mrand48()) % (last_pos - pos);
                rand_pos += pos;
                p_play_item = p_item->pp_children[rand_pos];
            }
            else
            {
                p_play_item = p_item->pp_children[pos];
                /* NOTE: this is a work around the general bug:
                if node-to-be-played contains sub-nodes, then second instead
                of first leaf starts playing (only in case the leafs have just
                been instered and playlist has not yet been rebuilt.)
                */
                while( p_play_item->i_children > 0 )
                    p_play_item = p_play_item->pp_children[0];
            }

            playlist_Control( p_playlist, PLAYLIST_VIEWPLAY,
                                  pl_Locked,
                                  get_current_status_node( p_playlist ),
                                  p_play_item );
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
    vlc_event_attach( p_em, vlc_InputItemSubItemTreeAdded,
                      input_item_add_subitem_tree, p_item );
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
    vlc_event_detach( p_em, vlc_InputItemSubItemTreeAdded,
                      input_item_add_subitem_tree, p_item );
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
    PL_ASSERT_LOCKED;
    playlist_item_t *p_item = playlist_ItemFindFromInputAndRoot(
        p_playlist, p_input, p_root, false );
    if( !p_item ) return VLC_EGENERIC;
    return playlist_DeleteItem( p_playlist, p_item, b_do_stop );
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
 * Search anywhere in playlist for an an input item and delete it
 * \param p_playlist playlist object
 * \param p_input the input to delete
 * \param b_locked TRUE if the playlist is locked
 * \return VLC_SUCCESS or VLC_ENOITEM
 */
int playlist_DeleteFromInput( playlist_t *p_playlist, input_item_t *p_input,
                              bool b_locked )
{
    int i_ret;
    PL_LOCK_IF( !b_locked );
    i_ret = DeleteFromInput( p_playlist, p_input,
                             p_playlist->p_root, true );
    PL_UNLOCK_IF( !b_locked );
    return ( i_ret == VLC_SUCCESS ? VLC_SUCCESS : VLC_ENOITEM );
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
    playlist_NodeEmpty( p_playlist, p_playlist->p_playing, true );
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
    return playlist_DeleteItem( p_playlist, p_item, true );
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

    p_input = input_item_NewExt( psz_uri, psz_name,
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
    playlist_item_t *p_item;
    if( p_playlist->b_die ) return VLC_EGENERIC;
    if( !pl_priv(p_playlist)->b_doing_ml )
        PL_DEBUG( "adding item `%s' ( %s )", p_input->psz_name,
                                             p_input->psz_uri );

    PL_LOCK_IF( !b_locked );

    p_item = playlist_ItemNewFromInput( p_playlist, p_input );
    if( p_item == NULL ) return VLC_ENOMEM;
    AddItem( p_playlist, p_item,
             b_playlist ? p_playlist->p_playing :
                          p_playlist->p_media_library , i_mode, i_pos );

    GoAndPreparse( p_playlist, i_mode, p_item );

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

    GoAndPreparse( p_playlist, i_mode, p_item );

    PL_UNLOCK_IF( !b_locked );

    return p_item;
}

/**
 * Copy an item (and all its children, if any) into another node
 *
 * \param p_playlist the playlist to operate on
 * \param p_item the playlist item to copy
 * \param p_parent the parent item to copy into
 * \param i_pos the position in the parent item for the new copy;
 *              if this is PLAYLIST_END, the copy is appended after all
 *              parent's children
 * \return the position in parent item just behind the last new item inserted
 */
int playlist_NodeAddCopy (
    playlist_t *p_playlist, playlist_item_t *p_item,
    playlist_item_t *p_parent, int i_pos )
{
    PL_ASSERT_LOCKED;
    assert( p_parent != NULL && p_item != NULL );
    assert( p_parent->i_children > -1 );

    if( i_pos == PLAYLIST_END ) i_pos = p_parent->i_children;

    bool b_flat = false;

    playlist_item_t *p_up = p_parent;
    while( p_up )
    {
        if( p_up == p_playlist->p_playing )
            if( !pl_priv(p_playlist)->b_tree ) b_flat = true;
        if( p_up == p_item )
            /* TODO: We don't support copying a node into itself (yet),
            because we insert items as we copy. Instead, we should copy
            all items first and then insert. */
            return i_pos;
        p_up = p_up->p_parent;
    }

    return RecursiveInsertCopy( p_playlist, p_item, p_parent, i_pos, b_flat );
}

/**
 * Insert a tree of input items into a given playlist node
 *
 * \param p_playlist the playlist to insert into
 * \param p_parent the receiving playlist node (can be an item)
 * \param p_node the root of input item tree,
          only it's contents will be inserted
 * \param i_pos the position in the playlist where to insert. If this is
 *        PLAYLIST_END the items will be added at the end of the playlist
 *        regardless of its size
 * \param b_flat TRUE if the new tree contents should be flattened into a list
 * \return the first new leaf inserted (in playing order)
 */
int playlist_InsertInputItemTree (
    playlist_t *p_playlist, playlist_item_t *p_parent,
    input_item_node_t *p_node, int i_pos, bool b_flat )
{
    playlist_item_t *p_first_leaf = NULL;
    return RecursiveAddIntoParent ( p_playlist, p_parent, p_node, i_pos, b_flat, &p_first_leaf );
}


/*****************************************************************************
 * Playlist item misc operations
 *****************************************************************************/

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

    var_SetAddress( p_playlist, "playlist-item-append", &add );
}

/***************************************************************************
 * The following functions are local
 ***************************************************************************/

/* Enqueue an item for preparsing, and play it, if needed */
static void GoAndPreparse( playlist_t *p_playlist, int i_mode,
                           playlist_item_t *p_item )
{
    PL_ASSERT_LOCKED;
    if( (i_mode & PLAYLIST_GO ) )
    {
        pl_priv(p_playlist)->request.b_request = true;
        pl_priv(p_playlist)->request.i_skip = 0;
        pl_priv(p_playlist)->request.p_item = p_item;
        if( pl_priv(p_playlist)->p_input )
            input_Stop( pl_priv(p_playlist)->p_input, true );
        pl_priv(p_playlist)->request.i_status = PLAYLIST_RUNNING;
        vlc_cond_signal( &pl_priv(p_playlist)->signal );
    }
    /* Preparse if no artist/album info, and hasn't been preparsed allready
       and if user has some preparsing option (auto-preparse variable)
       enabled*/
    char *psz_artist = input_item_GetArtist( p_item->p_input );
    char *psz_album = input_item_GetAlbum( p_item->p_input );
    if( pl_priv(p_playlist)->b_auto_preparse &&
        input_item_IsPreparsed( p_item->p_input ) == false &&
            ( EMPTY_STR( psz_artist ) || ( EMPTY_STR( psz_album ) ) )
          )
        playlist_PreparseEnqueue( p_playlist, p_item->p_input );
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
    if( p_item->i_children != -1 ) return;

    p_item->i_children = 0;

    input_item_t *p_input = p_item->p_input;
    vlc_mutex_lock( &p_input->lock );
    p_input->i_type = ITEM_TYPE_NODE;
    vlc_mutex_unlock( &p_input->lock );

    var_SetAddress( p_playlist, "item-change", p_item->p_input );

    /* Remove it from the array of available items */
    ARRAY_BSEARCH( p_playlist->items,->i_id, int, p_item->i_id, i );
    if( i != -1 )
        ARRAY_REMOVE( p_playlist->items, i );
}

/* Do the actual removal */
int playlist_DeleteItem( playlist_t * p_playlist, playlist_item_t *p_item,
                        bool b_stop )
{
    assert( b_stop );
    return playlist_NodeDelete( p_playlist, p_item, true, false );
}

static int RecursiveAddIntoParent (
    playlist_t *p_playlist, playlist_item_t *p_parent,
    input_item_node_t *p_node, int i_pos, bool b_flat,
    playlist_item_t **pp_first_leaf )
{
    *pp_first_leaf = NULL;

    if( p_parent->i_children == -1 ) ChangeToNode( p_playlist, p_parent );

    if( i_pos == PLAYLIST_END ) i_pos = p_parent->i_children;

    for( int i = 0; i < p_node->i_children; i++ )
    {
        input_item_node_t *p_child_node = p_node->pp_children[i];

        playlist_item_t *p_new_item = NULL;
        bool b_children = p_child_node->i_children > 0;

        //Create the playlist item represented by input node, if allowed.
        if( !(b_flat && b_children) )
        {
            p_new_item = playlist_NodeAddInput( p_playlist,
                    p_child_node->p_item,
                    p_parent,
                    PLAYLIST_INSERT, i_pos,
                    pl_Locked );
            if( !p_new_item ) return i_pos;

            i_pos++;
        }
        //Recurse if any children
        if( b_children )
        {
            //Substitute p_new_item for first child leaf
            //(If flat, continue counting from current position)
            int i_last_pos = RecursiveAddIntoParent(
                    p_playlist,
                    p_new_item ? p_new_item : p_parent,
                    p_child_node,
                    ( b_flat ? i_pos : 0 ),
                    b_flat,
                    &p_new_item );
            //If flat, take position after recursion as current position
            if( b_flat ) i_pos = i_last_pos;
        }

        assert( p_new_item != NULL );
        if( i == 0 ) *pp_first_leaf = p_new_item;
    }
    return i_pos;
}

static int RecursiveInsertCopy (
    playlist_t *p_playlist, playlist_item_t *p_item,
    playlist_item_t *p_parent, int i_pos, bool b_flat )
{
    PL_ASSERT_LOCKED;
    assert( p_parent != NULL && p_item != NULL );

    if( p_item == p_parent ) return i_pos;

    input_item_t *p_input = p_item->p_input;

    if( !(p_item->i_children != -1 && b_flat) )
    {
        input_item_t *p_new_input = input_item_Copy( p_input );
        if( !p_new_input ) return i_pos;

        playlist_item_t *p_new_item = NULL;
        if( p_item->i_children == -1 )
            p_new_item = playlist_NodeAddInput( p_playlist, p_new_input,
                                   p_parent, PLAYLIST_INSERT, i_pos,
                                   pl_Locked );
        else
            p_new_item = playlist_NodeCreate( p_playlist, NULL,
                                 p_parent, i_pos, 0, p_new_input );
        vlc_gc_decref( p_new_input );
        if( !p_new_item ) return i_pos;

        i_pos++;

        if( p_new_item->i_children != -1 )
            p_parent = p_new_item;
    }

    for( int i = 0; i < p_item->i_children; i++ )
    {
        if( b_flat )
            i_pos = RecursiveInsertCopy( p_playlist, p_item->pp_children[i],
                                         p_parent, i_pos, true );
        else
            RecursiveInsertCopy( p_playlist, p_item->pp_children[i],
                                 p_parent, p_parent->i_children, false );
    }

    return i_pos;
}
