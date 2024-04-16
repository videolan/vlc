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
#include "mlartist.hpp"

MLArtist::MLArtist(const vlc_ml_artist_t* _data)
    : MLItem    ( MLItemId( _data->i_id, VLC_ML_PARENT_ARTIST ) )
    , m_name    ( QString::fromUtf8( _data->psz_name ) )
    , m_shortBio( QString::fromUtf8( _data->psz_shortbio ) )
    , m_cover   ( QString::fromUtf8( _data->thumbnails[VLC_ML_THUMBNAIL_SMALL].psz_mrl ) )
    , m_nbAlbums( _data->i_nb_album )
    , m_nbTracks( _data->i_nb_tracks )
{
    assert( _data );
}

QString MLArtist::getName() const
{
    return m_name;
}

QString MLArtist::getShortBio() const
{
    return m_shortBio;
}

QString MLArtist::getCover() const
{
    return m_cover;
}

unsigned int MLArtist::getNbAlbums() const
{
    return m_nbAlbums;
}


unsigned int MLArtist::getNbTracks() const
{
    return m_nbTracks;
}
