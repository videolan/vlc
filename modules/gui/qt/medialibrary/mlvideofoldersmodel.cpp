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

#include "mlvideofoldersmodel.hpp"

// VLC includes
#include <vlc_media_library.h>

// Util includes
#include "util/covergenerator.hpp"

// MediaLibrary includes
#include "mlcustomcover.hpp"
#include "mlfolder.hpp"

// Static variables

// NOTE: We multiply by 3 to cover most dpi settings.
static const int MLVIDEOFOLDERSMODEL_COVER_WIDTH  = 260 * 3; // 16 / 10 ratio
static const int MLVIDEOFOLDERSMODEL_COVER_HEIGHT = 162 * 3;

static const QHash<QByteArray, vlc_ml_sorting_criteria_t> criterias =
{
    { "title",    VLC_ML_SORTING_ALPHA    },
    { "duration", VLC_ML_SORTING_DURATION }
};

// Ctor / dtor

/* explicit */ MLVideoFoldersModel::MLVideoFoldersModel(QObject * parent) : MLBaseModel(parent) {}

// MLBaseModel reimplementation

QHash<int, QByteArray> MLVideoFoldersModel::roleNames() const /* override */
{
    return {
        { FOLDER_ID, "id" },
        { FOLDER_TITLE, "title" },
        { FOLDER_THUMBNAIL, "thumbnail" },
        { FOLDER_DURATION, "duration" },
        { FOLDER_COUNT, "count"},
    };
}

// Protected MLVideoModel implementation

QVariant MLVideoFoldersModel::itemRoleData(MLItem * item, const int role) const /* override */
{
    if (item == nullptr)
        return QVariant();

    MLFolder * folder = static_cast<MLFolder *> (item);

    switch (role)
    {
        // NOTE: This is the condition for QWidget view(s).
        case Qt::DisplayRole:
            return QVariant::fromValue(folder->getTitle());
        // NOTE: These are the conditions for QML view(s).
        case FOLDER_ID:
            return QVariant::fromValue(folder->getId());
        case FOLDER_TITLE:
            return QVariant::fromValue(folder->getTitle());
        case FOLDER_THUMBNAIL:
        {
            return ml()->customCover()->get(folder->getId()
                                            , QSize(MLVIDEOFOLDERSMODEL_COVER_WIDTH, MLVIDEOFOLDERSMODEL_COVER_HEIGHT)
                                            , QStringLiteral(":/noart_videoCover.svg"));
        }
        case FOLDER_DURATION:
            return QVariant::fromValue(folder->getDuration());
        case FOLDER_COUNT:
            return QVariant::fromValue(folder->getVideoCount());
        default:
            return QVariant();
    }
}

vlc_ml_sorting_criteria_t MLVideoFoldersModel::roleToCriteria(int role) const /* override */
{
    switch (role)
    {
        case FOLDER_TITLE:
            return VLC_ML_SORTING_ALPHA;
        case FOLDER_DURATION:
            return VLC_ML_SORTING_DURATION;
        default:
            return VLC_ML_SORTING_DEFAULT;
    }
}

vlc_ml_sorting_criteria_t MLVideoFoldersModel::nameToCriteria(QByteArray name) const /* override */
{
    return criterias.value(name, VLC_ML_SORTING_DEFAULT);
}

QByteArray MLVideoFoldersModel::criteriaToName(vlc_ml_sorting_criteria_t criteria) const
/* override */
{
    return criterias.key(criteria, "");
}

std::unique_ptr<MLBaseModel::BaseLoader>
MLVideoFoldersModel::createLoader() const /* override */
{
    return std::make_unique<Loader>(*this);
}

// Protected MLBaseModel reimplementation

void MLVideoFoldersModel::onVlcMlEvent(const MLEvent & event) /* override */
{
    int type = event.i_type;

    switch (type)
    {
        case VLC_ML_EVENT_FOLDER_ADDED:
        {
            emit resetRequested();
            return;
        }
        case VLC_ML_EVENT_FOLDER_UPDATED:
        {
            MLItemId itemId(event.modification.i_entity_id, VLC_ML_PARENT_FOLDER);
            updateItemInCache(itemId);
            return;
        }
        case VLC_ML_EVENT_FOLDER_DELETED:
        {
            MLItemId itemId(event.deletion.i_entity_id, VLC_ML_PARENT_FOLDER);
            deleteItemInCache(itemId);
            return;
        }
        default:
            break;
    }

    MLBaseModel::onVlcMlEvent(event);
}

// Loader

MLVideoFoldersModel::Loader::Loader(const MLVideoFoldersModel & model)
    : MLBaseModel::BaseLoader(model) {}

size_t MLVideoFoldersModel::Loader::count(vlc_medialibrary_t * ml) const /* override */
{
    vlc_ml_query_params_t params = getParams().toCQueryParams();

    return vlc_ml_count_folders_by_type(ml, &params, VLC_ML_MEDIA_TYPE_VIDEO);
}

std::vector<std::unique_ptr<MLItem>>
MLVideoFoldersModel::Loader::load(vlc_medialibrary_t * ml,
                                  size_t index, size_t count) const /* override */
{
    vlc_ml_query_params_t params = getParams(index, count).toCQueryParams();

    ml_unique_ptr<vlc_ml_folder_list_t> list(vlc_ml_list_folders_by_type(ml, &params,
                                                                         VLC_ML_MEDIA_TYPE_VIDEO));

    if (list == nullptr)
        return {};

    std::vector<std::unique_ptr<MLItem>> result;

    for (const vlc_ml_folder_t & folder : ml_range_iterate<vlc_ml_folder_t>(list))
    {
        result.emplace_back(std::make_unique<MLFolder>(&folder));
    }

    return result;
}

std::unique_ptr<MLItem>
MLVideoFoldersModel::Loader::loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const
{
    assert(itemId.type == VLC_ML_PARENT_FOLDER);
    ml_unique_ptr<vlc_ml_folder_t> folder(vlc_ml_get_folder(ml, itemId.id));
    if (!folder)
        return nullptr;
    return std::make_unique<MLFolder>(folder.get());
}
