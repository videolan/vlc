/*****************************************************************************
 * media_player_internal.h : Definition of opaque structures for libvlc exported API
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

#ifndef _LIBVLC_MEDIA_PLAYER_INTERNAL_H
#define _LIBVLC_MEDIA_PLAYER_INTERNAL_H 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>
#include <vlc/libvlc_structures.h>
#include <vlc/libvlc_media.h>
#include <vlc_input.h>

struct libvlc_media_player_t
{
    VLC_COMMON_MEMBERS

    int                i_refcount;
    vlc_mutex_t        object_lock;

    struct
    {
        input_thread_t   *p_thread;
        input_resource_t *p_resource;
        vlc_mutex_t       lock;
    } input;

    struct libvlc_instance_t * p_libvlc_instance; /* Parent instance */
    libvlc_media_t * p_md; /* current media descriptor */
    libvlc_event_manager_t * p_event_manager;
    libvlc_state_t state;
};

/* Media player - audio, video */
input_thread_t *libvlc_get_input_thread(libvlc_media_player_t * );


libvlc_track_description_t * libvlc_get_track_description(
        libvlc_media_player_t *p_mi,
        const char *psz_variable );

#endif
