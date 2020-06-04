/*****************************************************************************
 * media_player_internal.h : Definition of opaque structures for libvlc exported API
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

#ifndef _LIBVLC_MEDIA_PLAYER_INTERNAL_H
#define _LIBVLC_MEDIA_PLAYER_INTERNAL_H 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>
#include <vlc/libvlc_media.h>
#include <vlc_input.h>
#include <vlc_player.h>
#include <vlc_viewpoint.h>
#include "media_internal.h"

#include "../modules/audio_filter/equalizer_presets.h"

struct libvlc_media_player_t
{
    struct vlc_object_t obj;

    int                i_refcount;

    vlc_player_t *player;
    vlc_player_listener_id *listener;
    vlc_player_aout_listener_id *aout_listener;

    struct libvlc_instance_t * p_libvlc_instance; /* Parent instance */
    libvlc_media_t * p_md; /* current media descriptor */
    libvlc_event_manager_t event_manager;
};

libvlc_track_description_t * libvlc_get_track_description(
        libvlc_media_player_t *p_mi,
        enum es_format_category_e cat );

/**
 * Internal equalizer structure.
 */
struct libvlc_equalizer_t
{
    float f_preamp;
    float f_amp[EQZ_BANDS_MAX];
};

#endif
