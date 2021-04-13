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

#ifndef MLGROUP_HPP
#define MLGROUP_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// Util includes
#include "util/covergenerator.hpp"

// MediaLibrary includes
#include "mlqmltypes.hpp"

class MLGroup : public MLItem
{
public:
    MLGroup(vlc_medialibrary_t * ml, const vlc_ml_group_t * data);

public: // Interface
    bool hasGenerator() const;
    void setGenerator(CoverGenerator * generator);

    QString getName() const;

    QString getCover() const;
    void    setCover(const QString & fileName);

    int64_t getDuration() const;

    unsigned int getDate() const;

    unsigned int getCount() const;

private:
    vlc_medialibrary_t * m_ml;

    TaskHandle<CoverGenerator> m_generator;

    QString m_name;

    QString m_cover;

    int64_t m_duration;

    unsigned int m_date;

    unsigned int m_count;
};

#endif
