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

#ifndef MLPLAYLIST_HPP
#define MLPLAYLIST_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// MediaLibrary includes
#include "mlqmltypes.hpp"

// Qt includes
#include <QObject>

class MLPlaylist : public QObject, public MLItem
{
    Q_OBJECT

public:
    MLPlaylist(vlc_medialibrary_t * ml,
               const vlc_ml_playlist_t * data, QObject * parent = nullptr);

public: // Interface
    QString getName () const;
    QString getCover() const;

    unsigned int getCount() const;

private:
    vlc_medialibrary_t * m_ml;

    QString m_name;
    QString m_cover;

    unsigned int m_count;
};

#endif
