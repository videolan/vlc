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

#ifndef MLBOOKMARKMODEL_HPP
#define MLBOOKMARKMODEL_HPP

#include <QAbstractListModel>
#include <memory>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_threads.h>
#include <vlc_input_item.h>
#include <vlc_player.h>
#include <vlc_cxx_helpers.hpp>

#include "mlhelper.hpp"
#include "mlevent.hpp"

Q_MOC_INCLUDE( "medialibrary/medialib.hpp" )
Q_MOC_INCLUDE( "player/player_controller.hpp" )

class PlayerController;
class MediaLib;
class MLBookmarkModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(PlayerController * player READ playerController WRITE setPlayer FINAL)

    Q_PROPERTY(MediaLib * ml READ ml WRITE setMl FINAL)

public:
    explicit MLBookmarkModel( QObject* parent = nullptr );
    virtual ~MLBookmarkModel();

    enum BookmarkRoles {
        NameRole = Qt::UserRole,
        TimeRole = Qt::UserRole + 1,
        PositionRole = Qt::UserRole + 2,
        DescriptionRole = Qt::UserRole + 3
    };
    QHash<int, QByteArray> roleNames() const override;

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole ) const override;
    bool setData( const QModelIndex& index, const QVariant& value, int role = Qt::EditRole ) override;
    Qt::ItemFlags flags( const QModelIndex & ) const override;
    int rowCount( const QModelIndex& index = {} ) const override;
    int columnCount( const QModelIndex& index = {} ) const override;
    QModelIndex index( int row, int column,
                       const QModelIndex& parent = QModelIndex() ) const override;
    QModelIndex parent( const QModelIndex& ) const override;
    QVariant headerData( int section, Qt::Orientation orientation, int role ) const override;
    void sort( int column, Qt::SortOrder order ) override;

    vlc_player_t* player() const;
    void setPlayer(vlc_player_t* player);
    PlayerController * playerController() const;
    void setPlayer(PlayerController* playerController);
    MediaLib* ml() const;
    void setMl(MediaLib* ml);

    void add();
    void remove( const QModelIndexList& indexes );
    void clear();
    Q_INVOKABLE void select( const QModelIndex& index );

private:
    static void onCurrentMediaChanged( vlc_player_t* player, input_item_t* media,
                                       void* data );
    static void onPlaybackStateChanged( vlc_player_t* player, vlc_player_state state,
                                        void* data );
    void playerLengthChanged();

    void updateMediaId(uint64_t revision, const QString mediaUri);
    static void onVlcMlEvent( void* data, const vlc_ml_event_t* event );

    int columnToRole(int column) const;
    void initModel();

    enum RefreshOperation {
        MLBOOKMARKMODEL_REFRESH,
        MLBOOKMARKMODEL_CLEAR,
    };
    void refresh( RefreshOperation forceClear );
private:
    using BookmarkListPtr = ml_unique_ptr<vlc_ml_bookmark_list_t>;
    using InputItemPtr = std::unique_ptr<input_item_t, decltype(&input_item_Release)>;

    MediaLib* m_mediaLib = nullptr;
    vlc_player_t* m_player = nullptr;
    vlc_player_listener_id* m_listener = nullptr;
    PlayerController* m_player_controller = nullptr;

    // Assume to be only used from the GUI thread
    BookmarkListPtr m_bookmarks;
    uint64_t m_currentMediaId = 0;

    mutable vlc::threads::mutex m_mutex;
    uint64_t m_revision = 0;
    // current item & media id can be accessed by any thread and therefore
    // must be accessed with m_mutex held
    InputItemPtr m_currentItem;

    vlc_ml_sorting_criteria_t m_sort = VLC_ML_SORTING_INSERTIONDATE;
    bool m_desc = false;

protected:
    std::unique_ptr<vlc_ml_event_callback_t,
                    std::function<void(vlc_ml_event_callback_t*)>> m_ml_event_handle;
    virtual void onVlcMlEvent( const MLEvent &event );
};

#endif // MLBOOKMARKMODEL_HPP
