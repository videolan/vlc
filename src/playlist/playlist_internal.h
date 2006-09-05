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

/**
 *  \file
 *  This file contain internal structures and function prototypes related
 *  to the playlist in vlc
 *
 * \defgroup vlc_playlist Playlist
 * @{
 */

struct playlist_preparse_t
{
    VLC_COMMON_MEMBERS
    vlc_mutex_t     lock;
    int             i_waiting;
    input_item_t  **pp_waiting;
};


/*****************************************************************************
 * Prototypes
 *****************************************************************************/

/* Creation/Deletion */
playlist_t *playlist_Create   ( vlc_object_t * );
void        playlist_Destroy  ( playlist_t * );

/* Engine */
void playlist_MainLoop( playlist_t * );
void playlist_LastLoop( playlist_t * );
void playlist_PreparseLoop( playlist_preparse_t * );

/* Control */
playlist_item_t * playlist_NextItem  ( playlist_t * );
int playlist_PlayItem  ( playlist_t *, playlist_item_t * );

/* Load/Save */
int playlist_MLLoad( playlist_t *p_playlist );
int playlist_MLDump( playlist_t *p_playlist );

/**********************************************************************
 * Item management
 **********************************************************************/

void playlist_SendAddNotify( playlist_t *p_playlist, int i_item_id, int i_node_id );

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

/**
 * @}
 */

#define PLAYLIST_DEBUG 1

#ifdef PLAYLIST_DEBUG
#define PL_DEBUG( msg, args... ) msg_Dbg( p_playlist, msg, ## args )
#else
#define PL_DEBUG( msg, args ... ) {}
#endif

#define PLI_NAME( p ) p ? p->p_input->psz_name : "null"
