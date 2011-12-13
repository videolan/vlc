/*****************************************************************************
 * media_internal.h : Definition of opaque structures for libvlc exported API
 * Also contains some internal utility functions
 *****************************************************************************
 * Copyright (C) 2005-2009 VLC authors and VideoLAN
 * $Id$
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

#ifndef _LIBVLC_MEDIA_INTERNAL_H
#define _LIBVLC_MEDIA_INTERNAL_H 1

#include <vlc/libvlc.h>
#include <vlc/libvlc_media.h>

#include <vlc_common.h>
#include <vlc_input.h>

struct libvlc_media_t
{
    libvlc_event_manager_t * p_event_manager;
    input_item_t      *p_input_item;
    int                i_refcount;
    libvlc_instance_t *p_libvlc_instance;
    libvlc_state_t     state;
    VLC_FORWARD_DECLARE_OBJECT(libvlc_media_list_t*) p_subitems; /* A media descriptor can have Sub items. This is the only dependancy we really have on media_list */
    void *p_user_data;

    vlc_cond_t parsed_cond;
    vlc_mutex_t parsed_lock;

    bool is_parsed;
    bool has_asked_preparse;
};

/* Media Descriptor */
libvlc_media_t * libvlc_media_new_from_input_item(
        libvlc_instance_t *, input_item_t * );

void libvlc_media_set_state( libvlc_media_t *, libvlc_state_t );

#endif
