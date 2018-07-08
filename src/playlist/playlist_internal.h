/*****************************************************************************
 * playlist_internal.h : Playlist internals
 *****************************************************************************
 * Copyright (C) 1999-2008 VLC authors and VideoLAN
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

#ifndef __LIBVLC_PLAYLIST_INTERNAL_H
# define __LIBVLC_PLAYLIST_INTERNAL_H 1

/**
 * \defgroup playlist_internals VLC playlist internals
 * \ingroup playlist
 *
 * @{
 * \file
 * VLC playlist internal interface
 */

#include "input/input_interface.h"
#include <assert.h>

#include "art.h"
#include "preparser.h"

typedef struct vlc_sd_internal_t vlc_sd_internal_t;

void playlist_ServicesDiscoveryKillAll( playlist_t *p_playlist );

typedef struct playlist_private_t
{
    playlist_t           public_data;
    struct intf_thread_t *interface; /**< Linked-list of interfaces */

    void *input_tree; /**< Search tree for input item
                           to playlist item mapping */
    void *id_tree; /**< Search tree for item ID to item mapping */

    vlc_sd_internal_t   **pp_sds;
    int                   i_sds;   /**< Number of service discovery modules */
    input_thread_t *      p_input;  /**< the input thread associated
                                     * with the current item */
    input_resource_t *   p_input_resource; /**< input resources */
    vlc_renderer_item_t *p_renderer;
    struct {
        /* Current status. These fields are readonly, only the playlist
         * main loop can touch it*/
        playlist_item_t *   p_item; /**< Currently playing/active item */
        playlist_item_t *   p_node; /**< Current node to play from */
    } status;

    struct {
        /* Request. Use this to give orders to the playlist main loop  */
        playlist_item_t *   p_node;   /**< requested node to play from */
        playlist_item_t *   p_item;   /**< requested item to play in the node */

        int                 i_skip;   /**< Number of items to skip */

        bool          b_request;/**< Set to true by the requester
                                           The playlist sets it back to false
                                           when processing the request */
        bool input_dead; /**< Set when input has finished. */
    } request;

    vlc_thread_t thread; /**< engine thread */
    vlc_mutex_t lock; /**< dah big playlist global lock */
    vlc_cond_t signal; /**< wakes up the playlist engine thread */
    bool     killed; /**< playlist is shutting down */
    bool     cork_effective; /**< Corked while actively playing */

    int      i_last_playlist_id; /**< Last id to an item */
    bool     b_reset_currently_playing; /** Reset current item array */

    bool     b_tree; /**< Display as a tree */
    bool     b_preparse; /**< Preparse items */
} playlist_private_t;

#define pl_priv( pl ) container_of(pl, playlist_private_t, public_data)

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

/* Creation/Deletion */
playlist_t *playlist_Create( vlc_object_t * );
void playlist_Destroy( playlist_t * );
void playlist_Activate( playlist_t * );

/* */
playlist_item_t *playlist_ItemNewFromInput( playlist_t *p_playlist,
                                            input_item_t *p_input );

/* Engine */
playlist_item_t * get_current_status_item( playlist_t * p_playlist);
playlist_item_t * get_current_status_node( playlist_t * p_playlist );
void set_current_status_item( playlist_t *, playlist_item_t * );
void set_current_status_node( playlist_t *, playlist_item_t * );

/* Load/Save */
int playlist_MLLoad( playlist_t *p_playlist );
int playlist_MLDump( playlist_t *p_playlist );

/**********************************************************************
 * Item management
 **********************************************************************/

void playlist_SendAddNotify( playlist_t *p_playlist, playlist_item_t *item );

int playlist_InsertInputItemTree ( playlist_t *,
        playlist_item_t *, input_item_node_t *, int, bool );

/* Tree walking */
int playlist_NodeInsert(playlist_item_t*, playlist_item_t *, int);

/**
 * Flags for playlist_NodeDeleteExplicit
 * \defgroup playlist_NodeDeleteExplicit_flags
 * @{
 **/
#define PLAYLIST_DELETE_FORCE 0x01 /**< delete node even if read-only */
#define PLAYLIST_DELETE_STOP_IF_CURRENT 0x02 /**< stop playlist playback if
                                                  node is currently the one
                                                  played */
/** @} */

/**
 * Delete a node with explicit semantics
 *
 * This function acts like \ref playlist_NodeDelete with the advantage of the
 * caller being able control some of the semantics of the function.
 *
 * \ref p_playlist the playlist where the node is to be deleted
 * \ref p_node the node to delete
 * \ref flags a bitfield consisting of \ref playlist_NodeDeleteExplicit_flags
 **/
void playlist_NodeDeleteExplicit(playlist_t*, playlist_item_t*,
    int flags);

void playlist_ItemRelease( playlist_t *, playlist_item_t * );

void ResetCurrentlyPlaying( playlist_t *p_playlist, playlist_item_t *p_cur );
void ResyncCurrentIndex( playlist_t *p_playlist, playlist_item_t *p_cur );

playlist_item_t * playlist_GetNextLeaf( playlist_t *, playlist_item_t *p_root,
    playlist_item_t *p_item, bool b_ena, bool b_unplayed ) VLC_USED;

#define PLAYLIST_DEBUG 1
//#undef PLAYLIST_DEBUG2

#ifdef PLAYLIST_DEBUG
 #define PL_DEBUG( ... ) msg_Dbg( p_playlist, __VA_ARGS__ )
 #ifdef PLAYLIST_DEBUG2
  #define PL_DEBUG2( msg, ... ) msg_Dbg( p_playlist, __VA_ARGS__ )
 #else
  #define PL_DEBUG2( msg, ... ) {}
 #endif
#else
 #define PL_DEBUG( msg, ... ) {}
 #define PL_DEBUG2( msg, ... ) {}
#endif

#define PLI_NAME( p ) p && p->p_input ? p->p_input->psz_name : "null"

#define PL_LOCK_IF( cond ) pl_lock_if( p_playlist, cond )
static inline void pl_lock_if( playlist_t * p_playlist, bool cond )
{
    if( cond ) PL_LOCK; else PL_ASSERT_LOCKED;
}

#define PL_UNLOCK_IF( cond ) pl_unlock_if( p_playlist, cond )
static inline void pl_unlock_if( playlist_t * p_playlist, bool cond )
{
    if( cond ) PL_UNLOCK;
}

/** @} */
#endif /* !__LIBVLC_PLAYLIST_INTERNAL_H */
