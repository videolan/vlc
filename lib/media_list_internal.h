/*****************************************************************************
 * media_list_internal.h : Definition of opaque structures for libvlc exported API
 * Also contains some internal utility functions
 *****************************************************************************
 * Copyright (C) 2005-2009 VLC authors and VideoLAN
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#ifndef _LIBVLC_MEDIA_LIST_INTERNAL_H
#define _LIBVLC_MEDIA_LIST_INTERNAL_H 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>
#include <vlc/libvlc_media.h>

#include <vlc_common.h>

struct libvlc_media_list_t
{
    libvlc_event_manager_t      event_manager;
    int                         i_refcount;
    vlc_mutex_t                 object_lock;
    vlc_mutex_t                 refcount_lock;
    libvlc_media_t * p_md; /* The media from which the
                                       * mlist comes, if any. */
    libvlc_media_t * p_internal_md; /* media set from media.c */
    vlc_array_t                items;

    /* This indicates if this media list is read-only
     * from a user point of view */
    bool                  b_read_only;
};

/* Media List */
void libvlc_media_list_internal_add_media(
        libvlc_media_list_t * p_mlist,
        libvlc_media_t * p_md );

void libvlc_media_list_internal_insert_media(
        libvlc_media_list_t * p_mlist,
        libvlc_media_t * p_md, int index );

int libvlc_media_list_internal_remove_index(
        libvlc_media_list_t * p_mlist, int index );

void libvlc_media_list_internal_end_reached(
        libvlc_media_list_t * p_mlist );

#endif
