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

MLPlaylist::MLPlaylist(vlc_medialibrary_t * ml, const vlc_ml_playlist_t * data, QObject * parent)
    : QObject(parent)
    , MLItem(MLItemId(data->i_id, VLC_ML_PARENT_PLAYLIST))
    , m_ml(ml)
{
    assert(data);

    m_name = qfu(data->psz_name);

    // TODO m_cover

    // TODO m_count
    m_count = 0;//data->i_nb_tracks;
}

//-------------------------------------------------------------------------------------------------
// Interface
//-------------------------------------------------------------------------------------------------

QString MLPlaylist::getName() const
{
    return m_name;
}

QString MLPlaylist::getCover() const
{
    return m_cover;
}

unsigned int MLPlaylist::getCount() const
{
    return m_count;
}
