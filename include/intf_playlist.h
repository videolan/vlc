/*****************************************************************************
 * intf_playlist.h : Playlist functions
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: intf_playlist.h,v 1.3 2001/05/15 01:01:44 stef Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

/*****************************************************************************
 * playlist_item_t: playlist item
 *****************************************************************************/
typedef struct playlist_item_s
{
    char*             psz_name;
    int               i_type;   /* unused yet */
    int               i_status; /* unused yet */
} playlist_item_t;

/*****************************************************************************
 * playlist_t: playlist structure
 *****************************************************************************
 * The structure contains information about the size and browsing mode of
 * the playlist, a change lock, a dynamic array of playlist items, and a
 * current item which is an exact copy of one of the array members.
 *****************************************************************************/
typedef struct playlist_s
{
    int                   i_index;                          /* current index */
    int                   i_size;                              /* total size */

    int                   i_mode;  /* parse mode (random, forward, backward) */
    int                   i_seed;               /* seed used for random mode */
    boolean_t             b_stopped;

    vlc_mutex_t           change_lock;

    playlist_item_t       current;
    playlist_item_t*      p_item;
} playlist_t;

/* Used by intf_PlaylistAdd */
#define PLAYLIST_START            0
#define PLAYLIST_END             -1

/* Playlist parsing mode */
#define PLAYLIST_REPEAT_CURRENT   0             /* Keep playing current item */
#define PLAYLIST_FORWARD          1              /* Parse playlist until end */
#define PLAYLIST_BACKWARD        -1                       /* Parse backwards */
#define PLAYLIST_FORWARD_LOOP     2               /* Parse playlist and loop */
#define PLAYLIST_BACKWARD_LOOP   -2              /* Parse backwards and loop */
#define PLAYLIST_RANDOM           3                          /* Shuffle play */
#define PLAYLIST_REVERSE_RANDOM  -3                  /* Reverse shuffle play */

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
playlist_t * intf_PlaylistCreate   ( void );
void         intf_PlaylistInit     ( playlist_t * p_playlist );
int          intf_PlaylistAdd      ( playlist_t * p_playlist,
                                     int i_pos, const char * psz_item );
int          intf_PlaylistDelete   ( playlist_t * p_playlist, int i_pos );
void         intf_PlaylistNext     ( playlist_t * p_playlist );
void         intf_PlaylistPrev     ( playlist_t * p_playlist );
void         intf_PlaylistDestroy  ( playlist_t * p_playlist );
void         intf_PlaylistJumpto   ( playlist_t * p_playlist , int i_pos);

