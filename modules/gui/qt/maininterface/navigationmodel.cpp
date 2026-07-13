/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

#include "navigationmodel.hpp"
#include "qt.hpp"
#include "style/vlcicons.hpp"

//hierachical model entry
struct ModelEntry
{
    ModelEntry( QString name, QString uri, QString icon = "")
        : name(name)
        , urinode(uri)
        , icon(icon)
    {}

    QString name;
    QString urinode;
    QString icon;
    std::vector<ModelEntry> children;
    size_t visibleChildren = 0;
    int expandedIndex = -1;

    typedef std::vector<size_t>::const_iterator PathIterator;

    bool getIndexPath(size_t index, std::vector<size_t>& vectorOut) const
    {
        if (index >= visibleChildren)
            return false;

        if (expandedIndex == -1)
        {
            assert(index <= children.size());
            vectorOut.push_back(index);
        }
        else
        {
            if (index <= static_cast<size_t>(expandedIndex))
            {
                vectorOut.push_back(index);
            }
            else
            {
                size_t childrenGap = children[expandedIndex].visibleChildren;
                if (index <= static_cast<size_t>(expandedIndex + childrenGap))
                {
                    vectorOut.push_back(expandedIndex);
                    return children[expandedIndex].getIndexPath(index - (expandedIndex + 1), vectorOut);
                }
                else
                {
                    vectorOut.push_back(index - childrenGap);
                }
            }
        }
        return true;
    }

    std::tuple<int, int> getExpandIndexRange(const PathIterator& it, const PathIterator& end, size_t index)
    {
        if (it == end)
        {
            //already expanded or not expandable
            if (visibleChildren != 0 || children.size() == 0)
                return {-1, -1};
            return { index + 1, index + children.size()};
        }
        else
        {
            size_t childIndex =  *it;
            if (expandedIndex == -1 || childIndex == static_cast<size_t>(expandedIndex))
                return children[childIndex].getExpandIndexRange(it+1, end, index + childIndex);
            return {-1,-1};
        }
    }

    void expand(const PathIterator& it, const PathIterator& end)
    {
        if (it == end)
        {
            visibleChildren = children.size();
        }
        else
        {
            size_t newExpandedIndex = *it;
            children[newExpandedIndex].expand(it+1, end);
            expandedIndex = newExpandedIndex;
            visibleChildren = children[expandedIndex].visibleChildren + children.size();
        }
    }

    enum RectractRangePolicy {
        RectractSelf, // we want to retract visible children
        RectractChildren // we want to retract expanded children
    };

    //return the range that will be removed
    std::tuple<int, int> getRetractRange(const PathIterator& it, const PathIterator& end, RectractRangePolicy policy, size_t index = 0)
    {
        if (it == end)
        {
            if (policy == RectractChildren) {
                if (expandedIndex == -1)
                    return {-1, -1};

                return {
                    index + expandedIndex + 1,
                    index + expandedIndex +  children[expandedIndex].visibleChildren
                };
            }
            else //RectractSelf
            {
                if (visibleChildren == 0)
                    return {-1, -1};
                return {
                    index + 1,
                    index + visibleChildren
                };
            }
        }
        else
        {
            size_t childIndex =  *it;
            if (childIndex != static_cast<size_t>(expandedIndex))
                return {-1, -1};

            return children[childIndex].getRetractRange(it+1, end, policy, index + childIndex);

        }
    }

    void _retractAll() {
        for (auto& child : children) {
            child._retractAll();
        }
        expandedIndex = -1;
        visibleChildren = 0;
    }

    void retract(const PathIterator& it, const PathIterator& end)
    {

        if (it == end || it+1 == end)
        {
            if (expandedIndex == -1)
                return;
            children[expandedIndex]._retractAll();
            expandedIndex = -1;
            visibleChildren = children.size();
        }
        else
        {
            size_t index = *it;
            children[index].retract(it+1, end);
            visibleChildren = children[index].visibleChildren + children.size();
        }
    }

    QString getName(const PathIterator& it, const PathIterator& end) const
    {
        if (it == end)
            return name;
        else
            return children[*it].getName(it + 1, end);
    }

    bool isExpandable(const PathIterator& it, const PathIterator& end) const
    {
        if (it == end)
            return children.size() > 0;
        else
            return children[*it].isExpandable(it + 1, end);
    }

    bool isExpanded(const PathIterator& it, const PathIterator& end) const
    {
        if (it == end)
            return visibleChildren != 0;
        else
            return children[*it].isExpanded(it + 1, end);
    }

    int getDepth(const PathIterator& it, const PathIterator& end) const
    {
        if (it == end)
            return 0;
        else
            return 1 + children[*it].getDepth(it + 1, end);
    }

    QString getIcon(const PathIterator& it, const PathIterator& end) const
    {
        if (it == end)
            return icon;
        else
            return children[*it].getIcon(it + 1, end);
    }

    int getIndex(const PathIterator& it, const PathIterator& end) const
    {
        if (it == end)
            return *it;
        else
            return *it + (getIndex(it + 1, end));
    }

    template<typename OutIterator>
    void getUri(
        PathIterator it, const PathIterator& end,
        OutIterator out) const
    {
        size_t index = *it;
        if (it == end) {
            *out = urinode;
            return;
        } else {
            *out = urinode;
            children[index].getUri(++it, end, ++out);
        }
    }
};


class NavigationModelPrivate
{
public:
    NavigationModelPrivate(NavigationModel* q)
        : q_ptr(q)
        , m_model{qtr("MC"), "mc"}
    {
    }

    void initilializeModel()
    {
        m_model.children.emplace_back(qtr("Home"), "home", VLCIcons::home);

        if (m_hasMedialib)
        {
            ModelEntry videoEntry{ qtr("Video"), "video", VLCIcons::topbar_video };
            videoEntry.children.emplace_back(qtr("All"), "all");
            videoEntry.children.emplace_back(qtr("Playlists"), "playlists");
            m_model.children.push_back(std::move(videoEntry));

            ModelEntry musicEntry{ qtr("Music"), "music", VLCIcons::topbar_music };
            musicEntry.children.emplace_back(qtr("Artists"), "artists");
            musicEntry.children.emplace_back(qtr("Albums"), "albums");
            musicEntry.children.emplace_back(qtr("Tracks"), "tracks");
            musicEntry.children.emplace_back(qtr("Genres"), "genres");
            musicEntry.children.emplace_back(qtr("Playlists"), "playlists");
            m_model.children.push_back(std::move(musicEntry));
        }

        m_model.children.emplace_back(qtr("Browse"), "network", VLCIcons::topbar_network);

        ModelEntry discoverEntry{ qtr("Discover"), "discover", VLCIcons::topbar_discover };
        discoverEntry.children.emplace_back(qtr("Services"), "services");

        discoverEntry.children.emplace_back(qtr("URL"), "url");
        m_model.children.push_back(std::move(discoverEntry));

        m_model.visibleChildren = m_model.children.size();
    }

    bool setExpanded(size_t newExpandedIndex, bool expanded)
    {
        Q_Q(NavigationModel);

        std::vector<size_t> oldExpandedPath;
        m_model.getIndexPath(m_model.expandedIndex, oldExpandedPath);

        std::vector<size_t> newExpandedPath;
        bool ret = m_model.getIndexPath(newExpandedIndex, newExpandedPath);
        if (!ret)
            return false;

        //we need to advertise before doing the operations
        //  - first retreive the flatten range to be added/removed
        //  - then perform the operation
        if (!expanded)
        {
            auto retractBeginIt = newExpandedPath.cbegin();
            auto retractEndIt = newExpandedPath.cend();
            std::tuple<int, int> retractRange = m_model.getRetractRange(
                retractBeginIt,
                retractEndIt,
                ModelEntry::RectractSelf);

            if (std::get<0>(retractRange) != -1)
            {
                q->beginRemoveRows({}, std::get<0>(retractRange), std::get<1>(retractRange));
                //retract the parent
                m_model.retract(retractBeginIt, retractEndIt);
                q->endRemoveRows();
            }

            //itemIndex doesn't change
            auto itemIndex = q->index(newExpandedIndex);
            q->dataChanged(itemIndex, itemIndex, {NavigationModel::EXPANDED});
            return true;
        }
        else
        {
            if (newExpandedIndex == static_cast<size_t>(m_model.expandedIndex))
                return true;

            ModelEntry::PathIterator retractBeginIt = oldExpandedPath.cbegin();
            ModelEntry::PathIterator retractEndIt = oldExpandedPath.cbegin();

            for (ModelEntry::PathIterator newPathIt = newExpandedPath.cbegin();
                 retractEndIt != oldExpandedPath.cend()
                 && newPathIt != newExpandedPath.cend();
                 ++newPathIt,
                 ++retractEndIt)
            {
                if (*retractEndIt != *newPathIt)
                    break;
            }

            std::tuple<int, int> retractRange = m_model.getRetractRange(
                retractBeginIt,
                retractEndIt,
                ModelEntry::RectractChildren);
            if (std::get<0>(retractRange) != -1)
            {
                q->beginRemoveRows({}, std::get<0>(retractRange), std::get<1>(retractRange));
                m_model.retract(retractBeginIt, retractEndIt);
                q->endRemoveRows();
            }

            std::tuple<int, int> expandRange = m_model.getExpandIndexRange(newExpandedPath.cbegin(), newExpandedPath.cend(), 0);
            if (std::get<0>(expandRange) != -1)
            {
                q->beginInsertRows({}, std::get<0>(expandRange), std::get<1>(expandRange));
                m_model.expand(newExpandedPath.cbegin(), newExpandedPath.cend());
                q->endInsertRows();
            }

            //get current item new position
            int itemIndexRow = 0;
            for (size_t i : newExpandedPath)
                itemIndexRow += i+1;
            itemIndexRow -= 1;

            auto itemIndex = q->index(itemIndexRow);

            q->dataChanged(itemIndex, itemIndex, {NavigationModel::EXPANDED});
        }

        return true;
    }


    NavigationModel* q_ptr = nullptr;

    ModelEntry m_model;
    bool m_hasMedialib = false;

    Q_DECLARE_PUBLIC(NavigationModel);
};

NavigationModel::NavigationModel(QObject *parent)
    : QAbstractListModel{parent}
    , d_ptr(new NavigationModelPrivate(this))
{
}

NavigationModel::~NavigationModel()
{}

//QAbstractListModel

QHash<int,QByteArray> NavigationModel::roleNames() const
{
    return {
        {TITLE, QByteArrayLiteral("title") },
        {URI, QByteArrayLiteral("uri") },
        {DEPTH, QByteArrayLiteral("depth") },
        {ICON, QByteArrayLiteral("icon") },
        {EXPANDABLE, QByteArrayLiteral("expandable") },
        {EXPANDED, QByteArrayLiteral("expanded") },
    };
}

QVariant NavigationModel::data(const QModelIndex &index, int role) const
{
    Q_D(const NavigationModel);
    const int row = index.row();
    if (row < 0)
        return {};

    std::vector<size_t> path;
    bool ret = d->m_model.getIndexPath(row, path);
    if (!ret)
        return {};

    switch (role) {
    case TITLE:
        return d->m_model.getName(path.cbegin(), path.cend());
    case URI:
    {
        QStringList outlist;
        d->m_model.getUri(path.cbegin(), path.cend(), std::back_inserter(outlist));
        //don't advertise our root note
        outlist.pop_front();
        return outlist;
    }
    case DEPTH:
        //don't avertise our root node as part of the depth
        return d->m_model.getDepth(path.cbegin(), path.cend()) - 1;
    case ICON:
        return d->m_model.getIcon(path.cbegin(), path.cend());
    case EXPANDABLE:
        return d->m_model.isExpandable(path.cbegin(), path.cend());
    case EXPANDED:
        return d->m_model.isExpanded(path.cbegin(), path.cend());
    default:
        break;
    }
    return QVariant{};
}

bool NavigationModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    Q_D(NavigationModel);
    const int row = index.row();
    if (row < 0)
        return {};

    if (role != EXPANDED)
        return false;

    return d->setExpanded(row, value.toBool());
}

int NavigationModel::rowCount(const QModelIndex &) const
{
    Q_D(const NavigationModel);
    return d->m_model.visibleChildren;
}

//QML parser status

void NavigationModel::classBegin()
{
}

void NavigationModel::componentComplete()
{
    Q_D(NavigationModel);
    d->initilializeModel();
}

//properties

bool NavigationModel::hasMedialib() const
{
    Q_D(const NavigationModel);
    return d->m_hasMedialib;
}

void NavigationModel::setHasMedialib(bool hasMedialib)
{
    Q_D(NavigationModel);
    if (d->m_hasMedialib == hasMedialib)
        return;
    d->m_hasMedialib = hasMedialib;
    emit hasMedialibChanged(hasMedialib);
}
