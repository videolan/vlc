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

#include <cassert>
#include "mlalbumtrack.hpp"
#include "mlhelper.hpp"

MLAlbumTrack::MLAlbumTrack(vlc_medialibrary_t* _ml, const vlc_ml_media_t *_data, QObject *_parent )
    : QObject( _parent )
    , m_id         ( _data->i_id, VLC_ML_PARENT_UNKNOWN )
    , m_title      ( QString::fromUtf8( _data->psz_title ) )
    , m_trackNumber( _data->album_track.i_track_nb )
    , m_discNumber( _data->album_track.i_disc_nb )
{
    assert( _data );
    assert( _data->i_type == VLC_ML_MEDIA_TYPE_AUDIO );

    int t_sec = _data->i_duration / 1000;
    int sec = t_sec % 60;
    int min = (t_sec / 60) % 60;
    int hour = t_sec / 3600;
    if (hour == 0)
    {
        m_duration = QString("%1:%2")
                .arg(min, 2, 10, QChar('0'))
                .arg(sec, 2, 10, QChar('0'));
        m_durationShort = m_duration;
    }
    else
    {
        m_duration = QString("%1:%2:%3")
                .arg(hour, 2, 10, QChar('0'))
                .arg(min, 2, 10, QChar('0'))
                .arg(sec, 2, 10, QChar('0'));
        m_durationShort = QString("%1h%2")
                .arg(hour)
                .arg(min, 2, 10, QChar('0'));
    }

    for( const vlc_ml_file_t& file: ml_range_iterate<vlc_ml_file_t>( _data->p_files ) )
        if( file.i_type == VLC_ML_FILE_TYPE_MAIN )
        {
            //FIXME should we store every mrl
            m_mrl = QString::fromUtf8(file.psz_mrl);
            break;
        }

    for( const vlc_ml_thumbnail_t& thumbnail: _data->thumbnails )
        if( thumbnail.b_generated )
        {
            m_cover = QString::fromUtf8(thumbnail.psz_mrl);
            break;
        }

    if ( _data->album_track.i_album_id != 0 )
    {
        ml_unique_ptr<vlc_ml_album_t> album(vlc_ml_get_album(_ml, _data->album_track.i_album_id));
        if (album)
             m_albumTitle =  album->psz_title;
    }

    if ( _data->album_track.i_artist_id != 0 )
    {
        ml_unique_ptr<vlc_ml_artist_t> artist(vlc_ml_get_artist(_ml, _data->album_track.i_artist_id));
        if (artist)
             m_artist =  artist->psz_name;
    }
}

MLAlbumTrack::MLAlbumTrack(const MLAlbumTrack &albumtrack, QObject *_parent)
    : QObject( _parent )
    , m_id           ( albumtrack.m_id )
    , m_title        ( albumtrack.m_title )
    , m_albumTitle   ( albumtrack.m_albumTitle )
    , m_artist       ( albumtrack.m_artist )
    , m_cover        ( albumtrack.m_cover )
    , m_trackNumber  ( albumtrack.m_trackNumber )
    , m_discNumber   ( albumtrack.m_discNumber )
    , m_duration     ( albumtrack.m_duration )
    , m_durationShort( albumtrack.m_durationShort )
    , m_mrl          ( albumtrack.m_mrl )
{
}

MLParentId MLAlbumTrack::getId() const
{
    return m_id;
}

QString MLAlbumTrack::getTitle() const
{
    return m_title;
}

QString MLAlbumTrack::getAlbumTitle() const
{
    return m_albumTitle;
}

QString MLAlbumTrack::getArtist() const
{
    return m_artist;
}

QString MLAlbumTrack::getCover() const
{
    return m_cover;
}

unsigned int MLAlbumTrack::getTrackNumber() const
{
    return m_trackNumber;
}

unsigned int MLAlbumTrack::getDiscNumber() const
{
    return m_discNumber;
}

QString MLAlbumTrack::getDuration() const
{
    return m_duration;
}

QString MLAlbumTrack::getDurationShort() const
{
    return m_durationShort;
}

QString MLAlbumTrack::getMRL() const
{
    return m_mrl;
}

MLAlbumTrack *MLAlbumTrack::clone(QObject *parent) const
{
    return new MLAlbumTrack(*this, parent);
}

