/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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
#ifndef COMPOSITOR_PLATFORM_HPP
#define COMPOSITOR_PLATFORM_HPP

#include "compositor.hpp"

#include <QWindow>
#include <QPointer>

#include <memory>

class QQuickView;

namespace vlc {

class CompositorPlatform : public CompositorVideo
{
    Q_OBJECT

public:
    CompositorPlatform(qt_intf_t *p_intf, QObject* parent = nullptr);

    bool init() override;

    bool makeMainInterface(MainCtx *, std::function<void(QQuickWindow*)> aboutToShowQuickWindowCallback = {}) override;
    void destroyMainInterface() override;
    void unloadGUI() override;
    bool setupVoutWindow(vlc_window_t*, VoutDestroyCb destroyCb) override;
    QWindow* interfaceMainWindow() const override;
    QQuickWindow* quickWindow() const override;
    Type type() const override;
    QQuickItem * activeFocusItem() const override;

    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    int windowEnable(const vlc_window_cfg_t *) override;
    void windowDisable() override;

private slots:
    void onSurfacePositionChanged(const QPointF& position) override;
    void onSurfaceSizeChanged(const QSizeF& size) override;

private:
    std::unique_ptr<QWindow> m_rootWindow;
    QPointer<QWindow> m_videoWindow;
    QPointer<QQuickView> m_quickWindow;
};

}

#endif // COMPOSITOR_PLATFORM_HPP
