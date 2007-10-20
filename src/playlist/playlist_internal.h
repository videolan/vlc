/*****************************************************************************
 * playlist_internal.h : Functions for use by the playlist
 *****************************************************************************
 * Copyright (C) 1999-2004 the VideoLAN team
 * $Id: vlc_playlist.h 16505 2006-09-03 21:53:38Z zorglub $
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

struct playlist_preparse_t
{
    VLC_COMMON_MEMBERS
    vlc_mutex_t     lock;
    int             i_waiting;
    input_item_t  **pp_waiting;
};

typedef struct preparse_item_t
{
    input_item_t *p_item;
    vlc_bool_t   b_fetch_art;
} preparse_item_t;

struct playlist_fetcher_t
{
    VLC_COMMON_MEMBERS
    vlc_mutex_t     lock;
    int             i_art_policy;
    int             i_waiting;
    preparse_item_t *p_waiting;

    DECL_ARRAY(playlist_album_t) albums;
};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

/* Global thread */
#define playlist_ThreadCreate(a) __playlist_ThreadCreate(VLC_OBJECT(a))
void        __playlist_ThreadCreate   ( vlc_object_t * );
int           playlist_ThreadDestroy  ( playlist_t * );

/* Creation/Deletion */
playlist_t *playlist_Create   ( vlc_object_t * );
void        playlist_Destroy  ( playlist_t * );

/* Engine */
void playlist_MainLoop( playlist_t * );
void playlist_LastLoop( playlist_t * );
void playlist_PreparseLoop( playlist_preparse_t * );
void playlist_FetcherLoop( playlist_fetcher_t * );

void ResetCurrentlyPlaying( playlist_t *, vlc_bool_t, playlist_item_t * );

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
                             int i_node_id, vlc_bool_t b_signal );

/* Tree walking */
int playlist_GetAllEnabledChildren( playlist_t *p_playlist,
                                    playlist_item_t *p_node,
                                    playlist_item_t ***ppp_items );
playlist_item_t *playlist_GetNextLeaf( playlist_t *p_playlist,
                                    playlist_item_t *p_root,
                                    playlist_item_t *, vlc_bool_t, vlc_bool_t );
playlist_item_t *playlist_GetPrevLeaf( playlist_t *p_playlist,
                                    playlist_item_t *p_root,
                                    playlist_item_t *, vlc_bool_t, vlc_bool_t );
playlist_item_t *playlist_GetLastLeaf( playlist_t *p_playlist,
                                    playlist_item_t *p_root );

int playlist_DeleteFromItemId( playlist_t*, int );
int playlist_ItemDelete ( playlist_item_t * );

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
#endif /* !__LIBVLC_PLAYLIST_INTERNAL_H */
