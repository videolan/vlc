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
#include <vlc_cxx_helpers.hpp>
#include "compositor_x11_renderclient.hpp"

#define _NET_WM_BYPASS_COMPOSITOR_NAME "_NET_WM_BYPASS_COMPOSITOR"

using namespace vlc;

CompositorX11RenderClient::CompositorX11RenderClient(qt_intf_t* p_intf, xcb_connection_t* conn,  xcb_window_t wid, QObject *parent)
    : QObject(parent)
    , m_intf(p_intf)
    , m_conn(conn)
    , m_wid(wid)
    , m_pixmap(m_conn)
    , m_picture(m_conn)
{
    xcb_generic_error_t* err = nullptr;
    xcb_get_window_attributes_cookie_t attrCookie = xcb_get_window_attributes(m_conn, m_wid);
    auto attrReply = wrap_cptr(xcb_get_window_attributes_reply(m_conn, attrCookie, &err));
    if (err)
    {
        msg_Info(m_intf, "can't get window attr");
        free(err);
        return;
    }

    xcb_visualid_t visual = attrReply->visual;
    bool ret = findVisualFormat(m_conn, visual, &m_format, nullptr);
    if (!ret)
    {
        msg_Info(m_intf, "can't find visual format");
        return;
    }

    xcb_void_cookie_t cookie = xcb_composite_redirect_window_checked(m_conn, m_wid, XCB_COMPOSITE_REDIRECT_MANUAL);
    err = xcb_request_check(m_conn, cookie);
    if (err)
    {
        msg_Warn(m_intf, " can't redirect window %u.%u : %u", err->major_code, err->minor_code, err->error_code);
        free(err);
        return;
    }

    xcb_atom_t _NET_WM_BYPASS_COMPOSITOR = getInternAtom(m_conn, _NET_WM_BYPASS_COMPOSITOR_NAME);
    if (_NET_WM_BYPASS_COMPOSITOR != XCB_ATOM_NONE)
    {
        uint32_t val = 1;
        xcb_change_property(m_conn, XCB_PROP_MODE_REPLACE, m_wid,
                            _NET_WM_BYPASS_COMPOSITOR, XCB_ATOM_CARDINAL, 32, 1, &val);
    }
}

CompositorX11RenderClient::~CompositorX11RenderClient()
{
    m_pixmap.reset();
    m_picture.reset();
}

xcb_drawable_t CompositorX11RenderClient::getWindowXid() const
{
    return m_wid;
}

void CompositorX11RenderClient::createPicture()
{
    xcb_void_cookie_t voidCookie;
    auto err = wrap_cptr<xcb_generic_error_t>(nullptr);

    m_pixmap.generateId();
    voidCookie = xcb_composite_name_window_pixmap_checked(m_conn, m_wid, m_pixmap.get());
    err.reset(xcb_request_check(m_conn, voidCookie));
    if (err)
    {
        msg_Warn(m_intf, "can't create name window pixmap");
        m_pixmap.reset();
        return;
    }

    m_picture.generateId();
    voidCookie = xcb_render_create_picture_checked(m_conn, m_picture.get(), m_pixmap.get(), m_format, 0, 0);
    err.reset(xcb_request_check(m_conn, voidCookie));
    if (err)
    {
        msg_Warn(m_intf, "can't create name window picture");
        m_pixmap.reset();
        m_picture.reset();
    }
}

xcb_render_picture_t CompositorX11RenderClient::getPicture()
{
    if (!m_picture)
        createPicture();
    return m_picture.get();
}

void CompositorX11RenderClient::resetPixmap()
{
    m_pixmap.reset();
    m_picture.reset();
}
