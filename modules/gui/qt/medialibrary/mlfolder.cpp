/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
 *
 * Authors: Benjamin Arnaud <bunjee@omega.gg>
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

#include "mlfolder.hpp"

// Ctor / dtor

MLFolder::MLFolder(const vlc_ml_folder_t * data)
    : MLItemCover(MLItemId(data->i_id, VLC_ML_PARENT_FOLDER))
    , m_present(data->b_present)
    , m_banned(data->b_banned)
    , m_title(data->psz_name)
    , m_mrl(data->psz_mrl)
    , m_duration(0) // FIXME: We should have a duration field in vlc_ml_folder_t.
    , m_count(data->i_nb_media)
    , m_audioCount(data->i_nb_audio)
    , m_videoCount(data->i_nb_video)
{}

// Interface

bool MLFolder::isPresent() const
{
    return m_present;
}

bool MLFolder::isBanned() const
{
    return m_banned;
}

QString MLFolder::getTitle() const
{
    return m_title;
}

QString MLFolder::getMRL() const
{
    return m_mrl;
}

int64_t MLFolder::getDuration() const
{
    return m_duration;
}

unsigned int MLFolder::getCount() const
{
    return m_count;
}

unsigned int MLFolder::getAudioCount() const
{
    return m_audioCount;
}

unsigned int MLFolder::getVideoCount() const
{
    return m_videoCount;
}
