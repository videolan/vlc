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

#ifndef COVERGENERATOR_HPP
#define COVERGENERATOR_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

// MediaLibrary includes
#include "medialibrary/mlqmltypes.hpp"

// Util includes
#include "util/asynctask.hpp"

// Qt includes
#include <QPainter>

// Forward declarations
struct vlc_medialibrary_t;
class MLItemId;

class CoverGenerator
{
public: // Enums
    enum Split
    {
        Divide,
        Duplicate
    };

public:
    CoverGenerator(const MLItemId & itemId);

public: // Interface
    MLItemId getId();

    void setSize(const QSize & size);

    void setCountX(int x);
    void setCountY(int y);

    // NOTE: Do we want to divide or duplicate thumbnails to reach the proper count ?
    void setSplit(Split split);

    // NOTE: Applies SmoothTransformation to thumbnails. Disabled by default.
    void setSmooth(bool enabled);

    // NOTE: You need to specify a radius to enable blur, 8 looks good.
    void setBlur(int radius);

    void setDefaultThumbnail(const QString & fileName);

    // NOTE: This lets us enforce a specific prefix for the cover fileName.
    void setPrefix(const QString & prefix);

    int requiredNoOfThumbnails() const;

    bool cachedFileAvailable() const;

    QString cachedFileURL() const;

    QImage execute(QStringList thumbnails) const;

private: // Functions
    QString fileName() const;

    void draw(QPainter & painter, const QStringList & fileNames, int countX, int countY) const;

    void drawImage(QPainter & painter, const QString & fileName, const QRect & rect) const;

    void blur(QImage &image) const;

    QString getPrefix(vlc_ml_parent_type type) const;

private:
    MLItemId m_id;

    QSize m_size;

    int m_countX;
    int m_countY;

    Split m_split;

    bool m_smooth;

    int m_blur;

    QString m_default;

    QString m_prefix;
};

#endif // COVERGENERATOR_HPP
