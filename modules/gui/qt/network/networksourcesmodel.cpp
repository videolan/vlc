/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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

#include <QQmlFile>

#include "networksourcesmodel.hpp"
#include "networkmediamodel.hpp"

#include "playlist/media.hpp"
#include "playlist/playlist_controller.hpp"

NetworkSourcesModel::NetworkSourcesModel( QObject* parent )
    : QAbstractListModel( parent )
{
}

QVariant NetworkSourcesModel::data( const QModelIndex& index, int role ) const
{
    if (!m_ctx)
        return {};
    auto idx = index.row();
    if ( idx < 0 || (size_t)idx >= m_items.size() )
        return {};
    const auto& item = m_items[idx];
    switch ( role )
    {
        case SOURCE_NAME:
            return item.name;
        case SOURCE_LONGNAME:
            return item.longName;
        case SOURCE_TYPE:
            return idx == 0 ? TYPE_DUMMY : TYPE_SOURCE;
        case SOURCE_ARTWORK:
            return item.artworkUrl;
        default:
            return {};
    }
}

QHash<int, QByteArray> NetworkSourcesModel::roleNames() const
{
    return {
        { SOURCE_NAME, "name" },
        { SOURCE_LONGNAME, "long_name" },
        { SOURCE_TYPE, "type" },
        { SOURCE_ARTWORK, "artwork" }
    };
}

int NetworkSourcesModel::rowCount(const QModelIndex& parent) const
{
    if ( parent.isValid() )
        return 0;
    return getCount();
}


void NetworkSourcesModel::setCtx(MainCtx* ctx)
{
    if (ctx) {
        m_ctx = ctx;
    }
    if (m_ctx) {
        initializeMediaSources();
    }
    emit ctxChanged();
}

int NetworkSourcesModel::getCount() const
{
    assert( m_items.size() < INT32_MAX );
    return static_cast<int>( m_items.size() );
}

QMap<QString, QVariant> NetworkSourcesModel::getDataAt(int idx)
{
    QMap<QString, QVariant> dataDict;
    QHash<int,QByteArray> roles = roleNames();
    for (auto role: roles.keys()) {
        dataDict[roles[role]] = data(index(idx), role);
    }
    return dataDict;
}

bool NetworkSourcesModel::initializeMediaSources()
{
    auto libvlc = vlc_object_instance(m_ctx->getIntf());

    if (!m_items.empty()) {
        beginResetModel();
        endResetModel();
        emit countChanged();
    }
    m_items = {Item{}}; // dummy item that UI uses to add entry for "add a service"

    auto provider = vlc_media_source_provider_Get( libvlc );

    using SourceMetaPtr = std::unique_ptr<vlc_media_source_meta_list_t,
                                          decltype( &vlc_media_source_meta_list_Delete )>;

    SourceMetaPtr providerList( vlc_media_source_provider_List( provider, static_cast<services_discovery_category_e>(m_sdSource) ),
                                &vlc_media_source_meta_list_Delete );
    if ( providerList == nullptr )
        return false;

    auto nbProviders = vlc_media_source_meta_list_Count( providerList.get() );

    beginResetModel();
    for ( auto i = 0u; i < nbProviders; ++i )
    {
        auto meta = vlc_media_source_meta_list_Get( providerList.get(), i );

        Item item;
        item.name = qfu(meta->name);
        item.longName = qfu(meta->longname);

        if ( item.name.startsWith( "podcast" ) )
        {
            item.artworkUrl = QUrl("qrc:///sd/podcast.svg");
        }
        else if ( item.name.startsWith("lua{") )
        {
            int i_head = item.name.indexOf( "sd='" ) + 4;
            int i_tail = item.name.indexOf( '\'', i_head );
            const QString iconName = QString( "qrc:///sd/%1.svg" ).arg( item.name.mid( i_head, i_tail - i_head ) );
            item.artworkUrl = QFileInfo::exists( QQmlFile::urlToLocalFileOrQrc(iconName) ) ? QUrl(iconName)
                                                            : QUrl("qrc:///sd/network.svg");
        }

        m_items.push_back( std::move(item) );
    }
    endResetModel();
    emit countChanged();

    return m_items.empty() == false;
}
