/*****************************************************************************
 * art.h:
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

#ifndef _PLAYLIST_ART_H
#define _PLAYLIST_ART_H 1

typedef struct
{
    char *psz_artist;
    char *psz_album;
    char *psz_arturl;
    bool b_found;

} playlist_album_t;

int playlist_FindArtInCache( input_item_t * );
int playlist_FindArtInCacheUsingItemUID( input_item_t * );

int playlist_SaveArt( vlc_object_t *, input_item_t *,
                      const void *, size_t, const char *psz_type );

#endif

