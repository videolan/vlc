/*****************************************************************************
 * lru.h : Last recently used cache implementation
 *****************************************************************************
 * Copyright (C) 2020 - VideoLabs, VLC authors and VideoLAN
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifndef LRU_H
#define LRU_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vlc_lru vlc_lru;

vlc_lru * vlc_lru_New( unsigned max,
                       void(*releaseValue)(void *, void *), void * );
void vlc_lru_Release( vlc_lru *lru );

bool   vlc_lru_HasKey( vlc_lru *lru, const char *psz_key );
void * vlc_lru_Get( vlc_lru *lru, const char *psz_key );
void   vlc_lru_Insert( vlc_lru *lru, const char *psz_key, void *value );

void   vlc_lru_Apply( vlc_lru *lru,
                      void(*func)(void *, const char *, void *),
                      void * );

#ifdef __cplusplus
}
#endif

#endif
