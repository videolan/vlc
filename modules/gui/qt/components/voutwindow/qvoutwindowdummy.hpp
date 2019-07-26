/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VIDEORENDERERDUMMY_HPP
#define VIDEORENDERERDUMMY_HPP

#include "qvoutwindow.hpp"
#include "videosurface.hpp"
#include <QtQuick/QSGRectangleNode>

/*
 * Video surface renderer and provider that won't perform anything
 */

class VideoSurfaceProviderDummy : public VideoSurfaceProvider
{
    Q_OBJECT
public:

    VideoSurfaceProviderDummy(QObject* parent = nullptr);
    QSGNode* updatePaintNode(QQuickItem* item, QSGNode* oldNode, QQuickItem::UpdatePaintNodeData*) override;
};

class QVoutWindowDummy : public QVoutWindow
{
    Q_OBJECT
public:
    QVoutWindowDummy(MainInterface* p_mi,  QObject *parent = nullptr);

    VideoSurfaceProvider* getVideoSurfaceProvider() override;

    bool setupVoutWindow(vout_window_t* voutWindow) override;

private:
    VideoSurfaceProviderDummy* m_surfaceProvider = nullptr;
};

#endif // VIDEORENDERERDUMMY_HPP
