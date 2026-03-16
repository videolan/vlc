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
#include "util/vlctick.hpp"

//-------------------------------------------------------------------------------------------------
// Ctor / dtor
//-------------------------------------------------------------------------------------------------

MLPlaylist::MLPlaylist(const vlc_ml_playlist_t * data)
    : MLItem(MLItemId(data->i_id, VLC_ML_PARENT_PLAYLIST))
    , m_name(qfu(data->psz_name))
    , m_duration(data->i_duration)
    , m_count(data->i_nb_media)
    , m_nbAudio(data->i_nb_audio)
    , m_nbVideo(data->i_nb_video)
    , m_nbUnknown(data->i_nb_unknown)
{
    assert(data);
}

//-------------------------------------------------------------------------------------------------
// Interface
//-------------------------------------------------------------------------------------------------

QString MLPlaylist::getName() const
{
    return m_name;
}

void MLPlaylist::setName(const QString & name)
{
    m_name = name;
}

//-------------------------------------------------------------------------------------------------

VLCDuration MLPlaylist::getDuration() const
{
    return VLCDuration::fromMS(m_duration);
}

unsigned int MLPlaylist::getCount() const
{
    return m_count;
}

unsigned int MLPlaylist::getNbAudio() const
{
    return m_nbAudio;
}

unsigned int MLPlaylist::getNbVideo() const
{
    return m_nbVideo;
}

unsigned int MLPlaylist::getNbUnknown() const
{
    return m_nbUnknown;
}
