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

#include "mlvideomodel.hpp"

namespace {

enum Role {
    VIDEO_ID = Qt::UserRole + 1,
    VIDEO_TITLE,
    VIDEO_THUMBNAIL,
    VIDEO_DURATION,
    VIDEO_PROGRESS,
    VIDEO_PLAYCOUNT,
};

}

QHash<QByteArray, vlc_ml_sorting_criteria_t> MLVideoModel::M_names_to_criteria = {
    {"id", VLC_ML_SORTING_DEFAULT},
    {"title", VLC_ML_SORTING_ALPHA},
    {"duration", VLC_ML_SORTING_DURATION},
    {"playcount", VLC_ML_SORTING_PLAYCOUNT},
};

MLVideoModel::MLVideoModel(QObject* parent)
    : MLSlidingWindowModel<MLVideo>(parent)
{
}

QVariant MLVideoModel::data(const QModelIndex& index, int role) const
{
    const auto video = item(static_cast<unsigned int>(index.row()));
    if ( video == nullptr )
        return {};
    switch (role)
    {
        case VIDEO_ID:
            return QVariant::fromValue( video->getId() );
        case VIDEO_TITLE:
            return QVariant::fromValue( video->getTitle() );
        case VIDEO_THUMBNAIL:
            return QVariant::fromValue( video->getThumbnail() );
        case VIDEO_DURATION:
            return QVariant::fromValue( video->getDuration() );
        case VIDEO_PROGRESS:
            return QVariant::fromValue( video->getProgress() );
        case VIDEO_PLAYCOUNT:
            return QVariant::fromValue( video->getPlayCount() );
        default:
            return {};
    }
}

QHash<int, QByteArray> MLVideoModel::roleNames() const
{
    return {
        { VIDEO_ID, "id" },
        { VIDEO_TITLE, "title" },
        { VIDEO_THUMBNAIL, "thumbnail" },
        { VIDEO_DURATION, "duration" },
        { VIDEO_PROGRESS, "progress" },
        { VIDEO_PLAYCOUNT, "playcount" },
    };
}

std::vector<std::unique_ptr<MLVideo> > MLVideoModel::fetch()
{
    ml_unique_ptr<vlc_ml_media_list_t> media_list{ vlc_ml_list_video_media(
                m_ml, &m_query_param ) };
    if ( media_list == nullptr )
        return {};
    std::vector<std::unique_ptr<MLVideo>> res;
    for( vlc_ml_media_t &media: ml_range_iterate<vlc_ml_media_t>( media_list ) )
        res.emplace_back( std::unique_ptr<MLVideo>{ new MLVideo(m_ml, &media) } );
    return res;
}

size_t MLVideoModel::countTotalElements() const
{
    vlc_ml_query_params_t params{};
    return vlc_ml_count_video_media(m_ml, &params);
}

vlc_ml_sorting_criteria_t MLVideoModel::roleToCriteria(int role) const
{
    switch(role)
    {
        case VIDEO_TITLE:
            return VLC_ML_SORTING_ALPHA;
        case VIDEO_DURATION:
            return VLC_ML_SORTING_DURATION;
        case VIDEO_PLAYCOUNT:
            return VLC_ML_SORTING_PLAYCOUNT;
        default:
            return VLC_ML_SORTING_DEFAULT;
    }
}

vlc_ml_sorting_criteria_t MLVideoModel::nameToCriteria(QByteArray name) const
{
    return M_names_to_criteria.value(name, VLC_ML_SORTING_DEFAULT);
}

void MLVideoModel::onVlcMlEvent(const vlc_ml_event_t* event)
{
    switch (event->i_type)
    {
        case VLC_ML_EVENT_MEDIA_ADDED:
        case VLC_ML_EVENT_MEDIA_UPDATED:
            if ( event->modification.p_media->i_type == VLC_ML_MEDIA_TYPE_VIDEO )
                m_need_reset = true;
            break;
        case VLC_ML_EVENT_MEDIA_DELETED:
            m_need_reset = true;
            break;
        default:
            break;
    }
    MLBaseModel::onVlcMlEvent( event );
}
