/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "servicesdiscoverymodel.hpp"
#include <vlc_addons.h>

#include "medialibrary/mlhelper.hpp"

#include "playlist/media.hpp"
#include "playlist/playlist_controller.hpp"

#include <QPixmap>

ServicesDiscoveryModel::ServicesDiscoveryModel( QObject* parent )
    : QAbstractListModel( parent )
{
}

ServicesDiscoveryModel::~ServicesDiscoveryModel()
{
    if ( m_manager )
    {
        addons_manager_Delete( m_manager );
    }
}

QVariant ServicesDiscoveryModel::data( const QModelIndex& index, int role ) const
{
    if (!m_ctx)
        return {};
    auto idx = index.row();
    if ( idx < 0 || (size_t)idx >= m_items.size() )
        return {};
    const auto& item = m_items[idx];
    switch ( role )
    {
        case Role::SERVICE_NAME:
            return item.name;
        case Role::SERVICE_AUTHOR:
            return item.author;
        case Role::SERVICE_SUMMARY:
            return item.summery;
        case Role::SERVICE_DESCRIPTION:
            return item.description;
        case Role::SERVICE_DOWNLOADS:
            return QVariant::fromValue( item.entry->i_downloads );
        case Role::SERVICE_SCORE:
            return item.entry->i_score / 100;
        case Role::SERVICE_STATE:
            return item.entry->e_state;
        case Role::SERVICE_ARTWORK:
            return item.artworkUrl;
        default:
            return {};
    }
}

QHash<int, QByteArray> ServicesDiscoveryModel::roleNames() const
{
    return {
        { Role::SERVICE_NAME, "name" },
        { Role::SERVICE_AUTHOR, "author"},
        { Role::SERVICE_SUMMARY, "summary" },
        { Role::SERVICE_DESCRIPTION, "description" },
        { Role::SERVICE_DOWNLOADS, "downloads" },
        { Role::SERVICE_SCORE, "score" },
        { Role::SERVICE_STATE, "state" },
        { Role::SERVICE_ARTWORK, "artwork" }
    };
}

QMap<QString, QVariant> ServicesDiscoveryModel::getDataAt(int idx)
{
    QMap<QString, QVariant> dataDict;
    QHash<int,QByteArray> roles = roleNames();
    for (auto role: roles.keys()) {
        dataDict[roles[role]] = data(index(idx), role);
    }
    return dataDict;
}

void ServicesDiscoveryModel::installService(int idx)
{
    if ( idx < 0 || idx >= (int)m_items.size() )
        return;

    addon_uuid_t uuid;
    memcpy( uuid, m_items[idx].entry->uuid, sizeof( uuid ) );
    addons_manager_Install( m_manager, uuid );
}

void ServicesDiscoveryModel::removeService(int idx)
{
    if ( idx < 0 || idx >= (int)m_items.size() )
        return;

    addon_uuid_t uuid;
    memcpy( uuid, m_items[idx].entry->uuid, sizeof( uuid ) );
    addons_manager_Remove( m_manager, uuid );
}

int ServicesDiscoveryModel::rowCount(const QModelIndex& parent) const
{
    if ( parent.isValid() )
        return 0;
    return getCount();
}

int ServicesDiscoveryModel::getCount() const
{
    assert( m_items.size() < INT32_MAX );
    return static_cast<int>( m_items.size() );
}

void ServicesDiscoveryModel::setCtx(QmlMainContext* ctx)
{
    if (ctx) {
        m_ctx = ctx;
    }
    if (m_ctx) {
        initializeManager();
    }
    emit ctxChanged();
}

void ServicesDiscoveryModel::initializeManager()
{
    if ( m_manager )
        addons_manager_Delete( m_manager );

    struct addons_manager_owner owner =
    {
        this,
        addonFoundCallback,
        addonsDiscoveryEndedCallback,
        addonChangedCallback,
    };

    m_manager = addons_manager_New( VLC_OBJECT( m_ctx->getIntf() ), &owner );
    assert( m_manager );

    m_parsingPending = true;
    emit parsingPendingChanged();
    addons_manager_LoadCatalog( m_manager );
    addons_manager_Gather( m_manager, "repo://" );
}

void ServicesDiscoveryModel::addonFoundCallback( addons_manager_t *manager,
                                        addon_entry_t *entry )
{
    if (entry->e_type != ADDON_SERVICE_DISCOVERY)
        return;
    ServicesDiscoveryModel *me = (ServicesDiscoveryModel *) manager->owner.sys;
    QMetaObject::invokeMethod( me, [me, entryPtr = AddonPtr(entry)]()
    {
        me->addonFound( std::move( entryPtr ) );
    }, Qt::QueuedConnection);
}

void ServicesDiscoveryModel::addonsDiscoveryEndedCallback( addons_manager_t *manager )
{
    ServicesDiscoveryModel *me = (ServicesDiscoveryModel *) manager->owner.sys;
    QMetaObject::invokeMethod( me, [me]()
    {
        me->discoveryEnded();
    }, Qt::QueuedConnection);
}

void ServicesDiscoveryModel::addonChangedCallback( addons_manager_t *manager,
                                          addon_entry_t *entry )
{
    if (entry->e_type != ADDON_SERVICE_DISCOVERY)
        return;
    ServicesDiscoveryModel *me = (ServicesDiscoveryModel *) manager->owner.sys;
    QMetaObject::invokeMethod( me, [me, entryPtr = AddonPtr(entry)]()
    {
        me->addonChanged( std::move( entryPtr ) );
    }, Qt::QueuedConnection);
}

void ServicesDiscoveryModel::addonFound( ServicesDiscoveryModel::AddonPtr addon )
{
    beginInsertRows( QModelIndex(), getCount(), getCount() );
    m_items.emplace_back(addon);
    endInsertRows();
    emit countChanged();
}

void ServicesDiscoveryModel::addonChanged( ServicesDiscoveryModel::AddonPtr addon )
{
    for ( int r = 0; r < getCount(); ++r )
    {
        if ( memcmp( m_items[r].entry->uuid, addon->uuid, sizeof( addon->uuid ) ) )
            continue;

        m_items[r] = addon;
        emit dataChanged( index( r, 0 ), index( r, 0 ) );
    }
}

void ServicesDiscoveryModel::discoveryEnded()
{
    assert( m_parsingPending );
    m_parsingPending = false;
    emit parsingPendingChanged();
}

ServicesDiscoveryModel::Item::Item( ServicesDiscoveryModel::AddonPtr addon )
{
    *this = addon;
}

ServicesDiscoveryModel::Item &ServicesDiscoveryModel::Item::operator=( ServicesDiscoveryModel::AddonPtr addon )
{
    name = qfu( addon->psz_name );
    summery = qfu( addon->psz_summary ).trimmed();
    description = qfu( addon->psz_description ).trimmed();
    author = qfu( addon->psz_author );
    sourceUrl = QUrl( addon->psz_source_uri );
    entry = addon;

    if ( addon->psz_image_data ) {
        QDir dir( config_GetUserDir( VLC_CACHE_DIR ) );
        dir.mkdir("art");
        dir.cd("art");
        dir.mkdir("qt-addon-covers");
        dir.cd("qt-addon-covers");

        QString id = addons_uuid_to_psz( &addon->uuid );
        QString filename = QString("addon_thumbnail_%1.png").arg(id);
        QString absoluteFilePath =  dir.absoluteFilePath(filename);

        if ( !QFileInfo::exists( absoluteFilePath )) {
            QPixmap pixmap;
            pixmap.loadFromData( QByteArray::fromBase64( QByteArray( addon->psz_image_data ) ),
                0,
                Qt::AutoColor
            );
            pixmap.save(absoluteFilePath);
        }
        artworkUrl = QUrl::fromLocalFile( absoluteFilePath );
    }
    else if ( addon->e_flags & ADDON_BROKEN )
        artworkUrl = QUrl( ":/addons/broken.svg" );
    else
        artworkUrl = QUrl( ":/addons/default.svg" );

    return *this;
}
