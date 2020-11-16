/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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
#ifndef COMPOSITOR_X11_RENDERCLIENT_HPP
#define COMPOSITOR_X11_RENDERCLIENT_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QObject>
#include <QX11Info>
#include <QWindow>

#include <vlc_common.h>

#include "qt.hpp"

#include "compositor_x11_utils.hpp"

namespace vlc {

class CompositorX11RenderClient : public QObject
{
    Q_OBJECT
public:
    CompositorX11RenderClient(
            qt_intf_t* p_intf, xcb_connection_t* conn,
            QWindow* window,
            QObject* parent = nullptr);

    ~CompositorX11RenderClient();

    bool registerDamageEvent(Display* dpy);

    xcb_drawable_t getWindowXid() const;

    void createPicture();
    xcb_render_picture_t getPicture();

public slots:
    void resetPixmap();

private:
    qt_intf_t* m_intf;
    QWindow* m_window = nullptr;

    xcb_connection_t* m_conn = 0;
    xcb_window_t m_wid = 0;
    PixmapPtr m_pixmap;
    PicturePtr m_picture;

    xcb_render_pictformat_t m_format;
};

}

#endif // RENDERCLIENT_HPP
