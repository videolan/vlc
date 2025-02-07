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
#ifndef VLC_COMPOSITOR_DUMMY
#define VLC_COMPOSITOR_DUMMY

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <memory>
#include "compositor.hpp"

class MainCtx;
class QQuickView;
class InterfaceWindowHandler;

namespace vlc {

class CompositorDummy : public QObject, public Compositor
{
    Q_OBJECT
public:
    CompositorDummy(qt_intf_t *p_intf, QObject* parent = nullptr);
    virtual ~CompositorDummy();

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

    bool setupVoutWindow(vlc_window_t *p_wnd, VoutDestroyCb destroyCb) override;

    QWindow* interfaceMainWindow() const override;

    Type type() const override;

    QQuickItem * activeFocusItem() const override;

protected:
    qt_intf_t *m_intf;

    std::unique_ptr<InterfaceWindowHandler> m_intfWindowHandler;
    MainCtx* m_mainCtx;
    std::unique_ptr<QQuickView> m_qmlWidget;
};

}

#endif // VLC_COMPOSITOR_DUMMY
