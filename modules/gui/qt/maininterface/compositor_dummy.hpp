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

#include "compositor.hpp"

class MainInterface;

namespace vlc {

class CompositorDummy : public QObject, public Compositor
{
    Q_OBJECT
public:
    CompositorDummy(intf_thread_t *p_intf, QObject* parent = nullptr);
    ~CompositorDummy() = default;

    MainInterface *makeMainInterface() override;
    virtual void destroyMainInterface() override;

    bool setupVoutWindow(vout_window_t *p_wnd) override;

private:

    intf_thread_t *m_intf;

    MainInterface* m_rootWindow;
};

}

#endif // VLC_COMPOSITOR_DUMMY
