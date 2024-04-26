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


#include "networksourcesmodel.hpp"
#include "networkmediamodel.hpp"

#include "util/locallistbasemodel.hpp"

#include "playlist/media.hpp"
#include "playlist/playlist_controller.hpp"

#include <QQmlFile>

#include <memory>

#include <vlc_media_source.h>
#include <vlc_cxx_helpers.hpp>

namespace {

struct SourceItem
{
    SourceItem()
    {
        isDummy = true;
    }

    explicit SourceItem(const vlc_media_source_meta* meta)
        : isDummy(false)
        , name(qfu(meta->name))
        , longName(qfu(meta->longname))
    {
        if ( name.startsWith( "podcast" ) )
        {
            artworkUrl = QUrl("qrc:///sd/podcast.svg");
        }
        else if ( name.startsWith("lua{") )
        {
            int i_head = name.indexOf( "sd='" ) + 4;
            int i_tail = name.indexOf( '\'', i_head );
            const QString iconName = QString( "qrc:///sd/%1.svg" ).arg( name.mid( i_head, i_tail - i_head ) );
            artworkUrl = QFileInfo::exists( QQmlFile::urlToLocalFileOrQrc(iconName) ) ? QUrl(iconName)
                                                                                          : QUrl("qrc:///sd/network.svg");
        }
    }

    bool isDummy;
    QString name;
    QString longName;
    QUrl artworkUrl;
};

using SourceItemPtr = std::shared_ptr<SourceItem>;
using SourceItemLists = std::vector<SourceItemPtr>;

}

class NetworkSourcesModelPrivate
    : public LocalListBaseModelPrivate<SourceItemPtr>
{
    Q_DECLARE_PUBLIC(NetworkSourcesModel);

public: //Ctor/Dtor
    NetworkSourcesModelPrivate(NetworkSourcesModel* pub)
        : LocalListBaseModelPrivate<SourceItemPtr>(pub)
    {}

public:
    const SourceItem* getItemForRow(int row) const
    {
        const SourceItemPtr* ref = item(row);
        if (ref)
            return ref->get();
        return nullptr;
    }

public: // BaseModelPrivate implementation

    LocalListCacheLoader<SourceItemPtr>::ItemCompare getSortFunction() const override
    {
        if (m_sortOrder == Qt::DescendingOrder)
            return [](const SourceItemPtr& a, const SourceItemPtr& b) {
                //always put our dummy item first
                if (a->isDummy)
                    return true;
                if (b->isDummy)
                    return false;
                return QString::compare(a->name, b->name, Qt::CaseInsensitive) > 0;
            };
        else
            return [](const SourceItemPtr& a, const SourceItemPtr& b) {
                //always put our dummy item first
                if (a->isDummy)
                    return true;
                if (b->isDummy)
                    return false;
                return QString::compare(a->name, b->name, Qt::CaseInsensitive) < 0;
            };
    }

    bool initializeModel() override
    {
        Q_Q(NetworkSourcesModel);
        if (m_qmlInitializing || !q->m_ctx)
            return false;

        auto libvlc = vlc_object_instance(q->m_ctx->getIntf());

        if (!m_items.empty())
            m_items.clear();

        auto provider = vlc_media_source_provider_Get( libvlc );
        //add a dummy item
        m_items.push_back(std::make_shared<SourceItem>());

        using SourceMetaPtr = std::unique_ptr<vlc_media_source_meta_list_t,
                                              decltype( &vlc_media_source_meta_list_Delete )>;

        SourceMetaPtr providerList( vlc_media_source_provider_List( provider, static_cast<services_discovery_category_e>(m_sdSource) ),
                                   &vlc_media_source_meta_list_Delete );
        if ( providerList == nullptr )
            return false;

        auto nbProviders = vlc_media_source_meta_list_Count( providerList.get() );

        for ( auto i = 0u; i < nbProviders; ++i )
        {
            auto meta = vlc_media_source_meta_list_Get( providerList.get(), i );
            SourceItemPtr item = std::make_shared<SourceItem>(meta);
            m_items.push_back( item );
        }
        //model is never udpdated but this is required to fit the LocalListBaseModelPrivate model
        ++m_revision;
        m_loading = false;
        emit q->loadingChanged();
        return true;
    }

public: // LocalListCacheLoader::ModelSource implementation

    std::vector<SourceItemPtr> getModelData(const QString& pattern) const override
    {
        if (pattern.isEmpty())
            return m_items;

        std::vector<SourceItemPtr> items;
        std::copy_if(
            m_items.cbegin(), m_items.cend(),
            std::back_inserter(items),
            [&pattern](const SourceItemPtr& item){
                return item->name.contains(pattern, Qt::CaseInsensitive);
            });
        return items;
    }

public: // Data
    services_discovery_category_e m_sdSource = services_discovery_category_e::SD_CAT_INTERNET;
    std::vector<SourceItemPtr> m_items;
};

// ListCache specialisation

template<>
bool ListCache<SourceItemPtr>::compareItems(const SourceItemPtr& a, const SourceItemPtr& b)
{
    //just compare the pointers here
    return a == b;
}

NetworkSourcesModel::NetworkSourcesModel( QObject* parent )
    : BaseModel( new NetworkSourcesModelPrivate(this), parent )
{
}

QVariant NetworkSourcesModel::data( const QModelIndex& index, int role ) const
{
    Q_D(const NetworkSourcesModel);
    if (!m_ctx)
        return {};

    const SourceItem* item = d->getItemForRow(index.row());
    if (!item)
        return {};

    switch ( role )
    {
    case SOURCE_NAME:
        return item->name;
    case SOURCE_LONGNAME:
        return item->longName;
    case SOURCE_TYPE:
        return item->isDummy ? TYPE_DUMMY : TYPE_SOURCE;
    case SOURCE_ARTWORK:
        return item->artworkUrl;
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

void NetworkSourcesModel::setCtx(MainCtx* ctx)
{
    Q_D(NetworkSourcesModel);
    if (ctx == m_ctx)
        return;

    m_ctx = ctx;
    d->initializeModel();
    emit ctxChanged();
}
