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

#include "mlhelper.hpp"

MLBookmarkModel::MLBookmarkModel( vlc_medialibrary_t *ml, vlc_player_t *player,
                                  QObject *parent )
    : QAbstractListModel( parent )
    , m_ml( ml )
    , m_player( player )
    , m_listener( nullptr )
    , m_currentItem( nullptr, &input_item_Release )
    , m_currentMediaId( 0 )
    , m_sort( VLC_ML_SORTING_INSERTIONDATE )
    , m_desc( false )
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
    {
        vlc_player_locker lock{ m_player };
        m_listener = vlc_player_AddListener( m_player, &cbs, this );
        if ( m_listener == nullptr )
            throw std::bad_alloc{};
        auto currentItem = vlc_player_GetCurrentMedia( m_player );
        m_currentItem = vlc::wrap_cptr( currentItem ? input_item_Hold( currentItem ) : nullptr,
                                        &input_item_Release );
    }
    refresh( false );
}

MLBookmarkModel::~MLBookmarkModel()
{
    vlc_player_locker lock{ m_player };
    vlc_player_RemoveListener( m_player, m_listener );
}

QVariant MLBookmarkModel::data( const QModelIndex &index, int role ) const
{
    if ( !index.isValid() || index.row() < 0 ||
         m_bookmarks == nullptr ||
         (uint32_t)index.row() >= m_bookmarks->i_nb_items )
    {
        return QVariant{};
    }

    const auto& bookmark = m_bookmarks->p_items[index.row()];
    if ( role != Qt::DisplayRole )
        return QVariant{};

    switch ( index.column() )
    {
    case 0:
        return QVariant::fromValue( QString::fromUtf8( bookmark.psz_name ) );
    case 1:
        return QVariant::fromValue( MsToString( bookmark.i_time ) );
    case 2:
        return QVariant::fromValue( QString::fromUtf8( bookmark.psz_description ) );
    default:
        return QVariant{};
    }
}

bool MLBookmarkModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if ( index.isValid() == false )
        return false;
    if ( role != Qt::EditRole )
        return false;
    if ( index.column() == 1 )
        /* Disable editing the Time value through the listing */
        return false;
    if ( value.canConvert<QString>() == false )
        return false;
    assert( index.column() == 0 || index.column() == 2 );
    assert( (size_t)index.row() < m_bookmarks->i_nb_items );
    auto& b = m_bookmarks->p_items[index.row()];
    auto str = value.toString();
    if ( index.column() == 0 )
    {
        if ( vlc_ml_media_update_bookmark( m_ml, b.i_media_id, b.i_time,
                                           qtu( str ), nullptr ) != VLC_SUCCESS )
            return false;
        free( b.psz_name );
        b.psz_name = strdup( qtu( str ) );
        return true;
    }
    if ( vlc_ml_media_update_bookmark( m_ml, b.i_media_id, b.i_time,
                                       nullptr, qtu( str ) ) != VLC_SUCCESS )
        return false;
    free( b.psz_description );
    b.psz_description = strdup( qtu( str ) );
    return true;
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
    if ( role != Qt::DisplayRole )
        return QVariant{};
    if ( orientation == Qt::Vertical )
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
    vlc::threads::mutex_locker lock{ m_mutex };

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
    refresh( false );
}

void MLBookmarkModel::add()
{
    vlc_tick_t currentTime;
    {
        vlc_player_locker lock{ m_player };
        currentTime = vlc_player_GetTime( m_player );
    }

    {
        vlc::threads::mutex_locker lock{ m_mutex };
        if ( m_currentItem == nullptr )
            return;
        if ( m_currentMediaId == 0 )
        {
            auto mlMedia = vlc_ml_get_media_by_mrl( m_ml, m_currentItem->psz_uri );
            if ( mlMedia == nullptr )
                return;
            m_currentMediaId = mlMedia->i_id;
        }
    }
    vlc_ml_media_add_bookmark( m_ml, m_currentMediaId,
                               MS_FROM_VLC_TICK( currentTime ) );
    refresh( false );
}

void MLBookmarkModel::remove( const QModelIndexList &indexes )
{
    int64_t mediaId;
    {
        vlc::threads::mutex_locker lock{ m_mutex };
        mediaId = m_currentMediaId;
    }
    for ( const auto& i : indexes )
    {
        if ( i.isValid() == false || (size_t)i.row() >= m_bookmarks->i_nb_items )
            continue;
        auto& b = m_bookmarks->p_items[i.row()];
        vlc_ml_media_remove_bookmark( m_ml, mediaId, b.i_time );
    }
    refresh( false );
}

void MLBookmarkModel::clear()
{
    int64_t mediaId;
    {
        vlc::threads::mutex_locker lock{ m_mutex };
        mediaId = m_currentMediaId;
    }
    beginResetModel();
    vlc_ml_media_remove_all_bookmarks( m_ml, mediaId );
    m_bookmarks.reset();
    endResetModel();
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
    auto self = static_cast<MLBookmarkModel*>( data );

    {
        vlc::threads::mutex_locker lock{ self->m_mutex };
        self->m_currentItem.reset( media ? input_item_Hold( media ) : nullptr );
        if ( media == nullptr )
            self->m_currentMediaId = 0;
    }
    self->refresh( false );
}

void MLBookmarkModel::onPlaybackStateChanged( vlc_player_t *, vlc_player_state state,
                                              void *data )
{
    auto self = static_cast<MLBookmarkModel*>( data );

    if ( state == VLC_PLAYER_STATE_STARTED )
        self->refresh( false );
    else if ( state == VLC_PLAYER_STATE_STOPPING )
        self->refresh( true );
}

void MLBookmarkModel::refresh( bool forceClear )
{
    callAsync([this, forceClear]() {
        vlc::threads::mutex_locker lock( m_mutex );

        if ( forceClear == false && m_currentMediaId == 0 && m_currentItem != nullptr )
        {
            auto mlMedia = vlc_ml_get_media_by_mrl( m_ml, m_currentItem->psz_uri );
            if ( mlMedia != nullptr )
            {
                m_currentMediaId = mlMedia->i_id;
                vlc_ml_release( mlMedia );
            }
        }
        beginResetModel();
        if ( m_currentMediaId == 0 || forceClear == true )
            m_bookmarks.reset();
        else
        {
            vlc_ml_query_params_t params{};
            params.i_sort = m_sort;
            params.b_desc = m_desc;
            m_bookmarks.reset( vlc_ml_list_media_bookmarks( m_ml, &params, m_currentMediaId ) );
        }
        endResetModel();
    });
}
