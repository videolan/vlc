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

#include "util/locallistbasemodel.hpp"

#include "medialibrary/mlhelper.hpp"

#include "playlist/media.hpp"
#include "playlist/playlist_controller.hpp"

#include <memory>
#include <QPixmap>

#include <vlc_media_source.h>
#include <vlc_addons.h>
#include <vlc_cxx_helpers.hpp>
#include <vlc_addons.h>

namespace {

using AddonPtr = vlc_shared_data_ptr_type(addon_entry_t,
                                          addon_entry_Hold, addon_entry_Release);

class SDItem {
public:
    SDItem( AddonPtr addon )
    {
        name = qfu( addon->psz_name );
        summery = qfu( addon->psz_summary ).trimmed();
        description = qfu( addon->psz_description ).trimmed();
        author = qfu( addon->psz_author );
        sourceUrl = QUrl( addon->psz_source_uri );
        entry = addon;

        if ( addon->psz_image_data ) {
            char *cDir = config_GetUserDir( VLC_CACHE_DIR );
            if (likely(cDir != nullptr))
            {
                QDir dir( cDir );
                free(cDir);
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
        }
        else if ( addon->e_flags & ADDON_BROKEN )
            artworkUrl = QUrl( ":/addons/addon_broken.svg" );
        else
            artworkUrl = QUrl( ":/addons/addon_default.svg" );
    }

public:
    QString name;
    QString summery;
    QString description;
    QString author;
    QUrl sourceUrl;
    QUrl artworkUrl;
    AddonPtr entry;
};

} //namespace

using SDItemPtr = std::shared_ptr<SDItem> ;
using SDItemList = std::vector<SDItemPtr> ;

// ListCache specialisation

template<>
bool ListCache<SDItemPtr>::compareItems(const SDItemPtr& a, const SDItemPtr& b)
{
    //just compare the pointers here
    return a == b;
}

// ServicesDiscoveryModelPrivate

class ServicesDiscoveryModelPrivate
    : public LocalListBaseModelPrivate<SDItemPtr>
{

public:
    Q_DECLARE_PUBLIC(ServicesDiscoveryModel)

public: //ctor/dtor
    ServicesDiscoveryModelPrivate(ServicesDiscoveryModel* pub)
        : LocalListBaseModelPrivate<SDItemPtr>(pub)
    {
    }

    ~ServicesDiscoveryModelPrivate()
    {
        if ( m_manager )
            addons_manager_Delete( m_manager );
    }

public:
    const SDItem* getItemForRow(int row) const
    {
        const SDItemPtr* ref = item(row);
        if (ref)
            return ref->get();
        return nullptr;
    }

public: //BaseModelPrivateT implementation
    bool initializeModel() override;

    LocalListCacheLoader<SDItemPtr>::ItemCompare getSortFunction() const override
    {
        if (m_sortOrder == Qt::SortOrder::DescendingOrder)
            return [](const SDItemPtr& a, const SDItemPtr& b){
                return QString::compare(a->name, b->name) > 0;
            };
        else
            return [](const SDItemPtr& a, const SDItemPtr& b) {
                return QString::compare(a->name, b->name) < 0;
            };
    }



public: //discovery callbacks
    void addonFound( AddonPtr addon );
    void addonChanged( AddonPtr addon );
    void discoveryEnded();

public: //LocalListCacheLoader implementation
    //return the data matching the pattern
    SDItemList getModelData(const QString& pattern) const override
    {
        if (pattern.isEmpty())
            return m_items;

        SDItemList items;
        std::copy_if(
            m_items.cbegin(), m_items.cend(),
            std::back_inserter(items),
            [&pattern](const SDItemPtr& item) {
                return item->name.contains(pattern, Qt::CaseInsensitive);
            });
        return items;
    }

public: // data
    addons_manager_t* m_manager = nullptr;
    SDItemList m_items;
};

ServicesDiscoveryModel::ServicesDiscoveryModel( QObject* parent )
    : BaseModel( new ServicesDiscoveryModelPrivate(this), parent )
{
}

QVariant ServicesDiscoveryModel::data( const QModelIndex& index, int role ) const
{
    Q_D(const ServicesDiscoveryModel);
    if (!m_ctx)
        return {};

    const SDItem* item = d->getItemForRow(index.row());
    if (!item)
        return {};

    switch ( role )
    {
        case Role::SERVICE_NAME:
            return item->name;
        case Role::SERVICE_AUTHOR:
            return item->author;
        case Role::SERVICE_SUMMARY:
            return item->summery;
        case Role::SERVICE_DESCRIPTION:
            return item->description;
        case Role::SERVICE_DOWNLOADS:
            return QVariant::fromValue( item->entry->i_downloads );
        case Role::SERVICE_SCORE:
            return item->entry->i_score / 100;
        case Role::SERVICE_STATE:
            return item->entry->e_state;
        case Role::SERVICE_ARTWORK:
            return item->artworkUrl;
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

void ServicesDiscoveryModel::installService(int idx)
{
    Q_D(ServicesDiscoveryModel);

    const SDItem* item = d->getItemForRow(idx);
    if (!item)
        return;

    addon_uuid_t uuid;
    memcpy( uuid, item->entry->uuid, sizeof( uuid ) );
    addons_manager_Install( d->m_manager, uuid );
}

void ServicesDiscoveryModel::removeService(int idx)
{
    Q_D(ServicesDiscoveryModel);

    const SDItem* item = d->getItemForRow(idx);
    if (!item)
        return;

    addon_uuid_t uuid;
    memcpy( uuid, item->entry->uuid, sizeof( uuid ) );
    addons_manager_Remove( d->m_manager, uuid );
}


void ServicesDiscoveryModel::setCtx(MainCtx* ctx)
{
    Q_D(ServicesDiscoveryModel);

    if (ctx == m_ctx)
        return;

    assert(ctx);
    m_ctx = ctx;
    d->initializeModel();
    emit ctxChanged();
}

static void addonFoundCallback( addons_manager_t *manager, addon_entry_t *entry )
{
    if (entry->e_type != ADDON_SERVICE_DISCOVERY)
        return;
    ServicesDiscoveryModelPrivate* d = (ServicesDiscoveryModelPrivate*) manager->owner.sys;
    QMetaObject::invokeMethod( d->q_func(), [d, entryPtr = AddonPtr(entry)]()
        {
            d->addonFound( std::move( entryPtr ) );
        }, Qt::QueuedConnection);
}

static void addonsDiscoveryEndedCallback( addons_manager_t *manager )
{
    ServicesDiscoveryModelPrivate* d = (ServicesDiscoveryModelPrivate*) manager->owner.sys;
    QMetaObject::invokeMethod( d->q_func(), [d]()
        {
            d->discoveryEnded();
        }, Qt::QueuedConnection);
}

static void addonChangedCallback( addons_manager_t *manager, addon_entry_t *entry )
{
    if (entry->e_type != ADDON_SERVICE_DISCOVERY)
        return;
    ServicesDiscoveryModelPrivate* d = (ServicesDiscoveryModelPrivate*) manager->owner.sys;
    QMetaObject::invokeMethod( d->q_func(), [d, entryPtr = AddonPtr(entry)]()
        {
            d->addonChanged( std::move( entryPtr ) );
        }, Qt::QueuedConnection);
}

bool ServicesDiscoveryModelPrivate::initializeModel()
{
    Q_Q(ServicesDiscoveryModel);
    if (m_qmlInitializing || !q->m_ctx)
        return false;

    if ( m_manager )
        addons_manager_Delete( m_manager );

    struct addons_manager_owner owner =
    {
        this,
        addonFoundCallback,
        addonsDiscoveryEndedCallback,
        addonChangedCallback,
    };

    m_manager = addons_manager_New( VLC_OBJECT( q->m_ctx->getIntf() ), &owner );
    assert( m_manager );

    emit q->loadingChanged();
    addons_manager_LoadCatalog( m_manager );
    addons_manager_Gather( m_manager, "repo://" );
    return true;
}


void ServicesDiscoveryModelPrivate::addonFound( AddonPtr addon )
{
    m_items.emplace_back(std::make_shared<SDItem>(addon));
    m_revision++;
    invalidateCache();
}

void ServicesDiscoveryModelPrivate::addonChanged( AddonPtr addon )
{
    for ( size_t r = 0; r < m_items.size(); ++r )
    {
        if ( memcmp( m_items[r]->entry->uuid, addon->uuid, sizeof( addon->uuid ) ) )
            continue;

        m_items[r] = std::make_shared<SDItem>(addon);
        break;
    }
    m_revision++;
    invalidateCache();
}

void ServicesDiscoveryModelPrivate::discoveryEnded()
{
    Q_Q(ServicesDiscoveryModel);
    assert( m_loading );
    m_loading = false;
    emit q->loadingChanged();
}
