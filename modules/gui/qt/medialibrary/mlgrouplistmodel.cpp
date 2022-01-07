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
    { "title",    VLC_ML_SORTING_ALPHA         },
    { "duration", VLC_ML_SORTING_DURATION      },
    { "date",     VLC_ML_SORTING_INSERTIONDATE }
};

//=================================================================================================
// MLGroupListModel
//=================================================================================================

/* explicit */ MLGroupListModel::MLGroupListModel(QObject * parent) : MLVideoModel(parent) {}

//-------------------------------------------------------------------------------------------------
// MLVideoModel reimplementation
//-------------------------------------------------------------------------------------------------

QHash<int, QByteArray> MLGroupListModel::roleNames() const /* override */
{
    QHash<int, QByteArray> hash = MLVideoModel::roleNames();

    hash.insert(GROUP_IS_VIDEO, "isVideo");
    hash.insert(GROUP_DATE,     "date");
    hash.insert(GROUP_COUNT,    "count");

    return hash;
}

// Protected MLVideoModel implementation

QVariant MLGroupListModel::itemRoleData(MLItem * item, const int role) const /* override */
{
    if (item == nullptr)
        return QVariant();

    if (item->getId().type == VLC_ML_PARENT_GROUP)
    {
        MLGroup * group = static_cast<MLGroup *> (item);

        switch (role)
        {
            // NOTE: This is the condition for QWidget view(s).
            case Qt::DisplayRole:
                return QVariant::fromValue(group->getTitle());
            // NOTE: These are the conditions for QML view(s).
            case VIDEO_ID:
                return QVariant::fromValue(group->getId());
            case VIDEO_TITLE:
                return QVariant::fromValue(group->getTitle());
            case VIDEO_THUMBNAIL:
                return getVideoListCover(this, group, MLGROUPLISTMODEL_COVER_WIDTH,
                                         MLGROUPLISTMODEL_COVER_HEIGHT, VIDEO_THUMBNAIL);
            case VIDEO_DURATION:
                return QVariant::fromValue(group->getDuration());
            case GROUP_IS_VIDEO:
                return false;
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
            case Qt::DisplayRole:
                return QVariant::fromValue(video->getTitle());
            case GROUP_IS_VIDEO:
                return true;
            case GROUP_DATE:
                return QVariant();
            case GROUP_COUNT:
                return 1;
            // NOTE: Media specific.
            default:
                return MLVideoModel::itemRoleData(item, role);
        }
    }
}

vlc_ml_sorting_criteria_t MLGroupListModel::roleToCriteria(int role) const /* override */
{
    switch (role)
    {
        case VIDEO_TITLE:
            return VLC_ML_SORTING_ALPHA;
        case VIDEO_DURATION:
            return VLC_ML_SORTING_DURATION;
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

ListCacheLoader<std::unique_ptr<MLItem>> * MLGroupListModel::createLoader() const /* override */
{
    return new Loader(*this);
}

void MLGroupListModel::onVlcMlEvent(const MLEvent & event) /* override */
{
    int type = event.i_type;

    switch (type)
    {
        case VLC_ML_EVENT_GROUP_ADDED:
        case VLC_ML_EVENT_GROUP_UPDATED:
        case VLC_ML_EVENT_GROUP_DELETED:
        {
            m_need_reset = true;

            // NOTE: Maybe we should call this from MLBaseModel ?
            emit resetRequested();
            break;
        }
        default:
            break;
    }

    MLBaseModel::onVlcMlEvent(event);
}

//=================================================================================================
// Loader
//=================================================================================================

MLGroupListModel::Loader::Loader(const MLGroupListModel & model)
    : MLBaseModel::BaseLoader(model) {}

size_t MLGroupListModel::Loader::count(vlc_medialibrary_t* ml) const /* override */
{
    vlc_ml_query_params_t params = getParams().toCQueryParams();

    return vlc_ml_count_groups(ml, &params);
}

std::vector<std::unique_ptr<MLItem>>
MLGroupListModel::Loader::load(vlc_medialibrary_t* ml, size_t index, size_t count) const /* override */
{
    vlc_ml_query_params_t params = getParams(index, count).toCQueryParams();

    ml_unique_ptr<vlc_ml_group_list_t> list(vlc_ml_list_groups(ml, &params));

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

            ml_unique_ptr<vlc_ml_media_list_t> list(vlc_ml_list_group_media(ml,
                                                                            &query, group.i_id));

            // NOTE: Do we really need to check 'i_nb_items' here ?
            if (list->i_nb_items == 1)
            {
                result.emplace_back(std::make_unique<MLVideo>(&(list->p_items[0])));

                continue;
            }
        }

        result.emplace_back(std::make_unique<MLGroup>(&group));
    }

    return result;
}
