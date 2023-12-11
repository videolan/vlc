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

    static bool preInit(qt_intf_t*);
    virtual bool init() override;

    virtual bool makeMainInterface(MainCtx*) override;

    /**
     * @brief release all resources used by the compositor.
     * this includes the GUI and the video surfaces.
     */
    virtual void destroyMainInterface() override;

    /**
     * @brief unloadGUI unload the UI view from the composition
     * video view might still be active
     */
    virtual void unloadGUI() override;


    QWindow* interfaceMainWindow() const override;

    Type type() const override;

    QQuickItem * activeFocusItem() const override;

    //video integration


    bool setupVoutWindow(vlc_window_t *p_wnd, VoutDestroyCb destroyCb) override;
    void windowDestroy() override;

    virtual int windowEnable(const vlc_window_cfg_t *) override;
    virtual void windowDisable() override;

protected slots:
    virtual void onSurfacePositionChanged(const QPointF&) override;
    virtual void onSurfaceSizeChanged(const QSizeF&) override;


protected:
    std::unique_ptr<QQuickView> m_qmlView;
    qtwayland_t* m_waylandImpl = nullptr;
};

}

#endif // VLC_COMPOSITOR_WAYLAND
