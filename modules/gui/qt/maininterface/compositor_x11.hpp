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
#ifndef COMPOSITORX11_HPP
#define COMPOSITORX11_HPP

#include "compositor.hpp"
#include "videosurface.hpp"
#include <memory>

#include <xcb/xcb.h>

class QObject;
class QWidget;

namespace vlc {

class CompositorX11RenderWindow;
class CompositorX11UISurface;
class CompositorX11 : public CompositorVideo
{
    Q_OBJECT
public:
    explicit CompositorX11(qt_intf_t *p_intf, QObject *parent = nullptr);
    virtual ~CompositorX11();

    static bool preInit(qt_intf_t *);
    bool init() override;

    bool makeMainInterface(MainCtx*) override;
    void destroyMainInterface() override;
    void unloadGUI() override;

    bool setupVoutWindow(vlc_window_t *p_wnd, VoutDestroyCb destroyCb)  override;

    inline Type type() const override { return X11Compositor; };

    QWindow* interfaceMainWindow() const override;

    QQuickItem * activeFocusItem() const override;

private:
    int windowEnable(const vlc_window_cfg_t *)  override;
    void windowDisable() override;

private slots:
    void onSurfacePositionChanged(const QPointF& position) override;
    void onSurfaceSizeChanged(const QSizeF& size) override;

private:
    xcb_connection_t* m_conn = nullptr;

    std::unique_ptr<QWidget> m_videoWidget;
    std::unique_ptr<CompositorX11UISurface> m_qmlView;
    std::unique_ptr<CompositorX11RenderWindow> m_renderWindow;
};

}

#endif // COMPOSITORX11_HPP
