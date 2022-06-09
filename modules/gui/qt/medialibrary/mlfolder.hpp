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

#ifndef MLFOLDER_HPP
#define MLFOLDER_HPP

// MediaLibrary includes
#include "mlitemcover.hpp"

class MLFolder : public MLItemCover
{
public:
    MLFolder(const vlc_ml_folder_t * data);

public: // Interface
    bool isPresent() const;
    bool isBanned() const;

    QString getTitle() const;

    QString getMRL() const;

    int64_t getDuration() const;

    unsigned int getCount() const;

    unsigned int getAudioCount() const;

    unsigned int getVideoCount() const;

private:
    bool m_present;
    bool m_banned;

    QString m_title;

    QString m_mrl;

    int64_t m_duration;

    unsigned int m_count;
    unsigned int m_audioCount;
    unsigned int m_videoCount;
};

#endif
