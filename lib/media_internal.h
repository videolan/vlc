/*****************************************************************************
 * media_internal.h : Definition of opaque structures for libvlc exported API
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

#ifndef _LIBVLC_MEDIA_INTERNAL_H
#define _LIBVLC_MEDIA_INTERNAL_H 1

#include <vlc/libvlc.h>
#include <vlc/libvlc_media.h>

#include <vlc_common.h>
#include <vlc_input.h>
#include <vlc_player.h>
#include <vlc_preparser.h>
#include <vlc_atomic.h>

/* internal parse state of a media */
typedef enum libvlc_media_parsed_status_t
{
    libvlc_media_parsed_status_none,
    libvlc_media_parsed_status_done,
} libvlc_media_parsed_status_t;

/* A media is a wrapper of its input item and shares its reference count:
 * libvlc_media_retain()/libvlc_media_release() forward to the input item and
 * the media is destroyed with it (input_item_t.libvlc_owner_release). */
struct libvlc_media_t
{
    input_item_t      *p_input_item;

    VLC_FORWARD_DECLARE_OBJECT(libvlc_media_list_t*) p_subitems; /* A media descriptor can have Sub items. This is the only dependency we really have on media_list */
    void *p_user_data;

    _Atomic libvlc_media_parsed_status_t parsed_status;
    vlc_preparser_req *req;
};

/* Media Descriptor */
libvlc_media_t * libvlc_media_new_from_input_item( input_item_t * );

void libvlc_media_add_subtree(libvlc_media_t *, const input_item_node_t *);

static inline enum es_format_category_e
libvlc_track_type_to_escat( libvlc_track_type_t i_type )
{
    switch( i_type )
    {
        case libvlc_track_audio:
            return AUDIO_ES;
        case libvlc_track_video:
            return VIDEO_ES;
        case libvlc_track_text:
            return SPU_ES;
        case libvlc_track_unknown:
        default:
            return UNKNOWN_ES;
    }
}

typedef struct libvlc_media_trackpriv_t
{
    libvlc_media_track_t t;
    union {
        libvlc_audio_track_t audio;
        libvlc_video_track_t video;
        libvlc_subtitle_track_t subtitle;
    };
    vlc_es_id_t *es_id;
    char *item_str_id;
    vlc_atomic_rc_t rc;
} libvlc_media_trackpriv_t;

static inline const libvlc_media_trackpriv_t *
libvlc_media_track_to_priv( const libvlc_media_track_t *track )
{
    return container_of( track, const libvlc_media_trackpriv_t, t );
}

void
libvlc_media_trackpriv_from_es( libvlc_media_trackpriv_t *trackpriv,
                                const es_format_t *es  );

libvlc_media_track_t *
libvlc_media_track_create_from_player_track( const struct vlc_player_track *track );

libvlc_media_tracklist_t *
libvlc_media_tracklist_from_item( input_item_t *item, libvlc_track_type_t type );

libvlc_media_tracklist_t *
libvlc_media_tracklist_from_player( vlc_player_t *player,
                                    libvlc_track_type_t type, bool selected );

#endif
