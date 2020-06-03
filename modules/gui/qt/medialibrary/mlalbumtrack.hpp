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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "vlc_common.h"

#include <QObject>
#include <QString>
#include <memory>

#include <vlc_media_library.h>
#include "mlhelper.hpp"
#include "mlqmltypes.hpp"

class MLAlbumTrack : public QObject
{
    Q_OBJECT

    Q_PROPERTY(MLParentId id READ getId CONSTANT)
    Q_PROPERTY(QString title READ getTitle CONSTANT)
    Q_PROPERTY(QString album_title READ getAlbumTitle CONSTANT)
    Q_PROPERTY(QString main_artist READ getArtist CONSTANT)
    Q_PROPERTY(QString cover READ getCover CONSTANT)
    Q_PROPERTY(unsigned int track_number READ getTrackNumber CONSTANT)
    Q_PROPERTY(unsigned int disc_number READ getDiscNumber CONSTANT)
    Q_PROPERTY(QString duration READ getDuration CONSTANT)
    Q_PROPERTY(QString durationShort READ getDurationShort CONSTANT)
    Q_PROPERTY(QString mrl READ getMRL CONSTANT)

public:
    MLAlbumTrack(vlc_medialibrary_t *_ml, const vlc_ml_media_t *_data, QObject *_parent = nullptr);

    MLParentId getId() const;
    QString getTitle() const;
    QString getAlbumTitle() const;
    QString getArtist() const;
    QString getCover() const;
    unsigned int getTrackNumber() const;
    unsigned int getDiscNumber() const;
    QString getDuration() const;
    QString getDurationShort() const;
    QString getMRL() const;

    MLAlbumTrack* clone(QObject *parent = nullptr) const;

private:
    MLAlbumTrack(const MLAlbumTrack& albumtrack, QObject *_parent = nullptr);

    MLParentId m_id;
    QString m_title;
    QString m_albumTitle;
    QString m_artist;
    QString m_cover;
    unsigned int m_trackNumber;
    unsigned int m_discNumber;
    QString m_duration;
    QString m_durationShort;
    QString m_mrl;

   ml_unique_ptr<vlc_ml_media_t> m_data;

signals:

public slots:
};
