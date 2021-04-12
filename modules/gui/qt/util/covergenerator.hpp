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
class vlc_medialibrary_t;
class MLItemId;

class CoverGenerator : public AsyncTask<QString>
{
    Q_OBJECT

    Q_ENUMS(Split)

public: // Enums
    enum Split
    {
        Divide,
        Duplicate
    };

public:
    CoverGenerator(vlc_medialibrary_t * ml, const MLItemId & itemId, int index = -1);

public: // Interface
    Q_INVOKABLE MLItemId getId();

    Q_INVOKABLE int getIndex();

    Q_INVOKABLE void setSize(const QSize & size);

    Q_INVOKABLE void setCountX(int x);
    Q_INVOKABLE void setCountY(int y);

    // NOTE: Do we want to divide or duplicate thumbnails to reach the proper count ?
    Q_INVOKABLE void setSplit(Split split);

    // NOTE: Applies SmoothTransformation to thumbnails. Disabled by default.
    Q_INVOKABLE void setSmooth(bool enabled);

    // NOTE: You need to specify a radius to enable blur, 8 looks good.
    Q_INVOKABLE void setBlur(int radius);

    Q_INVOKABLE void setDefaultThumbnail(const QString & fileName);

public: // AsyncTask implementation
    QString execute() override;

private: // Functions
    void draw(QPainter & painter, const QStringList & fileNames);

    void drawImage(QPainter & painter, const QString & fileName, const QRect & rect);

    void blur(QImage * image);

    QString getStringType(vlc_ml_parent_type type) const;

    QStringList getMedias(int count, int64_t id, vlc_ml_parent_type type) const;
    QStringList getGenre (int count, int64_t id) const;

private:
    vlc_medialibrary_t * m_ml;

    MLItemId m_id;

    int m_index;

    QSize m_size;

    int m_countX;
    int m_countY;

    Split m_split;

    bool m_smooth;

    int m_blur;

    QString m_default;
};

#endif // COVERGENERATOR_HPP
