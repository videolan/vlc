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

#include <cassert>
#include "medialib.hpp"
#include <vlc_cxx_helpers.hpp>

#include "util/listcache.hpp"

// MediaLibrary includes
#include "mlbasemodel.hpp"
#include "mlhelper.hpp"

#include "util/base_model_p.hpp"
#include "util/asynctask.hpp"

#include <QQmlEngine>


/// MLListCache

using MLListCache = ListCache<std::unique_ptr<MLItem>>;

template<>
bool MLListCache::compareItems(const ItemType& a, const ItemType& b)
{
    return a->getId() == b->getId();
}

static constexpr ssize_t COUNT_UNINITIALIZED = MLListCache::COUNT_UNINITIALIZED;

/// MLBaseModelPrivate

class MLBaseModelPrivate : public BaseModelPrivateT<std::unique_ptr<MLItem>>
{
    Q_DECLARE_PUBLIC(MLBaseModel)

private:
    using Parent = BaseModelPrivateT<std::unique_ptr<MLItem>>;

public:
    MLBaseModelPrivate(MLBaseModel* pub)
        : Parent(pub)
    {}

    void validateCache() const override
    {
        Q_Q(const MLBaseModel);

        // this will cancel all queued up loadItemsTask and related ML specific operations
        q->m_itemLoader = nullptr;

        Parent::validateCache();
    }

    std::unique_ptr<ListCacheLoader<std::unique_ptr<MLItem>>> createLoader() const override
    {
        Q_Q(const MLBaseModel);
        return q->createMLLoader();
    }

    bool cachable() const override {
        Q_Q(const MLBaseModel);
        return Parent::cachable() && q->m_mediaLib;
    }

    bool loading() const override
    {
        Q_Q(const MLBaseModel);
        return Parent::loading() || !q->m_mediaLib;
    }

    bool initializeModel() override
    {
        Q_Q(MLBaseModel);
        if (m_qmlInitializing || !q->m_mediaLib)
            return false;

        if ( q->m_ml_event_handle == nullptr )
            q->m_ml_event_handle.reset( q->m_mediaLib->registerEventListener(MLBaseModel::onVlcMlEvent, q ) );

        invalidateCache();
        return true;
    }

    //this will load data until id is in the cache, then returns its position
    void getIndexFromIdImpl(MLItemId id, std::function<void (std::optional<int> index)> cb) const
    {
        Q_Q(const MLBaseModel);

        int index;
        MLItem* item = q->findInCache(id, &index);

        if (item) {
            cb(index);
        }
        else
        {
            unsigned int loaded = q->getLoadedCount();
            int count = q->getCount();
            int limit = q->getLimit();
            //item doesn't exists
            if ((limit != 0 && count >= limit) || loaded == getMaximumCount()) {
                cb(std::nullopt);
            }
            else
            {
                //load data until we have our item in cache
                QObject::connect(q, &MLBaseModel::dataChanged, q, [this, id, cb](){
                    getIndexFromIdImpl(id, cb);
                }, Qt::SingleShotConnection);

                if (m_cache)
                    m_cache->fetchMore();
            }
        }
    }

    QJSValue itemToJSValue(const MLItem* item) const {
        Q_Q(const MLBaseModel);
        if (!item)
            return QJSValue::NullValue;

        QMap<QString, QVariant> dataDict;
        const QHash<int, QByteArray> roles = q->roleNames();
        if (item) // item may fail to load
            for (int role: roles.keys())
                dataDict[roles[role]] = q->itemRoleData(item, role);
        auto jsEngine = qjsEngine(q);
        return jsEngine->toScriptValue(dataDict);
    }

    //FIXME: building Promise from C++ should be available to other classes
    std::tuple<QJSValue, QJSValue, QJSValue> makeJSPromise() const
    {
        Q_Q(const MLBaseModel);
        auto jsEngine = qjsEngine(q);
        QJSValue promiseRet = jsEngine->evaluate(
            //no newline at beginning of string, () surrounding is mandatory
            R"raw((function() {
    let resolve;
    let reject;
    let p = new Promise((_resolve, _reject) => {
        resolve = _resolve;
        reject = _reject;
    });
    return [p, resolve, reject];
}())
)raw");
        assert(promiseRet.isArray());
        assert(promiseRet.property(QStringLiteral("length")).toInt() == 3);
        QJSValue p = promiseRet.property(0);
        QJSValue resolve = promiseRet.property(1);
        assert(resolve.isCallable());
        QJSValue reject = promiseRet.property(2);
        assert(reject.isCallable());
        return {p, resolve, reject};
    }
};

// MLBaseModel

MLBaseModel::MLBaseModel(QObject *parent)
    : BaseModel(new MLBaseModelPrivate(this), parent)
    , m_ml_event_handle( nullptr, [this](vlc_ml_event_callback_t* cb ) {
            assert( m_mediaLib != nullptr );
            m_mediaLib->unregisterEventListener( cb );
        })
{
    connect( this, &MLBaseModel::resetRequested, this, &MLBaseModel::onResetRequested );

    connect( this, &MLBaseModel::mlChanged, this, &MLBaseModel::loadingChanged );
    connect( this, &MLBaseModel::countChanged, this, &MLBaseModel::loadingChanged );
    connect( this, &MLBaseModel::favoriteOnlyChanged, this, &MLBaseModel::resetRequested );
}

/* For std::unique_ptr, see Effective Modern C++, Item 22 */
MLBaseModel::~MLBaseModel() = default;

void MLBaseModel::sortByColumn(QByteArray criteria, Qt::SortOrder order)
{
    Q_D(MLBaseModel);
    if (d->m_sortOrder == order && d->m_sortCriteria == criteria)
        return;

    d->m_sortOrder = order;
    d->m_sortCriteria = criteria;
    emit sortOrderChanged();
    emit sortCriteriaChanged();
    resetCache();
}

quint64 MLBaseModel::loadItems(const QVector<int> &indexes, MLBaseModel::ItemCallback cb)
{
    Q_D(MLBaseModel);
    if (!m_itemLoader)
        m_itemLoader = createMLLoader();

    return m_itemLoader->loadItemsTask(d->m_offset, indexes, cb);
}

void MLBaseModel::getData(const QModelIndexList &indexes, QJSValue callback)
{
    if (!callback.isCallable()) // invalid argument
        return;

    QVector<int> indx;
    std::transform(indexes.begin(), indexes.end()
                   , std::back_inserter(indx)
                   , std::mem_fn(&QModelIndex::row));

    getDataFlat(indx, callback);
}

void MLBaseModel::getDataFlat(const QVector<int> &indexes, QJSValue callback)
{
    if (!callback.isCallable()) // invalid argument
        return;

    std::shared_ptr<quint64> requestId = std::make_shared<quint64>();

    ItemCallback cb = [this, indxSize = indexes.size(), callback, requestId]
    (quint64 id, std::vector<std::unique_ptr<MLItem>> &items) mutable
    {
        auto jsEngine = qjsEngine(this);
         if (!jsEngine || *requestId != id)
            return;

        assert((int)items.size() == indxSize);

        const QHash<int, QByteArray> roles = roleNames();
        auto jsArray = jsEngine->newArray(indxSize);

        for (int i = 0; i < indxSize; ++i)
        {
            const auto &item = items[i];

            QJSValue jsitem;
            if (item) // item may fail to load
                jsitem = d_func()->itemToJSValue(item.get());
            else
                jsitem = jsEngine->newObject();


            jsArray.setProperty(i, jsitem);
        }

        callback.call({jsArray});
    };

    *requestId = loadItems(indexes, cb);
}


Q_INVOKABLE QJSValue MLBaseModel::getDataById(MLItemId id)
{
    Q_D(const MLBaseModel);

    auto [p, resolve, reject] = d->makeJSPromise();

    MLItem* item = findInCache(id, nullptr);
    if (item)
    {
        resolve.call({d->itemToJSValue(item)});
    }
    else
    {
        if (!m_itemLoader)
            m_itemLoader = createMLLoader();

        m_itemLoader->loadItemByIdTask(
            id,
            [this, resolve=std::move(resolve), reject=std::move(reject)](size_t taskId, MLListCache::ItemType&& item)
            {
                Q_UNUSED(taskId);

                if (!item)
                {
                    reject.call();
                    return;
                }

                resolve.call({d_func()->itemToJSValue(item.get())});
            }
        );
    }
    return p;
}


Q_INVOKABLE QJSValue MLBaseModel::getIndexFromId(MLItemId id)
{
    Q_D(const MLBaseModel);

    auto [p, resolve, reject] = d->makeJSPromise();
    auto cb = [resolve = resolve, reject = reject](std::optional<int> index)
    {
        if (index.has_value())
            resolve.call({*index});
        else
            reject.call(); //not present
    };

    getIndexFromId2(id, cb);
    return p;
}

void MLBaseModel::getIndexFromId2(MLItemId id, std::function<void (std::optional<int>)> cb)
{
    Q_D(const MLBaseModel);

    d->getIndexFromIdImpl(id, cb);
}

QVariant MLBaseModel::data(const QModelIndex &index, int role) const
{
    Q_D(const MLBaseModel);
    const std::unique_ptr<MLItem>* mlItem = d->item(index.row());
    if (mlItem && *mlItem)
        return itemRoleData(mlItem->get(), role);

    return {};
}

void MLBaseModel::addAndPlay(const QModelIndexList &list, const QStringList &options)
{
    QVector<int> indx;
    std::transform(list.begin(), list.end(), std::back_inserter(indx), std::mem_fn(&QModelIndex::row));

    ItemCallback play = [this, options](quint64, std::vector<std::unique_ptr<MLItem>> &items)
    {
        if (!m_mediaLib)
            return;

        QVariantList ids;
        std::transform(items.begin(), items.end()
                       , std::back_inserter(ids)
                       , [](const std::unique_ptr<MLItem> &item) { return item ? QVariant::fromValue(item->getId()) : QVariant {}; });

        m_mediaLib->addAndPlay(ids, options);
    };

    loadItems(indx, play);
}

vlc_ml_sorting_criteria_t MLBaseModel::getMLSortingCriteria() const
{
    Q_D(const MLBaseModel);
    return nameToCriteria(d->m_sortCriteria.toUtf8());
}

//-------------------------------------------------------------------------------------------------

void MLBaseModel::onVlcMlEvent(const MLEvent &event)
{
    Q_D(MLBaseModel);
    switch(event.i_type)
    {
        case VLC_ML_EVENT_BACKGROUND_IDLE_CHANGED:
            if ( event.background_idle_changed.b_idle && m_need_reset )
            {
                emit resetRequested();
                m_need_reset = false;
            }
            break;
        case VLC_ML_EVENT_MEDIA_THUMBNAIL_GENERATED:
        {
            if (event.media_thumbnail_generated.b_success) {
                if (!d->m_cache)
                    break;

                ssize_t stotal = d->m_cache->queryCount();
                if (stotal == COUNT_UNINITIALIZED)
                    break;

                int row = 0;

                /* Only consider items available locally in cache */
                MLItemId itemId{event.media_thumbnail_generated.i_media_id, VLC_ML_PARENT_UNKNOWN};
                MLItem* item = findInCache(itemId, &row);
                if (item)
                {
                    vlc_ml_thumbnail_status_t status = VLC_ML_THUMBNAIL_STATUS_FAILURE;
                    QString mrl;
                    if (event.media_thumbnail_generated.b_success)
                    {
                        mrl = qfu(event.media_thumbnail_generated.psz_mrl);
                        status = event.media_thumbnail_generated.i_status;
                    }
                    thumbnailUpdated(index(row), item, mrl, status);
                }
            }
            break;
        }
    }

    if (m_mediaLib && m_mediaLib->idle() && m_need_reset)
    {
        emit resetRequested();
        m_need_reset = false;
    }
}

void MLBaseModel::onVlcMlEvent(void* data, const vlc_ml_event_t* event)
{
    auto self = static_cast<MLBaseModel*>(data);
    //MLEvent is not copiable, but lambda needs to be copiable
    auto  mlEvent = std::make_shared<MLEvent>(event);
    QMetaObject::invokeMethod(self, [self, mlEvent] () mutable {
        self->onVlcMlEvent(*mlEvent);
    });
}

MLItemId MLBaseModel::parentId() const
{
    return m_parent;
}

void MLBaseModel::setParentId(MLItemId parentId)
{
    m_parent = parentId;
    resetCache();
    m_itemLoader.reset();
    emit parentIdChanged();
}

void MLBaseModel::unsetParentId()
{
    setParentId({});
}

MediaLib* MLBaseModel::ml() const
{
    return m_mediaLib;
}

void MLBaseModel::setMl(MediaLib* medialib)
{
    Q_D(MLBaseModel);
    assert(medialib);

    if (m_mediaLib == medialib)
        return;

    m_mediaLib = medialib;
    d->initializeModel();
    mlChanged();
}

bool MLBaseModel::getFavoriteOnly() const
{
    return m_favoriteOnly;
}

void MLBaseModel::setFavoriteOnly(const bool favoriteOnly)
{
    if (m_favoriteOnly == favoriteOnly)
        return;
    m_favoriteOnly = favoriteOnly;
    emit favoriteOnlyChanged();
}

MLItem *MLBaseModel::item(int signedidx) const
{
    Q_D(const MLBaseModel);
    if (!d->m_cache)
        return nullptr;

    ssize_t count = d->m_cache->queryCount();

    if (count == 0 || signedidx < 0 || signedidx >= count)
        return nullptr;

    unsigned int idx = static_cast<unsigned int>(signedidx);

    d->m_cache->refer(idx);

    const std::unique_ptr<MLItem> *item = d->m_cache->get(idx);

    if (!item)
        /* Not in cache */
        return nullptr;

    /* Return raw pointer */
    return item->get();
}

MLItem *MLBaseModel::itemCache(int signedidx) const
{
    Q_D(const MLBaseModel);
    unsigned int idx = static_cast<unsigned int>(signedidx);

    if (!d->m_cache)
        return nullptr;

    const std::unique_ptr<MLItem> *item = d->m_cache->get(idx);

    if (!item)
        /* Not in cache */
        return nullptr;

    /* Return raw pointer */
    return item->get();
}

MLItem *MLBaseModel::findInCache(const MLItemId& id, int *index) const
{
    Q_D(const MLBaseModel);
    if (!d->m_cache)
        return nullptr;

    const auto item = d->m_cache->find([id](const auto &item)
    {
        return item->getId() == id;
    }, index);

    return item ? item->get() : nullptr;
}

void MLBaseModel::updateItemInCache(const MLItemId&)
{
    // we can't safely update the item in the cache because our view may have a filter
    // and the update may cause the item filtered state to change requiring it to enter/leave
    // the cache
    emit resetRequested();
    return;
}

void MLBaseModel::deleteItemInCache(const MLItemId& mlid)
{
    Q_D(MLBaseModel);
    if (!d->m_cache)
    {
        emit resetRequested();
        return;
    }
    d->m_cache->deleteItem([mlid](const MLListCache::ItemType& item){
        return item->getId() == mlid;
    });
}


void MLBaseModel::insertItemInCache(std::unique_ptr<MLItem> mlItem, int position)
{
    Q_D(MLBaseModel);
    if (!d->m_cache)
    {
        emit resetRequested();
        return;
    }
    d->m_cache->insertItem(std::move(mlItem), position);
}

void MLBaseModel::insertItemListInCache(std::vector<std::unique_ptr<MLItem>>&& items, int position)
{
    Q_D(MLBaseModel);
    if (!d->m_cache)
    {
        emit resetRequested();
        return;
    }
    d->m_cache->insertItemList(items.begin(), items.end(), position);
}

void MLBaseModel::moveRangeInCache(int first, int last, int to)
{
    Q_D(MLBaseModel);
    if (!d->m_cache)
    {
        emit resetRequested();
        return;
    }
    d->m_cache->moveRange(first, last, to);
}

void MLBaseModel::deleteRangeInCache(int first, int last)
{
    Q_D(MLBaseModel);
    if (!d->m_cache)
    {
        emit resetRequested();
        return;
    }
    d->m_cache->deleteRange(first, last);
}

//-------------------------------------------------------------------------------------------------

MLListCacheLoader::MLListCacheLoader(MediaLib* medialib, std::shared_ptr<MLListCacheLoader::MLOp> op, QObject* parent)
    : QObject(parent)
    , m_medialib(medialib)
    , m_op(op)
{
}

void MLListCacheLoader::cancelTask(size_t taskId)
{
    m_medialib->cancelMLTask(this, taskId);
}

size_t MLListCacheLoader::countTask(std::function<void(size_t taskId, size_t count)> cb)
{
    struct Ctx {
        size_t count;
    };

    return m_medialib->runOnMLThread<Ctx>(this,
        //ML thread
        [op = m_op]
        (vlc_medialibrary_t* ml, Ctx& ctx) {
            auto query = op->getQueryParams();
            ctx.count = op->count(ml, &query);
        },
        //UI thread
        [cb](quint64 taskId, Ctx& ctx)
        {
            cb(taskId,  ctx.count);
        });
}

size_t MLListCacheLoader::loadTask(size_t offset, size_t limit,
    std::function<void (size_t, std::vector<ItemType>&)> cb)
{
    struct Ctx {
        std::vector<MLListCacheLoader::ItemType> list;
    };

    return m_medialib->runOnMLThread<Ctx>(this,
        //ML thread
        [op = m_op, offset, limit]
        (vlc_medialibrary_t* ml, Ctx& ctx)
        {
            auto query = op->getQueryParams(offset, limit);
            ctx.list = op->load(ml, &query);
        },
        //UI thread
        [cb](quint64 taskId, Ctx& ctx)
        {
            cb(taskId, ctx.list);
        });
}

size_t MLListCacheLoader::countAndLoadTask(size_t offset, size_t limit,
    std::function<void (size_t, size_t, std::vector<ItemType>&)> cb)
{
    struct Ctx {
        size_t maximumCount;
        std::vector<ItemType> list;
    };

    return m_medialib->runOnMLThread<Ctx>(this,
        //ML thread
        [offset, limit, op = m_op]
        (vlc_medialibrary_t* ml, Ctx& ctx) {
            auto query = op->getQueryParams(offset, limit);
            ctx.list = op->load(ml, &query);
            ctx.maximumCount = op->count(ml, &query);
        },
        //UI thread
        [cb](quint64 taskId, Ctx& ctx) {
            cb(taskId,  ctx.maximumCount, ctx.list);
        });
}

quint64 MLListCacheLoader::loadItemsTask(size_t offset, const QVector<int> &indexes, MLBaseModel::ItemCallback cb)
{
    struct Ctx
    {
        std::vector<std::unique_ptr<MLItem>> items;
    };

    return m_medialib->runOnMLThread<Ctx>(this,
        //ML thread
        [offset, op = m_op, indexes](vlc_medialibrary_t* ml, Ctx& ctx)
        {
            if (indexes.isEmpty())
                return;

            auto sortedIndexes = indexes;
            std::sort(sortedIndexes.begin(), sortedIndexes.end());

            struct Range
            {
                int low, high; // [low, high] (all inclusive)
            };

            QVector<Range> ranges;
            ranges.push_back(Range {sortedIndexes[0], sortedIndexes[0]});
            const int MAX_DIFFERENCE = 4;
            for (const auto index : sortedIndexes)
            {
                if ((index - ranges.back().high) < MAX_DIFFERENCE)
                    ranges.back().high = index;
                else
                    ranges.push_back(Range {index, index});
            }

            ctx.items.resize(indexes.size());

            vlc_ml_query_params_t queryParam = op->getQueryParams();
            for (const auto range : ranges)
            {
                queryParam.i_offset = offset + range.low;
                queryParam.i_nbResults = offset + range.high - range.low + 1;
                auto data = op->load(ml, &queryParam);
                for (int i = 0; i < indexes.size(); ++i)
                {
                    const auto targetIndex = indexes[i];
                    if (targetIndex >= range.low && targetIndex <= range.high)
                    {
                        ctx.items.at(i) = std::move(data.at(targetIndex - range.low));
                    }
                }
            }
        },
        // UI thread
        [cb](quint64 id, Ctx &ctx) {
            cb(id, ctx.items);
        });
}


size_t MLListCacheLoader::loadItemByIdTask(MLItemId itemId, std::function<void (size_t, ItemType&&)> cb) const
{
    struct Ctx {
        ItemType item;
    };
    return m_medialib->runOnMLThread<Ctx>(this,
        //ML thread
        [itemId, op = m_op](vlc_medialibrary_t* ml, Ctx& ctx) {
            ctx.item = op->loadItemById(ml, itemId);
        },
        //UI thread
        [cb](qint64 taskId, Ctx& ctx) {
            if (!ctx.item)
                return;
            cb(taskId, std::move(ctx.item));
        });
}

MLListCacheLoader::MLOp::MLOp(MLItemId parentId, QString searchPattern, vlc_ml_sorting_criteria_t sort, bool sort_desc, bool fav_only)
    : m_parent(parentId)
    , m_searchPattern(searchPattern.toUtf8())
    , m_sort(sort)
    , m_sortDesc(sort_desc)
    , m_favoriteOnly(fav_only)
{
}

vlc_ml_query_params_t MLListCacheLoader::MLOp::getQueryParams(size_t offset, size_t limit) const
{
    vlc_ml_query_params_t params = vlc_ml_query_params_create();
    if (!m_searchPattern.isNull())
        params.psz_pattern = m_searchPattern.constData();
    params.i_nbResults = limit;
    params.i_offset = offset;
    params.i_sort = m_sort;
    params.b_desc = m_sortDesc;
    params.b_favorite_only = m_favoriteOnly;
    return params;
}
