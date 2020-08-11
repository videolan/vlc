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

VideoDescription::VideoDescription(const QString &codec, const QString &language, const unsigned int fps, QObject *parent)
    : QObject(parent)
    , m_codec(codec)
    , m_language(language)
    , m_fps(fps)
{
}

QString VideoDescription::getCodec() const
{
    return m_codec;
}

QString VideoDescription::getLanguage() const
{
    return m_language;
}

unsigned int VideoDescription::getFps() const
{
    return m_fps;
}

AudioDescription::AudioDescription(const QString &codec, const QString &language, const unsigned int nbChannels, const unsigned int sampleRate, QObject *parent)
    : QObject(parent)
    , m_codec(codec)
    , m_language(language)
    , m_nbchannels(nbChannels)
    , m_sampleRate(sampleRate)
{
}

QString AudioDescription::getCodec() const
{
    return m_codec;
}

QString AudioDescription::getLanguage() const
{
    return m_language;
}

unsigned int AudioDescription::getNbChannels() const
{
    return m_nbchannels;
}

unsigned int AudioDescription::getSampleRate() const
{
    return m_sampleRate;
}

MLVideo::MLVideo(vlc_medialibrary_t* ml, const vlc_ml_media_t* data, QObject* parent)
    : QObject( parent )
    , m_ml( ml )
    , m_id( data->i_id, VLC_ML_PARENT_UNKNOWN )
    , m_title( QString::fromUtf8( data->psz_title ) )
    , m_thumbnail( QString::fromUtf8( data->thumbnails[VLC_ML_THUMBNAIL_SMALL].psz_mrl ) )
    , m_progress( -1.f )
    , m_playCount( data->i_playcount )
    , m_thumbnailGenerated( data->thumbnails[VLC_ML_THUMBNAIL_SMALL].b_generated )
    , m_ml_event_handle( nullptr, [this](vlc_ml_event_callback_t* cb ) {
        assert( m_ml != nullptr );
        vlc_ml_event_unregister_callback( m_ml, cb );
    })
{
    assert( data->i_type == VLC_ML_MEDIA_TYPE_VIDEO || data->i_type == VLC_ML_MEDIA_TYPE_UNKNOWN );

    m_duration = data->i_duration;

    for( const vlc_ml_file_t& file: ml_range_iterate<vlc_ml_file_t>( data->p_files ) )
        if( file.i_type == VLC_ML_FILE_TYPE_MAIN )
        {
            //FIXME should we store every mrl
            m_mrl = QUrl::fromEncoded(file.psz_mrl);
            break;
        }
    char *psz_progress;
    if ( vlc_ml_media_get_playback_state( ml, data->i_id, VLC_ML_PLAYBACK_STATE_PROGRESS,
                                    &psz_progress ) == VLC_SUCCESS && psz_progress != NULL )
    {
        m_progress = atof( psz_progress );
        free( psz_progress );
    }

    unsigned int numChannel = 0 , maxWidth = 0 , maxHeight = 0;
    for( const vlc_ml_media_track_t& track: ml_range_iterate<vlc_ml_media_track_t>( data->p_tracks ) ) {
        if ( track.i_type == VLC_ML_TRACK_TYPE_AUDIO ) {
            numChannel = std::max( numChannel , track.a.i_nbChannels );

            m_audioDesc.push_back( new AudioDescription ( QString::fromUtf8( track.psz_codec ) ,
                                                         QString::fromUtf8( track.psz_language  ) ,
                                                         track.a.i_nbChannels ,
                                                         track.a.i_sampleRate ,
                                                         this )
                                   );
        }
        else if ( track.i_type == VLC_ML_TRACK_TYPE_VIDEO ){
            maxWidth = std::max( maxWidth, track.v.i_width );
            maxHeight = std::max( maxHeight, track.v.i_height );

            m_videoDesc.push_back(  new VideoDescription( QString::fromUtf8( track.psz_codec ) ,
                                                        QString::fromUtf8( track.psz_language ) ,
                                                        track.v.i_fpsNum,
                                                        this )
                                  );
        }
    }

    m_channel = "";
    if ( numChannel >= 8 )
        m_channel = "7.1";
    else if ( numChannel >= 6 )
        m_channel = "5.1";

    m_resolution = "";
    if ( maxWidth >= 7680 && maxHeight >= 4320 )
        m_resolution = "8K";
    else if ( maxWidth >= 3840 && maxHeight >= 2160 )
        m_resolution = "4K";
    else if ( maxWidth >= 1440 && maxHeight >= 1080 )
        m_resolution = "HD";
    else if ( maxWidth >= 720 && maxHeight >= 1280 )
        m_resolution = "720p";  
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
    if ( event->i_type != VLC_ML_EVENT_MEDIA_THUMBNAIL_GENERATED ||
         event->media_thumbnail_generated.i_size != VLC_ML_THUMBNAIL_SMALL )
        return;
    m_thumbnailGenerated = true;
    if ( event->media_thumbnail_generated.p_media->i_id != m_id.id )
        return;
    if ( event->media_thumbnail_generated.b_success == false )
        return;
    auto thumbnailMrl = event->media_thumbnail_generated
            .p_media->thumbnails[event->media_thumbnail_generated.i_size].psz_mrl;
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
        vlc_ml_media_generate_thumbnail( m_ml, m_id.id, VLC_ML_THUMBNAIL_SMALL,
                                         512, 320, .15 );
    }

    return m_thumbnail;
}

QString MLVideo::getDuration() const
{
    return MsToString( m_duration );
}

QString MLVideo::getDurationShort() const
{
    return MsToString( m_duration, true );
}

QString MLVideo::getMRL() const
{
    return m_mrl.toEncoded();
}

QString MLVideo::getDisplayMRL() const
{
    return m_mrl.toString(QUrl::PrettyDecoded | QUrl::RemoveUserInfo | QUrl::PreferLocalFile | QUrl::NormalizePathSegments);
}

QString MLVideo::getResolutionName() const
{
    return m_resolution;
}
QString MLVideo::getChannel() const
{
    return m_channel;
}

float MLVideo::getProgress() const
{
    return m_progress;
}

unsigned int MLVideo::getPlayCount() const
{
    return m_playCount;
}

QString MLVideo::getProgressTime() const
{
    return MsToString(m_duration * m_progress);
}

QObjectList MLVideo::getVideoDesc() const
{
    return m_videoDesc;
}

QObjectList MLVideo::getAudioDesc() const
{
    return m_audioDesc;
}

MLVideo*MLVideo::clone(QObject* parent) const
{
    return new MLVideo(*this, parent);
}
