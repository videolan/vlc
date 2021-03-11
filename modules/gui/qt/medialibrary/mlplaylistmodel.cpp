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
    {"duration_short", VLC_ML_SORTING_DURATION},
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
    assert(m_ml);

    int64_t id = parentId().id;

    assert(id);

    for (const QVariant & variant : items)
    {
        if (variant.canConvert<QmlInputItem>() == false)
            continue;

        const QmlInputItem & item = variant.value<QmlInputItem>();

        const char * psz_uri = item.item->psz_uri;

        if (psz_uri == nullptr)
            continue;

        vlc_ml_media_t * media = vlc_ml_get_media_by_mrl(m_ml, psz_uri);

        if (media == nullptr)
        {
            media = vlc_ml_new_external_media(m_ml, psz_uri);

            if (media == nullptr)
                continue;
        }

        vlc_ml_playlist_insert(m_ml, id, media->i_id, at);

        vlc_ml_media_release(media);

        at++;
    }
}

/* Q_INVOKABLE */ void MLPlaylistModel::move(const QModelIndexList & indexes, int to)
{
    assert(m_ml);

    int64_t id = parentId().id;

    assert(id);

    int count = rowCount();

    QList<int> rows = getRows(indexes);

    std::sort(rows.begin(), rows.end());

    for (auto it = rows.begin(); it != rows.end(); it++)
    {
        int from = *it;

        if (from < 0 || from > count || to < 0 || to > count)
            continue;

        if (from > to || from < (to - 1))
        {
            beginMoveRows(QModelIndex(), from, from, QModelIndex(), to);

            if (from < to)
                to--;

            vlc_ml_playlist_move(m_ml, id, from, to);

            endMoveRows();

            to++;
        }
        else if (from == to) {
            to++;
        }

        // NOTE: Fixing the next index(s) according to the previous move.
        for (auto itB = it; itB != rows.end(); itB++)
        {
            int index = *itB;

            if (index > from && index < to)
                (*itB)--;
        }
    }
}

/* Q_INVOKABLE */ void MLPlaylistModel::remove(const QModelIndexList & indexes)
{
    assert(m_ml);

    int64_t id = parentId().id;

    assert(id);

    QList<int> rows = getRows(indexes);

    // NOTE: This is useful to avoid fixing the next index after each remove.
    std::sort(rows.begin(), rows.end(), std::greater<int>());

    for (int from : rows)
    {
        if (from < 0 || from >= rowCount())
            continue;

        beginRemoveRows(QModelIndex(), from, from);

        vlc_ml_playlist_remove(m_ml, id, from);

        endRemoveRows();
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
        { MEDIA_TITLE,              "title"              },
        { MEDIA_THUMBNAIL,          "thumbnail"          },
        { MEDIA_DURATION,           "duration"           },
        { MEDIA_DURATION_SHORT,     "duration_short"     },
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

QVariant MLPlaylistModel::data(const QModelIndex & index, int role) const /* override */
{
    MLPlaylistMedia * media = static_cast<MLPlaylistMedia *>(item(index.row()));

    if (media == nullptr)
        return QVariant();

    switch (role)
    {
        case MEDIA_ID:
            return QVariant::fromValue(media->getId());
        case MEDIA_TITLE:
            return QVariant::fromValue(media->getTitle());
        case MEDIA_THUMBNAIL:
            return QVariant::fromValue(media->getThumbnail());
        case MEDIA_DURATION:
            return QVariant::fromValue(media->getDuration());
        case MEDIA_DURATION_SHORT:
            return QVariant::fromValue(media->getDurationShort());
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
        case MEDIA_DURATION_SHORT:
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

ListCacheLoader<std::unique_ptr<MLItem>> * MLPlaylistModel::createLoader() const /* override */
{
    return new Loader(*this);
}

//-------------------------------------------------------------------------------------------------
// Private functions

QList<int> MLPlaylistModel::getRows(const QModelIndexList & indexes) const
{
    QList<int> rows;

    for (const QModelIndex & index : indexes)
    {
        rows.append(index.row());
    }

    return rows;
}

//-------------------------------------------------------------------------------------------------
// Private MLBaseModel reimplementation
//-------------------------------------------------------------------------------------------------

void MLPlaylistModel::onVlcMlEvent(const MLEvent & event) /* override */
{
    if (event.i_type == VLC_ML_EVENT_PLAYLIST_UPDATED)
    {
        m_need_reset = true;

        // NOTE: Maybe we should call this from MLBaseModel ?
        emit resetRequested();
    }

    MLBaseModel::onVlcMlEvent(event);
}

//=================================================================================================
// Loader
//=================================================================================================

MLPlaylistModel::Loader::Loader(const MLPlaylistModel & model) : MLBaseModel::BaseLoader(model) {}

size_t MLPlaylistModel::Loader::count() const /* override */
{
    vlc_ml_query_params_t params = getParams().toCQueryParams();

    return vlc_ml_count_playlist_media(m_ml, &params, m_parent.id);
}

std::vector<std::unique_ptr<MLItem>>
MLPlaylistModel::Loader::load(size_t index, size_t count) const /* override */
{
    vlc_ml_query_params_t params = getParams(index, count).toCQueryParams();

    ml_unique_ptr<vlc_ml_media_list_t> list {
        vlc_ml_list_playlist_media(m_ml, &params, m_parent.id)
    };

    if (list == nullptr)
        return {};

    std::vector<std::unique_ptr<MLItem>> result;

    for (const vlc_ml_media_t & media : ml_range_iterate<vlc_ml_media_t> (list))
    {
        result.emplace_back(std::make_unique<MLPlaylistMedia>(m_ml, &media));
    }

    return result;
}
