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

#include "util/vlctick.hpp"

MLAlbum::MLAlbum(const vlc_ml_album_t *_data)
    : MLItem        ( MLItemId( _data->i_id, VLC_ML_PARENT_ALBUM ) )
    , m_title       ( QString::fromUtf8( _data->psz_title ) )
    , m_releaseYear ( _data->i_year )
    , m_shortSummary( QString::fromUtf8( _data->psz_summary ) )
    , m_cover       ( QString::fromUtf8( _data->thumbnails[VLC_ML_THUMBNAIL_SMALL].psz_mrl ) )
    , m_mainArtist  ( QString::fromUtf8( _data->psz_artist ) )
    , m_nbTracks    ( _data->i_nb_tracks )
    , m_duration    ( _data->i_duration )
{
    assert( _data );
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

VLCTick MLAlbum::getDuration() const
{
    return VLCTick::fromMS(m_duration);
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


