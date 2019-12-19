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

#ifndef VLC_QT_PLAYLIST_NEW_HPP_
#define VLC_QT_PLAYLIST_NEW_HPP_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QObject>
#include <QVector>
#include "media.hpp"
#include "playlist_common.hpp"
#include "playlist_item.hpp"

namespace vlc {
  namespace playlist {

class PlaylistControllerModelPrivate;
class PlaylistControllerModel : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(PlaylistControllerModel)

public:
    enum PlaybackRepeat
    {
        PLAYBACK_REPEAT_NONE = VLC_PLAYLIST_PLAYBACK_REPEAT_NONE,
        PLAYBACK_REPEAT_CURRENT = VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT,
        PLAYBACK_REPEAT_ALL = VLC_PLAYLIST_PLAYBACK_REPEAT_ALL
    };
    Q_ENUM(PlaybackRepeat)

    enum SortKey
    {
        SORT_KEY_TITLE = VLC_PLAYLIST_SORT_KEY_TITLE,
        SORT_KEY_DURATION = VLC_PLAYLIST_SORT_KEY_DURATION,
        SORT_KEY_ARTIST = VLC_PLAYLIST_SORT_KEY_ARTIST,
        SORT_KEY_ALBUM = VLC_PLAYLIST_SORT_KEY_ALBUM,
        SORT_KEY_ALBUM_ARTIST = VLC_PLAYLIST_SORT_KEY_ALBUM_ARTIST,
        SORT_KEY_GENRE = VLC_PLAYLIST_SORT_KEY_GENRE,
        SORT_KEY_DATE = VLC_PLAYLIST_SORT_KEY_DATE,
        SORT_KEY_TRACK_NUMBER = VLC_PLAYLIST_SORT_KEY_TRACK_NUMBER,
        SORT_KEY_DISC_NUMBER = VLC_PLAYLIST_SORT_KEY_DISC_NUMBER,
        SORT_KEY_URL = VLC_PLAYLIST_SORT_KEY_URL,
        SORT_KEY_RATIN = VLC_PLAYLIST_SORT_KEY_RATING
    };
    Q_ENUM(SortKey)

    enum SortOrder
    {
        SORT_ORDER_ASC = VLC_PLAYLIST_SORT_ORDER_ASCENDING,
        SORT_ORDER_DESC = VLC_PLAYLIST_SORT_ORDER_DESCENDING,
    };
    Q_ENUM(SortOrder)

    Q_PROPERTY(PlaylistPtr playlistPtr READ getPlaylistPtr WRITE setPlaylistPtr NOTIFY playlistPtrChanged)

    Q_PROPERTY(PlaylistItem currentItem READ getCurrentItem NOTIFY currentItemChanged)

    Q_PROPERTY(bool hasNext READ hasNext NOTIFY hasNextChanged)
    Q_PROPERTY(bool hasPrev READ hasPrev NOTIFY hasPrevChanged)
    Q_PROPERTY(bool random READ isRandom WRITE setRandom NOTIFY randomChanged )
    Q_PROPERTY(PlaybackRepeat repeatMode READ getRepeatMode WRITE setRepeatMode NOTIFY repeatModeChanged)
    Q_PROPERTY(bool playAndExit READ isPlayAndExit WRITE setPlayAndExit NOTIFY playAndExitChanged)
    Q_PROPERTY(bool empty READ isEmpty NOTIFY isEmptyChanged)
    Q_PROPERTY(size_t count READ count NOTIFY countChanged)

public:
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void next();
    Q_INVOKABLE void prev();
    Q_INVOKABLE void prevOrReset();
    Q_INVOKABLE void togglePlayPause();

    Q_INVOKABLE void toggleRandom();
    Q_INVOKABLE void toggleRepeatMode();

    Q_INVOKABLE void clear();
    Q_INVOKABLE void goTo(uint index, bool startPlaying = false);

    Q_INVOKABLE void append(const QVariantList&, bool startPlaying = false);
    Q_INVOKABLE void insert(unsigned index, const QVariantList&, bool startPlaying = false);

    void append(const QVector<Media> &, bool startPlaying = false);
    void insert(size_t index, const QVector<Media> &, bool startPlaying = false);
    void move(const QVector<PlaylistItem> &, size_t target, ssize_t indexHint);
    void remove(const QVector<PlaylistItem> &, ssize_t indexHint);

    Q_INVOKABLE void shuffle();
    void sort(const QVector<vlc_playlist_sort_criterion> &);
    Q_INVOKABLE void sort(SortKey key, SortOrder order);

public:
    PlaylistControllerModel(QObject *parent = nullptr);
    PlaylistControllerModel(vlc_playlist_t *playlist, QObject *parent = nullptr);
    virtual ~PlaylistControllerModel();

public slots:
    PlaylistItem getCurrentItem() const;

    bool hasNext() const;
    bool hasPrev() const;

    bool isRandom() const;
    void setRandom( bool );

    PlaybackRepeat getRepeatMode() const;
    void setRepeatMode( PlaybackRepeat mode );

    bool isPlayAndExit() const;
    void setPlayAndExit(bool );

    bool isEmpty() const;
    size_t count() const;

    PlaylistPtr getPlaylistPtr() const;
    void setPlaylistPtr(PlaylistPtr id);
    void setPlaylistPtr(vlc_playlist_t* newPlaylist);

signals:
    void playlistPtrChanged( PlaylistPtr );

    void currentItemChanged( );

    void hasNextChanged( bool );
    void hasPrevChanged( bool );
    void randomChanged( bool );
    void playAndExitChanged( bool );
    void repeatModeChanged( PlaybackRepeat );
    void isEmptyChanged( bool empty );
    void countChanged(size_t );

    void currentIndexChanged(ssize_t index);
    void itemsReset(QVector<PlaylistItem>);
    void itemsAdded(size_t index, QVector<PlaylistItem>);
    void itemsMoved(size_t index, size_t count, size_t target);
    void itemsRemoved(size_t index, size_t count);
    void itemsUpdated(size_t index, QVector<PlaylistItem>);

    void playlistInitialized();

private:
    Q_DECLARE_PRIVATE(PlaylistControllerModel)
    QScopedPointer<PlaylistControllerModelPrivate> d_ptr;
};

  } // namespace playlist
} // namespace vlc

#endif
