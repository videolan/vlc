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

#include "mlgenre.hpp"

MLGenre::MLGenre(vlc_medialibrary_t* ml, const vlc_ml_genre_t *_data )
    : MLItem     ( MLItemId( _data->i_id, VLC_ML_PARENT_GENRE ) )
    , m_ml       ( ml )
    , m_generator( nullptr )
    , m_name     ( QString::fromUtf8( _data->psz_name ) )
    , m_nbTracks ( (unsigned int)_data->i_nb_tracks )

{
    assert(_data);
}

bool MLGenre::hasGenerator() const
{
    return m_generator.get();
}

void MLGenre::setGenerator(CoverGenerator * generator)
{
    m_generator.reset(generator);
}

QString MLGenre::getName() const
{
    return m_name;
}

unsigned int MLGenre::getNbTracks() const
{
    return m_nbTracks;
}

QString MLGenre::getCover() const
{
    return m_cover;
}

void MLGenre::setCover(const QString & fileName)
{
    m_cover = fileName;
}

