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

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mlbookmarkmodel.hpp"
#include "qt.hpp"

#include <vlc_cxx_helpers.hpp>

#include "medialib.hpp"
#include "mlhelper.hpp"

#include "util/vlctick.hpp"
#include "player/player_controller.hpp"

MLBookmarkModel::MLBookmarkModel( QObject *parent )
    : QAbstractListModel( parent )
    , m_currentItem( nullptr, &input_item_Release )
    , m_ml_event_handle( nullptr, [this](vlc_ml_event_callback_t* cb ) {
        if (!cb)
            return;
        if ( m_mediaLib )
            m_mediaLib->unregisterEventListener(cb);
    })
{}

MLBookmarkModel::~MLBookmarkModel()
{
    if(m_player && m_listener){
        vlc_player_locker lock{ m_player };
        vlc_player_RemoveListener( m_player, m_listener );
    }
}

QVariant MLBookmarkModel::data( const QModelIndex &index, int role ) const
{
    if ( !index.isValid() || index.row() < 0 || m_player == nullptr ||
         !m_bookmarks || (uint32_t)index.row() >= m_bookmarks->i_nb_items )
        return QVariant{};

    const auto& bookmark = m_bookmarks->p_items[index.row()];

    if (role == Qt::DisplayRole || role == Qt::EditRole)
        role = columnToRole(index.column());

    switch(role){
        case BookmarkRoles::NameRole:
            return QVariant::fromValue( qfu( bookmark.psz_name ) );
        case BookmarkRoles::TimeRole:
            return QVariant::fromValue( VLCTick::fromMS( bookmark.i_time ).formatHMS() );
        case BookmarkRoles::PositionRole:
        {
            vlc_player_locker lock{ m_player };
            return !vlc_player_GetLength( m_player ) ? QVariant{} :
                    QVariant::fromValue<float> ((float) VLC_TICK_FROM_MS(bookmark.i_time) / vlc_player_GetLength( m_player ));
        }
        case BookmarkRoles::DescriptionRole:
            return QVariant::fromValue( qfu( bookmark.psz_description ) );
        default:
            return QVariant{};
    }
}

bool MLBookmarkModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if ( index.isValid() == false )
        return false;
    if ( role == Qt::EditRole )
        role = columnToRole(index.column());
    if ( role != BookmarkRoles::NameRole && role != BookmarkRoles::DescriptionRole )
        return false;
    if ( value.canConvert<QString>() == false )
        return false;

    size_t row = index.row();
    bool updateName = (index.column() == 0);

    if ( ! m_bookmarks || row >= m_bookmarks->i_nb_items )
        return false;

    auto& b = m_bookmarks->p_items[row];
    auto str = value.toString();

    struct Ctx {
        bool updateSucceed;
    };
    m_mediaLib->runOnMLThread<Ctx>(this,
    //ML thread
    [mediaId = b.i_media_id, bookmarkTime = b.i_time, updateName, str]
    (vlc_medialibrary_t* ml, Ctx& ctx)
    {
        int ret;
        if ( updateName )
            ret = vlc_ml_media_update_bookmark( ml, mediaId, bookmarkTime, qtu( str ), nullptr );
        else
            ret = vlc_ml_media_update_bookmark( ml, mediaId, bookmarkTime, nullptr, qtu( str ) );
        ctx.updateSucceed = (ret == VLC_SUCCESS);
    },
    //UI thread
    [this, updateName, mediaId = m_currentMediaId, row, str]
    (quint64, Ctx& ctx)
    {
        if (!ctx.updateSucceed)
            return;

        if (m_currentMediaId != mediaId)
            return;

        auto& b = m_bookmarks->p_items[row];
        if (updateName)
        {
            free( b.psz_name );
            b.psz_name = strdup( qtu( str ) );
            emit dataChanged(this->index(row, 0), this->index(row, 0),
                             {Qt::DisplayRole, BookmarkRoles::NameRole});
        }
        else
        {
            free( b.psz_description );
            b.psz_description = strdup( qtu( str ) );
            emit dataChanged(this->index(row, 2), this->index(row, 2),
                             {Qt::DisplayRole, BookmarkRoles::DescriptionRole});
        }
    });

    return true;
}

QHash<int, QByteArray> MLBookmarkModel::roleNames() const{
    return QHash<int, QByteArray>{
        {BookmarkRoles::NameRole, "name"},
        {BookmarkRoles::TimeRole, "time"},
        {BookmarkRoles::PositionRole, "position"},
        {BookmarkRoles::DescriptionRole, "description"}
    };
}

int MLBookmarkModel::columnToRole(int column) const{
    switch (column)
    {
      case 0:
         return BookmarkRoles::NameRole;
      case 1:
         return BookmarkRoles::TimeRole;
      case 2:
        return BookmarkRoles::DescriptionRole;
      default:
         return -1;
    }
}

Qt::ItemFlags MLBookmarkModel::flags( const QModelIndex& index ) const
{
    auto f = QAbstractItemModel::flags( index );
    if ( index.column() != 1 )
        f |= Qt::ItemIsEditable;
    return f;
}

int MLBookmarkModel::rowCount(const QModelIndex &) const
{
    if ( m_bookmarks == nullptr )
        return 0;
    return m_bookmarks->i_nb_items;
}

int MLBookmarkModel::columnCount(const QModelIndex &) const
{
    return 3;
}

QModelIndex MLBookmarkModel::index( int row, int column, const QModelIndex& ) const
{
    return createIndex( row, column, nullptr );
}

QModelIndex MLBookmarkModel::parent(const QModelIndex &) const
{
    return createIndex( -1, -1, nullptr );
}

QVariant MLBookmarkModel::headerData( int section, Qt::Orientation orientation,
                                      int role ) const
{
    if (role != Qt::DisplayRole || orientation == Qt::Vertical)
        return QVariant{};

    switch ( section )
    {
        case 0:
            return QVariant::fromValue( qtr( "Name" ) );
        case 1:
            return QVariant::fromValue( qtr( "Time" ) );
        case 2:
            return QVariant::fromValue( qtr( "Description" ) );
        default:
            return QVariant{};
    }
}

void MLBookmarkModel::sort( int column, Qt::SortOrder order )
{
    switch ( column )
    {
        case 0:
            m_sort = VLC_ML_SORTING_ALPHA;
            break;
        case 1:
            // For bookmarks, default sort order is their time in the media
            m_sort = VLC_ML_SORTING_DEFAULT;
            break;
        case 2:
            // We don't support ordering by description, just ignore it
            return;
        default:
            // In all other cases, just list the items as they are inserted
            m_sort = VLC_ML_SORTING_INSERTIONDATE;
            break;
    }
    m_desc = order == Qt::DescendingOrder ? true : false;
    refresh( MLBOOKMARKMODEL_REFRESH );
}

void MLBookmarkModel::add()
{
    vlc_tick_t currentTime;
    {
        vlc_player_locker lock{ m_player };
        currentTime = vlc_player_GetTime( m_player );
    }

    if (m_currentMediaId == 0)
        return;

    m_mediaLib->runOnMLThread(this,
    //ML thread
    [mediaId = m_currentMediaId, currentTime, count = rowCount()](vlc_medialibrary_t* ml)
    {
        int64_t time = MS_FROM_VLC_TICK(currentTime);

        vlc_ml_media_add_bookmark(ml, mediaId, time);

        ml_unique_ptr<vlc_ml_media_t> media { vlc_ml_get_media(ml, mediaId) };

        if (media)
        {
            QString name = qtr("Bookmark at %1").arg(VLCTick::fromMS( time ).formatHMS());

            vlc_ml_media_update_bookmark(ml, mediaId, time, qtu(name), nullptr);
        }
    },
    //UI thread
    [this](){
        refresh( MLBOOKMARKMODEL_REFRESH );
    });
}

void MLBookmarkModel::remove( const QModelIndexList &indexes )
{
    if (m_currentMediaId == 0)
        return;

    std::vector<int64_t> bookmarkTimeList;
    for ( const auto& i : indexes )
    {
        if ( i.isValid() == false || (size_t)i.row() >= m_bookmarks->i_nb_items )
            continue;
        auto& b = m_bookmarks->p_items[i.row()];
        bookmarkTimeList.push_back(b.i_time);
    }

    m_mediaLib->runOnMLThread(this,
    //ML thread
    [mediaId = m_currentMediaId, bookmarkTimeList]
    (vlc_medialibrary_t* ml)
    {
        for (int64_t bookmarkTime : bookmarkTimeList)
            vlc_ml_media_remove_bookmark(ml, mediaId, bookmarkTime);
    },
    //UI thread
    [this](){
        refresh( MLBOOKMARKMODEL_REFRESH );
    });
}

void MLBookmarkModel::clear()
{
    if (m_currentMediaId == 0)
        return;

    m_mediaLib->runOnMLThread(this,
    //ML thread
    [mediaId = m_currentMediaId](vlc_medialibrary_t* ml)
    {
        vlc_ml_media_remove_all_bookmarks( ml, mediaId );
    },
    //UI thread
    [this, mediaId = m_currentMediaId]()
    {
        if (mediaId == m_currentMediaId)
        {
            beginResetModel();
            m_bookmarks.reset();
            endResetModel();
        }
    });
}

void MLBookmarkModel::select(const QModelIndex &index)
{
    if ( index.isValid() == false )
        return;
    const auto& b = m_bookmarks->p_items[index.row()];
    vlc_player_locker lock{ m_player };
    vlc_player_SetTime( m_player, VLC_TICK_FROM_MS( b.i_time ) );
}

void MLBookmarkModel::onCurrentMediaChanged( vlc_player_t*, input_item_t* media,
                                             void *data )
{
    //Player thread
    auto self = static_cast<MLBookmarkModel*>( data );
    QmlInputItem item(media, true);
    QString mediaUri;
    uint64_t revision;
    {
        vlc::threads::mutex_locker lock{ self->m_mutex };
        self->m_currentItem.reset( media ? input_item_Hold( media ) : nullptr );
        revision = ++self->m_revision;
        if (media)
            mediaUri = media->psz_uri;
    }

    //UI Thread
    QMetaObject::invokeMethod(self,
    [self, revision, mediaUri]()
    {
        {
            vlc::threads::mutex_locker lock{ self->m_mutex };
            //did we start playing a new media in between
            if (self->m_revision != revision)
                return;
        }
        self->updateMediaId(revision, mediaUri);
    });
}

void MLBookmarkModel::onPlaybackStateChanged( vlc_player_t *, vlc_player_state state,
                                              void *data )
{
    auto self = static_cast<MLBookmarkModel*>( data );

    if(self->m_currentMediaId == 0)
        return;

    QMetaObject::invokeMethod(self, [self, state](){
        if ( state == VLC_PLAYER_STATE_STARTED )
            self->refresh( MLBOOKMARKMODEL_REFRESH );
        else if ( state == VLC_PLAYER_STATE_STOPPING )
            self->refresh( MLBOOKMARKMODEL_CLEAR );
    });
}


void MLBookmarkModel::updateMediaId(uint64_t revision, const QString mediaUri)
{
    if (mediaUri.isEmpty())
    {
        refresh(MLBOOKMARKMODEL_CLEAR);
        return;
    }

    //retrieve the media id in the medialib
    struct Ctx{
        uint64_t newMLid = 0;
        BookmarkListPtr newBookmarks;
    };
    m_mediaLib->runOnMLThread<Ctx>(this,
    //ML thread
    [mediaUri, sort = m_sort, desc = m_desc](vlc_medialibrary_t* ml, Ctx& ctx){

        auto mlMedia = vlc_ml_get_media_by_mrl( ml, qtu(mediaUri) );
        if ( mlMedia != nullptr )
        {
            ctx.newMLid = mlMedia->i_id;
            vlc_ml_release( mlMedia );
        }
        vlc_ml_query_params_t params{};
        params.i_sort = sort;
        params.b_desc = desc;
        ctx.newBookmarks.reset( vlc_ml_list_media_bookmarks( ml, &params, ctx.newMLid ) );
    },
    //UI thread
    [this, revision](quint64, Ctx& ctx) {
        bool valid;
        {
            vlc::threads::mutex_locker lock{ m_mutex };
            //did we start playing a new media in between
            valid = (m_revision == revision);
        }
        if (valid)
        {
            beginResetModel();
            m_bookmarks = std::move(ctx.newBookmarks);
            m_currentMediaId = ctx.newMLid;
            refresh(MLBOOKMARKMODEL_REFRESH);
            endResetModel();
        }
    });
}

void MLBookmarkModel::refresh(MLBookmarkModel::RefreshOperation forceClear )
{
    if (m_currentMediaId == 0 || forceClear == MLBOOKMARKMODEL_CLEAR)
    {
        beginResetModel();
        m_bookmarks.reset();
        endResetModel();
    }
    else
    {
        uint64_t mediaId = m_currentMediaId;
        struct Ctx
        {
            BookmarkListPtr newBookmarks;
        };
        m_mediaLib->runOnMLThread<Ctx>(this,
        //ML thread
        [mediaId, sort = m_sort, desc = m_desc]
        (vlc_medialibrary_t* ml, Ctx& ctx) {
            vlc_ml_query_params_t params{};
            params.i_sort = sort;
            params.b_desc = desc;
            ctx.newBookmarks.reset( vlc_ml_list_media_bookmarks( ml, &params, mediaId ) );
        },
        //UI thread
        [this, mediaId](quint64, Ctx& ctx)
        {
            beginResetModel();
            if (m_currentMediaId == mediaId)
                m_bookmarks = std::move(ctx.newBookmarks);
            else
                m_bookmarks.reset();
            endResetModel();
        });
    }
}

vlc_player_t * MLBookmarkModel::player() const
{
    return m_player;
}

void MLBookmarkModel::setPlayer(vlc_player_t * player)
{
    if (m_player == player) return;

    if(m_player && m_listener){
        vlc_player_locker lock{ m_player };
        vlc_player_RemoveListener( m_player, m_listener );
    }

    m_player = player;

    if (m_player && m_mediaLib)
        initModel();
}


PlayerController * MLBookmarkModel::playerController() const
{
    return m_player_controller;
}

void MLBookmarkModel::setPlayer(PlayerController * player)
{
    if (m_player_controller == player) return;

    if (m_player_controller)
        disconnect(m_player_controller, nullptr, this, nullptr);

    m_player_controller = player;

    connect(m_player_controller, &PlayerController::lengthChanged, this, &MLBookmarkModel::playerLengthChanged);

    setPlayer(player->getPlayer());
}

MediaLib* MLBookmarkModel::ml() const
{
    return m_mediaLib;
}

void MLBookmarkModel::playerLengthChanged()
{
    refresh(MLBOOKMARKMODEL_REFRESH);
}

void MLBookmarkModel::setMl(MediaLib* medialib)
{
    if(m_mediaLib == medialib) return;

    if(m_mediaLib)
        m_ml_event_handle.reset(nullptr);

    m_mediaLib = medialib;

    if (m_player && m_mediaLib)
        initModel();
}

void MLBookmarkModel::initModel()
{
    static const vlc_player_cbs cbs {
        &onCurrentMediaChanged,
        &onPlaybackStateChanged,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
    };
    QString uri;
    {
        vlc_player_locker lock{ m_player };
        vlc::threads::mutex_locker selflock{ m_mutex };
        m_listener = vlc_player_AddListener( m_player, &cbs, this );
        if ( m_listener == nullptr )
            throw std::bad_alloc{};
        auto currentItem = vlc_player_GetCurrentMedia( m_player );
        m_currentItem = vlc::wrap_cptr( currentItem ? input_item_Hold( currentItem ) : nullptr,
                                        &input_item_Release );
        if (m_currentItem)
        {
            uri = m_currentItem->psz_uri;
        }
    }
    updateMediaId(0, uri);

    if (m_ml_event_handle == nullptr)
        m_ml_event_handle.reset(m_mediaLib->registerEventListener(onVlcMlEvent, this));
}

void MLBookmarkModel::onVlcMlEvent(void* data, const vlc_ml_event_t* event)
{
    auto self = static_cast<MLBookmarkModel*>(data);
    auto  mlEvent = std::make_shared<MLEvent>(event);
    QMetaObject::invokeMethod(self, [self, mlEvent] () mutable {
        self->onVlcMlEvent(*mlEvent);
    });
}

void MLBookmarkModel::onVlcMlEvent(const MLEvent &event){
    switch (event.i_type)
    {
        case VLC_ML_EVENT_BOOKMARKS_ADDED:
        case VLC_ML_EVENT_BOOKMARKS_DELETED:
        case VLC_ML_EVENT_BOOKMARKS_UPDATED:
            refresh(MLBOOKMARKMODEL_REFRESH);
        break;
        default:
            break;
    }
}
