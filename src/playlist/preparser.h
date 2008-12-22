/*****************************************************************************
 * playlist_preparser.h:
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

#ifndef _PLAYLIST_PREPARSER_H
#define _PLAYLIST_PREPARSER_H 1

typedef struct
{
    playlist_t          *p_playlist;
    playlist_fetcher_t  *p_fetcher;

    vlc_thread_t    thread;
    vlc_mutex_t     lock;
    vlc_cond_t      wait;
    input_item_t  **pp_waiting;
    int             i_waiting;
} playlist_preparser_t;

playlist_preparser_t *playlist_preparser_New( playlist_t *, playlist_fetcher_t * );
void playlist_preparser_Push( playlist_preparser_t *, input_item_t * );
void playlist_preparser_Delete( playlist_preparser_t * );

#endif

