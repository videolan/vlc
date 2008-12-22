/*****************************************************************************
 * playlist_fetcher.h:
 *****************************************************************************
 * Copyright (C) 1999-2008 the VideoLAN team
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

#ifndef _PLAYLIST_FETCHER_H
#define _PLAYLIST_FETCHER_H 1

typedef struct
{
    playlist_t      *p_playlist;

    vlc_thread_t    thread;
    vlc_mutex_t     lock;
    vlc_cond_t      wait;
    int             i_art_policy;
    int             i_waiting;
    input_item_t    **pp_waiting;

    DECL_ARRAY(playlist_album_t) albums;
} playlist_fetcher_t;

playlist_fetcher_t *playlist_fetcher_New( playlist_t * );
void playlist_fetcher_Push( playlist_fetcher_t *, input_item_t * );
void playlist_fetcher_Delete( playlist_fetcher_t * );

#endif

