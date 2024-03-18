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
#ifndef VLC_QT_PLAYLIST_NEW_ITEM_HPP_
#define VLC_QT_PLAYLIST_NEW_ITEM_HPP_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_tick.h>
#include <vlc_cxx_helpers.hpp>
#include <vlc_playlist.h>
#include <QExplicitlySharedDataPointer>
#include <QUrl>
#include <QMetaType>


//namespace vlc {
//  namespace playlist {

using SharedPlaylistItem = vlc_shared_data_ptr_type(vlc_playlist_item_t,
                                                    vlc_playlist_item_Hold,
                                                    vlc_playlist_item_Release);

/**
 * Playlist item wrapper.
 *
 * It contains both the SharedPlaylistItem and cached data saved while the playlist
 * is locked, so that the fields may be read without synchronization or race
 * conditions.
 */
class PlaylistItem
{
    Q_GADGET
public:
    Q_PROPERTY(QString title READ getTitle CONSTANT  FINAL)
    Q_PROPERTY(QString artist READ getArtist CONSTANT  FINAL)
    Q_PROPERTY(QString album READ getAlbum CONSTANT  FINAL)
    Q_PROPERTY(QUrl artwork READ getArtwork CONSTANT  FINAL)
    Q_PROPERTY(vlc_tick_t duration READ getDuration CONSTANT  FINAL)
    Q_PROPERTY(QUrl url READ getUrl CONSTANT  FINAL)

    PlaylistItem(vlc_playlist_item_t *item = nullptr);

    operator bool() const;

    vlc_playlist_item_t *raw() const {
        return d ? d->item.get() : nullptr;
    }

    input_item_t *inputItem() const {
        if (const auto item = raw())
            return vlc_playlist_item_GetMedia(item);
        else
            return nullptr;
    }

    bool isSelected() const;
    void setSelected(bool selected);

    QString getTitle() const;

    QString getArtist() const;

    QString getAlbum() const;

    QUrl getArtwork() const;

    vlc_tick_t getDuration() const;

    QUrl getUrl() const;


    void sync();

private:
    struct Data : public QSharedData {
        SharedPlaylistItem item;

        bool selected = false;

        /* cached values */
        QString title;
        QString artist;
        QString album;
        QUrl artwork;

        vlc_tick_t duration;

        QUrl url;
    };

    QExplicitlySharedDataPointer<Data> d;
};

/* PlaylistItem has the same size as a raw pointer */
static_assert(sizeof(PlaylistItem) == sizeof(void *), "invalid size of PlaylistItem");

//  } // namespace playlist
//} // namespace vlc

#endif
