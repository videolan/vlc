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

#include <QAbstractListModel>
#include <QVector>
#include "playlist_common.hpp"
#include "playlist_item.hpp"
#include "util/selectable_list_model.hpp"
#include "util/vlctick.hpp"

namespace vlc {
namespace playlist {

class PlaylistListModelPrivate;
class PlaylistListModel : public SelectableListModel
{
    Q_OBJECT
    Q_PROPERTY(PlaylistPtr playlistId READ getPlaylistId WRITE setPlaylistId NOTIFY playlistIdChanged)
    Q_PROPERTY(int currentIndex READ getCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(VLCTick duration READ getDuration NOTIFY countChanged)

public:
    enum Roles
    {
        TitleRole = Qt::UserRole,
        DurationRole,
        IsCurrentRole,
        ArtistRole,
        AlbumRole,
        ArtworkRole,
        SelectedRole,
    };

    PlaylistListModel(QObject *parent = nullptr);
    PlaylistListModel(vlc_playlist_t *playlist, QObject *parent = nullptr);
    ~PlaylistListModel();

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    VLCTick getDuration() const;
    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;

    /* provided for convenience */
    const PlaylistItem &itemAt(int index) const;

    Q_INVOKABLE virtual void removeItems(const QList<int> &indexes);
    Q_INVOKABLE virtual void moveItemsPre(const QList<int> &indexes, int preTarget);
    Q_INVOKABLE virtual void moveItemsPost(const QList<int> &indexes, int postTarget);

    int getCurrentIndex() const;

protected:
    bool isRowSelected(int row) const override;
    void setRowSelected(int row, bool selected) override;
    int getSelectedRole() const override;

public slots:
    PlaylistPtr getPlaylistId() const;
    void setPlaylistId(PlaylistPtr id);
    void setPlaylistId(vlc_playlist_t* playlist);

signals:
    void playlistIdChanged(const PlaylistPtr& );
    void currentIndexChanged( int );
    void countChanged(int);

private:
    Q_DECLARE_PRIVATE(PlaylistListModel)

    void moveItems(const QList<int> &indexes, int target, bool isPreTarget);

    QScopedPointer<PlaylistListModelPrivate> d_ptr;

};


  } // namespace playlist
} // namespace vlc

#endif
