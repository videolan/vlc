/*****************************************************************************
 * intf_plst.h : Playlist functions
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
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

typedef struct playlist_item_s
{
    char*             psz_name;
    int               i_type;   /* unused yet */
    int               i_status; /* unused yet */
} playlist_item_t;

typedef struct playlist_s
{
    int                   i_index;                          /* current index */
    int                   i_size;                              /* total size */

    int                   i_mode;  /* parse mode (random, forward, backward) */
    int                   i_seed;               /* seed used for random mode */

    vlc_mutex_t           change_lock;

    playlist_item_t       current;
    playlist_item_t*      p_item;
} playlist_t;

/* Used by playlist_Add */
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

playlist_t * playlist_Create   ( void );
void         playlist_Init     ( playlist_t * p_playlist );
int          playlist_Add      ( playlist_t * p_playlist,
                                 int i_pos, char * psz_item );
void         playlist_Next     ( playlist_t * p_playlist );
void         playlist_Prev     ( playlist_t * p_playlist );
void         playlist_Destroy  ( playlist_t * p_playlist );

