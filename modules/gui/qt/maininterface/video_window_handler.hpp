/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
#ifndef VIDEOWINDOWHANDLER_HPP
#define VIDEOWINDOWHANDLER_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include <QWindow>
#include <QObject>

#include "qt.hpp"

class MainInterface;

class VideoWindowHandler : public QObject
{
    Q_OBJECT
public:
    explicit VideoWindowHandler(qt_intf_t *intf, MainInterface* mainInterace, QObject *parent = nullptr);

public:
    void setWindow(QWindow* window);

    void disable();
    void requestResizeVideo( unsigned, unsigned );
    void requestVideoState( unsigned );
    void requestVideoWindowed( );
    void requestVideoFullScreen( const char * );

signals:
    void askVideoToResize( unsigned int, unsigned int );
    void askVideoSetFullScreen( bool );
    void askVideoOnTop( bool );

protected slots:
    /* Manage the Video Functions from the vout threads */
    void setVideoSize(unsigned int w, unsigned int h);
    virtual void setVideoFullScreen( bool );
    void setVideoOnTop( bool );

private:
    qt_intf_t *m_intf = nullptr;
    MainInterface* m_interface = nullptr;
    QWindow* m_window = nullptr;

    bool m_videoFullScreen = false;
    bool m_autoresize = false;

    QRect   m_lastWinGeometry;
    QScreen* m_lastWinScreen = nullptr;

#ifdef QT5_HAS_WAYLAND
    bool m_hasWayland = false;
#endif
};

#endif // VIDEOWINDOWHANDLER_HPP
