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

#ifndef MLGENRE_HPP
#define MLGENRE_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "vlc_common.h"

#include <memory>
#include <QObject>
#include <QString>
#include <QList>
#include <QRunnable>
#include <vlc_media_library.h>
#include "mlhelper.hpp"
#include "mlqmltypes.hpp"

class MLGenre : public QObject
{
    Q_OBJECT

    Q_PROPERTY(MLParentId id READ getId CONSTANT)
    Q_PROPERTY(QString name READ getName CONSTANT)
    Q_PROPERTY(unsigned int nbtracks READ getNbTracks CONSTANT)
    Q_PROPERTY(QString cover READ getCover WRITE setCover NOTIFY coverChanged)

public:
    MLGenre( vlc_medialibrary_t* _ml, const vlc_ml_genre_t *_data, QObject *_parent = nullptr);
    ~MLGenre();

    MLParentId getId() const;
    QString getName() const;
    unsigned int getNbTracks() const;
    QString getCover() const;

    MLGenre* clone(QObject *parent = nullptr) const;

signals:
    void coverChanged( const QString );
    void askGenerateCover( QPrivateSignal ) const;

public slots:
    void setCover(const QString cover);

private slots:
    void generateThumbnail();

private:
    MLGenre( const MLGenre& genre, QObject *_parent = nullptr);

    vlc_medialibrary_t* m_ml;

    MLParentId m_id;
    QString m_name;
    QString m_cover;
    QRunnable* m_coverTask = nullptr;
    unsigned int m_nbTracks;
};

#endif
