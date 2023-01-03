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

#include "mllistcache.hpp"
#include "util/qmlinputitem.hpp"

// MediaLibrary includes
#include "mlbasemodel.hpp"
#include "mlhelper.hpp"

#include "util/asynctask.hpp"

static constexpr ssize_t COUNT_UNINITIALIZED = MLListCache::COUNT_UNINITIALIZED;

MLBaseModel::MLBaseModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_ml_event_handle( nullptr, [this](vlc_ml_event_callback_t* cb ) {
            assert( m_mediaLib != nullptr );
            m_mediaLib->unregisterEventListener( cb );
        })
{
    connect( this, &MLBaseModel::resetRequested, this, &MLBaseModel::onResetRequested );

    connect( this, &MLBaseModel::mlChanged, this, &MLBaseModel::hasContentChanged );
    connect( this, &MLBaseModel::countChanged, this, &MLBaseModel::hasContentChanged );
}

/* For std::unique_ptr, see Effective Modern C++, Item 22 */
MLBaseModel::~MLBaseModel() = default;

void MLBaseModel::sortByColumn(QByteArray name, Qt::SortOrder order)
{
    vlc_ml_sorting_criteria_t sort = nameToCriteria(name);
    bool desc = (order == Qt::SortOrder::DescendingOrder);
    if (m_sort_desc == desc && m_sort == sort)
        return;

    m_sort_desc = (order == Qt::SortOrder::DescendingOrder);
    m_sort = nameToCriteria(name);
    resetCache();
}

//-------------------------------------------------------------------------------------------------

/* Q_INVOKABLE */ QMap<QString, QVariant> MLBaseModel::getDataAt(const QModelIndex & index)
{
    QMap<QString, QVariant> dataDict;

    QHash<int, QByteArray> roles = roleNames();

    for (int role: roles.keys())
    {
        dataDict[roles[role]] = data(index, role);
    }

    return dataDict;
}

/* Q_INVOKABLE */ QMap<QString, QVariant> MLBaseModel::getDataAt(int idx)
{
    return getDataAt(index(idx));
}

void MLBaseModel::getData(const QModelIndexList &indexes, QJSValue callback)
{
    if (!callback.isCallable()) // invalid argument
        return;

    QVector<int> indx;
    std::transform(indexes.begin(), indexes.end(), std::back_inserter(indx),
    [](const auto &index) {
        return index.row();
    });

    QSharedPointer<BaseLoader> loader{ createLoader().release() };
    struct Ctx {
        std::vector<std::unique_ptr<MLItem>> items;
    };
    m_mediaLib->runOnMLThread<Ctx>(this,
    //ML thread
    [loader, indx](vlc_medialibrary_t* ml, Ctx& ctx){
        if (indx.isEmpty())
            return;

        auto sortedIndexes = indx;
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

        ctx.items.resize(indx.size());
        for (const auto range : ranges)
        {
            auto data = loader->load(ml, range.low, range.high - range.low + 1);
            for (int i = 0; i < indx.size(); ++i)
            {
                const auto targetIndex = indx[i];
                if (targetIndex >= range.low && targetIndex <= range.high)
                {
                    ctx.items.at(i) = std::move(data.at(targetIndex - range.low));
                }
            }
        }

    },
    //UI thread
    [this, indxSize = indx.size(), callback]
    (quint64, Ctx& ctx) mutable
    {
        auto jsEngine = qjsEngine(this);
        if (!jsEngine)
            return;

        assert((int)ctx.items.size() == indxSize);

        const QHash<int, QByteArray> roles = roleNames();
        auto jsArray = jsEngine->newArray(indxSize);

        for (int i = 0; i < indxSize; ++i)
        {
            const auto &item = ctx.items[i];
            QMap<QString, QVariant> dataDict;

            if (item) // item may fail to load
                for (int role: roles.keys())
                    dataDict[roles[role]] = itemRoleData(item.get(), role);

            jsArray.setProperty(i, jsEngine->toScriptValue(dataDict));
        }

        callback.call({jsArray});
    });
}

QVariant MLBaseModel::data(const QModelIndex &index, int role) const
{
    const auto mlItem = item(index.row());
    if (mlItem)
        return itemRoleData(mlItem, role);

    return {};
}

//-------------------------------------------------------------------------------------------------

void MLBaseModel::onResetRequested()
{
    invalidateCache();
}

void MLBaseModel::onLocalSizeChanged(size_t size)
{
    emit countChanged(size);
}

void MLBaseModel::onVlcMlEvent(const MLEvent &event)
{
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
                if (!m_cache)
                    break;

                ssize_t stotal = m_cache->count();
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

QString MLBaseModel::getFirstSymbol(QString str)
{
    QString ret("#");
    if ( str.length() > 0 && str[0].isLetter() )
        ret = str[0].toUpper();
    return ret;
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
    emit parentIdChanged();
}

void MLBaseModel::unsetParentId()
{
    m_parent = MLItemId();
    resetCache();
    emit parentIdChanged();
}

MediaLib* MLBaseModel::ml() const
{
    return m_mediaLib;
}

void MLBaseModel::setMl(MediaLib* medialib)
{
    assert(medialib);

    if (m_mediaLib == medialib)
        return;

    m_mediaLib = medialib;
    if ( m_ml_event_handle == nullptr )
        m_ml_event_handle.reset( m_mediaLib->registerEventListener(onVlcMlEvent, this ) );
    mlChanged();
}

const QString& MLBaseModel::searchPattern() const
{
    return m_search_pattern;
}

void MLBaseModel::setSearchPattern( const QString& pattern )
{
    QString patternToApply = pattern.length() == 0 ? QString{} : pattern;
    if (patternToApply == m_search_pattern)
        /* No changes */
        return;

    m_search_pattern = patternToApply;
    resetCache();
}

Qt::SortOrder MLBaseModel::getSortOrder() const
{
    return m_sort_desc ? Qt::SortOrder::DescendingOrder : Qt::SortOrder::AscendingOrder;
}

void MLBaseModel::setSortOder(Qt::SortOrder order)
{
    bool desc = (order == Qt::SortOrder::DescendingOrder);
    if (m_sort_desc == desc)
        return;
    m_sort_desc = desc;
    resetCache();
    emit sortOrderChanged();
}

const QString MLBaseModel::getSortCriteria() const
{
    return criteriaToName(m_sort);
}

void MLBaseModel::setSortCriteria(const QString& criteria)
{
    vlc_ml_sorting_criteria_t sort = nameToCriteria(qtu(criteria));
    if (m_sort == sort)
        return;
    m_sort = sort;
    resetCache();
    emit sortCriteriaChanged();
}

void MLBaseModel::unsetSortCriteria()
{
    if (m_sort == VLC_ML_SORTING_DEFAULT)
        return;

    m_sort = VLC_ML_SORTING_DEFAULT;
    resetCache();
    emit sortCriteriaChanged();
}

int MLBaseModel::rowCount(const QModelIndex &parent) const
{
    if (!m_mediaLib || parent.isValid())
        return 0;

    validateCache();

    return m_cache->count();
}

QVariant MLBaseModel::getIdForIndex(QVariant index) const
{
    MLItem* obj = nullptr;
    if (index.canConvert<int>())
        obj = item( index.toInt() );
    else if ( index.canConvert<QModelIndex>() )
        obj = item( index.value<QModelIndex>().row() );

    if (!obj)
        return {};

    return QVariant::fromValue(obj->getId());
}

QVariantList MLBaseModel::getIdsForIndexes(const QModelIndexList & indexes) const
{
    QVariantList idList;
    idList.reserve(indexes.length());
    std::transform( indexes.begin(), indexes.end(),std::back_inserter(idList), [this](const QModelIndex& index) -> QVariant {
        MLItem* obj = item( index.row() );
        if (!obj)
            return {};
        return QVariant::fromValue(obj->getId());
    });
    return idList;
}

QVariantList MLBaseModel::getIdsForIndexes(const QVariantList & indexes) const
{
    QVariantList idList;

    idList.reserve(indexes.length());
    std::transform( indexes.begin(), indexes.end(),std::back_inserter(idList),
                    [this](const QVariant& index) -> QVariant {
        MLItem* obj = nullptr;
        if (index.canConvert<int>())
            obj = item( index.toInt() );
        else if ( index.canConvert<QModelIndex>() )
            obj = item( index.value<QModelIndex>().row() );

        if (!obj)
            return {};

        return QVariant::fromValue(obj->getId());
    });
    return idList;
}

//-------------------------------------------------------------------------------------------------

unsigned MLBaseModel::getCount() const
{
    if (!m_mediaLib)
        return 0;
    validateCache();
    if (m_cache->count() == COUNT_UNINITIALIZED)
        return 0;
    return static_cast<unsigned>(m_cache->count());
}


void MLBaseModel::onCacheDataChanged(int first, int last)
{
    emit dataChanged(index(first), index(last));
}

void MLBaseModel::onCacheBeginInsertRows(int first, int last)
{
    emit beginInsertRows({}, first, last);
}

void MLBaseModel::onCacheBeginRemoveRows(int first, int last)
{
    emit beginRemoveRows({}, first, last);
}

void MLBaseModel::onCacheBeginMoveRows(int first, int last, int destination)
{
    emit beginMoveRows({}, first, last, {}, destination);
}

void MLBaseModel::validateCache() const
{
    if (m_cache)
        return;

    if (!m_mediaLib)
        return;

    auto loader = createLoader();
    m_cache = std::make_unique<MLListCache>(m_mediaLib, std::move(loader), false);
    connect(m_cache.get(), &MLListCache::localSizeChanged,
            this, &MLBaseModel::onLocalSizeChanged);

    connect(m_cache.get(), &MLListCache::localDataChanged,
            this, &MLBaseModel::onCacheDataChanged);

    connect(m_cache.get(), &MLListCache::beginInsertRows,
            this, &MLBaseModel::onCacheBeginInsertRows);
    connect(m_cache.get(), &MLListCache::endInsertRows,
            this, &MLBaseModel::endInsertRows);

    connect(m_cache.get(), &MLListCache::beginRemoveRows,
            this, &MLBaseModel::onCacheBeginRemoveRows);
    connect(m_cache.get(), &MLListCache::endRemoveRows,
            this, &MLBaseModel::endRemoveRows);

    connect(m_cache.get(), &MLListCache::endMoveRows,
            this, &MLBaseModel::endMoveRows);
    connect(m_cache.get(), &MLListCache::beginMoveRows,
            this, &MLBaseModel::onCacheBeginMoveRows);

    m_cache->initCount();
}


void MLBaseModel::resetCache()
{
    beginResetModel();
    m_cache.reset();
    endResetModel();
    validateCache();
}

void MLBaseModel::invalidateCache()
{
    if (m_cache)
        m_cache->invalidate();
    else
        validateCache();
}

//-------------------------------------------------------------------------------------------------

MLItem *MLBaseModel::item(int signedidx) const
{
    validateCache();

    if (!m_cache)
        return nullptr;

    ssize_t count = m_cache->count();

    if (count == 0 || signedidx < 0 || signedidx >= count)
        return nullptr;

    unsigned int idx = static_cast<unsigned int>(signedidx);

    m_cache->refer(idx);

    const std::unique_ptr<MLItem> *item = m_cache->get(idx);

    if (!item)
        /* Not in cache */
        return nullptr;

    /* Return raw pointer */
    return item->get();
}

MLItem *MLBaseModel::itemCache(int signedidx) const
{
    unsigned int idx = static_cast<unsigned int>(signedidx);

    if (!m_cache)
        return nullptr;

    const std::unique_ptr<MLItem> *item = m_cache->get(idx);

    if (!item)
        /* Not in cache */
        return nullptr;

    /* Return raw pointer */
    return item->get();
}

MLItem *MLBaseModel::findInCache(const MLItemId& id, int *index) const
{
    const auto item = m_cache->find([id](const auto &item)
    {
        return item->getId() == id;
    }, index);

    return item ? item->get() : nullptr;
}

void MLBaseModel::updateItemInCache(const MLItemId& mlid)
{
    if (!m_cache)
    {
        emit resetRequested();
        return;
    }
    MLItem* item = findInCache(mlid, nullptr);
    if (!item) // items isn't loaded
        return;

    if (!m_itemLoader)
        m_itemLoader = createLoader();
    struct Ctx {
        std::unique_ptr<MLItem> item;
    };
    m_mediaLib->runOnMLThread<Ctx>(this,
    //ML thread
    [mlid, itemLoader = m_itemLoader](vlc_medialibrary_t* ml, Ctx& ctx){
        ctx.item = itemLoader->loadItemById(ml, mlid);
    },
    //UI thread
    [this](qint64, Ctx& ctx) {
        if (!ctx.item)
            return;

        m_cache->updateItem(std::move(ctx.item));
    });
}

void MLBaseModel::deleteItemInCache(const MLItemId& mlid)
{
    if (!m_cache)
    {
        emit resetRequested();
        return;
    }
    m_cache->deleteItem(mlid);
}


void MLBaseModel::moveRangeInCache(int first, int last, int to)
{
    if (!m_cache)
    {
        emit resetRequested();
        return;
    }
    m_cache->moveRange(first, last, to);
}

void MLBaseModel::deleteRangeInCache(int first, int last)
{
    if (!m_cache)
    {
        emit resetRequested();
        return;
    }
    m_cache->deleteRange(first, last);
}

//-------------------------------------------------------------------------------------------------

MLBaseModel::BaseLoader::BaseLoader(MLItemId parent, QString searchPattern,
                                    vlc_ml_sorting_criteria_t sort, bool sort_desc)
    : m_parent(parent)
    , m_searchPattern(searchPattern)
    , m_sort(sort)
    , m_sort_desc(sort_desc)
{
}

MLBaseModel::BaseLoader::BaseLoader(const MLBaseModel &model)
    : BaseLoader(model.m_parent, model.m_search_pattern, model.m_sort, model.m_sort_desc)
{
}

MLQueryParams MLBaseModel::BaseLoader::getParams(size_t index, size_t count) const
{
    return { m_searchPattern.toUtf8(), m_sort, m_sort_desc, index, count };
}

bool MLBaseModel::hasContent() const
{
    return m_mediaLib && (getCount() > 0);
}
