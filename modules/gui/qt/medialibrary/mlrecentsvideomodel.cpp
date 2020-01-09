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

namespace {

enum Role {
    VIDEO_ID = Qt::UserRole + 1,
    VIDEO_TITLE,
    VIDEO_THUMBNAIL,
    VIDEO_DURATION,
    VIDEO_PROGRESS,
    VIDEO_PLAYCOUNT,
    VIDEO_RESOLUTION,
    VIDEO_CHANNEL,
    VIDEO_MRL,
    VIDEO_VIDEO_TRACK,
    VIDEO_AUDIO_TRACK,
};

}

MLRecentsVideoModel::MLRecentsVideoModel( QObject* parent )
    : MLSlidingWindowModel<MLVideo>( parent )
{
}

QVariant MLRecentsVideoModel::data( const QModelIndex& index , int role ) const
{
    const auto video = item( static_cast<unsigned int>( index.row() ) );
    if ( video == nullptr )
        return {};
    switch ( role )
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
        case VIDEO_RESOLUTION:
            return QVariant::fromValue( video->getResolutionName() );
        case VIDEO_CHANNEL:
            return QVariant::fromValue( video->getChannel() );
        case VIDEO_MRL:
            return QVariant::fromValue( video->getMRL() );
        case VIDEO_VIDEO_TRACK:
            return QVariant::fromValue( video->getVideoDesc() );
        case VIDEO_AUDIO_TRACK:
            return QVariant::fromValue( video->getAudioDesc() );
        default:
            return {};
    }
}

QHash<int, QByteArray> MLRecentsVideoModel::roleNames() const
{
    return {
        { VIDEO_ID, "id" },
        { VIDEO_TITLE, "title" },
        { VIDEO_THUMBNAIL, "thumbnail" },
        { VIDEO_DURATION, "duration" },
        { VIDEO_PROGRESS, "progress" },
        { VIDEO_PLAYCOUNT, "playcount" },
        { VIDEO_RESOLUTION, "resolution_name" },
        { VIDEO_CHANNEL, "channel" },
        { VIDEO_MRL, "mrl" },
        { VIDEO_AUDIO_TRACK, "audioDesc" },
        { VIDEO_VIDEO_TRACK, "videoDesc" },
    };
}

std::vector<std::unique_ptr<MLVideo> > MLRecentsVideoModel::fetch()
{
    ml_unique_ptr<vlc_ml_media_list_t> media_list{ vlc_ml_list_history(
                m_ml, &m_query_param ) };
    if ( media_list == nullptr )
        return {};
    std::vector<std::unique_ptr<MLVideo>> res;
    m_video_count = 0;
    for( vlc_ml_media_t &media: ml_range_iterate<vlc_ml_media_t>( media_list ) )
        if( media.i_type == VLC_ML_MEDIA_TYPE_VIDEO )
        {
            m_video_count++;
            res.emplace_back( std::make_unique<MLVideo>( m_ml, &media ) );
        }
    return res;
}

size_t MLRecentsVideoModel::countTotalElements() const
{

    if(numberOfItemsToShow == -1){
        return m_video_count;
    }
    return std::min(m_video_count,numberOfItemsToShow);
}

void MLRecentsVideoModel::onVlcMlEvent( const vlc_ml_event_t* event )
{
    switch ( event->i_type )
    {
        case VLC_ML_EVENT_MEDIA_ADDED:
        case VLC_ML_EVENT_MEDIA_UPDATED:
        case VLC_ML_EVENT_MEDIA_DELETED:
            m_need_reset = true;
            break;
        default:
            break;
    }
    MLBaseModel::onVlcMlEvent( event );
}
void MLRecentsVideoModel::setNumberOfItemsToShow( int n ){
    numberOfItemsToShow = n;
}
int MLRecentsVideoModel::getNumberOfItemsToShow(){
    return numberOfItemsToShow;
}
