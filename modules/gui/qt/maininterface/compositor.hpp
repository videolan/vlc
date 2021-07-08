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
#ifndef VLC_QT_COMPOSITOR
#define VLC_QT_COMPOSITOR

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_interface.h>
#include <vlc_vout_window.h>

#include <QQuickView>

#include "qt.hpp"

class MainInterface;

namespace vlc {

class Compositor {
public:
    enum Type
    {
        DummyCompositor,
        Win7Compositor,
        DirectCompositionCompositor
    };

    typedef void (*VoutDestroyCb)(vout_window_t *p_wnd);

public:
    virtual ~Compositor() = default;

    virtual bool init() = 0;

    virtual MainInterface* makeMainInterface() = 0;
    virtual void destroyMainInterface() = 0;

    virtual void unloadGUI() = 0;

    virtual bool setupVoutWindow(vout_window_t *p_wnd, VoutDestroyCb destroyCb) = 0;

    virtual Type type() const = 0;

    virtual QWindow* interfaceMainWindow() const = 0;

protected:
    void onWindowDestruction(vout_window_t *p_wnd);

    VoutDestroyCb m_destroyCb = nullptr;
};

/**
 * @brief The CompositorFactory class will instanciate a compositor
 * in auto mode, compositor will be instantiated from the list by order declaration,
 * compositor can be explicitly defined by passing its name.
 *
 * the usual scenario is:
 *
 *   - call to preInit that will try to preInit compositors from list until we find
 *     a matching candidate
 *
 *   - start Qt main loop
 *
 *   - call to createCompositor to instantiate the compositor, if it fails it will
 *     try to initialize the next compositors from the list
 */
class CompositorFactory {
public:

    CompositorFactory(qt_intf_t *p_intf, const char* compositor = "auto");

    /**
     * @brief preInit will check whether a compositor can be used, before starting Qt,
     * each candidate will may perform some basic checks and can setup Qt enviroment variable if required
     *
     * @note if a compositor return true on preinit but fails to initialize afterwards, next
     * compositor in chain will be initialized without the preinit phaze (as Qt will be already started)
     * this might lead to an unstable configuration if incompatible operations are done in the preInit phase
     *
     * @return true if a compositor can be instantiated
     */
    bool preInit();

    /**
     * @brief createCompositor will instantiate a compositor
     *
     * @return the instantaied compositor, null if no compsitor can be instanciated
     */
    Compositor* createCompositor();

private:
    qt_intf_t* m_intf = nullptr;
    QString m_compositorName;
    size_t m_compositorIndex = 0;
};

}

#endif /* VLC_QT_COMPOSITOR */
