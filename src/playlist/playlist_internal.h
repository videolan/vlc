/*****************************************************************************
 * playlist_internal.h : Functions for use by the playlist
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

#ifndef __LIBVLC_PLAYLIST_INTERNAL_H
# define __LIBVLC_PLAYLIST_INTERNAL_H 1

/**
 *  \file
 *  This file contain internal structures and function prototypes related
 *  to the playlist in vlc
 *
 * \defgroup vlc_playlist Playlist
 * @{
 */

#include "input/input_internal.h"
#include <assert.h>

struct playlist_preparse_t
{
    VLC_COMMON_MEMBERS
    vlc_mutex_t     lock;
    int             i_waiting;
    input_item_t  **pp_waiting;
};

struct playlist_fetcher_t
{
    VLC_COMMON_MEMBERS
    vlc_mutex_t     lock;
    int             i_art_policy;
    int             i_waiting;
    input_item_t    **pp_waiting;

    DECL_ARRAY(playlist_album_t) albums;
};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

/* Global thread */
#define playlist_ThreadCreate(a) __playlist_ThreadCreate(VLC_OBJECT(a))
void        __playlist_ThreadCreate   ( vlc_object_t * );

playlist_item_t *playlist_ItemNewFromInput( playlist_t *p_playlist,
                                              input_item_t *p_input );

/* Creation/Deletion */
playlist_t *playlist_Create   ( vlc_object_t * );

/* Engine */
void playlist_MainLoop( playlist_t * );
void playlist_LastLoop( playlist_t * );
void playlist_PreparseLoop( playlist_preparse_t * );
void playlist_FetcherLoop( playlist_fetcher_t * );

void ResetCurrentlyPlaying( playlist_t *, bool, playlist_item_t * );

playlist_item_t * get_current_status_item( playlist_t * p_playlist);
playlist_item_t * get_current_status_node( playlist_t * p_playlist );
void set_current_status_item( playlist_t *, playlist_item_t * );
void set_current_status_node( playlist_t *, playlist_item_t * );

/* Control */
playlist_item_t * playlist_NextItem  ( playlist_t * );
int playlist_PlayItem  ( playlist_t *, playlist_item_t * );

/* Load/Save */
int playlist_MLLoad( playlist_t *p_playlist );
int playlist_MLDump( playlist_t *p_playlist );

/**********************************************************************
 * Item management
 **********************************************************************/

void playlist_SendAddNotify( playlist_t *p_playlist, int i_item_id,
                             int i_node_id, bool b_signal );

playlist_item_t * playlist_NodeAddInput( playlist_t *, input_item_t *,
        playlist_item_t *,int , int, bool );

/* Tree walking */
playlist_item_t *playlist_ItemFindFromInputAndRoot( playlist_t *p_playlist,
                                   int i_input_id, playlist_item_t *p_root,
                                   bool );

int playlist_DeleteFromInputInParent( playlist_t *, int, playlist_item_t *, bool );
int playlist_DeleteFromItemId( playlist_t*, int );
int playlist_ItemRelease( playlist_item_t * );

void playlist_release_current_input( playlist_t * p_playlist );
void playlist_set_current_input(
    playlist_t * p_playlist, input_thread_t * p_input );

/**
 * @}
 */

#define PLAYLIST_DEBUG 1
//#undef PLAYLIST_DEBUG2

#ifdef PLAYLIST_DEBUG
 #define PL_DEBUG( msg, args... ) msg_Dbg( p_playlist, msg, ## args )
 #ifdef PLAYLIST_DEBUG2
  #define PL_DEBUG2( msg, args... ) msg_Dbg( p_playlist, msg, ## args )
 #else
  #define PL_DEBUG2( msg, args... ) {}
 #endif
#else
 #define PL_DEBUG( msg, args ... ) {}
 #define PL_DEBUG2( msg, args... ) {}
#endif

#define PLI_NAME( p ) p && p->p_input ? p->p_input->psz_name : "null"

#define PL_ASSERT_LOCKED vlc_assert_locked( &(vlc_internals(p_playlist)->lock) )

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

#endif /* !__LIBVLC_PLAYLIST_INTERNAL_H */
