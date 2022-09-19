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

// VLC includes
#include "networkmediamodel.hpp"

// Ctor / dtor

StandardPathModel::StandardPathModel(QObject * parent)
    : ClipListModel(parent)
{
    m_comparator = ascendingName;

#ifdef Q_OS_UNIX
    addItem(QVLCUserDir(VLC_HOME_DIR), qtr("Home"), QUrl());
#endif
    addItem(QVLCUserDir(VLC_DESKTOP_DIR), qtr("Desktop"), QUrl());
    addItem(QVLCUserDir(VLC_DOCUMENTS_DIR), qtr("Documents"), QUrl());
    addItem(QVLCUserDir(VLC_MUSIC_DIR), qtr("Music"), QUrl());
    addItem(QVLCUserDir(VLC_VIDEOS_DIR), qtr("Videos"), QUrl());
    addItem(QVLCUserDir(VLC_DOWNLOAD_DIR), qtr("Download"), QUrl());

    updateItems();
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
    int row = index.row();

    if (row < 0 || row >= count())
        return QVariant();

    const StandardPathItem & item = m_items[row];

    switch (role)
    {
        case PATH_NAME:
            return item.name;
        case PATH_MRL:
            return item.mrl;
        case PATH_PROTOCOL:
            return item.protocol;
        case PATH_TYPE:
            return item.type;
        case PATH_TREE:
            return QVariant::fromValue(NetworkTreeItem(item.tree, item.inputItem.get()));
        case PATH_ARTWORK:
            return item.artwork;
        default:
            return QVariant();
    }
}

// Protected ClipListModel implementation

void StandardPathModel::onUpdateSort(const QString & criteria, Qt::SortOrder order) /* override */
{
    if (criteria == "mrl")
    {
        if (order == Qt::AscendingOrder)
            m_comparator = ascendingMrl;
        else
            m_comparator = descendingMrl;
    }
    else
    {
        if (order == Qt::AscendingOrder)
            m_comparator = ascendingName;
        else
            m_comparator = descendingName;
    }
}

// Private static function

/* static */ bool StandardPathModel::ascendingName(const StandardPathItem & a,
                                                   const StandardPathItem & b)
{
    return (QString::compare(a.name, b.name, Qt::CaseInsensitive) <= 0);
}

/* static */ bool StandardPathModel::ascendingMrl(const StandardPathItem & a,
                                                  const StandardPathItem & b)
{
    return (QString::compare(a.mrl.toString(), b.mrl.toString(), Qt::CaseInsensitive) <= 0);
}

/* static */ bool StandardPathModel::descendingName(const StandardPathItem & a,
                                                    const StandardPathItem & b)
{
    return (QString::compare(a.name, b.name, Qt::CaseInsensitive) >= 0);
}

/* static */ bool StandardPathModel::descendingMrl(const StandardPathItem & a,
                                                   const StandardPathItem & b)
{
    return (QString::compare(a.mrl.toString(), b.mrl.toString(), Qt::CaseInsensitive) >= 0);
}

// Private functions

void StandardPathModel::addItem(const QString & path, const QString & name, const QUrl & artwork)
{
    QUrl url = QUrl::fromLocalFile(path);

    StandardPathItem item;

    item.name = name;
    item.mrl  = url;

    item.protocol = url.scheme();

    item.type = NetworkDeviceModel::TYPE_DIRECTORY;

    input_item_t * inputItem = input_item_NewDirectory(qtu(url.toString()), qtu(name), ITEM_LOCAL);

    item.inputItem = InputItemPtr(inputItem, false);

    vlc_media_tree_t * tree = vlc_media_tree_New();

    vlc_media_tree_Lock(tree);

    vlc_media_tree_Add(tree, &(tree->root), inputItem);

    vlc_media_tree_Unlock(tree);

    item.tree = MediaTreePtr(tree, false);

    item.artwork = artwork;

    m_items.push_back(item);
}
