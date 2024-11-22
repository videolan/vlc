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

#include "util/locallistbasemodel.hpp"

// VLC includes
#include <vlc_media_source.h>
#include <vlc_configuration.h>

#include <QAbstractListModel>
#include <QStandardPaths>
#include <QUrl>

#include "vlcmediasourcewrapper.hpp"

struct StandardPathItem : public NetworkBaseItem
{
    SharedInputItem inputItem;
    MediaTreePtr tree;
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
    return (QString::compare(a->mainMrl.toString(), b->mainMrl.toString(), Qt::CaseInsensitive) <= 0);
}

bool descendingName(const StandardPathItemPtr& a,
                   const StandardPathItemPtr& b)
{
    return (QString::compare(a->name, b->name, Qt::CaseInsensitive) >= 0);
}

bool descendingMrl(const StandardPathItemPtr& a,
                   const StandardPathItemPtr& b)
{
    return (QString::compare(a->mainMrl.toString(), b->mainMrl.toString(), Qt::CaseInsensitive) >= 0);
}

}


class StandardPathModelPrivate
    : public LocalListBaseModelPrivate<StandardPathItemPtr>
{
    Q_DECLARE_PUBLIC(StandardPathModel)
public:

    StandardPathModelPrivate(StandardPathModel* pub)
        : LocalListBaseModelPrivate<StandardPathItemPtr>(pub)
    {}

    StandardPathLoader::ItemCompare getSortFunction() const override
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

    bool initializeModel() override
    {
        Q_Q(StandardPathModel);
        assert(m_qmlInitializing == false);
#ifdef Q_OS_UNIX
        addItem(QVLCUserDir(VLC_HOME_DIR), qtr("Home"), {});
#endif
        addItem(QVLCUserDir(VLC_DESKTOP_DIR), qtr("Desktop"), {});
        addItem(QVLCUserDir(VLC_DOCUMENTS_DIR), qtr("Documents"), {});
        addItem(QVLCUserDir(VLC_MUSIC_DIR), qtr("Music"), {});
        addItem(QVLCUserDir(VLC_VIDEOS_DIR), qtr("Videos"), {});
        addItem(QVLCUserDir(VLC_DOWNLOAD_DIR), qtr("Download"), {});
        //model is never updated, but this is still needed to fit the LocalListBaseModelPrivate requirements
        ++m_revision;
        m_loading = false;
        emit q->loadingChanged();
        return true;
    }

    const StandardPathItem* getItemForRow(int row) const
    {
        const StandardPathItemPtr* ref = item(row);
        if (ref)
            return ref->get();
        return nullptr;
    }

    void addItem(const QString & path, const QString & name, const QString& artwork)
    {
        QUrl url = QUrl::fromLocalFile(path);

        auto item = std::make_shared<StandardPathItem>();

        item->name = name;
        item->mainMrl  = url;
        item->protocol = url.scheme();
        item->type = NetworkDeviceModel::TYPE_DIRECTORY;

        input_item_t * inputItem = input_item_NewDirectory(qtu(url.toString()), qtu(name), ITEM_LOCAL);
        item->inputItem = SharedInputItem(inputItem, false);

        item->tree = MediaTreePtr(vlc_media_tree_New(), false);
        {
            MediaTreeLocker lock{item->tree};
            vlc_media_tree_Add(item->tree.get(), &(item->tree->root), inputItem);
        }

        item->artwork = artwork;

        m_items.emplace_back(std::move(item));
    }

public: //LocalListCacheLoader::ModelSource implementation
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
    : NetworkBaseModel(new StandardPathModelPrivate(this), parent)
{
}

// QAbstractItemModel implementation

QHash<int, QByteArray> StandardPathModel::roleNames() const /* override */
{
    auto roles = NetworkBaseModel::roleNames();
    roles[PATH_SOURCE] = "source";
    roles[PATH_TREE] = "tree";
    return roles;
}

QVariant StandardPathModel::data(const QModelIndex & index, int role) const /* override */
{
    Q_D(const StandardPathModel);

    const StandardPathItem* item = d->getItemForRow(index.row());
    if (!item)
        return {};

    switch (role)
    {
        case PATH_TREE:
            return QVariant::fromValue(NetworkTreeItem(item->tree, item->inputItem));
        default:
            return NetworkBaseModel::basedata(*item, role);
    }
}
