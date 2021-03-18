/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#pragma once

#include <vlc_media_library.h>

/**
 * Owned (and simplified) version of vlc_ml_event_t, which can be copied and
 * moved to another thread.
 */
struct MLEvent
{
    int i_type;
    union
    {
        struct
        {
            int64_t i_entity_id;
            union {
                struct {
                    vlc_ml_media_subtype_t i_subtype;
                } media;
            };
        } creation;
        struct
        {
            int64_t i_entity_id;
        } modification;
        struct
        {
            int64_t i_entity_id;
        } deletion;
        struct
        {
            bool b_idle;
        } background_idle_changed;
        struct
        {
            int64_t i_media_id;
            bool b_success;
        } media_thumbnail_generated;
    };

    explicit MLEvent(const vlc_ml_event_t *event)
    {
        i_type = event->i_type;
        switch (event->i_type)
        {
            case VLC_ML_EVENT_MEDIA_ADDED:
                creation.i_entity_id = event->creation.p_media->i_id;
                creation.media.i_subtype = event->creation.p_media->i_subtype;
                break;
            case VLC_ML_EVENT_ARTIST_ADDED:
                creation.i_entity_id = event->creation.p_artist->i_id;
                break;
            case VLC_ML_EVENT_ALBUM_ADDED:
                creation.i_entity_id = event->creation.p_album->i_id;
                break;
            case VLC_ML_EVENT_GROUP_ADDED:
                creation.i_entity_id = event->creation.p_group->i_id;
                break;
            case VLC_ML_EVENT_PLAYLIST_ADDED:
                creation.i_entity_id = event->creation.p_playlist->i_id;
                break;
            case VLC_ML_EVENT_GENRE_ADDED:
                creation.i_entity_id = event->creation.p_genre->i_id;
                break;
            case VLC_ML_EVENT_BOOKMARKS_ADDED:
                creation.i_entity_id = event->creation.p_bookmark->i_media_id;
                break;
            case VLC_ML_EVENT_MEDIA_UPDATED:
            case VLC_ML_EVENT_ARTIST_UPDATED:
            case VLC_ML_EVENT_ALBUM_UPDATED:
            case VLC_ML_EVENT_GROUP_UPDATED:
            case VLC_ML_EVENT_PLAYLIST_UPDATED:
            case VLC_ML_EVENT_GENRE_UPDATED:
            case VLC_ML_EVENT_BOOKMARKS_UPDATED:
                modification.i_entity_id = event->modification.i_entity_id;
                break;
            case VLC_ML_EVENT_MEDIA_DELETED:
            case VLC_ML_EVENT_ARTIST_DELETED:
            case VLC_ML_EVENT_ALBUM_DELETED:
            case VLC_ML_EVENT_GROUP_DELETED:
            case VLC_ML_EVENT_PLAYLIST_DELETED:
            case VLC_ML_EVENT_GENRE_DELETED:
            case VLC_ML_EVENT_BOOKMARKS_DELETED:
                deletion.i_entity_id = event->deletion.i_entity_id;
                break;
            case VLC_ML_EVENT_BACKGROUND_IDLE_CHANGED:
                background_idle_changed.b_idle = event->background_idle_changed.b_idle;
                break;
            case VLC_ML_EVENT_MEDIA_THUMBNAIL_GENERATED:
                media_thumbnail_generated.i_media_id = event->media_thumbnail_generated.p_media->i_id;
                media_thumbnail_generated.b_success = event->media_thumbnail_generated.b_success;
                break;
        }
    }
};
