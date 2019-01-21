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

#include "mlvideo.hpp"

#include <cassert>

#include <vlc_thumbnailer.h>

MLVideo::MLVideo(vlc_medialibrary_t* ml, const vlc_ml_media_t* data, QObject* parent)
    : QObject( parent )
    , m_ml( ml )
    , m_id( data->i_id, VLC_ML_PARENT_UNKNOWN )
    , m_title( QString::fromUtf8( data->psz_title ) )
    , m_thumbnail( QString::fromUtf8( data->psz_artwork_mrl ) )
    , m_playCount( data->i_playcount )
    , m_thumbnailGenerated( data->b_artwork_generated )
    , m_ml_event_handle( nullptr, [this](vlc_ml_event_callback_t* cb ) {
        assert( m_ml != nullptr );
        vlc_ml_event_unregister_callback( m_ml, cb );
    })
{
    assert( data->i_type == VLC_ML_MEDIA_TYPE_VIDEO );

    int t_sec = data->i_duration / 1000000;
    int sec = t_sec % 60;
    int min = (t_sec / 60) % 60;
    int hour = t_sec / 3600;
    if (hour == 0)
        m_duration = QString("%1:%2")
                .arg(min, 2, 10, QChar('0'))
                .arg(sec, 2, 10, QChar('0'));
    else
        m_duration = QString("%1:%2:%3")
                .arg(hour, 2, 10, QChar('0'))
                .arg(min, 2, 10, QChar('0'))
                .arg(sec, 2, 10, QChar('0'));

    for( const vlc_ml_file_t& file: ml_range_iterate<vlc_ml_file_t>( data->p_files ) )
        if( file.i_type == VLC_ML_FILE_TYPE_MAIN )
        {
            //FIXME should we store every mrl
            m_mrl = QString::fromUtf8(file.psz_mrl);
            break;
        }
    char *psz_progress;
    if ( vlc_ml_media_get_playback_pref( ml, data->i_id, VLC_ML_PLAYBACK_PREF_PROGRESS,
                                    &psz_progress ) == VLC_SUCCESS && psz_progress != NULL )
    {
        m_progress = atoi( psz_progress );
        free( psz_progress );
    }
}

MLVideo::MLVideo(const MLVideo& video, QObject* parent)
    : QObject( parent )
    , m_ml( video.m_ml )
    , m_id( video.m_id )
    , m_title( video.m_title )
    , m_thumbnail( video.m_thumbnail )
    , m_duration( video.m_duration )
    , m_mrl( video.m_mrl )
    , m_progress( video.m_progress )
    , m_playCount( video.m_playCount )
{
}

void MLVideo::onMlEvent( void* data, const vlc_ml_event_t* event )
{
    auto self = static_cast<MLVideo*>(data);
    self->onMlEvent(event);
}

void MLVideo::onMlEvent( const vlc_ml_event_t* event )
{
    if ( event->i_type != VLC_ML_EVENT_MEDIA_THUMBNAIL_GENERATED )
        return;
    m_thumbnailGenerated = true;
    if ( event->media_thumbnail_generated.p_media->i_id != m_id.id )
        return;
    if ( event->media_thumbnail_generated.b_success == false )
        return;
    auto thumbnailMrl = event->media_thumbnail_generated.p_media->psz_artwork_mrl;
    m_thumbnail = QString::fromUtf8( thumbnailMrl );
    vlc_ml_event_unregister_from_callback( m_ml, m_ml_event_handle.release() );
    emit onThumbnailChanged( m_thumbnail );
}

MLParentId MLVideo::getId() const
{
    return m_id;
}

QString MLVideo::getTitle() const
{
    return m_title;
}

QString MLVideo::getThumbnail()
{
    if ( m_thumbnailGenerated == false )
    {
        m_ml_event_handle.reset( vlc_ml_event_register_callback( m_ml, onMlEvent, this ) );
        vlc_ml_media_generate_thumbnail( m_ml, m_id.id );
    }

    return m_thumbnail;
}

QString MLVideo::getDuration() const
{
    return m_duration;
}

QString MLVideo::getMRL() const
{
    return m_mrl;
}

unsigned int MLVideo::getProgress() const
{
    return m_progress;
}

unsigned int MLVideo::getPlayCount() const
{
    return m_playCount;
}

MLVideo*MLVideo::clone(QObject* parent) const
{
    return new MLVideo(*this, parent);
}
