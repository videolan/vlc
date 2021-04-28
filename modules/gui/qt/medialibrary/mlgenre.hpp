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

// Util includes
#include "util/covergenerator.hpp"

// MediaLibrary includes
#include "mlqmltypes.hpp"

class MLGenre : public MLItem
{
public:
    MLGenre( vlc_medialibrary_t* _ml, const vlc_ml_genre_t *_data );

    bool hasGenerator() const;
    void setGenerator(CoverGenerator * generator);

    QString getName() const;
    unsigned int getNbTracks() const;

    QString getCover() const;
    void    setCover(const QString & fileName);

private slots:
    void generateThumbnail();

private:
    vlc_medialibrary_t* m_ml;

    TaskHandle<CoverGenerator> m_generator;

    QString m_name;
    QString m_cover;

    unsigned int m_nbTracks;
};

#endif
