/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mlplaylistmodel.hpp"

// Util includes
#include "util/qmlinputitem.hpp"
#include "util/vlctick.hpp"

// MediaLibrary includes
#include "mlhelper.hpp"
#include "mlplaylistmedia.hpp"

//-------------------------------------------------------------------------------------------------
// Static variables

static const QHash<QByteArray, vlc_ml_sorting_criteria_t> criterias =
{
    {"id",             VLC_ML_SORTING_DEFAULT},
    {"title",          VLC_ML_SORTING_ALPHA},
    {"duration",       VLC_ML_SORTING_DURATION},
    {"playcount",      VLC_ML_SORTING_PLAYCOUNT},
};

//=================================================================================================
// MLPlaylistModel
//=================================================================================================

/* explicit */ MLPlaylistModel::MLPlaylistModel(QObject * parent)
    : MLBaseModel(parent) {}

//-------------------------------------------------------------------------------------------------
// Interface
//-------------------------------------------------------------------------------------------------

/* Q_INVOKABLE */ void MLPlaylistModel::insert(const QVariantList & items, int at)
{
    assert(m_mediaLib);

    int64_t id = parentId().id;

    assert(id);

    if (unlikely(m_transactionPending))
        return;

    //build the list of MRL to insert in the playlist
    std::vector<QString> mrlList;
    for (const QVariant & variant : items)
    {
        if (variant.canConvert<QmlInputItem>() == false)
            continue;

        const QmlInputItem & item = variant.value<QmlInputItem>();

        const char * psz_uri = item.item ? item.item->psz_uri : nullptr;

        if (psz_uri == nullptr)
            continue;

        mrlList.emplace_back(psz_uri);
    }

    m_transactionPending = true;

    m_mediaLib->runOnMLThread(this,
    //ML thread
    [mrlList, id, at](vlc_medialibrary_t* ml) {
        int insertPos = at;
        for (const QString& uri : mrlList)
        {
            vlc_ml_media_t * media = vlc_ml_get_media_by_mrl(ml, qtu(uri));

            if (media == nullptr)
            {
                media = vlc_ml_new_external_media(ml, qtu(uri));
                if (media == nullptr)
                    continue;
            }

            vlc_ml_playlist_insert(ml, id, media->i_id, insertPos);
            vlc_ml_media_release(media);

            insertPos++;
        }

    },
    //UI thread
    [this]() {
        m_need_reset = true;
        endTransaction();
    });
}


void MLPlaylistModel::moveImpl(int64_t playlistId, HighLowRanges&& ranges)
{
    struct Ctx {
        int newTo;
    };
    int low;
    int high;
    if (ranges.lowRangeIt > 0)
    {
        std::tie(low, high) = ranges.lowRanges[ranges.lowRangeIt - 1];

        assert(low <= high);
        m_mediaLib->runOnMLThread<Ctx>(this,
        //ML thread
        [playlistId, high, low, to = ranges.lowTo]
        (vlc_medialibrary_t* ml, Ctx& ctx) {
            int localTo = to;
            for (int i = high; i >= low; i--)
            {
                vlc_ml_playlist_move(ml, playlistId, i, localTo  - 1);
                localTo--;
            }
            ctx.newTo = localTo;
        },
        //UI thread
        [this, playlistId, high, low, r = std::move(ranges)](quint64, Ctx& ctx) mutable {
            moveRangeInCache(low, high, r.lowTo);
            r.lowTo = ctx.newTo;
            --r.lowRangeIt;
            moveImpl(playlistId, std::move(r));
        });
    }
    else if (ranges.highRangeIt < ranges.highRanges.size())
    {
        std::tie(low, high) = ranges.highRanges[ranges.highRangeIt];
        assert(low <= high);

        m_mediaLib->runOnMLThread<Ctx>(this,
        //ML thread
        [playlistId, high, low, to = ranges.highTo](vlc_medialibrary_t* ml, Ctx& ctx) {
            int localTo = to;
            for (int i = low; i <= high; i++)
            {
                vlc_ml_playlist_move(ml, playlistId, i, localTo);
                localTo++;
            }
            ctx.newTo = localTo;
        },
        //UI thread
        [this, playlistId, low, high, r = std::move(ranges)](quint64, Ctx& ctx) mutable {
            moveRangeInCache(low, high, r.highTo);
            r.highTo = ctx.newTo;
            ++r.highRangeIt;
            moveImpl(playlistId, std::move(r));
        });
    }
    else
    {
        //we're done
        endTransaction();
    }
}


/**
 * the move operation is done in separate phases
 * * we sort the items, and organise them as "ranges" of consecutive items to
 *   be move (think beginMoveRows/endMoveRows)
 *
 * * and split ranges between thoose who are before and
 *   those who are after the destination point
 *
 * * for each range to move, we will
 *    * beginMoveRows(range)
 *    * move the items in the range on ML thread
 *    * endMoveRows on the UI thread
 *    * move the next range
 *
 *  * the ranges before the destination needs to be moved from the closest to the
 *    destination point to the farthest, so this doesn't mess the indexe, the
 *    same policy applies when moving the items from a given range for the same
 *    reason. Having the data split in before/after the destination point, allows to
 *    do it sequentially without having to guess what is the next segment to move
 */
/* Q_INVOKABLE */ void MLPlaylistModel::move(const QModelIndexList & indexes, int to)
{
    assert(m_mediaLib);

    if (indexes.size() == 0)
        return;

    if (unlikely(m_transactionPending))
        return;

    int64_t id = parentId().id;
    assert(id);
    //get Ranges in asc order
    auto rangeList = getSortedRowsRanges(indexes, true);
    assert(rangeList.size() > 0);

    //split ranges between those who are above and those who below the destination
    HighLowRanges highLowRanges;
    highLowRanges.lowTo = to;
    highLowRanges.highTo = to;

    auto it = rangeList.cbegin();
    while (it != rangeList.end())
    {
        if (it->second < to - 1)
            highLowRanges.lowRanges.push_back(*it);
        else if (it->first > to)
            highLowRanges.highRanges.push_back(*it);
        else
        {
            //range is overlapping destination, it doesn't need to be moved
            highLowRanges.lowTo = it->first;
            highLowRanges.highTo = it->second;
        }
        ++it;
    }
    highLowRanges.lowRangeIt = highLowRanges.lowRanges.size();
    highLowRanges.highRangeIt = 0;

    m_transactionPending = true;

    moveImpl(id, std::move(highLowRanges));
}


void MLPlaylistModel::removeImpl(int64_t playlistId, const std::vector<std::pair<int, int> >&& rangeList, size_t index)
{
    if (index >= rangeList.size())
    {
        //we're done
        endTransaction();
        return;
    }

    std::pair<int, int> range = rangeList[index];

    m_mediaLib->runOnMLThread(this,
    //ML thread
    [playlistId, range](vlc_medialibrary_t* ml) {
        for (int i = range.second; i >= range.first; i--)
        {
            vlc_ml_playlist_remove(ml, playlistId, i);
        }
    },
    //UI thread
    [this, playlistId, range, rows = std::move(rangeList), index]() {
        deleteRangeInCache(range.first, range.second);
        removeImpl(playlistId, std::move(rows), index+1);
    });
}

/* Q_INVOKABLE */ void MLPlaylistModel::remove(const QModelIndexList & indexes)
{
    assert(m_mediaLib);
    if (indexes.size() == 0)
        return;

    if (unlikely(m_transactionPending))
        return;

    int64_t id = parentId().id;
    assert(id);

    //get range in decreasing order to avoid having to update the index
    auto rangeList = getSortedRowsRanges(indexes, false);
    assert(rangeList.size() > 0);

    m_transactionPending = true;
    removeImpl(id, std::move(rangeList), 0);
}

void MLPlaylistModel::endTransaction()
{
    m_transactionPending = false;
    if (m_resetAfterTransaction)
    {
        m_resetAfterTransaction = false;
        emit resetRequested();
    }

}

//-------------------------------------------------------------------------------------------------
// QAbstractItemModel implementation
//-------------------------------------------------------------------------------------------------

QHash<int, QByteArray> MLPlaylistModel::roleNames() const /* override */
{
    return
    {
        { MEDIA_ID,                 "id"                 },
        { MEDIA_IS_NEW,             "isNew"              },
        { MEDIA_TITLE,              "title"              },
        { MEDIA_THUMBNAIL,          "thumbnail"          },
        { MEDIA_DURATION,           "duration"           },
        { MEDIA_PROGRESS,           "progress"           },
        { MEDIA_PLAYCOUNT,          "playcount"          },
        { MEDIA_RESOLUTION,         "resolution_name"    },
        { MEDIA_CHANNEL,            "channel"            },
        { MEDIA_MRL,                "mrl"                },
        { MEDIA_DISPLAY_MRL,        "display_mrl"        },
        { MEDIA_AUDIO_TRACK,        "audioDesc"          },
        { MEDIA_VIDEO_TRACK,        "videoDesc"          },
        { MEDIA_TITLE_FIRST_SYMBOL, "title_first_symbol" }
    };
}

QVariant MLPlaylistModel::itemRoleData(MLItem *item, int role) const /* override */
{
    MLPlaylistMedia * media = static_cast<MLPlaylistMedia *>(item);
    if (media == nullptr)
        return QVariant();

    switch (role)
    {
        case MEDIA_ID:
            return QVariant::fromValue(media->getId());
        case MEDIA_IS_NEW:
            return QVariant::fromValue(media->isNew());
        case MEDIA_TITLE:
            return QVariant::fromValue(media->getTitle());
        case MEDIA_THUMBNAIL:
        {
            vlc_ml_thumbnail_status_t status;
            QString thumbnail = media->getThumbnail(&status);
            if ((media->getType() == VLC_ML_MEDIA_TYPE_VIDEO)
                && (status == VLC_ML_THUMBNAIL_STATUS_MISSING
                    || status == VLC_ML_THUMBNAIL_STATUS_FAILURE))
            {
                generateThumbnail(item->getId());
            }

            return QVariant::fromValue(thumbnail);
        }
        case MEDIA_DURATION:
            return QVariant::fromValue(media->getDuration());
        case MEDIA_PROGRESS:
            return QVariant::fromValue(media->getProgress());
        case MEDIA_PLAYCOUNT:
            return QVariant::fromValue(media->getPlayCount());
        case MEDIA_RESOLUTION:
            return QVariant::fromValue(media->getResolutionName());
        case MEDIA_CHANNEL:
            return QVariant::fromValue(media->getChannel());
        case MEDIA_MRL:
            return QVariant::fromValue(media->getMRL());
        case MEDIA_DISPLAY_MRL:
            return QVariant::fromValue(media->getMRLDisplay());
        case MEDIA_VIDEO_TRACK:
            return QVariant::fromValue(media->getVideo());
        case MEDIA_AUDIO_TRACK:
            return QVariant::fromValue(media->getAudio());
        case MEDIA_TITLE_FIRST_SYMBOL:
            return QVariant::fromValue(getFirstSymbol(media->getTitle()));
        default:
            return QVariant();
    }
}

//-------------------------------------------------------------------------------------------------
// Protected MLBaseModel implementation
//-------------------------------------------------------------------------------------------------

vlc_ml_sorting_criteria_t MLPlaylistModel::roleToCriteria(int role) const /* override */
{
    switch (role)
    {
        case MEDIA_TITLE:
            return VLC_ML_SORTING_ALPHA;
        case MEDIA_DURATION:
            return VLC_ML_SORTING_DURATION;
        case MEDIA_PLAYCOUNT:
            return VLC_ML_SORTING_PLAYCOUNT;
        default:
            return VLC_ML_SORTING_DEFAULT;
    }
}

vlc_ml_sorting_criteria_t MLPlaylistModel::nameToCriteria(QByteArray name) const /* override */
{
    return criterias.value(name, VLC_ML_SORTING_DEFAULT);
}

QByteArray MLPlaylistModel::criteriaToName(vlc_ml_sorting_criteria_t criteria) const /* override */
{
    return criterias.key(criteria, "");
}

//-------------------------------------------------------------------------------------------------

std::unique_ptr<MLBaseModel::BaseLoader>
MLPlaylistModel::createLoader() const /* override */
{
    return std::make_unique<Loader>(*this);
}

//-------------------------------------------------------------------------------------------------
// Protected MLBaseModel reimplementation
//-------------------------------------------------------------------------------------------------

void MLPlaylistModel::onVlcMlEvent(const MLEvent & event) /* override */
{
    switch (event.i_type)
    {
    case VLC_ML_EVENT_MEDIA_UPDATED:
    {
        MLItemId itemId(event.modification.i_entity_id, VLC_ML_PARENT_UNKNOWN);
        updateItemInCache(itemId);
        return;
    }
    case VLC_ML_EVENT_PLAYLIST_UPDATED:
    {
        MLItemId itemId(event.modification.i_entity_id, VLC_ML_PARENT_PLAYLIST);
        if (m_parent == itemId)
        {
            if (m_transactionPending)
                m_resetAfterTransaction = true;
            else
                emit resetRequested();
        }
        return;
    }
    default:
        break;
    }

    MLBaseModel::onVlcMlEvent(event);
}

void MLPlaylistModel::thumbnailUpdated(const QModelIndex& idx, MLItem* mlitem, const QString& mrl, vlc_ml_thumbnail_status_t status) /* override */
{
    auto playlistItem = static_cast<MLPlaylistMedia*>(mlitem);
    playlistItem->setThumbnail(mrl, status);
    emit dataChanged(idx, idx, { MEDIA_THUMBNAIL });
}

//-------------------------------------------------------------------------------------------------
// Private functions
//-------------------------------------------------------------------------------------------------

std::vector<std::pair<int, int>> MLPlaylistModel::getSortedRowsRanges(const QModelIndexList & indexes, bool asc) const
{
    assert (indexes.size() > 0);;

    QList<int> rows;
    for (const QModelIndex & index : indexes)
        rows.append(index.row());

    std::sort(rows.begin(), rows.end());

    //build continuous ranges
    std::vector<std::pair<int, int>> rangeList;
    auto it = rows.cbegin();
    int high = *it;
    int low = *it;
    if (rows.count() == 1)
    {
        rangeList.emplace_back(low, high);
    }
    else
    {
        ++it;
        while (it != rows.cend())
        {
            int value = *it;
            if (value != high + 1)
            {
                rangeList.emplace_back(low, high);
                low = value;
            }
            high = value;
            ++it;
        }
        rangeList.emplace_back(low, high);
    }

    if (!asc)
        std::reverse(rangeList.begin(), rangeList.end());

    return rangeList;
}

void MLPlaylistModel::generateThumbnail(const MLItemId& itemid) const
{
    m_mediaLib->runOnMLThread(this,
    //ML thread
    [id = itemid.id](vlc_medialibrary_t* ml)
    {
        vlc_ml_media_generate_thumbnail(ml, id, VLC_ML_THUMBNAIL_SMALL,
                                        512, 320, 0.15);
    });
}

//=================================================================================================
// Loader
//=================================================================================================

MLPlaylistModel::Loader::Loader(const MLPlaylistModel & model) : MLBaseModel::BaseLoader(model) {}

size_t MLPlaylistModel::Loader::count(vlc_medialibrary_t* ml) const /* override */
{
    vlc_ml_query_params_t params = getParams().toCQueryParams();

    return vlc_ml_count_playlist_media(ml, &params, m_parent.id);
}

std::vector<std::unique_ptr<MLItem>>
MLPlaylistModel::Loader::load(vlc_medialibrary_t* ml, size_t index, size_t count) const /* override */
{
    vlc_ml_query_params_t params = getParams(index, count).toCQueryParams();

    ml_unique_ptr<vlc_ml_media_list_t> list {
        vlc_ml_list_playlist_media(ml, &params, m_parent.id)
    };

    if (list == nullptr)
        return {};

    std::vector<std::unique_ptr<MLItem>> result;

    for (const vlc_ml_media_t & media : ml_range_iterate<vlc_ml_media_t> (list))
    {
        result.emplace_back(std::make_unique<MLPlaylistMedia>(&media));
    }

    return result;
}

std::unique_ptr<MLItem>
MLPlaylistModel::Loader::loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const
{
    assert(itemId.type == VLC_ML_PARENT_UNKNOWN);
    ml_unique_ptr<vlc_ml_media_t> media(vlc_ml_get_media(ml, itemId.id));
    if (!media)
        return nullptr;
    return std::make_unique<MLPlaylistMedia>(media.get());
}
