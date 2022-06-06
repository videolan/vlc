/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
 *
 * Authors: Prince Gupta <guptaprince8832@gmail.com>
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

#ifndef MLCUSTOMCOVER_HPP
#define MLCUSTOMCOVER_HPP

#include <QQuickAsyncImageProvider>

#include <memory>

class MLItemId;
class MediaLib;

class MLCustomCover : public QQuickAsyncImageProvider
{
public:
    MLCustomCover(const QString &providerId, MediaLib *ml);

    QString get(const MLItemId &parentId
                , const QSize &size
                , const QString &defaultCover
                , const int countX = 2
                , const int countY = 2
                , const int blur = 0
                , const bool split_duplicate = false);

    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize);

private:
    const QString m_providerId;
    MediaLib *m_ml = nullptr;
};

#endif // MLCUSTOMCOVER_HPP
