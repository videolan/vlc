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
#include <QQmlFile>

#include <vlc_media_source.h>
#include <vlc_addons.h>
#include <vlc_cxx_helpers.hpp>
#include <vlc_addons.h>

#define CHECK_ADDON_TYPE_MATCH(type) \
    static_assert(static_cast<addon_type_t>(ServicesDiscoveryModel::Type::TYPE_ ## type) == ADDON_##type)

CHECK_ADDON_TYPE_MATCH(UNKNOWN);
CHECK_ADDON_TYPE_MATCH(PLAYLIST_PARSER);
CHECK_ADDON_TYPE_MATCH(SERVICE_DISCOVERY);
CHECK_ADDON_TYPE_MATCH(SKIN2);
CHECK_ADDON_TYPE_MATCH(PLUGIN);
CHECK_ADDON_TYPE_MATCH(INTERFACE);
CHECK_ADDON_TYPE_MATCH(META);
CHECK_ADDON_TYPE_MATCH(OTHER);

#undef CHECK_ADDON_TYPE_MATCH

#define CHECK_ADDON_STATE_MATCH(type) \
    static_assert(static_cast<addon_state_t>(ServicesDiscoveryModel::State::STATE_ ## type) == ADDON_##type)

CHECK_ADDON_STATE_MATCH(INSTALLING);
CHECK_ADDON_STATE_MATCH(INSTALLED);
CHECK_ADDON_STATE_MATCH(UNINSTALLING);

#undef CHECK_ADDON_STATE_MATCH

namespace {

using AddonPtr = vlc_shared_data_ptr_type(addon_entry_t,
                                          addon_entry_Hold, addon_entry_Release);

class SDItem {
public:
    //
    SDItem( AddonPtr addon )
    {
        vlc_mutex_locker locker{&addon->lock};
        name = qfu( addon->psz_name );
        summary = qfu( addon->psz_summary ).trimmed();
        description = qfu( addon->psz_description ).trimmed();
        author = qfu( addon->psz_author );
        sourceUrl = QUrl( addon->psz_source_uri );
        entry = addon;
        uuid = QByteArray( reinterpret_cast<const char*>(addon->uuid), sizeof( addon_uuid_t ));

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
                char* uuidStr = addons_uuid_to_psz( &addon->uuid );
                QString filename = QString("addon_thumbnail_%1.png").arg(uuidStr);
                free(uuidStr);
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
    QByteArray  uuid;
    QString name;
    QString summary;
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
    return a->uuid == b->uuid;
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

    bool gatherRepository(const char* uri)
    {
        Q_Q(ServicesDiscoveryModel);
        if (!m_manager)
            return false;
        m_loading = true;
        emit q->loadingChanged();
        addons_manager_Gather( m_manager, uri);
        return true;
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
        if (pattern.isEmpty()
            && m_typeFilter == ServicesDiscoveryModel::Type::TYPE_NONE
            && m_stateFilter == ServicesDiscoveryModel::State::STATE_NONE)
            return m_items;

        SDItemList items;
        std::copy_if(
            m_items.cbegin(), m_items.cend(),
            std::back_inserter(items),
            [&pattern, filter = m_typeFilter, state = m_stateFilter](const SDItemPtr& item) {
                if (state != ServicesDiscoveryModel::State::STATE_NONE
                    && static_cast<ServicesDiscoveryModel::State>(item->entry->e_state) != state)
                    return false;
                if (filter != ServicesDiscoveryModel::Type::TYPE_NONE
                    && static_cast<ServicesDiscoveryModel::Type>(item->entry->e_type) != filter)
                        return false;
                return item->name.contains(pattern, Qt::CaseInsensitive);
            });
        return items;
    }

public: // data
    MainCtx* m_ctx = nullptr;
    addons_manager_t* m_manager = nullptr;
    SDItemList m_items;
    ServicesDiscoveryModel::Type m_typeFilter = ServicesDiscoveryModel::Type::TYPE_NONE;
    ServicesDiscoveryModel::State m_stateFilter = ServicesDiscoveryModel::State::STATE_NONE;
};

ServicesDiscoveryModel::ServicesDiscoveryModel( QObject* parent )
    : BaseModel( new ServicesDiscoveryModelPrivate(this), parent )
{
}

QVariant ServicesDiscoveryModel::data( const QModelIndex& index, int role ) const
{
    Q_D(const ServicesDiscoveryModel);
    if (!d->m_ctx)
        return {};

    const SDItem* item = d->getItemForRow(index.row());
    if (!item)
        return {};

    switch ( role )
    {
        case Qt::DisplayRole:
        case Role::NAME:
            return item->name;
        case Role::AUTHOR:
            return item->author;
        case Role::SUMMARY:
            return item->summary;
        case Role::DESCRIPTION:
            return item->description;
        case Role::DOWNLOADS:
        {
            vlc_mutex_locker locker{&item->entry->lock};
            return QVariant::fromValue( item->entry->i_downloads );
        }
        case Role::SCORE:
        {
            vlc_mutex_locker locker{&item->entry->lock};
            return item->entry->i_score;
        }
        case Role::STATE:
        {
            vlc_mutex_locker locker{&item->entry->lock};
            return item->entry->e_state;
        }
        case Role::ARTWORK:
        {
            vlc_mutex_locker locker{&item->entry->lock};
            return item->artworkUrl;
        }
        case Role::TYPE:
        {
            vlc_mutex_locker locker{&item->entry->lock};
            return QVariant::fromValue(item->entry->e_type);
        }
        case Role::LINK:
        {
            vlc_mutex_locker locker{&item->entry->lock};
            return QString{ item->entry->psz_source_uri };
        }
        case Role::ADDON_VERSION:
        {
            vlc_mutex_locker locker{&item->entry->lock};
            return qfu(item->entry->psz_version);
        }
        case Role::UUID:
            return item->uuid;
        case Role::DOWNLOAD_COUNT:
        {
            vlc_mutex_locker locker{&item->entry->lock};
            return QVariant::fromValue(item->entry->i_downloads);
        }
        case Role::FILENAME:
        {
            vlc_mutex_locker locker{&item->entry->lock};
            QList<QString> list;
            addon_file_t *p_file;
            ARRAY_FOREACH( p_file, item->entry->files )
            list << qfu( p_file->psz_filename );
            return QVariant{ list };
        }
        case  Role::BROKEN:
        {
            vlc_mutex_locker locker{&item->entry->lock};
            return item->entry->e_flags & ADDON_BROKEN;
        }
        case  Role::MANAGEABLE:
        {
            vlc_mutex_locker locker{&item->entry->lock};
            return item->entry->e_flags & ADDON_MANAGEABLE;
        }
        case  Role::UPDATABLE:
        {
            vlc_mutex_locker locker{&item->entry->lock};
            return item->entry->e_flags & ADDON_UPDATABLE;
        }

        case Qt::ToolTipRole:
        {
            vlc_mutex_locker locker{&item->entry->lock};
            if ( !( item->entry->e_flags & ADDON_MANAGEABLE ) )
            {
                return qtr("This addon has been installed manually. VLC can't manage it by itself.");
            }
            return QVariant{};
        }
        case Qt::DecorationRole:
            return QPixmap{QQmlFile::urlToLocalFileOrQrc(item->artworkUrl)};
        default:
            return {};
    }
}

QHash<int, QByteArray> ServicesDiscoveryModel::roleNames() const
{
    return {
        { Role::NAME, "name" },
        { Role::AUTHOR, "author"},
        { Role::SUMMARY, "summary" },
        { Role::DESCRIPTION, "description" },
        { Role::DOWNLOADS, "downloads" },
        { Role::SCORE, "score" },
        { Role::STATE, "state" },
        { Role::ARTWORK, "artwork" },
        { Role::TYPE, "type" },
        { Role::LINK, "link" },
        { Role::FILENAME, "filename" },
        { Role::ADDON_VERSION, "version" },
        { Role::UUID, "uuid" },
        { Role::DOWNLOAD_COUNT, "downloadCount" },
        { Role::BROKEN, "broken" },
        { Role::MANAGEABLE, "manageable" },
        { Role::UPDATABLE, "updatable" },
    };
}

void ServicesDiscoveryModel::installService(int idx)
{
    Q_D(ServicesDiscoveryModel);

    const SDItem* item = d->getItemForRow(idx);
    if (!item)
        return;

    addon_uuid_t uuid;
    assert(sizeof(uuid) == item->uuid.size());
    memcpy( uuid, item->uuid.constData(), sizeof( uuid ) );
    addons_manager_Install( d->m_manager, uuid );
}

void ServicesDiscoveryModel::removeService(int idx)
{
    Q_D(ServicesDiscoveryModel);

    const SDItem* item = d->getItemForRow(idx);
    if (!item)
        return;

    addon_uuid_t uuid;
    assert(sizeof(uuid) == item->uuid.size());
    memcpy( uuid, item->uuid.constData(), sizeof( uuid ) );
    addons_manager_Remove( d->m_manager, uuid );
}

void ServicesDiscoveryModel::loadFromDefaultRepository()
{
    Q_D(ServicesDiscoveryModel);
    d->gatherRepository("repo://");
}

void ServicesDiscoveryModel::loadFromExternalRepository(QUrl uri)
{
    Q_D(ServicesDiscoveryModel);
    d->gatherRepository(uri.toEncoded().constData());
}

void ServicesDiscoveryModel::setCtx(MainCtx* ctx)
{
    Q_D(ServicesDiscoveryModel);

    if (ctx == d->m_ctx)
        return;

    assert(ctx);
    d->m_ctx = ctx;
    d->initializeModel();
    emit ctxChanged();
}

MainCtx* ServicesDiscoveryModel::getCtx() const
{
    Q_D(const ServicesDiscoveryModel);
    return d->m_ctx;
}

ServicesDiscoveryModel::Type ServicesDiscoveryModel::getTypeFilter() const
{
    Q_D(const ServicesDiscoveryModel);
    return d->m_typeFilter;
}

void ServicesDiscoveryModel::setTypeFilter(ServicesDiscoveryModel::Type type)
{
    Q_D(ServicesDiscoveryModel);
    if (type == d->m_typeFilter)
        return;
    d->m_typeFilter = type;

    d->m_revision++;
    invalidateCache();

    emit typeFilterChanged();
}

ServicesDiscoveryModel::State ServicesDiscoveryModel::getStateFilter() const
{
    Q_D(const ServicesDiscoveryModel);
    return d->m_stateFilter;
}

void ServicesDiscoveryModel::setStateFilter(ServicesDiscoveryModel::State state)
{
    Q_D(ServicesDiscoveryModel);
    if (state == d->m_stateFilter)
        return;
    d->m_stateFilter = state;

    d->m_revision++;
    invalidateCache();

    emit stateFilterChanged();
}

int ServicesDiscoveryModel::getMaxScore()
{
    return ADDON_MAX_SCORE;
}

static void addonFoundCallback( addons_manager_t *manager, addon_entry_t *entry )
{
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
    ServicesDiscoveryModelPrivate* d = (ServicesDiscoveryModelPrivate*) manager->owner.sys;
    QMetaObject::invokeMethod( d->q_func(), [d, entryPtr = AddonPtr(entry)]()
        {
            d->addonChanged( std::move( entryPtr ) );
        }, Qt::QueuedConnection);
}

bool ServicesDiscoveryModelPrivate::initializeModel()
{
    Q_Q(ServicesDiscoveryModel);
    if (m_qmlInitializing || !m_ctx)
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

    m_manager = addons_manager_New( VLC_OBJECT( m_ctx->getIntf() ), &owner );
    assert( m_manager );

    emit q->loadingChanged();
    addons_manager_LoadCatalog( m_manager );

    m_revision++;
    m_loading = false;
    emit q->loadingChanged();
    return true;
}


void ServicesDiscoveryModelPrivate::addonFound( AddonPtr addon )
{
    m_items.emplace_back(std::make_shared<SDItem>(addon));
}

void ServicesDiscoveryModelPrivate::addonChanged( AddonPtr addon )
{
    SDItemPtr sdItem = std::make_shared<SDItem>(addon);
    m_cache->updateItem(std::move(sdItem));

    m_revision++;
    invalidateCache();
}

void ServicesDiscoveryModelPrivate::discoveryEnded()
{
    Q_Q(ServicesDiscoveryModel);
    assert( m_loading );
    m_loading = false;
    emit q->loadingChanged();
    m_revision++;
    invalidateCache();
}
