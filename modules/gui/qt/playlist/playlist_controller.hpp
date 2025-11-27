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

#include "qt.hpp"

#include <QObject>
#include <QVector>
#include <QVariantList>

#include "media.hpp"
#include "playlist_common.hpp"
#include "playlist_item.hpp"

namespace vlc {
namespace playlist {

QVector<vlc::playlist::Media> toMediaList(const QVariantList &sources);

using vlc_playlist_locker = vlc_locker<vlc_playlist_t, vlc_playlist_Lock, vlc_playlist_Unlock>;

class PlaylistControllerPrivate;
class PlaylistController : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(PlaylistController)

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
        SORT_KEY_RATING = VLC_PLAYLIST_SORT_KEY_RATING,
        SORT_KEY_FILE_SIZE = VLC_PLAYLIST_SORT_KEY_FILE_SIZE,
        SORT_KEY_FILE_MODIFIED = VLC_PLAYLIST_SORT_KEY_FILE_MODIFIED,
        SORT_KEY_NONE
    };
    Q_ENUM(SortKey)

    enum SortOrder
    {
        SORT_ORDER_ASC = VLC_PLAYLIST_SORT_ORDER_ASCENDING,
        SORT_ORDER_DESC = VLC_PLAYLIST_SORT_ORDER_DESCENDING,
    };
    Q_ENUM(SortOrder)

    enum MediaStopAction
    {
        MEDIA_STOPPED_CONTINUE = VLC_PLAYLIST_MEDIA_STOPPED_CONTINUE,
        MEDIA_STOPPED_STOP = VLC_PLAYLIST_MEDIA_STOPPED_STOP,
        MEDIA_STOPPED_EXIT = VLC_PLAYLIST_MEDIA_STOPPED_EXIT
    };
    Q_ENUM(MediaStopAction)

    Q_PROPERTY(bool initialized READ isInitialized NOTIFY initializedChanged FINAL)
    Q_PROPERTY(QVariantList sortKeyTitleList READ getSortKeyTitleList CONSTANT FINAL)

    Q_PROPERTY(Playlist playlist READ getPlaylist CONSTANT FINAL)

    Q_PROPERTY(PlaylistItem currentItem READ getCurrentItem NOTIFY currentItemChanged FINAL)

    Q_PROPERTY(bool hasNext READ hasNext NOTIFY hasNextChanged FINAL)
    Q_PROPERTY(bool hasPrev READ hasPrev NOTIFY hasPrevChanged FINAL)
    Q_PROPERTY(bool random READ isRandom WRITE setRandom NOTIFY randomChanged  FINAL)
    Q_PROPERTY(PlaybackRepeat repeatMode READ getRepeatMode WRITE setRepeatMode NOTIFY repeatModeChanged FINAL)
    Q_PROPERTY(bool empty READ isEmpty NOTIFY isEmptyChanged FINAL)
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
    Q_PROPERTY(SortKey sortKey READ getSortKey WRITE setSortKey NOTIFY sortKeyChanged FINAL)
    Q_PROPERTY(SortOrder sortOrder READ getSortOrder WRITE setSortOrder NOTIFY sortOrderChanged FINAL)
    Q_PROPERTY(MediaStopAction mediaStopAction READ getMediaStopAction WRITE setMediaStopAction NOTIFY mediaStopActionChanged FINAL)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged FINAL)


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
    Q_INVOKABLE void append(const QVariant&, bool startPlaying = false);
    Q_INVOKABLE void insert(unsigned index, const QVariantList&, bool startPlaying = false);

    void append(const QVector<Media> &, bool startPlaying = false);
    void insert(size_t index, const QVector<Media> &, bool startPlaying = false);
    void move(const QVector<PlaylistItem> &, size_t target, ssize_t indexHint);
    void remove(const QVector<PlaylistItem> &, ssize_t indexHint);

    Q_INVOKABLE void shuffle();
    void sort(const QVector<vlc_playlist_sort_criterion> &);

    Q_INVOKABLE void sort(SortKey key, SortOrder order);
    Q_INVOKABLE void sort(SortKey key);
    Q_INVOKABLE void sort(void);

    Q_INVOKABLE void explore(const PlaylistItem& pItem);

    int serialize(const QString& fileName);

public:
    PlaylistController(vlc_playlist_t *playlist, QObject *parent = nullptr);
    virtual ~PlaylistController();


public:
    SortKey getSortKey() const;
    bool hasNext() const;
    bool hasPrev() const;

    bool isRandom() const;
    MediaStopAction getMediaStopAction() const;
    PlaybackRepeat getRepeatMode() const;
    bool isEmpty() const;
    int count() const;
    int currentIndex() const;
    SortOrder getSortOrder() const;
    bool isInitialized() const;

public slots:
    PlaylistItem getCurrentItem() const;

    void setRandom( bool );
    void setMediaStopAction(MediaStopAction );
    void setRepeatMode( PlaybackRepeat mode );
    void setSortKey(SortKey sortKey);
    void setSortOrder(SortOrder sortOrder);
    void switchSortOrder();

    QVariantList getSortKeyTitleList() const;
    Playlist getPlaylist() const;
    void resetSortKey();

signals:
    void currentItemChanged( );

    void hasNextChanged( bool );
    void hasPrevChanged( bool );
    void randomChanged( bool );
    void mediaStopActionChanged( MediaStopAction );
    void repeatModeChanged( PlaybackRepeat );
    void isEmptyChanged( bool empty );
    void countChanged(int);

    void sortKeyChanged();
    void sortOrderChanged();

    void currentIndexChanged(ssize_t index);
    void itemsReset(QVector<PlaylistItem>);
    void itemsAdded(size_t index, const QVector<PlaylistItem>&);
    void itemsMoved(size_t index, size_t count, size_t target);
    void itemsRemoved(size_t index, size_t count);
    void itemsUpdated(size_t index, const QVector<PlaylistItem>&);

    void initializedChanged();

private:
    Q_DECLARE_PRIVATE(PlaylistController)
    QScopedPointer<PlaylistControllerPrivate> d_ptr;
};

} // namespace playlist
} // namespace vlc

#endif
