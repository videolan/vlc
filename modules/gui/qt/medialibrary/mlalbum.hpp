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
#include <QList>
#include <memory>
#include "vlc_media_library.h"
#include "mlhelper.hpp"
#include "mlqmltypes.hpp"

class MLAlbum : public QObject
{
    Q_OBJECT

    Q_PROPERTY(MLParentId id READ getId CONSTANT)
    Q_PROPERTY(QString title READ getTitle CONSTANT)
    Q_PROPERTY(unsigned int releaseyear READ getReleaseYear CONSTANT)
    Q_PROPERTY(QString shortsummary READ getShortSummary CONSTANT)
    Q_PROPERTY(QString cover READ getCover CONSTANT)
    Q_PROPERTY(QString artist READ getArtist CONSTANT)
    Q_PROPERTY(unsigned int nbtracks READ getNbTracks CONSTANT)
    Q_PROPERTY(QString duration READ getDuration CONSTANT)
    Q_PROPERTY(QString durationShort READ getDuration CONSTANT)

public:
    MLAlbum(vlc_medialibrary_t* _ml, const vlc_ml_album_t *_data, QObject *_parent = nullptr);

    MLParentId getId() const;
    QString getTitle() const;
    unsigned int getReleaseYear() const;
    QString getShortSummary() const;
    QString getCover() const;
    QString getArtist() const;
    unsigned int getNbTracks() const;
    QString getDuration() const;
    QString getDurationShort() const;

    MLAlbum* clone(QObject *parent = nullptr) const;

    Q_INVOKABLE QString getPresName() const;
    Q_INVOKABLE QString getPresImage() const;
    Q_INVOKABLE QString getPresInfo() const;

private:
    //private ctor for cloning
    MLAlbum(const MLAlbum &_album, QObject *_parent = nullptr);

    vlc_medialibrary_t* m_ml;

    MLParentId m_id;
    QString m_title;
    unsigned int m_releaseYear;
    QString m_shortSummary;
    QString m_cover;
    QString m_mainArtist;
    QList<QString> m_otherArtists;
    unsigned int m_nbTracks;
    QString m_duration;
    QString m_durationShort;
};
