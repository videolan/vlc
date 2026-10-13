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


#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "../util/base_model.hpp"
#include "../util/base_model_p.hpp"
#include "../util/locallistcacheloader.hpp"

#include <algorithm>
#include <deque>
#include <functional>
#include <memory>
#include <vector>

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>


struct Item
{
    int id;
    Item(int i) : id {i} {}
};

using ItemPtr = std::shared_ptr<Item>;
using ItemLoader = LocalListCacheLoader<ItemPtr>;

template<>
bool ListCache<ItemPtr>::compareItems(const ItemPtr& a, const ItemPtr& b)
{
    //just compare the pointers here
    return a == b;
}

class ModelPrivate;

class Model : public BaseModel
{
    Q_DECLARE_PRIVATE(Model);

public:
    Model(QObject *parent = nullptr);

    QVariant data(const QModelIndex & index, int role) const override;

    void append(int id);

private:
};

class ModelPrivate
        : public BaseModelPrivateT<ItemPtr>
        , public LocalListCacheLoader<ItemPtr>::ModelSource
{
    Q_DECLARE_PUBLIC(Model)
public:
    ModelPrivate(Model* pub)
        : BaseModelPrivateT<ItemPtr>(pub)
    {}

    ItemLoader::ItemCompare getSortFunction() const
    {
        return [](const ItemPtr& l, const ItemPtr& r) -> bool
        {
            return l->id < r->id;
        };
    }

    std::unique_ptr<ListCacheLoader<ItemPtr>> createLoader() const override
    {
        return std::make_unique<ItemLoader>(this, m_searchPattern, getSortFunction());
    }

    bool initializeModel() override
    {
        return true;
    }

public: //LocalListCacheLoader::ModelSource implementation
    size_t getModelRevision() const override
    {
        return rev;
    }

    std::vector<ItemPtr> getModelData(const QString& pattern) const override
    {
        Q_UNUSED(pattern);
        return m_items;
    }

private:
    size_t rev = 0;
    std::vector<ItemPtr> m_items;
};

Model::Model(QObject *parent)
    : BaseModel(new ModelPrivate(this), parent)
{
}

QVariant Model::data(const QModelIndex &index, int role) const
{
    Q_D(const Model);
    Q_UNUSED(role);

    const ItemPtr *item = d->item(index.row());
    if (!item)
        return {};

    return (*item)->id;
}

void Model::append(int id)
{
    Q_D(Model);

    d->m_items.push_back(std::make_shared<Item>(id));
    d->rev++;
    d->invalidateCache();
}

/*
 * Loader whose tasks only complete when the test asks for it, this allows to
 * interleave model changes (limit, offset, ...) with pending loads
 */
struct ManualBackend
{
    enum class Type { Count, Load, CountAndLoad };

    struct Request
    {
        size_t id;
        Type type;
        size_t offset;
        size_t limit;
    };

    struct Task
    {
        Request req;
        std::function<void(size_t, size_t)> countCb;
        std::function<void(size_t, std::vector<ItemPtr>&)> loadCb;
        std::function<void(size_t, size_t, std::vector<ItemPtr>&)> countAndLoadCb;
    };

    std::vector<ItemPtr> items;
    std::deque<Task> pending;
    //every request issued since the last call to clearHistory
    std::vector<Request> history;
    std::vector<size_t> cancelled;
    size_t nextId = 1;

    void setItems(int count)
    {
        items.clear();
        for (int i = 0; i < count; ++i)
            items.push_back(std::make_shared<Item>(i));
    }

    void clearHistory()
    {
        history.clear();
        cancelled.clear();
    }

    size_t push(Task&& task)
    {
        task.req.id = nextId++;
        history.push_back(task.req);
        pending.push_back(std::move(task));
        return history.back().id;
    }

    bool isCancelled(size_t id) const
    {
        return std::find(cancelled.cbegin(), cancelled.cend(), id) != cancelled.cend();
    }

    std::vector<ItemPtr> slice(size_t offset, size_t limit) const
    {
        std::vector<ItemPtr> data;
        if (offset >= items.size())
            return data;
        //limit may be huge, don't compute offset + limit
        size_t count = std::min(limit, items.size() - offset);
        if (limit == 0)
            count = items.size() - offset;
        std::copy_n(items.cbegin() + offset, count, std::back_inserter(data));
        return data;
    }

    //complete the oldest pending task, returns false if there is none
    bool runNext()
    {
        if (pending.empty())
            return false;
        Task task = std::move(pending.front());
        pending.pop_front();
        switch (task.req.type)
        {
        case Type::Count:
            task.countCb(task.req.id, items.size());
            break;
        case Type::Load:
        {
            auto data = slice(task.req.offset, task.req.limit);
            task.loadCb(task.req.id, data);
            break;
        }
        case Type::CountAndLoad:
        {
            auto data = slice(task.req.offset, task.req.limit);
            task.countAndLoadCb(task.req.id, items.size(), data);
            break;
        }
        }
        return true;
    }

    //returns false if the cache keeps requesting data
    bool runAll()
    {
        for (int i = 0; i < 100 && runNext(); ++i)
            ;
        return pending.empty();
    }
};

struct ManualLoader : public ListCacheLoader<ItemPtr>
{
    explicit ManualLoader(std::shared_ptr<ManualBackend> backend)
        : m_backend(std::move(backend))
    {}

    void cancelTask(size_t taskId) override
    {
        m_backend->cancelled.push_back(taskId);
        auto& pending = m_backend->pending;
        pending.erase(std::remove_if(pending.begin(), pending.end(),
                                     [taskId](const ManualBackend::Task& t) { return t.req.id == taskId; }),
                      pending.end());
    }

    size_t countTask(std::function<void(size_t taskId, size_t count)> cb) override
    {
        ManualBackend::Task task;
        task.req = {0, ManualBackend::Type::Count, 0, 0};
        task.countCb = std::move(cb);
        return m_backend->push(std::move(task));
    }

    size_t loadTask(size_t offset, size_t limit,
                    std::function<void(size_t taskId, std::vector<ItemPtr>& data)> cb) override
    {
        ManualBackend::Task task;
        task.req = {0, ManualBackend::Type::Load, offset, limit};
        task.loadCb = std::move(cb);
        return m_backend->push(std::move(task));
    }

    size_t countAndLoadTask(size_t offset, size_t limit,
                            std::function<void(size_t taskId, size_t count, std::vector<ItemPtr>& data)> cb) override
    {
        ManualBackend::Task task;
        task.req = {0, ManualBackend::Type::CountAndLoad, offset, limit};
        task.countAndLoadCb = std::move(cb);
        return m_backend->push(std::move(task));
    }

    std::shared_ptr<ManualBackend> m_backend;
};

class ManualModelPrivate;

class ManualModel : public BaseModel
{
    Q_DECLARE_PRIVATE(ManualModel);

public:
    ManualModel(QObject *parent = nullptr);

    QVariant data(const QModelIndex & index, int role) const override;

    ManualBackend& backend();

    //remove an item from the cache as a medialibrary event would do
    void deleteFromCache(int id);
};

class ManualModelPrivate : public BaseModelPrivateT<ItemPtr>
{
    Q_DECLARE_PUBLIC(ManualModel)
public:
    using BaseModelPrivateT<ItemPtr>::BaseModelPrivateT;

    std::unique_ptr<ListCacheLoader<ItemPtr>> createLoader() const override
    {
        return std::make_unique<ManualLoader>(m_backend);
    }

    bool initializeModel() override
    {
        return true;
    }

    std::shared_ptr<ManualBackend> m_backend = std::make_shared<ManualBackend>();
};

ManualModel::ManualModel(QObject *parent)
    : BaseModel(new ManualModelPrivate(this), parent)
{
}

QVariant ManualModel::data(const QModelIndex &index, int role) const
{
    Q_D(const ManualModel);
    Q_UNUSED(role);

    const ItemPtr *item = d->item(index.row());
    if (!item)
        return {};

    return (*item)->id;
}

ManualBackend& ManualModel::backend()
{
    Q_D(ManualModel);
    return *d->m_backend;
}

void ManualModel::deleteFromCache(int id)
{
    Q_D(ManualModel);
    d->m_cache->deleteItem([id](const ItemPtr& item) { return item->id == id; });
}

using Ranges = QList<std::pair<int, int>>;

//rows ranges reported by rowsInserted/rowsRemoved
static Ranges rowRanges(const QSignalSpy& spy)
{
    Ranges ranges;
    for (const QList<QVariant>& args : spy)
        ranges.append({args.at(1).toInt(), args.at(2).toInt()});
    return ranges;
}

static QVariant idAt(const QAbstractListModel& m, int row)
{
    return m.data(m.index(row), 0);
}

//create a model with @a itemCount items, and complete its first load
static bool initModel(ManualModel& m, int itemCount, unsigned chunkSize,
                      unsigned limit, unsigned offset = 0)
{
    m.backend().setItems(itemCount);
    m.setChunkSize(chunkSize);
    m.setLimit(limit);
    m.setOffset(offset);
    m.invalidateCache(); // need to force initialize model (FIXME??)
    bool ret = m.backend().runAll();
    m.backend().clearHistory();
    return ret;
}

class TestBaseModel : public QObject
{
    Q_OBJECT

private slots:
    void testNullReset()
    {
        Model m;
        // should not crash or give out warning
        // bug of MR!4786
        m.resetCache();
    }

    void testInvalidateOnInvalidate()
    {
        Model m;
        m.invalidateCache(); // need to force initialize model (FIXME??)

        // bug of MR!4485 MR!4684
        m.connect(&m, &Model::loadingChanged, [&m]() { m.setLimit(1); });
        QTRY_VERIFY(!m.loading());
    }

    void testSanity()
    {
        Model m;
        m.invalidateCache(); // need to force initialize model (FIXME??)

        m.setLimit(2);
        m.append(1);
        m.append(2);
        QTRY_COMPARE(m.getCount(), 2u);

        m.setLimit(1);
        QTRY_COMPARE(m.getCount(), 1u);
        QCOMPARE(m.getMaximumCount(), 2u);

        m.setLimit(2);
        QTRY_COMPARE(m.getCount(), 2u);
        for (int i = 0; i < 2; ++i)
            QCOMPARE(idAt(m, i), QVariant(i + 1));
    }

    //changing the limit within loaded data must not reset or reload the model
    void testLimitChangeWithinLoadedData()
    {
        ManualModel m;
        QVERIFY(initModel(m, 50, 20, 10));
        QCOMPARE(m.getCount(), 10u);
        QCOMPARE(m.getLoadedCount(), 20u);

        QSignalSpy resetSpy(&m, &QAbstractItemModel::modelReset);
        QSignalSpy insertedSpy(&m, &QAbstractItemModel::rowsInserted);
        QSignalSpy removedSpy(&m, &QAbstractItemModel::rowsRemoved);

        m.setLimit(15);
        QCOMPARE(resetSpy.count(), 0);
        QCOMPARE(rowRanges(insertedSpy), Ranges({{10, 14}}));
        QCOMPARE(m.getCount(), 15u);
        QCOMPARE(idAt(m, 12), QVariant(12));
        QVERIFY(m.backend().history.empty());

        m.setLimit(5);
        QCOMPARE(resetSpy.count(), 0);
        QCOMPARE(rowRanges(removedSpy), Ranges({{5, 14}}));
        QCOMPARE(m.getCount(), 5u);
        QVERIFY(m.backend().history.empty());
    }

    //growing the limit past the loaded data exposes rows that are loaded on access
    void testLimitGrowPastLoadedData()
    {
        ManualModel m;
        QVERIFY(initModel(m, 50, 20, 10));

        QSignalSpy resetSpy(&m, &QAbstractItemModel::modelReset);
        QSignalSpy insertedSpy(&m, &QAbstractItemModel::rowsInserted);

        m.setLimit(30);
        QCOMPARE(resetSpy.count(), 0);
        QCOMPARE(rowRanges(insertedSpy), Ranges({{10, 29}}));
        QCOMPARE(m.getCount(), 30u);

        //row is not loaded yet, accessing it requests the missing data
        QVERIFY(!idAt(m, 25).isValid());
        QCOMPARE(m.backend().history.size(), 1u);
        QCOMPARE(m.backend().history.front().offset, 20u);

        QVERIFY(m.backend().runAll());
        QCOMPARE(idAt(m, 25), QVariant(25));
        QCOMPARE(idAt(m, 29), QVariant(29));
        QCOMPARE(resetSpy.count(), 0);
    }

    //fetching more data must not notify rows past the limit, dataChanged with an
    //invalid index prevents the view from updating the newly loaded rows
    void testFetchDataChangedWithinRowCount()
    {
        ManualModel m;
        QVERIFY(initModel(m, 50, 20, 10));

        QSignalSpy changedSpy(&m, &QAbstractItemModel::dataChanged);

        m.setLimit(25);
        QVERIFY(m.backend().runAll());
        idAt(m, 22);

        QVERIFY(m.backend().runAll());
        QCOMPARE(idAt(m, 24), QVariant(24));

        //dataChanged must stay within [0, rowCount[
        QVERIFY(!changedSpy.isEmpty());
        for (const QList<QVariant>& args : changedSpy)
        {
            const auto topLeft = args.at(0).value<QModelIndex>();
            const auto bottomRight = args.at(1).value<QModelIndex>();
            QVERIFY(topLeft.isValid());
            QVERIFY(bottomRight.isValid());
            QVERIFY(bottomRight.row() < 25);
        }
    }

    //shrinking the limit while more data is being fetched must not underflow
    //the number of items requested to the loader
    void testLimitShrinkDuringFetch()
    {
        ManualModel m;
        QVERIFY(initModel(m, 100, 10, 0));
        QCOMPARE(m.getLoadedCount(), 10u);

        //request rows [10, 20[
        idAt(m, 15);
        QCOMPARE(m.backend().pending.size(), 1u);
        //view scrolls further while the fetch is pending
        idAt(m, 45);
        QCOMPARE(m.backend().pending.size(), 1u);

        m.backend().clearHistory();
        m.setLimit(5);

        //complete the pending fetch, the view still refers row 45 which is now
        //past the limit
        QVERIFY(m.backend().runAll());
        for (const ManualBackend::Request& req : m.backend().history)
            QVERIFY2(req.limit <= m.getMaximumCount(),
                     qPrintable(QString("requested %1 items at offset %2").arg(req.limit).arg(req.offset)));
        QCOMPARE(m.getCount(), 5u);
    }

    //the number of rows must account for the offset
    void testOffsetCount_data()
    {
        QTest::addColumn<int>("itemCount");
        QTest::addColumn<unsigned>("limit");
        QTest::addColumn<unsigned>("offset");
        QTest::addColumn<unsigned>("expectedCount");

        QTest::newRow("no limit") << 50 << 0u << 45u << 5u;
        QTest::newRow("limit past the end") << 50 << 10u << 45u << 5u;
        QTest::newRow("limit before the end") << 50 << 2u << 45u << 2u;
        QTest::newRow("offset past the end") << 50 << 0u << 60u << 0u;
    }

    void testOffsetCount()
    {
        QFETCH(int, itemCount);
        QFETCH(unsigned, limit);
        QFETCH(unsigned, offset);
        QFETCH(unsigned, expectedCount);

        ManualModel m;
        QVERIFY(initModel(m, itemCount, 20, limit, offset));
        QCOMPARE(m.getCount(), expectedCount);
        QCOMPARE(m.getMaximumCount(), static_cast<unsigned>(itemCount));
        if (expectedCount > 0)
        {
            QCOMPARE(idAt(m, 0), QVariant(static_cast<int>(offset)));
            QCOMPARE(idAt(m, expectedCount - 1), QVariant(static_cast<int>(offset + expectedCount - 1)));
        }
    }

    void testOffsetCountLimitGrow()
    {
        ManualModel m;
        QVERIFY(initModel(m, 50, 20, 2, 45));
        QCOMPARE(m.getCount(), 2u);

        m.setLimit(10);
        QVERIFY(m.backend().runAll());
        QCOMPARE(m.getCount(), 5u);
    }

    //changing the offset must update the model without resetting it
    void testOffsetChangeNoReset()
    {
        ManualModel m;
        QVERIFY(initModel(m, 50, 20, 0));

        QSignalSpy resetSpy(&m, &QAbstractItemModel::modelReset);

        m.setOffset(5);
        QVERIFY(m.backend().runAll());
        QCOMPARE(resetSpy.count(), 0);
        QCOMPARE(idAt(m, 0), QVariant(5));
        QCOMPARE(idAt(m, 10), QVariant(15));
    }

    //changing the offset while a load is pending must not display the result of
    //the outdated load
    void testOffsetChangeDuringLoad()
    {
        ManualModel m;
        QVERIFY(initModel(m, 50, 20, 0));

        m.setOffset(10);
        QCOMPARE(m.backend().pending.size(), 1u);
        m.setOffset(20);

        for (int i = 0; i < 100 && m.backend().runNext(); ++i)
        {
            //row 0 is either the original data or the final one
            QVERIFY(idAt(m, 0) != QVariant(10));
        }
        QCOMPARE(idAt(m, 0), QVariant(20));
    }

    //removing an item hidden by the limit must not notify the view
    void testDeleteHiddenItem()
    {
        ManualModel m;
        QVERIFY(initModel(m, 20, 20, 0));
        QCOMPARE(m.getLoadedCount(), 20u);

        m.setLimit(5);
        QVERIFY(m.backend().runAll());
        QCOMPARE(m.getCount(), 5u);

        QSignalSpy removedSpy(&m, &QAbstractItemModel::rowsRemoved);
        m.deleteFromCache(10);
        QCOMPARE(rowRanges(removedSpy), Ranges());
        QCOMPARE(m.getCount(), 5u);
    }
};

QTEST_GUILESS_MAIN(TestBaseModel)
#include "test_base_model.moc"
