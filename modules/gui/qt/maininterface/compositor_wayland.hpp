/*****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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
#ifndef VLC_COMPOSITOR_WAYLAND
#define VLC_COMPOSITOR_WAYLAND

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <memory>
#include "compositor.hpp"

#if (QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)) && defined(QT_GUI_PRIVATE)
#define QT_WAYLAND_HAS_CUSTOM_MARGIN_SUPPORT
#endif

class MainCtx;
class QQuickView;
class InterfaceWindowHandler;

extern "C" {
typedef struct qtwayland_t qtwayland_t;
}

namespace vlc {

class CompositorWayland : public CompositorVideo
{
    Q_OBJECT
public:
    CompositorWayland(qt_intf_t *p_intf, QObject* parent = nullptr);
    virtual ~CompositorWayland();

    bool init() override;

    bool makeMainInterface(MainCtx*, std::function<void(QQuickWindow*)> aboutToShowQuickWindowCallback = {}) override;

    /**
     * @brief release all resources used by the compositor.
     * this includes the GUI and the video surfaces.
     */
    void destroyMainInterface() override;

    /**
     * @brief unloadGUI unload the UI view from the composition
     * video view might still be active
     */
    void unloadGUI() override;


    QWindow* interfaceMainWindow() const override;

    Type type() const override;

    QQuickItem * activeFocusItem() const override;

    //video integration


    bool setupVoutWindow(vlc_window_t *p_wnd, VoutDestroyCb destroyCb) override;
    void windowDestroy() override;

    int windowEnable(const vlc_window_cfg_t *) override;
    void windowDisable() override;

    bool canDoThreadedSurfaceUpdates() const override { return true; };

protected:
    bool canDoCombinedSurfaceUpdates() const override { return true; };
    void commitSurface() override;

protected slots:
    void onSurfacePositionChanged(const QPointF&) override;
    void onSurfaceSizeChanged(const QSizeF&) override;
    void onSurfaceScaleChanged(qreal) override;
#ifdef QT_WAYLAND_HAS_CUSTOM_MARGIN_SUPPORT
    void adjustQuickWindowMask();
#endif

protected:
    std::unique_ptr<QQuickView> m_qmlView;
    qtwayland_t* m_waylandImpl = nullptr;
};

}

#endif // VLC_COMPOSITOR_WAYLAND
