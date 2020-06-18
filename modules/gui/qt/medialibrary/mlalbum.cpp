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
#include "mlalbum.hpp"

MLAlbum::MLAlbum(vlc_medialibrary_t* _ml, const vlc_ml_album_t *_data, QObject *_parent)
    : QObject( _parent )
    , m_ml          ( _ml )
    , m_id          ( _data->i_id, VLC_ML_PARENT_ALBUM )
    , m_title       ( QString::fromUtf8( _data->psz_title ) )
    , m_releaseYear ( _data->i_year )
    , m_shortSummary( QString::fromUtf8( _data->psz_summary ) )
    , m_cover       ( QString::fromUtf8( _data->thumbnails[VLC_ML_THUMBNAIL_SMALL].psz_mrl ) )
    , m_mainArtist  ( QString::fromUtf8( _data->psz_artist ) )
    , m_nbTracks    ( _data->i_nb_tracks )
{
    assert( _data );
    assert( _ml );

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
}

//private ctor for cloning
MLAlbum::MLAlbum(const MLAlbum& _album, QObject *_parent)
    : QObject( _parent )
    , m_ml          ( _album.m_ml )
    , m_id          ( _album.m_id )
    , m_title       ( _album.m_title )
    , m_releaseYear ( _album.m_releaseYear )
    , m_shortSummary( _album.m_shortSummary )
    , m_cover       ( _album.m_cover )
    , m_mainArtist  ( _album.m_mainArtist )
    , m_nbTracks    ( _album.m_nbTracks )
    , m_duration    ( _album.m_duration )
{
}

MLParentId MLAlbum::getId() const
{
    return m_id;
}

QString MLAlbum::getTitle() const
{
    return m_title;
}

unsigned int MLAlbum::getReleaseYear() const
{
    return  m_releaseYear;
}

QString MLAlbum::getShortSummary() const
{
    return m_shortSummary;
}

QString MLAlbum::getCover() const
{
    return m_cover;
}


QString MLAlbum::getArtist() const
{
    return m_mainArtist;
}

unsigned int MLAlbum::getNbTracks() const
{
    return m_nbTracks;
}

QString MLAlbum::getDuration() const
{
    return m_duration;
}

QString MLAlbum::getDurationShort() const
{
    return m_durationShort;
}

MLAlbum *MLAlbum::clone(QObject *parent) const
{
    return new MLAlbum(*this, parent);
}

QString MLAlbum::getPresName() const
{
    return m_title;
}

QString MLAlbum::getPresImage() const
{
    return m_cover;
}

QString MLAlbum::getPresInfo() const
{
    return m_shortSummary;
}


