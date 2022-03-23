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
                    vlc_ml_media_type_t i_type;
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
            vlc_ml_thumbnail_size_t i_size;
            bool b_success;
            vlc_ml_thumbnail_status_t i_status;
            char* psz_mrl;
        } media_thumbnail_generated;
    };

    explicit MLEvent(const vlc_ml_event_t *event)
    {
        i_type = event->i_type;
        switch (event->i_type)
        {
            case VLC_ML_EVENT_MEDIA_ADDED:
                creation.i_entity_id = event->creation.p_media->i_id;
                creation.media.i_type = event->creation.p_media->i_type;
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
            {
                vlc_ml_thumbnail_size_t thumbnailSize = event->media_thumbnail_generated.i_size;
                const vlc_ml_thumbnail_t& thumbnail = event->media_thumbnail_generated.p_media->thumbnails[thumbnailSize];
                const char* mrl = thumbnail.psz_mrl;

                media_thumbnail_generated.i_media_id = event->media_thumbnail_generated.p_media->i_id;
                media_thumbnail_generated.b_success = event->media_thumbnail_generated.b_success;
                media_thumbnail_generated.i_size = thumbnailSize;
                media_thumbnail_generated.i_status = thumbnail.i_status;
                if (media_thumbnail_generated.b_success && mrl)
                    media_thumbnail_generated.psz_mrl = strdup(mrl);
                else
                    media_thumbnail_generated.psz_mrl = nullptr;
                break;
            }
        }
    }

    ~MLEvent()
    {
        switch (i_type)
        {
        case VLC_ML_EVENT_MEDIA_THUMBNAIL_GENERATED:
        {
            if (media_thumbnail_generated.psz_mrl)
                free(media_thumbnail_generated.psz_mrl);
            break;
         }
        default:
            break;
        }
    }

    //allow move
    MLEvent(MLEvent&&) = default;
    MLEvent& operator=(MLEvent&&) = default;

    //forbid copy
    MLEvent(MLEvent const&) = delete;
    MLEvent& operator=(MLEvent const&) = delete;
};
