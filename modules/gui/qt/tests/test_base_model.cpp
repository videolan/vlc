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

#include <memory>
#include <vector>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QDebug>


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

void await(int msec)
{
    QElapsedTimer timer;
    timer.start();
    while (!timer.hasExpired(msec))
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, msec);
    }
}

void test_invalidate_on_invalidate()
{
    Model m;
    m.invalidateCache(); // need to force initialize model (FIXME??)

    // bug of MR!4485 MR!4684
    m.connect(&m, &Model::loadingChanged, [&m]() { m.setLimit(1); });
    await(1);
    assert(!m.loading());
}


void test_null_reset()
{
    Model m;
    // should not crash or give out warning
    // bug of MR!4786
    m.resetCache();
}

void test_sanity()
{
    Model m;
    m.invalidateCache(); // need to force initialize model (FIXME??)

    m.setLimit(2);
    m.append(1);
    m.append(2);
    await(1);
    assert(m.getCount() == 2);

    m.setLimit(1);
    await(1);
    assert(m.getCount() == 1);
    assert(m.getMaximumCount() == 2);

    m.setLimit(2);
    await(1);
    assert(m.getCount() == 2);
    for (int i = 0; i < 2; ++i)
    {
        assert(m.data(m.index(i), 0) == i + 1);
    }
}

int main(int argc, char **argv)
{
    QCoreApplication a(argc, argv);
    QMetaObject::invokeMethod(&a, [&a]()
    {
        test_null_reset();
        test_invalidate_on_invalidate();
        test_sanity();

        a.quit();
    }, Qt::QueuedConnection);
    return a.exec();
}
