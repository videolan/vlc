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

MLArtist::MLArtist(const vlc_ml_artist_t* _data, QObject *_parent)
    : QObject(_parent)
    , m_id      ( _data->i_id, VLC_ML_PARENT_ARTIST )
    , m_name    ( QString::fromUtf8( _data->psz_name ) )
    , m_shortBio( QString::fromUtf8( _data->psz_shortbio ) )
    , m_cover   ( QString::fromUtf8( _data->thumbnails[VLC_ML_THUMBNAIL_SMALL].psz_mrl ) )
    , m_nbAlbums( _data->i_nb_album )
{
    assert( _data );
}

MLArtist::MLArtist(const MLArtist &artist, QObject *_parent)
    : QObject(_parent)
    , m_id      ( artist.m_id )
    , m_name    ( artist.m_name )
    , m_shortBio( artist.m_shortBio )
    , m_cover   ( artist.m_cover )
    , m_nbAlbums( artist.m_nbAlbums )
{

}

MLParentId MLArtist::getId() const
{
    return m_id;
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

MLArtist *MLArtist::clone(QObject *parent) const
{
    return new MLArtist(*this, parent);
}


QString MLArtist::getPresName() const
{
    return m_name;
}

QString MLArtist::getPresImage() const
{
    return m_cover;
}

QString MLArtist::getPresInfo() const
{
    return m_shortBio;
}

