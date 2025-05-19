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

#ifndef VLC_QT_PLAYLIST_NEW_MODEL_HPP_
#define VLC_QT_PLAYLIST_NEW_MODEL_HPP_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "playlist_common.hpp"
#include "playlist_item.hpp"
#include "util/vlctick.hpp"

#include <QAbstractListModel>

namespace vlc {
namespace playlist {

class PlaylistListModelPrivate;
class PlaylistListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(Playlist playlist READ getPlaylist WRITE setPlaylist NOTIFY playlistChanged FINAL)
    Q_PROPERTY(int currentIndex READ getCurrentIndex NOTIFY currentIndexChanged FINAL)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)
    Q_PROPERTY(VLCDuration duration READ getDuration NOTIFY countChanged FINAL)

public:
    enum Roles
    {
        TitleRole = Qt::UserRole,
        DurationRole,
        IsCurrentRole,
        ArtistRole,
        AlbumRole,
        ArtworkRole,
        UrlRole,
        PreparsedRole
    };

    PlaylistListModel(QObject *parent = nullptr);
    PlaylistListModel(vlc_playlist_t *playlist, QObject *parent = nullptr);
    ~PlaylistListModel();

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    VLCDuration getDuration() const;
    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;

    /* provided for convenience */
    Q_INVOKABLE PlaylistItem itemAt(int index) const;

    Q_INVOKABLE virtual void removeItems(const QVector<int> &indexes);
    Q_INVOKABLE virtual void moveItemsPre(const QVector<int> &indexes, int preTarget);
    Q_INVOKABLE virtual void moveItemsPost(const QVector<int> &indexes, int postTarget);

    int getCurrentIndex() const;

    Q_INVOKABLE QVariantList getItemsForIndexes(const QVector<int> & indexes) const;

public slots:
    Playlist getPlaylist() const;
    void setPlaylist(const Playlist& playlist);
    void setPlaylist(vlc_playlist_t* playlist);

signals:
    void playlistChanged(const Playlist&);
    void currentIndexChanged( int );
    void countChanged(int);

private:
    Q_DECLARE_PRIVATE(PlaylistListModel)

    void moveItems(const QVector<int> &indexes, int target, bool isPreTarget);

    QScopedPointer<PlaylistListModelPrivate> d_ptr;

};


  } // namespace playlist
} // namespace vlc

#endif
