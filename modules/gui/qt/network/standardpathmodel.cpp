/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Benjamin Arnaud <bunjee@omega.gg>
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

#include "standardpathmodel.hpp"
#include "qt.hpp"
#include "networkdevicemodel.hpp"
#include "networkmediamodel.hpp"

#include "util/base_model_p.hpp"
#include "util/locallistcacheloader.hpp"
#include "util/shared_input_item.hpp"

// VLC includes
#include <vlc_media_source.h>
#include <vlc_configuration.h>

#include <QAbstractListModel>
#include <QStandardPaths>
#include <QUrl>

using MediaTreePtr = vlc_shared_data_ptr_type(vlc_media_tree_t,
                                              vlc_media_tree_Hold,
                                              vlc_media_tree_Release);

struct StandardPathItem
{
    QString name;
    QUrl    mrl;

    QString protocol;

    NetworkDeviceModel::ItemType type;

    SharedInputItem inputItem;
    MediaTreePtr tree;

    QUrl artwork;
};

using StandardPathItemPtr =  std::shared_ptr<StandardPathItem>;
using StandardPathLoader = LocalListCacheLoader<StandardPathItemPtr>;

template<>
bool ListCache<StandardPathItemPtr>::compareItems(const StandardPathItemPtr& a, const StandardPathItemPtr& b)
{
    //just compare the pointers here
    return a == b;
}

namespace {

bool itemMatchPattern(const StandardPathItemPtr& a, const QString& pattern)
{
    return a->name.contains(pattern, Qt::CaseInsensitive);
}

bool ascendingName(const StandardPathItemPtr& a,
                   const StandardPathItemPtr& b)
{
    return (QString::compare(a->name, b->name, Qt::CaseInsensitive) <= 0);
}

bool ascendingMrl(const StandardPathItemPtr& a,
                   const StandardPathItemPtr& b)
{
    return (QString::compare(a->mrl.toString(), b->mrl.toString(), Qt::CaseInsensitive) <= 0);
}

bool descendingName(const StandardPathItemPtr& a,
                   const StandardPathItemPtr& b)
{
    return (QString::compare(a->name, b->name, Qt::CaseInsensitive) >= 0);
}

bool descendingMrl(const StandardPathItemPtr& a,
                   const StandardPathItemPtr& b)
{
    return (QString::compare(a->mrl.toString(), b->mrl.toString(), Qt::CaseInsensitive) >= 0);
}

}


class StandardPathModelPrivate
    : public BaseModelPrivateT<StandardPathItemPtr>
    , public LocalListCacheLoader<StandardPathItemPtr>::ModelSource
{
    Q_DECLARE_PUBLIC(StandardPathModel)
public:

    StandardPathModelPrivate(StandardPathModel* pub)
        : BaseModelPrivateT<StandardPathItemPtr>(pub)
    {}

    StandardPathLoader::ItemCompare getSortFunction() const
    {
        if (m_sortCriteria == "mrl")
        {
            if (m_sortOrder == Qt::AscendingOrder)
                return ascendingMrl;
            else
                return descendingMrl;
        }
        else
        {
            if (m_sortOrder == Qt::AscendingOrder)
                return ascendingName;
            else
                return descendingName;
        }
    }

    std::unique_ptr<ListCacheLoader<StandardPathItemPtr>> createLoader() const override
    {
        return std::make_unique<StandardPathLoader>(
            this, m_searchPattern,
            getSortFunction());
    }

    bool initializeModel() override
    {
        assert(m_qmlInitializing == false);
#ifdef Q_OS_UNIX
        addItem(QVLCUserDir(VLC_HOME_DIR), qtr("Home"), QUrl());
#endif
        addItem(QVLCUserDir(VLC_DESKTOP_DIR), qtr("Desktop"), QUrl());
        addItem(QVLCUserDir(VLC_DOCUMENTS_DIR), qtr("Documents"), QUrl());
        addItem(QVLCUserDir(VLC_MUSIC_DIR), qtr("Music"), QUrl());
        addItem(QVLCUserDir(VLC_VIDEOS_DIR), qtr("Videos"), QUrl());
        addItem(QVLCUserDir(VLC_DOWNLOAD_DIR), qtr("Download"), QUrl());
        return true;
    }

    const StandardPathItem* getItemForRow(int row) const
    {
        const StandardPathItemPtr* ref = item(row);
        if (ref)
            return ref->get();
        return nullptr;
    }

    void addItem(const QString & path, const QString & name, const QUrl & artwork)
    {
        QUrl url = QUrl::fromLocalFile(path);

        auto item = std::make_shared<StandardPathItem>();

        item->name = name;
        item->mrl  = url;
        item->protocol = url.scheme();
        item->type = NetworkDeviceModel::TYPE_DIRECTORY;

        input_item_t * inputItem = input_item_NewDirectory(qtu(url.toString()), qtu(name), ITEM_LOCAL);
        item->inputItem = SharedInputItem(inputItem, false);

        vlc_media_tree_t * tree = vlc_media_tree_New();
        vlc_media_tree_Lock(tree);
        vlc_media_tree_Add(tree, &(tree->root), inputItem);
        vlc_media_tree_Unlock(tree);

        item->tree = MediaTreePtr(tree, false);
        item->artwork = artwork;

        m_items.emplace_back(std::move(item));
    }

public: //LocalListCacheLoader::ModelSource implementation

    size_t getModelRevision() const override
    {
        //model never changes
        return 1;
    }

    std::vector<StandardPathItemPtr> getModelData(const QString& pattern) const override
    {
        if (pattern.isEmpty())
            return m_items;

        std::vector<StandardPathItemPtr> items;
        std::copy_if(
            m_items.cbegin(), m_items.cend(),
            std::back_inserter(items),
            [&pattern](const StandardPathItemPtr& item){
                return itemMatchPattern(item, pattern);
            });
        return items;
    }

public:
    std::vector<StandardPathItemPtr> m_items;
};

// Ctor / dtor

StandardPathModel::StandardPathModel(QObject * parent)
    : BaseModel(new StandardPathModelPrivate(this), parent)
{
}

// QAbstractItemModel implementation

QHash<int, QByteArray> StandardPathModel::roleNames() const /* override */
{
    return
    {
        { PATH_NAME, "name" },
        { PATH_MRL, "mrl" },
        { PATH_PROTOCOL, "protocol" },
        { PATH_TYPE, "type" },
        { PATH_SOURCE, "source" },
        { PATH_TREE, "tree" },
        { PATH_ARTWORK, "artwork" }
    };
}

QVariant StandardPathModel::data(const QModelIndex & index, int role) const /* override */
{
    Q_D(const StandardPathModel);

    const StandardPathItem* item = d->getItemForRow(index.row());
    if (!item)
        return {};

    switch (role)
    {
        case PATH_NAME:
            return item->name;
        case PATH_MRL:
            return item->mrl;
        case PATH_PROTOCOL:
            return item->protocol;
        case PATH_TYPE:
            return item->type;
        case PATH_TREE:
            return QVariant::fromValue(NetworkTreeItem(item->tree, item->inputItem.get()));
        case PATH_ARTWORK:
            return item->artwork;
        default:
            return QVariant();
    }
}
