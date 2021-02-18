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

#include "medialib.hpp"
#include "mlhelper.hpp"

#include <vlc_playlist.h>
#include <vlc_input_item.h>
#include "playlist/media.hpp"
#include "playlist/playlist_controller.hpp"
#include <QSettings>

MediaLib::MediaLib(intf_thread_t *_intf, QObject *_parent)
    : QObject( _parent )
    , m_intf( _intf )
    , m_ml( vlcMl() )
    , m_event_cb( nullptr, [this](vlc_ml_event_callback_t* cb ) {
        vlc_ml_event_unregister_callback( m_ml, cb );
      })
{
    m_event_cb.reset( vlc_ml_event_register_callback( m_ml, MediaLib::onMediaLibraryEvent,
                                                      this ) );

    /* https://xkcd.com/221/ */
    m_threadPool.setMaxThreadCount(4);
}

void MediaLib::addToPlaylist(const QString& mrl, const QStringList* options)
{
    vlc::playlist::Media media{ mrl, mrl, options };
    m_intf->p_sys->p_mainPlaylistController->append( {media}, false );
}

void MediaLib::addToPlaylist(const QUrl& mrl, const QStringList* options)
{
    vlc::playlist::Media media{ mrl.toString(QUrl::None), mrl.fileName(), options };
    m_intf->p_sys->p_mainPlaylistController->append( {media} , false );
}

// A specific item has been asked to be added to the playlist
void MediaLib::addToPlaylist(const MLItemId & itemId, const QStringList* options)
{
    //invalid item
    if (itemId.id == 0)
        return;

    if (itemId.type == VLC_ML_PARENT_UNKNOWN)
    {
        vlc::playlist::InputItemPtr item( vlc_ml_get_input_item( m_ml, itemId.id ), false );
        if (item) {
            QVector<vlc::playlist::Media> medias = { vlc::playlist::Media(item.get(), options) };
            m_intf->p_sys->p_mainPlaylistController->append(medias, false);
        }
    }
    else
    {
        vlc_ml_query_params_t query;
        memset(&query, 0, sizeof(vlc_ml_query_params_t));
        ml_unique_ptr<vlc_ml_media_list_t> media_list(vlc_ml_list_media_of( m_ml, &query, itemId.type, itemId.id));
        if (media_list == nullptr)
            return;

        auto mediaRange = ml_range_iterate<vlc_ml_media_t>( media_list );
        QVector<vlc::playlist::Media> medias;
        std::transform(mediaRange.begin(), mediaRange.end(), std::back_inserter(medias), [&](vlc_ml_media_t& m) {
            vlc::playlist::InputItemPtr item(vlc_ml_get_input_item( m_ml, m.i_id ), false);
            return vlc::playlist::Media(item.get(), options);
        });
        m_intf->p_sys->p_mainPlaylistController->append(medias, false);
    }
}

void MediaLib::addToPlaylist(const QVariantList& itemIdList, const QStringList* options)
{
    for (const QVariant& varValue: itemIdList)
    {
        if (varValue.canConvert<QUrl>())
        {
            auto mrl = varValue.value<QUrl>();
            addToPlaylist(mrl, options);
        }
        else if (varValue.canConvert<QString>())
        {
            auto mrl = varValue.value<QString>();
            addToPlaylist(mrl, options);
        }
        else if (varValue.canConvert<MLItemId>())
        {
            MLItemId itemId = varValue.value<MLItemId>();
            addToPlaylist(itemId, options);
        }
    }
}

// A specific item has been asked to be played,
// so it's added to the playlist and played
void MediaLib::addAndPlay(const MLItemId & itemId, const QStringList* options )
{
    if (itemId.id == 0)
        return;
    if (itemId.type == VLC_ML_PARENT_UNKNOWN)
    {
        vlc::playlist::InputItemPtr item(vlc_ml_get_input_item( m_ml, itemId.id ), false);
        if (item) {
            QVector<vlc::playlist::Media> medias = { vlc::playlist::Media(item.get(), options) };
            m_intf->p_sys->p_mainPlaylistController->append(medias, true);
        }
    }
    else
    {
        vlc_ml_query_params_t query;
        memset(&query, 0, sizeof(vlc_ml_query_params_t));
        ml_unique_ptr<vlc_ml_media_list_t> media_list(vlc_ml_list_media_of( m_ml, &query, itemId.type, itemId.id));
        if (media_list == nullptr)
            return;

        auto mediaRange = ml_range_iterate<vlc_ml_media_t>( media_list );
        QVector<vlc::playlist::Media> medias;
        std::transform(mediaRange.begin(), mediaRange.end(), std::back_inserter(medias), [&](vlc_ml_media_t& m) {
            vlc::playlist::InputItemPtr item(vlc_ml_get_input_item( m_ml, m.i_id ), false);
            return vlc::playlist::Media(item.get(), options);
        });
        m_intf->p_sys->p_mainPlaylistController->append(medias, true);
    }
}

void MediaLib::addAndPlay(const QString& mrl, const QStringList* options)
{
    vlc::playlist::Media media{ mrl, mrl, options };
    m_intf->p_sys->p_mainPlaylistController->append( {media}, true );
}

void MediaLib::addAndPlay(const QUrl& mrl, const QStringList* options)
{
    vlc::playlist::Media media{ mrl.toString(QUrl::None), mrl.fileName(), options };
    m_intf->p_sys->p_mainPlaylistController->append( {media}, true );
}


void MediaLib::addAndPlay(const QVariantList& itemIdList, const QStringList* options)
{
    bool b_start = true;
    for (const QVariant& varValue: itemIdList)
    {
        if (varValue.canConvert<QUrl>())
        {
            auto mrl = varValue.value<QUrl>();
            if (b_start)
                addAndPlay(mrl, options);
            else
                addToPlaylist(mrl, options);
        }
        if (varValue.canConvert<QString>())
        {
            auto mrl = varValue.value<QString>();
            if (b_start)
                addAndPlay(mrl, options);
            else
                addToPlaylist(mrl, options);
        }
        else if (varValue.canConvert<MLItemId>())
        {
            MLItemId itemId = varValue.value<MLItemId>();
            if (b_start)
                addAndPlay(itemId, options);
            else
                addToPlaylist(itemId, options);
        } else {
            continue;
        }
        b_start = false;
    }
}

void MediaLib::insertIntoPlaylist(const size_t index, const QVariantList &itemIds, const QStringList *options)
{
    QVector<vlc::playlist::Media> medias;
    for ( const auto &id : itemIds )
    {
        if (!id.canConvert<MLItemId>())
            continue;

        const MLItemId itemId = id.value<MLItemId>();
        if (itemId.id == 0)
            continue;
        if (itemId.type == VLC_ML_PARENT_UNKNOWN)
        {
            vlc::playlist::InputItemPtr item(vlc_ml_get_input_item( m_ml, itemId.id ), false);
            if (item)
                medias.push_back(vlc::playlist::Media(item.get(), options));
        }
        else
        {
            vlc_ml_query_params_t query;
            memset(&query, 0, sizeof(vlc_ml_query_params_t));
            ml_unique_ptr<vlc_ml_media_list_t> media_list(vlc_ml_list_media_of( m_ml, &query, itemId.type, itemId.id));
            if (media_list == nullptr)
                return;

            auto mediaRange = ml_range_iterate<vlc_ml_media_t>( media_list );
            std::transform(mediaRange.begin(), mediaRange.end(), std::back_inserter(medias), [&](vlc_ml_media_t& m) {
                vlc::playlist::InputItemPtr item(vlc_ml_get_input_item( m_ml, m.i_id ), false);
                return vlc::playlist::Media(item.get(), options);
            });
        }
    }
    if (!medias.isEmpty())
        m_intf->p_sys->p_mainPlaylistController->insert( index, medias );
}

void MediaLib::reload()
{
    m_threadPool.start([ml = m_ml] {
        vlc_ml_reload_folder(ml, nullptr);
    });
}

vlc_medialibrary_t* MediaLib::vlcMl()
{
    return vlc_ml_instance_get( m_intf );
}

void MediaLib::onMediaLibraryEvent( void* data, const vlc_ml_event_t* event )
{
    MediaLib* self = static_cast<MediaLib*>( data );
    switch ( event->i_type )
    {
        case VLC_ML_EVENT_PARSING_PROGRESS_UPDATED:
        {
            int percent =  event->parsing_progress.i_percent;
            QMetaObject::invokeMethod(self, [self, percent]() {
                self->m_parsingProgress = percent;
                self->emit parsingProgressChanged(percent);
            });
            break;
        }
        case VLC_ML_EVENT_DISCOVERY_STARTED:
        {
            QMetaObject::invokeMethod(self, [self]() {
                self->m_discoveryPending = true;
                self->emit discoveryPendingChanged(self->m_discoveryPending);
                self->emit discoveryStarted();
            });
            break;
        }
        case VLC_ML_EVENT_DISCOVERY_PROGRESS:
        {
            QString entryPoint{ event->discovery_progress.psz_entry_point };
            QMetaObject::invokeMethod(self, [self, entryPoint]() {
                self->m_discoveryEntryPoint = entryPoint;
                self->emit discoveryEntryPointChanged(entryPoint);
            });
            break;
        }
        case VLC_ML_EVENT_DISCOVERY_COMPLETED:
        {
            QMetaObject::invokeMethod(self, [self]() {
                self->m_discoveryPending = false;
                self->emit discoveryPendingChanged(self->m_discoveryPending);
                self->emit discoveryCompleted();
            });
            break;
        }
        case VLC_ML_EVENT_RELOAD_STARTED:
        {
            QMetaObject::invokeMethod(self, [self]() {
                self->m_discoveryPending = true;
                self->emit discoveryPendingChanged(self->m_discoveryPending);
                self->emit reloadStarted();
            });
            break;
        }
        case VLC_ML_EVENT_RELOAD_COMPLETED:
        {
            QMetaObject::invokeMethod(self, [self]() {
                self->m_discoveryPending = false;
                self->emit discoveryPendingChanged(self->m_discoveryPending);
                self->emit reloadCompleted();
            });
            break;
        }
        case VLC_ML_EVENT_BACKGROUND_IDLE_CHANGED:
        {
            bool idle = event->background_idle_changed.b_idle;
            QMetaObject::invokeMethod(self, [self, idle]() {
                self->m_idle = idle;
                self->emit idleChanged();
            });
            break;
        }
        default:
            break;
    }
}
