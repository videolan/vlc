/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mlgrouplistmodel.hpp"

// VLC includes
#include <vlc_media_library.h>

// Util includes
#include "util/covergenerator.hpp"

// MediaLibrary includes
#include "mlhelper.hpp"
#include "mlgroup.hpp"
#include "mlvideo.hpp"

//-------------------------------------------------------------------------------------------------
// Static variables

// NOTE: We multiply by 2 to cover most dpi settings.
static const int MLGROUPLISTMODEL_COVER_WIDTH  = 512 * 2; // 16 / 10 ratio
static const int MLGROUPLISTMODEL_COVER_HEIGHT = 320 * 2;

static const QHash<QByteArray, vlc_ml_sorting_criteria_t> criterias =
{
    {"id",   VLC_ML_SORTING_DEFAULT},
    {"name", VLC_ML_SORTING_ALPHA},
    {"date", VLC_ML_SORTING_INSERTIONDATE}
};

//=================================================================================================
// MLGroupListModel
//=================================================================================================

/* explicit */ MLGroupListModel::MLGroupListModel(vlc_medialibrary_t * ml, QObject * parent)
    : MLBaseModel(parent)
{
    m_ml = ml;
}

/* explicit */ MLGroupListModel::MLGroupListModel(QObject * parent)
    : MLBaseModel(parent) {}

//-------------------------------------------------------------------------------------------------
// QAbstractItemModel implementation
//-------------------------------------------------------------------------------------------------

QHash<int, QByteArray> MLGroupListModel::roleNames() const /* override */
{
    return
    {
        { GROUP_ID,                 "id"                 },
        { GROUP_NAME,               "name"               },
        { GROUP_THUMBNAIL,          "thumbnail"          },
        { GROUP_DURATION,           "duration"           },
        { GROUP_DATE,               "date"               },
        { GROUP_COUNT,              "count"              },
        // NOTE: Media specific.
        { GROUP_TITLE,              "title"              },
        { GROUP_RESOLUTION,         "resolution_name"    },
        { GROUP_CHANNEL,            "channel"            },
        { GROUP_MRL,                "mrl"                },
        { GROUP_MRL_DISPLAY,        "display_mrl"        },
        { GROUP_PROGRESS,           "progress"           },
        { GROUP_PLAYCOUNT,          "playcount"          },
        { GROUP_VIDEO_TRACK,        "videoDesc"          },
        { GROUP_AUDIO_TRACK,        "audioDesc"          },
        { GROUP_TITLE_FIRST_SYMBOL, "title_first_symbol" }
    };
}

QVariant MLGroupListModel::data(const QModelIndex & index, int role) const /* override */
{
    int row = index.row();

    MLItem * item = this->item(row);

    if (item == nullptr)
        return QVariant();

    if (item->getId().type == VLC_ML_PARENT_GROUP)
    {
        MLGroup * group = static_cast<MLGroup *> (item);

        switch (role)
        {
            // NOTE: This is the condition for QWidget view(s).
            case Qt::DisplayRole:
                if (index.column() == 0)
                    return QVariant::fromValue(group->getName());
                else
                    return QVariant();
            // NOTE: These are the conditions for QML view(s).
            case GROUP_ID:
                return QVariant::fromValue(group->getId());
            case GROUP_NAME:
                return QVariant::fromValue(group->getName());
            case GROUP_THUMBNAIL:
                return getCover(group, row);
            case GROUP_DURATION:
                return QVariant::fromValue(group->getDuration());
            case GROUP_DATE:
                return QVariant::fromValue(group->getDate());
            case GROUP_COUNT:
                return QVariant::fromValue(group->getCount());
            default:
                return QVariant();
        }
    }
    else
    {
        MLVideo * video = static_cast<MLVideo *> (item);

        switch (role)
        {
            // NOTE: This is the condition for QWidget view(s).
            case Qt::DisplayRole:
                if (index.column() == 0)
                    return QVariant::fromValue(video->getTitle());
                else
                    return QVariant();
            // NOTE: These are the conditions for QML view(s).
            case GROUP_ID:
                return QVariant::fromValue(video->getId());
            case GROUP_NAME:
                return QVariant::fromValue(video->getTitle());
            case GROUP_THUMBNAIL:
                return QVariant::fromValue(video->getThumbnail());
            case GROUP_DURATION:
                return QVariant::fromValue(video->getDuration());
            case GROUP_DATE:
                return QVariant();
            case GROUP_COUNT:
                return 1;
            // NOTE: Media specific.
            case GROUP_TITLE:
                return QVariant::fromValue(video->getTitle());
            case GROUP_RESOLUTION:
                return QVariant::fromValue(video->getResolutionName());
            case GROUP_CHANNEL:
                return QVariant::fromValue(video->getChannel());
            case GROUP_MRL:
                return QVariant::fromValue(video->getMRL());
            case GROUP_MRL_DISPLAY:
                return QVariant::fromValue(video->getDisplayMRL());
            case GROUP_PROGRESS:
                return QVariant::fromValue(video->getProgress());
            case GROUP_PLAYCOUNT:
                return QVariant::fromValue(video->getPlayCount());
            case GROUP_VIDEO_TRACK:
                return QVariant::fromValue(video->getVideoDesc());
            case GROUP_AUDIO_TRACK:
                return QVariant::fromValue(video->getAudioDesc());
            case GROUP_TITLE_FIRST_SYMBOL:
                return QVariant::fromValue(getFirstSymbol(video->getTitle()));
            default:
                return QVariant();
        }
    }
}

//-------------------------------------------------------------------------------------------------
// Protected MLBaseModel implementation
//-------------------------------------------------------------------------------------------------

vlc_ml_sorting_criteria_t MLGroupListModel::roleToCriteria(int role) const /* override */
{
    switch (role)
    {
        case GROUP_NAME:
            return VLC_ML_SORTING_ALPHA;
        case GROUP_DATE:
            return VLC_ML_SORTING_INSERTIONDATE;
        default:
            return VLC_ML_SORTING_DEFAULT;
    }
}

vlc_ml_sorting_criteria_t MLGroupListModel::nameToCriteria(QByteArray name) const /* override */
{
    return criterias.value(name, VLC_ML_SORTING_DEFAULT);
}

QByteArray MLGroupListModel::criteriaToName(vlc_ml_sorting_criteria_t criteria) const
/* override */
{
    return criterias.key(criteria, "");
}

//-------------------------------------------------------------------------------------------------

ListCacheLoader<std::unique_ptr<MLItem>> * MLGroupListModel::createLoader() const /* override */
{
    return new Loader(*this);
}

//-------------------------------------------------------------------------------------------------
// Private functions
//-------------------------------------------------------------------------------------------------

QString MLGroupListModel::getCover(MLGroup * group, int index) const
{
    QString cover = group->getCover();

    // NOTE: Making sure we're not already generating a cover.
    if (cover.isNull() == false || group->hasGenerator())
        return cover;

    CoverGenerator * generator = new CoverGenerator(m_ml, group->getId(), index);

    generator->setSize(QSize(MLGROUPLISTMODEL_COVER_WIDTH,
                             MLGROUPLISTMODEL_COVER_HEIGHT));

    generator->setDefaultThumbnail(":/noart_videoCover.svg");

    // NOTE: We'll apply the new thumbnail once it's loaded.
    connect(generator, &CoverGenerator::result, this, &MLGroupListModel::onCover);

    generator->start(*QThreadPool::globalInstance());

    group->setGenerator(generator);

    return cover;
}

//-------------------------------------------------------------------------------------------------
// Private MLBaseModel reimplementation
//-------------------------------------------------------------------------------------------------

void MLGroupListModel::onVlcMlEvent(const MLEvent & event) /* override */
{
    int type = event.i_type;

    if (type == VLC_ML_EVENT_GROUP_ADDED || type == VLC_ML_EVENT_GROUP_UPDATED
        ||
        type == VLC_ML_EVENT_GROUP_DELETED)
    {
        m_need_reset = true;

        // NOTE: Maybe we should call this from MLBaseModel ?
        emit resetRequested();
    }

    MLBaseModel::onVlcMlEvent(event);
}

void MLGroupListModel::thumbnailUpdated(int idx) /* override */
{
    QModelIndex index = this->index(idx);

    emit dataChanged(index, index, { GROUP_THUMBNAIL });
}

//-------------------------------------------------------------------------------------------------
// Private slots
//-------------------------------------------------------------------------------------------------

void MLGroupListModel::onCover()
{
    CoverGenerator * generator = static_cast<CoverGenerator *> (sender());

    int index = generator->getIndex();

    // NOTE: We want to avoid calling 'MLBaseModel::item' for performance issues.
    MLItem * item = this->itemCache(index);

    // NOTE: When the item is no longer cached or has been moved we return right away.
    if (item == nullptr || item->getId() != generator->getId())
    {
        generator->deleteLater();

        return;
    }

    MLGroup * group = static_cast<MLGroup *> (item);

    QString fileName = QUrl::fromLocalFile(generator->takeResult()).toString();

    group->setCover(fileName);

    group->setGenerator(nullptr);

    thumbnailUpdated(index);
}

//=================================================================================================
// Loader
//=================================================================================================

MLGroupListModel::Loader::Loader(const MLGroupListModel & model)
    : MLBaseModel::BaseLoader(model) {}

size_t MLGroupListModel::Loader::count() const /* override */
{
    vlc_ml_query_params_t params = getParams().toCQueryParams();

    return vlc_ml_count_groups(m_ml, &params);
}

std::vector<std::unique_ptr<MLItem>>
MLGroupListModel::Loader::load(size_t index, size_t count) const /* override */
{
    vlc_ml_query_params_t params = getParams(index, count).toCQueryParams();

    ml_unique_ptr<vlc_ml_group_list_t> list(vlc_ml_list_groups(m_ml, &params));

    if (list == nullptr)
        return {};

    std::vector<std::unique_ptr<MLItem>> result;

    for (const vlc_ml_group_t & group : ml_range_iterate<vlc_ml_group_t>(list))
    {
        // NOTE: When it's a group of one we convert it to a MLVideo.
        if (group.i_nb_total_media == 1)
        {
            vlc_ml_query_params_t query;

            memset(&query, 0, sizeof(vlc_ml_query_params_t));

            ml_unique_ptr<vlc_ml_media_list_t> list(vlc_ml_list_group_media(m_ml,
                                                                            &query, group.i_id));

            // NOTE: Do we really need to check 'i_nb_items' here ?
            if (list->i_nb_items == 1)
            {
                result.emplace_back(std::make_unique<MLVideo>(m_ml, &(list->p_items[0])));

                continue;
            }
        }

        result.emplace_back(std::make_unique<MLGroup>(m_ml, &group));
    }

    return result;
}
