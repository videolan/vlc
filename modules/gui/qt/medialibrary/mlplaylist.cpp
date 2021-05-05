/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

#include "mlplaylist.hpp"

// VLC includes
#include "qt.hpp"

//-------------------------------------------------------------------------------------------------
// Ctor / dtor
//-------------------------------------------------------------------------------------------------

MLPlaylist::MLPlaylist(vlc_medialibrary_t * ml, const vlc_ml_playlist_t * data)
    : MLItem(MLItemId(data->i_id, VLC_ML_PARENT_PLAYLIST))
    , m_ml(ml)
    , m_generator(nullptr)
    , m_name(qfu(data->psz_name))
    , m_duration(0) // TODO m_duration
    , m_count(data->i_nb_media)
{
    assert(data);
}

//-------------------------------------------------------------------------------------------------
// Interface
//-------------------------------------------------------------------------------------------------

bool MLPlaylist::hasGenerator() const
{
    return m_generator.get();
}

void MLPlaylist::setGenerator(CoverGenerator * generator)
{
    m_generator.reset(generator);
}

//-------------------------------------------------------------------------------------------------

QString MLPlaylist::getName() const
{
    return m_name;
}

//-------------------------------------------------------------------------------------------------

QString MLPlaylist::getCover() const
{
    return m_cover;
}

void MLPlaylist::setCover(const QString & fileName)
{
    m_cover = fileName;
}

//-------------------------------------------------------------------------------------------------

int64_t MLPlaylist::getDuration() const
{
    return m_duration;
}

unsigned int MLPlaylist::getCount() const
{
    return m_count;
}
