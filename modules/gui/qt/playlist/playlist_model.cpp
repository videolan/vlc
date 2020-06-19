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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "playlist_model.hpp"
#include "playlist_model_p.hpp"
#include <algorithm>
#include <assert.h>

namespace vlc {
namespace playlist {

namespace {

static QVector<PlaylistItem> toVec(vlc_playlist_item_t *const items[],
                                   size_t len)
{
    QVector<PlaylistItem> vec;
    for (size_t i = 0; i < len; ++i)
        vec.push_back(items[i]);
    return vec;
}

template <typename RAW, typename WRAPPER>
static QVector<RAW> toRaw(const QVector<WRAPPER> &items)
{
    QVector<RAW> vec;
    int count = items.size();
    vec.reserve(count);
    for (int i = 0; i < count; ++i)
        vec.push_back(items[i].raw());
    return vec;
}
}

extern "C" { // for C callbacks

static void
on_playlist_items_reset(vlc_playlist_t *playlist,
                        vlc_playlist_item_t *const items[],
                        size_t len, void *userdata)
{
    PlaylistListModelPrivate *that = static_cast<PlaylistListModelPrivate *>(userdata);
    QVector<PlaylistItem> newContent = toVec(items, len);
    that->callAsync([=]() {
        if (that->m_playlist != playlist)
            return;
        that->onItemsReset(newContent);
    });
}

static void
on_playlist_items_added(vlc_playlist_t *playlist, size_t index,
                        vlc_playlist_item_t *const items[], size_t len,
                        void *userdata)
{
    PlaylistListModelPrivate *that = static_cast<PlaylistListModelPrivate *>(userdata);
    QVector<PlaylistItem> added = toVec(items, len);
    that->callAsync([=]() {
        if (that->m_playlist != playlist)
            return;
        that->onItemsAdded(added, index);
    });
}

static void
on_playlist_items_moved(vlc_playlist_t *playlist, size_t index, size_t count,
                        size_t target, void *userdata)
{
    PlaylistListModelPrivate *that = static_cast<PlaylistListModelPrivate *>(userdata);
    that->callAsync([=]() {
        if (that->m_playlist != playlist)
            return;
        that->onItemsMoved(index, count, target);
    });
}

static void
on_playlist_items_removed(vlc_playlist_t *playlist, size_t index, size_t count,
                          void *userdata)
{
    PlaylistListModelPrivate *that = static_cast<PlaylistListModelPrivate *>(userdata);
    that->callAsync([=](){
        if (that->m_playlist != playlist)
            return;
        that->onItemsRemoved(index, count);
    });
}

static void
on_playlist_items_updated(vlc_playlist_t *playlist, size_t index,
                          vlc_playlist_item_t *const items[], size_t len,
                          void *userdata)
{
    PlaylistListModelPrivate *that = static_cast<PlaylistListModelPrivate *>(userdata);
    QVector<PlaylistItem> updated = toVec(items, len);
    that->callAsync([=](){
        if (that->m_playlist != playlist)
            return;
        int count = updated.size();
        for (int i = 0; i < count; ++i)
            that->m_items[index + i] = updated[i]; /* sync metadata */
        that->notifyItemsChanged(index, count);
    });
}

static void
on_playlist_current_item_changed(vlc_playlist_t *playlist, ssize_t index,
                                 void *userdata)
{
    PlaylistListModelPrivate *that = static_cast<PlaylistListModelPrivate *>(userdata);
    that->callAsync([=](){
        if (that->m_playlist != playlist)
            return;
        ssize_t oldCurrent = that->m_current;
        that->m_current = index;
        if (oldCurrent != -1)
            that->notifyItemsChanged(oldCurrent, 1, {PlaylistListModel::IsCurrentRole});
        if (index != -1)
            that->notifyItemsChanged(index, 1, {PlaylistListModel::IsCurrentRole});
        emit that->q_func()->currentIndexChanged(index);
    });
}

} // extern "C"

static const struct vlc_playlist_callbacks playlist_callbacks = {
    /* C++ (before C++20) does not support designated initializers */
    on_playlist_items_reset,
    on_playlist_items_added,
    on_playlist_items_moved,
    on_playlist_items_removed,
    on_playlist_items_updated,
    nullptr,
    nullptr,
    on_playlist_current_item_changed,
    nullptr,
    nullptr,
};

// private API

PlaylistListModelPrivate::PlaylistListModelPrivate(PlaylistListModel* playlistListModel)
    : q_ptr(playlistListModel)
{
}

PlaylistListModelPrivate::~PlaylistListModelPrivate()
{
    if (m_playlist && m_listener)
    {
        PlaylistLocker locker(m_playlist);
        vlc_playlist_RemoveListener(m_playlist, m_listener);
    }
}

void PlaylistListModelPrivate::onItemsReset(const QVector<PlaylistItem>& newContent)
{
    Q_Q(PlaylistListModel);
    q->beginResetModel();
    m_items = newContent;
    q->endResetModel();

    m_duration = VLC_TICK_FROM_SEC(0);
    if (m_items.size())
    {
        for(const auto& i : m_items)
        {
            m_duration += i.getDuration();
        }
    }

    emit q->countChanged(m_items.size());
    emit q->selectedCountChanged();
}

void PlaylistListModelPrivate::onItemsAdded(const QVector<PlaylistItem>& added, size_t index)
{
    Q_Q(PlaylistListModel);
    int count = added.size();
    q->beginInsertRows({}, index, index + count - 1);
    m_items.insert(index, count, nullptr);
    std::move(added.cbegin(), added.cend(), m_items.begin() + index);
    q->endInsertRows();

    for(const auto& i : added)
    {
        m_duration += i.getDuration();
    }

    emit q->countChanged(m_items.size());
}

void PlaylistListModelPrivate::onItemsMoved(size_t index, size_t count, size_t target)
{
    Q_Q(PlaylistListModel);
    size_t qtTarget = target;
    if (qtTarget > index)
        /* Qt interprets the target index as the index of the insertion _before_
       * the move, while the playlist core interprets it as the new index of
       * the slice _after_ the move. */
        qtTarget += count;

    q->beginMoveRows({}, index, index + count - 1, {}, qtTarget);
    if (index < target)
        std::rotate(m_items.begin() + index,
                    m_items.begin() + index + count,
                    m_items.begin() + target + count);
    else
        std::rotate(m_items.begin() + target,
                    m_items.begin() + index,
                    m_items.begin() + index + count);
    q->endMoveRows();
}

void PlaylistListModelPrivate::onItemsRemoved(size_t index, size_t count)
{
    Q_Q(PlaylistListModel);

    for(size_t i = index; i < count; ++i)
    {
        m_duration -= m_items.at(i).getDuration();
    }

    q->beginRemoveRows({}, index, index + count - 1);
    m_items.remove(index, count);
    q->endRemoveRows();

    emit q->countChanged(m_items.size());
    emit q->selectedCountChanged();
}


void
PlaylistListModelPrivate::notifyItemsChanged(int idx, int count, const QVector<int> &roles)
{
    Q_Q(PlaylistListModel);
    QModelIndex first = q->index(idx, 0);
    QModelIndex last = q->index(idx + count - 1);
    emit q->dataChanged(first, last, roles);
}

// public API

PlaylistListModel::PlaylistListModel(QObject *parent)
    : SelectableListModel(parent)
    , d_ptr(new PlaylistListModelPrivate(this))
{
}

PlaylistListModel::PlaylistListModel(vlc_playlist_t *raw_playlist, QObject *parent)
    : SelectableListModel(parent)
    , d_ptr(new PlaylistListModelPrivate(this))
{
    setPlaylistId(PlaylistPtr(raw_playlist));
}

PlaylistListModel::~PlaylistListModel()
{
}

bool PlaylistListModel::isRowSelected(int row) const
{
    Q_D(const PlaylistListModel);
    return d->m_items[row].isSelected();
}

void PlaylistListModel::setRowSelected(int row, bool selected)
{
    Q_D(PlaylistListModel);
    return d->m_items[row].setSelected(selected);
}

int PlaylistListModel::getSelectedRole() const
{
    return SelectedRole;
}

QHash<int, QByteArray>
PlaylistListModel::roleNames() const
{
    return {
        { TitleRole, "title" },
        { DurationRole, "duration" },
        { IsCurrentRole, "isCurrent" },
        { ArtistRole , "artist" },
        { AlbumRole  , "album" },
        { ArtworkRole, "artwork" },
        { SelectedRole, "selected" },
    };
}

int
PlaylistListModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    Q_D(const PlaylistListModel);
    if (! d->m_playlist)
        return 0;
    return d->m_items.size();
}

VLCTick
PlaylistListModel::getDuration() const
{
    Q_D(const PlaylistListModel);
    return VLCTick(d->m_duration);
}

const PlaylistItem &
PlaylistListModel::itemAt(int index) const
{
    Q_D(const PlaylistListModel);
    return d->m_items[index];
}

void PlaylistListModel::removeItems(const QList<int>& indexes)
{
    Q_D(PlaylistListModel);
    if (!d->m_playlist)
        return;
    if (indexes.size() == 0)
        return;
    QVector<vlc_playlist_item_t *> itemsToRemove;
    std::transform(indexes.begin(), indexes.end(),std::back_inserter(itemsToRemove), [&] (int index) {
        return d->m_items[index].raw();
    });

    {
        PlaylistLocker locker(d->m_playlist);
        int ret = vlc_playlist_RequestRemove(d->m_playlist, itemsToRemove.constData(),
                                             itemsToRemove.size(), indexes[0]);
        if (ret != VLC_SUCCESS)
            throw std::bad_alloc();
    }
}

/**
 * Return the target position *after* the move has been applied, knowing the
 * index *before* the move and the list of indexes to move.
 *
 * The core playlist interprets the move target as the index of the (first)
 * item *after* the move. During a drag&drop, we know the index *before* the
 * move.
 */
static int
getMovePostTarget(const QList<int> &sortedIndexesToMove, int preTarget)
{
    int postTarget = preTarget;
    for (int index : sortedIndexesToMove)
    {
        if (index >= preTarget)
            break;
        postTarget--;
    }
    return postTarget;
}

void
PlaylistListModel::moveItems(const QList<int> &sortedIndexes, int target,
                             bool isPreTarget)
{
    Q_D(PlaylistListModel);
    if (!d->m_playlist)
        return;
    if (sortedIndexes.size() == 0)
        return;
    QVector<vlc_playlist_item_t*> itemsToMove;
    std::transform(sortedIndexes.begin(), sortedIndexes.end(),
                   std::back_inserter(itemsToMove),
                   [&] (int index) {
                       return d->m_items[index].raw();
                   });

    if (isPreTarget) {
        /* convert pre-target to post-target */
        target = getMovePostTarget(sortedIndexes, target);
    }

    PlaylistLocker locker(d->m_playlist);
    int ret = vlc_playlist_RequestMove(d->m_playlist, itemsToMove.constData(),
                                       itemsToMove.size(), target,
                                       sortedIndexes[0]);
    if (ret != VLC_SUCCESS)
        throw std::bad_alloc();
}

void
PlaylistListModel::moveItemsPre(const QList<int> &sortedIndexes, int preTarget)
{
    return moveItems(sortedIndexes, preTarget, true);
}

void
PlaylistListModel::moveItemsPost(const QList<int> &sortedIndexes,
                                 int postTarget)
{
    return moveItems(sortedIndexes, postTarget, false);
}

int PlaylistListModel::getCurrentIndex() const
{
    Q_D(const PlaylistListModel);
    return d->m_current;
}

PlaylistPtr PlaylistListModel::getPlaylistId() const
{
    Q_D(const PlaylistListModel);
    if (!d->m_playlist)
        return {};
    return PlaylistPtr(d->m_playlist);
}

void PlaylistListModel::setPlaylistId(vlc_playlist_t* playlist)
{
    Q_D(PlaylistListModel);
    if (d->m_playlist && d->m_listener)
    {
        PlaylistLocker locker(d->m_playlist);
        vlc_playlist_RemoveListener(d->m_playlist, d->m_listener);
        d->m_playlist = nullptr;
        d->m_listener = nullptr;
    }
    if (playlist)
    {
        PlaylistLocker locker(playlist);
        d->m_playlist = playlist;
        d->m_listener = vlc_playlist_AddListener(d->m_playlist, &playlist_callbacks, d, true);
    }
    emit playlistIdChanged( PlaylistPtr(d->m_playlist) );
}

void PlaylistListModel::setPlaylistId(PlaylistPtr id)
{
    setPlaylistId(id.m_playlist);
}

QVariant
PlaylistListModel::data(const QModelIndex &index, int role) const
{
    Q_D(const PlaylistListModel);
    if (!d->m_playlist)
        return {};

    ssize_t row = index.row();
    if (row < 0 || row >= d->m_items.size())
        return {};

    switch (role)
    {
    case TitleRole:
        return d->m_items[row].getTitle();
    case IsCurrentRole:
        return row == d->m_current;
    case DurationRole:
    {
        int64_t t_sec = SEC_FROM_VLC_TICK(d->m_items[row].getDuration());
        int sec = t_sec % 60;
        int min = (t_sec / 60) % 60;
        int hour = t_sec / 3600;
        if (hour == 0)
            return QString("%1:%2")
                    .arg(min, 2, 10, QChar('0'))
                    .arg(sec, 2, 10, QChar('0'));
        else
            return QString("%1:%2:%3")
                    .arg(hour, 2, 10, QChar('0'))
                    .arg(min, 2, 10, QChar('0'))
                    .arg(sec, 2, 10, QChar('0'));
    }
    case ArtistRole:
        return d->m_items[row].getArtist();
    case AlbumRole:
        return d->m_items[row].getAlbum();
    case ArtworkRole:
        return d->m_items[row].getArtwork();
    case SelectedRole:
        return d->m_items[row].isSelected();
    default:
        return {};
    }
}

} // namespace playlist
} // namespace vlc
