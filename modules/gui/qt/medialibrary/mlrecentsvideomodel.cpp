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

#include "mlrecentsvideomodel.hpp"
#include "mlhelper.hpp"

// Ctor / dtor

MLRecentsVideoModel::MLRecentsVideoModel(QObject * parent) : MLVideoModel(parent) {}

// Protected MLBaseModel implementation

std::unique_ptr<MLBaseModel::BaseLoader>
MLRecentsVideoModel::createLoader() const
/* override */
{
    return std::make_unique<Loader>(*this);
}

// Private MLVideoModel reimplementation

void MLRecentsVideoModel::onVlcMlEvent(const MLEvent & event) /* override */
{
    switch (event.i_type)
    {
        case VLC_ML_EVENT_HISTORY_CHANGED:
            emit resetRequested();
            return;
        default:
            break;
    }

    MLVideoModel::onVlcMlEvent(event);
}

// Loader

MLRecentsVideoModel::Loader::Loader(const MLRecentsVideoModel & model)
    : MLBaseModel::BaseLoader(model)
{}

size_t MLRecentsVideoModel::Loader::count(vlc_medialibrary_t* ml) const /* override */
{
    MLQueryParams params = getParams();

    auto queryParams = params.toCQueryParams();
    return vlc_ml_count_history_by_type(ml, &queryParams, VLC_ML_MEDIA_TYPE_VIDEO);
}

std::vector<std::unique_ptr<MLItem>>
MLRecentsVideoModel::Loader::load(vlc_medialibrary_t* ml, size_t index, size_t count) const /* override */
{
    MLQueryParams params = getParams(index, count);
    auto queryParams = params.toCQueryParams();

    ml_unique_ptr<vlc_ml_media_list_t> media_list {
        vlc_ml_list_history_by_type(ml, &queryParams, VLC_ML_MEDIA_TYPE_VIDEO)
    };

    if (media_list == nullptr)
        return {};

    std::vector<std::unique_ptr<MLItem>> res;

    for (const vlc_ml_media_t & media : ml_range_iterate<vlc_ml_media_t>(media_list))
    {
        res.emplace_back( std::make_unique<MLVideo>(&media));
    }

    return res;
}

std::unique_ptr<MLItem>
MLRecentsVideoModel::Loader::loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const
{
    assert(itemId.type == VLC_ML_PARENT_UNKNOWN);
    ml_unique_ptr<vlc_ml_media_t> media(vlc_ml_get_media(ml, itemId.id));
    if (!media)
        return nullptr;
    return std::make_unique<MLVideo>(media.get());
}
