/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

#include "devicesourceprovider.hpp"
#include "networkmediamodel.hpp"
#include "maininterface/mainctx.hpp"
#include "medialibrary/mlthreadpool.hpp"

//handle discovery events from the media source provider
struct MediaSourceModel::ListenerCb : public MediaTreeListener::MediaTreeListenerCb {
    ListenerCb(MediaSourceModel* model, MediaSourcePtr& mediaSource)
        : m_model(model)
        , m_mediaSource(mediaSource)
    {}

    inline void onItemPreparseEnded( MediaTreePtr, input_item_node_t *, int ) override final {}

    void onItemCleared( MediaTreePtr tree, input_item_node_t* node ) override
    {
        if (node != &tree->root)
            return;

        refresh( node->pp_children, node->i_children, true);
    }

    void onItemAdded( MediaTreePtr tree, input_item_node_t* parent, input_item_node_t *const children[], size_t count ) override
    {
        if (parent != &tree->root)
            return;

        refresh( children, count, false );
    }

    void onItemRemoved( MediaTreePtr tree, input_item_node_t * node, input_item_node_t *const children[], size_t count ) override
    {
        if (node != &tree->root)
            return;

        std::vector<SharedInputItem> itemList;

        itemList.reserve( count );
        for ( auto i = 0u; i < count; ++i )
            itemList.emplace_back( children[i]->p_item );

        QMetaObject::invokeMethod(m_model, [model = this->m_model,
                                  itemList = std::move(itemList),
                                  mediaSource = this->m_mediaSource]()
        {
            model->removeItems(itemList, mediaSource);
        });
    }

    void refresh(input_item_node_t * const children[], size_t count,
                 bool clear)
    {
        std::vector<SharedInputItem> itemList;

        itemList.reserve(count);
        for (size_t i = 0; i < count; i++)
            itemList.emplace_back(children[i]->p_item);

        QMetaObject::invokeMethod(m_model, [model = this->m_model,
                                  itemList = std::move(itemList),
                                  mediaSource = this->m_mediaSource, clear]()
        {
            model->addItems(itemList, mediaSource, clear);
        });
    }

    MediaSourceModel* m_model;
    MediaSourcePtr m_mediaSource;
};

MediaSourceModel::MediaSourceModel(MediaSourcePtr& mediaSource)
    : m_mediaSource(mediaSource)
{
}

MediaSourceModel::~MediaSourceModel()
{
    //reset the listenner before the source
    m_listenner.reset();

    for (const SharedInputItem & media : m_medias)
        emit mediaRemoved(media);
    m_medias.clear();

    m_mediaSource.reset();
}

void MediaSourceModel::init()
{
    assert(m_mediaSource);

    if (m_listenner)
        return;

    m_listenner = std::make_unique<MediaTreeListener>(
        MediaTreePtr{ m_mediaSource->tree },
        std::make_unique<MediaSourceModel::ListenerCb>(this, m_mediaSource)
        );
}

const std::vector<SharedInputItem>& MediaSourceModel::getMedias() const
{
    return m_medias;
}

QString MediaSourceModel::getDescription() const
{
    return qfu(m_mediaSource->description);
}

MediaTreePtr MediaSourceModel::getTree() const
{
    return MediaTreePtr(m_mediaSource->tree);
}

void MediaSourceModel::addItems(const std::vector<SharedInputItem> &inputList,
                                    const MediaSourcePtr &mediaSource, const bool clear)
{
    if (mediaSource != m_mediaSource)
    {
        qWarning() << "unexpected media source";
        return;
    }

    if (clear)
    {
        for (const SharedInputItem & media : m_medias)
            emit mediaRemoved(media);
        m_medias.clear();
    }

    for (const SharedInputItem & inputItem : inputList)
    {
        auto it = std::find(
            m_medias.cbegin(), m_medias.cend(),
            inputItem
        );
        if (it != m_medias.end())
            continue;

        emit mediaAdded(inputItem);
        m_medias.push_back(std::move(inputItem));
    }
}

void MediaSourceModel::removeItems(const std::vector<SharedInputItem> &inputList,
                                       const MediaSourcePtr &mediaSource)
{
    if (mediaSource != m_mediaSource)
    {
        qWarning() << "unexpected media source";
        return;
    }

    for (const SharedInputItem& inputItem : inputList)
    {
        auto it = std::remove(m_medias.begin(), m_medias.end(), inputItem);
        if (it != m_medias.end())
        {
            m_medias.erase(it);
            mediaRemoved(inputItem);
        }
    }
}

SharedMediaSourceModel MediaSourceCache::getMediaSourceModel(vlc_media_source_provider_t* provider, const char* name)
{
    //MediaSourceCache may be accessed by multiple threads
    QMutexLocker lock{&m_mutex};

    QString key = qfu(name);
    auto it = m_cache.find(key);
    if (it != m_cache.end())
    {
        SharedMediaSourceModel ref = it->second.toStrongRef();
        if (ref)
            return ref;
    }
    MediaSourcePtr mediaSource(
        vlc_media_source_provider_GetMediaSource(provider, name),
        false );

    if (!mediaSource)
        return nullptr;

    SharedMediaSourceModel item = SharedMediaSourceModel::create(mediaSource);
    m_cache[key] = item;
    return item;
}

DeviceSourceProvider::DeviceSourceProvider(
    NetworkDeviceModel::SDCatType sdSource,
    const QString &sourceName,
    MainCtx* ctx,
    QObject* parent)
    : QObject(parent)
    , m_ctx(ctx)
    , m_sdSource {sdSource}
    , m_sourceName {sourceName}
{
}

DeviceSourceProvider::~DeviceSourceProvider()
{
    if (m_taskId != 0)
        m_ctx->threadRunner()->cancelTask(this, m_taskId);
}

void DeviceSourceProvider::init()
{
    using SourceMetaPtr = std::unique_ptr<vlc_media_source_meta_list_t,
                                          decltype( &vlc_media_source_meta_list_Delete )>;

    struct Ctx {
        bool success = false;
        QString name;
        std::vector<SharedMediaSourceModel> sources;
    };
    QThread* thread = QThread::currentThread();
    m_taskId = m_ctx->threadRunner()->runOnThread<Ctx>(
        this,
        //Worker thread
        [intf = m_ctx->getIntf(), sdSource = m_sdSource, nameFilter = m_sourceName, thread](Ctx& ctx){
            auto libvlc = vlc_object_instance(intf);

            auto provider = vlc_media_source_provider_Get( libvlc );
            SourceMetaPtr providerList( vlc_media_source_provider_List(
                                           provider,
                                           static_cast<services_discovery_category_e>(sdSource) ),
                                       &vlc_media_source_meta_list_Delete );

            if (!providerList)
                return;

            size_t nbProviders = vlc_media_source_meta_list_Count( providerList.get() );

            for ( size_t i = 0u; i < nbProviders; ++i )
            {
                auto meta = vlc_media_source_meta_list_Get( providerList.get(), i );
                const QString sourceName = qfu( meta->name );
                if ( nameFilter != '*' && nameFilter != sourceName )
                    continue;

                SharedMediaSourceModel mediaSource = MediaSourceCache::getInstance()->getMediaSourceModel(provider, meta->name);

                if (!mediaSource)
                    continue;

                //ensure this QObject don't live in the worker thread
                mediaSource->moveToThread(thread);

                ctx.name += ctx.name.isEmpty() ? qfu( meta->longname ) : ", " + qfu( meta->longname );
                ctx.sources.push_back(mediaSource);
            }
        },
        //UI thread
        [this](quint64, Ctx& ctx){
            m_name = ctx.name;
            emit nameUpdated( m_name );

            for (auto& mediaSource : ctx.sources) {
                mediaSource->init();
                m_mediaSources.push_back( mediaSource );
            }

            if ( !m_mediaSources.empty() )
                emit itemsUpdated();
            else
                emit failed();
        });
}

const std::vector<SharedMediaSourceModel>& DeviceSourceProvider::getMediaSources() const
{
    return m_mediaSources;
}
