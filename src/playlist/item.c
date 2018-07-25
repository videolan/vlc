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

#include <assert.h>
#include <limits.h>
#ifdef HAVE_SEARCH_H
# include <search.h>
#endif

#include <vlc_common.h>
#include <vlc_playlist.h>
#include <vlc_rand.h>
#include "playlist_internal.h"
#include "libvlc.h"

static void playlist_Preparse( playlist_t *, playlist_item_t * );

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
    playlist_t *p_playlist = user_data;
    playlist_private_t *p_sys = pl_priv( p_playlist );
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

    bool b_redirect_request = false;

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

        playlist_NodeDeleteExplicit( p_playlist, p_item, 0 );

        /* If there is a pending request referring to the item we just deleted
         * it needs to be updated so that we do not try to play an entity that
         * is no longer part of the playlist. */

        if( p_sys->request.b_request &&
            ( p_sys->request.p_item == p_item ||
              p_sys->request.p_node == p_item ) )
        {
            b_redirect_request = true;
        }

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
    if( b_redirect_request )
    {
        /* a redirect of the pending request is required, as such we update the
         * request to refer to the item that would have been the next in line
         * (if any). */

        assert( b_flat );

        playlist_item_t* p_redirect = NULL;

        if( p_item->i_children > pos )
            p_redirect = p_item->pp_children[pos];

        p_sys->request.p_item = p_redirect;
        p_sys->request.p_node = NULL;
    }

    if( !b_flat ) var_SetInteger( p_playlist, "leaf-to-parent", p_item->i_id );

    //control playback only if it was the current playing item that got subitems
    if( b_current )
    {
        if( ( b_stop && !b_flat ) || !b_autostart )
        {
            playlist_Control( p_playlist, PLAYLIST_STOP, pl_Locked );
        }
        else if( last_pos != pos ) /* any children? */
        {
            /* Continue to play, either random or the first new item */
            playlist_item_t *p_play_item;

            if( var_GetBool( p_playlist, "random" ) )
            {
                p_play_item = NULL;
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

            playlist_ViewPlay( p_playlist, NULL, p_play_item );
        }
        else if( b_flat && p_playlist->current.i_size > 0 )
        {
            /* If the playlist is flat, empty nodes are automatically deleted;
             * meaning that moving from the current index (the index of a now
             * removed node) to the next would result in a skip of one entry
             * (as the empty node is deleted, the logical next item would be
             * the one that now resides in its place).
             *
             * Imagine [ A, B, C, D ], where B (at index 1) is currently being
             * played and deleted. C is the logically next item, but since the
             * list now looks like [ A, C, D ], advancing to index 2 would mean
             * D is played - and not C.
             *
             * By positioning the playlist-head at index 0 (A), when the
             * playlist advance to the next item, C will correctly be played.
             *
             * Note: Of course, if the deleted item is at index 0, we should
             * play whatever item is at position 0 since we cannot advance to
             * index -1 (as there cannot possibly be any item there).
             **/

            if( last_pos )
                ResetCurrentlyPlaying( p_playlist,
                    ARRAY_VAL( p_playlist->current, last_pos - 1 ) );
            else
                playlist_ViewPlay( p_playlist, NULL,
                    ARRAY_VAL( p_playlist->current, 0 ) );
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
    playlist_t *p_playlist = user_data;

    var_SetAddress( p_playlist, "item-change", p_event->p_obj );
}

static int playlist_ItemCmpId( const void *a, const void *b )
{
    const playlist_item_t *pa = a, *pb = b;

    /* ID are between 1 and INT_MAX, this cannot overflow. */
    return pa->i_id - pb->i_id;
}

static int playlist_ItemCmpInput( const void *a, const void *b )
{
    const playlist_item_t *pa = a, *pb = b;

    if( pa->p_input == pb->p_input )
        return 0;
    return (((uintptr_t)pa->p_input) > ((uintptr_t)pb->p_input))
        ? +1 : -1;
}

/*****************************************************************************
 * Playlist item creation
 *****************************************************************************/
playlist_item_t *playlist_ItemNewFromInput( playlist_t *p_playlist,
                                              input_item_t *p_input )
{
    playlist_private_t *p = pl_priv(p_playlist);
    playlist_item_t **pp, *p_item;

    p_item = malloc( sizeof( playlist_item_t ) );
    if( unlikely(p_item == NULL) )
        return NULL;

    assert( p_input );

    p_item->p_input = p_input;
    p_item->i_id = p->i_last_playlist_id;
    p_item->p_parent = NULL;
    p_item->i_children = (p_input->i_type == ITEM_TYPE_NODE) ? 0 : -1;
    p_item->pp_children = NULL;
    p_item->i_nb_played = 0;
    p_item->i_flags = 0;

    PL_ASSERT_LOCKED;

    do  /* Find an unused ID for the item */
    {
        if( unlikely(p_item->i_id == INT_MAX) )
            p_item->i_id = 0;

        p_item->i_id++;

        if( unlikely(p_item->i_id == p->i_last_playlist_id) )
            goto error; /* All IDs taken */

        pp = tsearch( p_item, &p->id_tree, playlist_ItemCmpId );
        if( unlikely(pp == NULL) )
            goto error;

        assert( (*pp)->i_id == p_item->i_id );
        assert( (*pp) == p_item || (*pp)->p_input != p_input );
    }
    while( p_item != *pp );

    pp = tsearch( p_item, &p->input_tree, playlist_ItemCmpInput );
    if( unlikely(pp == NULL) )
    {
        tdelete( p_item, &p->id_tree, playlist_ItemCmpId );
        goto error;
    }
    /* Same input item cannot be inserted twice. */
    assert( p_item == *pp );

    p->i_last_playlist_id = p_item->i_id;
    input_item_Hold( p_item->p_input );

    vlc_event_manager_t *p_em = &p_item->p_input->event_manager;

    vlc_event_attach( p_em, vlc_InputItemSubItemTreeAdded,
                      input_item_add_subitem_tree, p_playlist );
    vlc_event_attach( p_em, vlc_InputItemDurationChanged,
                      input_item_changed, p_playlist );
    vlc_event_attach( p_em, vlc_InputItemMetaChanged,
                      input_item_changed, p_playlist );
    vlc_event_attach( p_em, vlc_InputItemNameChanged,
                      input_item_changed, p_playlist );
    vlc_event_attach( p_em, vlc_InputItemInfoChanged,
                      input_item_changed, p_playlist );
    vlc_event_attach( p_em, vlc_InputItemErrorWhenReadingChanged,
                      input_item_changed, p_playlist );

    return p_item;

error:
    free( p_item );
    return NULL;
}

/***************************************************************************
 * Playlist item destruction
 ***************************************************************************/

/**
 * Release an item
 *
 * \param p_item item to delete
*/
void playlist_ItemRelease( playlist_t *p_playlist, playlist_item_t *p_item )
{
    playlist_private_t *p = pl_priv(p_playlist);

    PL_ASSERT_LOCKED;

    vlc_event_manager_t *p_em = &p_item->p_input->event_manager;

    vlc_event_detach( p_em, vlc_InputItemSubItemTreeAdded,
                      input_item_add_subitem_tree, p_playlist );
    vlc_event_detach( p_em, vlc_InputItemMetaChanged,
                      input_item_changed, p_playlist );
    vlc_event_detach( p_em, vlc_InputItemDurationChanged,
                      input_item_changed, p_playlist );
    vlc_event_detach( p_em, vlc_InputItemNameChanged,
                      input_item_changed, p_playlist );
    vlc_event_detach( p_em, vlc_InputItemInfoChanged,
                      input_item_changed, p_playlist );
    vlc_event_detach( p_em, vlc_InputItemErrorWhenReadingChanged,
                      input_item_changed, p_playlist );

    input_item_Release( p_item->p_input );

    tdelete( p_item, &p->input_tree, playlist_ItemCmpInput );
    tdelete( p_item, &p->id_tree, playlist_ItemCmpId );
    free( p_item->pp_children );
    free( p_item );
}

/**
 * Finds a playlist item by ID.
 *
 * Searches for a playlist item with the given ID.
 *
 * \note The playlist must be locked, and the result is only valid until the
 * playlist is unlocked.
 *
 * \warning If an item with the given ID is deleted, it is unlikely but
 * possible that another item will get the same ID. This can result in
 * mismatches.
 * Where holding a reference to an input item is a viable option, then
 * playlist_ItemGetByInput() should be used instead - to avoid this issue.
 *
 * @param p_playlist the playlist
 * @param id ID to look for
 * @return the matching item or NULL if not found
 */
playlist_item_t *playlist_ItemGetById( playlist_t *p_playlist , int id )
{
    playlist_private_t *p = pl_priv(p_playlist);
    playlist_item_t key, **pp;

    PL_ASSERT_LOCKED;
    key.i_id = id;
    pp = tfind( &key, &p->id_tree, playlist_ItemCmpId );
    return (pp != NULL) ? *pp : NULL;
}

/**
 * Finds a playlist item by input item.
 *
 * Searches for a playlist item for the given input item.
 *
 * \note The playlist must be locked, and the result is only valid until the
 * playlist is unlocked.
 *
 * \param p_playlist the playlist
 * \param item input item to look for
 * \return the playlist item or NULL on failure
 */
playlist_item_t *playlist_ItemGetByInput( playlist_t * p_playlist,
                                          const input_item_t *item )
{
    playlist_private_t *p = pl_priv(p_playlist);
    playlist_item_t key, **pp;

    PL_ASSERT_LOCKED;
    key.p_input = (input_item_t *)item;
    pp = tfind( &key, &p->input_tree, playlist_ItemCmpInput );
    return (pp != NULL) ? *pp : NULL;
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
    playlist_item_t *p_root = p_playlist->p_playing;

    PL_LOCK_IF( !b_locked );

    for( int i = p_root->i_children - 1; i >= 0 ;i-- )
        playlist_NodeDelete( p_playlist, p_root->pp_children[i] );

    PL_UNLOCK_IF( !b_locked );
}

/***************************************************************************
 * Playlist item addition
 ***************************************************************************/
/**
 * Playlist add
 *
 * Add an item to the playlist
 * \param p_playlist the playlist to add into
 * \param psz_uri the mrl to add to the playlist
 * \param play_now whether to start playing immediately or not
 * \return VLC_SUCCESS or a VLC error code
 */
int playlist_Add( playlist_t *p_playlist, const char *psz_uri, bool play_now )
{
    return playlist_AddExt( p_playlist, psz_uri, NULL, play_now,
                            0, NULL, 0 );
}

/**
 * Add a MRL into the playlist or the media library, duration and options given
 *
 * \param p_playlist the playlist to add into
 * \param psz_uri the mrl to add to the playlist
 * \param psz_name a text giving a name or description of this item
 * \param play_now whether to start playing immediately or not
 * \param i_options the number of options
 * \param ppsz_options an array of options
 * \param i_option_flags options flags
 * \return VLC_SUCCESS or a VLC error code
*/
int playlist_AddExt( playlist_t *p_playlist, const char * psz_uri,
                     const char *psz_name, bool play_now,
                     int i_options, const char *const *ppsz_options,
                     unsigned i_option_flags )
{
    input_item_t *p_input = input_item_New( psz_uri, psz_name );
    if( !p_input )
        return VLC_ENOMEM;
    input_item_AddOptions( p_input, i_options, ppsz_options, i_option_flags );
    int i_ret = playlist_AddInput( p_playlist, p_input, play_now );
    input_item_Release( p_input );
    return i_ret;
}

/**
 * Add an input item to the playlist node
 *
 * \param p_playlist the playlist to add into
 * \param p_input the input item to add
 * \param i_mode the mode used when adding
 * \param b_playlist TRUE for playlist, FALSE for media library
 * \return VLC_SUCCESS or VLC_ENOMEM or VLC_EGENERIC
*/
int playlist_AddInput( playlist_t* p_playlist, input_item_t *p_input,
                       bool play_now)
{
    PL_LOCK;
    playlist_item_t *item = p_playlist->p_playing;

    item = playlist_NodeAddInput( p_playlist, p_input, item, PLAYLIST_END );

    if( likely(item != NULL) && play_now )
        playlist_ViewPlay( p_playlist, NULL, item );
    PL_UNLOCK;
    return (item != NULL) ? VLC_SUCCESS : VLC_ENOMEM;
}

/**
 * Add an input item to a given node
 *
 * \param p_playlist the playlist to add into
 * \param p_input the input item to add
 * \param p_parent the parent item to add into
 * \param i_pos the position in the playlist where to add. If this is
 *        PLAYLIST_END the item will be added at the end of the playlist
 *        regardless of its size
 * \return the new playlist item
 */
playlist_item_t * playlist_NodeAddInput( playlist_t *p_playlist,
                                         input_item_t *p_input,
                                         playlist_item_t *p_parent, int i_pos )
{
    PL_ASSERT_LOCKED;

    assert( p_input );
    assert( p_parent && p_parent->i_children != -1 );

    playlist_item_t *p_item = playlist_ItemNewFromInput( p_playlist, p_input );
    if( unlikely(p_item == NULL) )
        return NULL;

    if( p_input->i_type != ITEM_TYPE_NODE )
        ARRAY_APPEND(p_playlist->items, p_item);

    playlist_NodeInsert( p_parent, p_item, i_pos );
    playlist_SendAddNotify( p_playlist, p_item );
    playlist_Preparse( p_playlist, p_item );

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
int playlist_NodeAddCopy( playlist_t *p_playlist, playlist_item_t *p_item,
    playlist_item_t *p_parent, int i_pos )
{
    PL_ASSERT_LOCKED;
    assert( p_parent != NULL && p_item != NULL );
    assert( p_parent->i_children > -1 );

    if( i_pos == PLAYLIST_END )
        i_pos = p_parent->i_children;

    bool b_flat = false;

    for( playlist_item_t* p_up = p_parent; p_up; p_up = p_up->p_parent )
    {
        if( p_up == p_playlist->p_playing && !pl_priv(p_playlist)->b_tree )
            b_flat = true;

        if( p_up == p_item )
            /* TODO: We don't support copying a node into itself (yet),
            because we insert items as we copy. Instead, we should copy
            all items first and then insert. */
            return i_pos;
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
    return RecursiveAddIntoParent( p_playlist, p_parent, p_node, i_pos, b_flat,
                                   &(playlist_item_t*){ NULL } );
}


/*****************************************************************************
 * Playlist item misc operations
 *****************************************************************************/

static int ItemIndex ( playlist_item_t *p_item )
{
    int idx;

    TAB_FIND( p_item->p_parent->i_children,
              p_item->p_parent->pp_children,
              p_item,
              idx );

    return idx;
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

    TAB_ERASE(p_detach->i_children, p_detach->pp_children, i_index);

    if( p_detach == p_node && i_index < i_newpos )
        i_newpos--;

    TAB_INSERT(p_node->i_children, p_node->pp_children, p_item, i_newpos);
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

    for( int i = 0; i < i_items; i++ )
    {
        playlist_item_t *p_item = pp_items[i];
        int i_index = ItemIndex( p_item );
        playlist_item_t *p_parent = p_item->p_parent;
        TAB_ERASE(p_parent->i_children, p_parent->pp_children, i_index);
        if ( p_parent == p_node && i_index < i_newpos ) i_newpos--;
    }
    for( int i = i_items - 1; i >= 0; i-- )
    {
        playlist_item_t *p_item = pp_items[i];
        TAB_INSERT(p_node->i_children, p_node->pp_children, p_item, i_newpos);
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
 * \param i_node_id id of the node in which the item was added
 * \return nothing
 */
void playlist_SendAddNotify( playlist_t *p_playlist, playlist_item_t *item )
{
    playlist_private_t *p_sys = pl_priv(p_playlist);
    PL_ASSERT_LOCKED;

    p_sys->b_reset_currently_playing = true;
    vlc_cond_signal( &p_sys->signal );

    var_SetAddress( p_playlist, "playlist-item-append", item );
}

/**
 * Get the duration of all items in a node.
 */
vlc_tick_t playlist_GetNodeDuration( playlist_item_t* node )
{
    vlc_tick_t duration = input_item_GetDuration( node->p_input );
    if( duration == VLC_TICK_INVALID )
        duration = 0;

    for( int i = 0; i < node->i_children; i++ )
        duration += playlist_GetNodeDuration( node->pp_children[i] );

    return duration;
}

/***************************************************************************
 * The following functions are local
 ***************************************************************************/

/* Enqueue an item for preparsing */
static void playlist_Preparse( playlist_t *p_playlist,
                               playlist_item_t *p_item )
{
    playlist_private_t *sys = pl_priv(p_playlist);
    input_item_t *input = p_item->p_input;

    PL_ASSERT_LOCKED;
    /* Preparse if no artist/album info, and hasn't been preparsed already
       and if user has some preparsing option (auto-preparse variable)
       enabled*/
    char *psz_artist = input_item_GetArtist( input );
    char *psz_album = input_item_GetAlbum( input );

    if( sys->b_preparse && !input_item_IsPreparsed( input )
     && (EMPTY_STR(psz_artist) || EMPTY_STR(psz_album)) )
        vlc_MetadataRequest( p_playlist->obj.libvlc, input, 0, -1, p_item );
    free( psz_artist );
    free( psz_album );
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
                                                p_parent, i_pos );
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

    if( p_item->i_children == -1 || !b_flat )
    {
        playlist_item_t *p_new_item = NULL;

        if( p_item->i_children == -1 )
        {
            input_item_t *p_new_input = input_item_Copy( p_input );

            if( likely(p_new_input != NULL) )
            {
                p_new_item = playlist_NodeAddInput( p_playlist, p_new_input,
                                                    p_parent, i_pos );
                input_item_Release( p_new_input );
            }
        }
        else
        {
            vlc_mutex_lock( &p_input->lock );
            p_new_item = playlist_NodeCreate( p_playlist, p_input->psz_name,
                                              p_parent, i_pos, 0 );
            vlc_mutex_unlock( &p_input->lock );
        }
        if( unlikely(p_new_item == NULL) )
            return i_pos;

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
