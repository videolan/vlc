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

#include "compositor.hpp"
#include "compositor_dummy.hpp"

#ifdef _WIN32
#ifdef HAVE_DCOMP_H
#  include "compositor_dcomp.hpp"
#endif
#  include "compositor_win7.hpp"
#endif

namespace vlc {

Compositor* Compositor::createCompositor(qt_intf_t *p_intf)
{
    bool ret;
    VLC_UNUSED(ret);
#ifdef _WIN32
#ifdef HAVE_DCOMP_H
    CompositorDirectComposition* dcomp_compositor = new CompositorDirectComposition(p_intf);
    ret = dcomp_compositor->init();
    if (ret)
        return dcomp_compositor;
    delete dcomp_compositor;
    msg_Dbg(p_intf, "failed to create DirectComposition backend, use fallback");
#endif
    CompositorWin7* win7_compositor = new CompositorWin7(p_intf);
    if (win7_compositor->init())
        return win7_compositor;
    delete win7_compositor;
    msg_Dbg(p_intf, "failed to create Win7 compositor backend, use fallback");
#endif
    return new CompositorDummy(p_intf);
}

}
