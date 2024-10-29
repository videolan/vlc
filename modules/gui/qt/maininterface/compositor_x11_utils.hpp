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
#ifndef COMPOSITOR_X11_UTILS_HPP
#define COMPOSITOR_X11_UTILS_HPP
#include <memory>

#include <xcb/xcb.h>
#include <xcb/render.h>
#include <xcb/composite.h>

namespace vlc {

template<typename T, typename R, R RELEASE>
class X11Resource {
public:
    X11Resource() = delete;

    explicit X11Resource(xcb_connection_t* conn, T _xid = 0)
        : m_conn(conn)
        , xid(_xid)
    {}

    X11Resource(const X11Resource &other) = delete;
    X11Resource(X11Resource &&other)
        : m_conn (other.m_conn)
        , xid (other.xid)
    {
        other.xid = 0;
    }

    ~X11Resource()
    {
        if (!m_conn)
            return;
        if (xid)
            RELEASE(m_conn, xid);
    }

    void generateId() {
        reset(xcb_generate_id(m_conn));
    }

    X11Resource &operator=(const X11Resource &other) = delete;
    X11Resource &operator=(X11Resource &&other) noexcept
    {
        reset(other.xid);
        other.xid = 0;
        return *this;
    }
    X11Resource &operator=(T value) noexcept
    {
        reset(value);
        return *this;
    }

    void reset(T newval = 0) {
        if (xid)
            RELEASE(m_conn, xid);
        xid = newval;
    }

    operator bool() noexcept
    {
        return xid != 0;
    }

    T get() const { return xid; }

    xcb_connection_t* m_conn;
    T xid = 0;
};

using  PixmapPtr = X11Resource<xcb_pixmap_t, decltype(&xcb_free_pixmap), xcb_free_pixmap>;
using  PicturePtr = X11Resource<xcb_render_picture_t, decltype(&xcb_render_free_picture), xcb_render_free_picture>;
using  WindowPtr = X11Resource<xcb_window_t, decltype(&xcb_destroy_window), xcb_destroy_window>;

bool queryExtension(xcb_connection_t* conn, const char* name, uint8_t* first_event_out, uint8_t* first_error_out);

bool findVisualFormat(xcb_connection_t* conn, xcb_visualid_t visual, xcb_render_pictformat_t* fmtOut, uint8_t* depthOut);

xcb_atom_t getInternAtom(xcb_connection_t* conn, const char* atomName);

void setTransparentForMouseEvent(xcb_connection_t* conn, xcb_window_t window);

bool wmScreenHasCompositor(xcb_connection_t* conn, int screen);

bool wmNetSupport(xcb_connection_t* conn, xcb_window_t rootWindow, xcb_atom_t atom);

}


#endif /* COMPOSITOR_X11_UTILS_HPP */
