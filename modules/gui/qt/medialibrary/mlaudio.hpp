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

#pragma once

#include <QString>

#include <vlc_media_library.h>

#include "mlmedia.hpp"

class MLAudio : public MLMedia
{
public:
    MLAudio(vlc_medialibrary_t *_ml, const vlc_ml_media_t *_data);

    QString getAlbumTitle() const;
    QString getArtist() const;
    unsigned int getTrackNumber() const;
    unsigned int getDiscNumber() const;

private:
    QString m_albumTitle;
    QString m_artist;
    unsigned int m_trackNumber;
    unsigned int m_discNumber;
};
