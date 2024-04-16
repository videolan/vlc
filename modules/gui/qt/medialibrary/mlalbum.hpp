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
#include <QList>
#include <memory>
#include "mlhelper.hpp"
#include "mlqmltypes.hpp"

class VLCTick;

class MLAlbum : public MLItem
{
public:
    MLAlbum(const vlc_ml_album_t *_data);

    QString getTitle() const;
    unsigned int getReleaseYear() const;
    QString getShortSummary() const;
    QString getCover() const;
    QString getArtist() const;
    unsigned int getNbTracks() const;
    VLCTick getDuration() const;

    QString getPresName() const;
    QString getPresImage() const;
    QString getPresInfo() const;

private:
    QString m_title;
    unsigned int m_releaseYear;
    QString m_shortSummary;
    QString m_cover;
    QString m_mainArtist;
    QList<QString> m_otherArtists;
    unsigned int m_nbTracks;
    int64_t m_duration;
};
