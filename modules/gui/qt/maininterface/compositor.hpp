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

    virtual MainInterface* makeMainInterface() = 0;
    virtual void destroyMainInterface() = 0;

    virtual bool setupVoutWindow(vout_window_t *p_wnd, VoutDestroyCb destroyCb) = 0;

    virtual Type type() const = 0;

    //factory
    static Compositor* createCompositor(qt_intf_t *p_intf);

protected:
    void onWindowDestruction(vout_window_t *p_wnd);

    VoutDestroyCb m_destroyCb = nullptr;
};


}

#endif /* VLC_QT_COMPOSITOR */
