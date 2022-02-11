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
#include <xcb/xfixes.h>
#include <vlc_cxx_helpers.hpp>
#include "compositor_x11_utils.hpp"

#include <QWindow>

namespace vlc {

DummyNativeWidget::DummyNativeWidget(QWidget* parent, Qt::WindowFlags f)
    : QWidget(parent, f)
{
    setAttribute(Qt::WA_NativeWindow, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_PaintOnScreen, true);
    setAttribute(Qt::WA_MouseTracking, true);
    setAttribute(Qt::WA_TranslucentBackground, false);
    QWindow* w =  window()->windowHandle();
    assert(w);
    /*
     * force the window not to have an alpha channel, the  parent window
     * may have an alpha channel and child widget would inhertit the format
     * even if we set Qt::WA_TranslucentBackground to false. having an alpha
     * in this surface would lead to the video begin semi-tranparent.
     */
    QSurfaceFormat format = w->format();
    format.setAlphaBufferSize(0);
    w->setFormat(format);
}

DummyNativeWidget::~DummyNativeWidget()
{

}

QPaintEngine* DummyNativeWidget::paintEngine() const
{
    return nullptr;
}

bool queryExtension(xcb_connection_t* conn, const char* name, uint8_t* first_event_out, uint8_t* first_error_out)
{
    xcb_query_extension_cookie_t cookie = xcb_query_extension(conn, (uint16_t)strlen(name), name);
    xcb_generic_error_t* error = NULL;
    auto reply = wrap_cptr(xcb_query_extension_reply(conn, cookie, &error));
    auto errorPtr = wrap_cptr(error);
    if (errorPtr || !reply)
        return false;
    if (!reply->present)
        return false;

    if (first_event_out)
      *first_event_out = reply->first_event;
    if (first_error_out)
      *first_error_out = reply->first_error;
    return true;
}

bool findVisualFormat(xcb_connection_t* conn, xcb_visualid_t visual,
                  xcb_render_pictformat_t* formatOut = nullptr,
                  uint8_t* depthOut = nullptr
                  )
{
    xcb_render_query_pict_formats_cookie_t pictFormatC = xcb_render_query_pict_formats(conn);
    auto pictFormatR = wrap_cptr(xcb_render_query_pict_formats_reply(conn, pictFormatC, nullptr));

    if (!pictFormatR)
        return false;

    auto screenIt = xcb_render_query_pict_formats_screens_iterator(pictFormatR.get());
    for (; screenIt.rem > 0; xcb_render_pictscreen_next(&screenIt))
    {
        xcb_render_pictscreen_t* pictScreen = screenIt.data;
        auto depthIt = xcb_render_pictscreen_depths_iterator(pictScreen);
        for (; depthIt.rem > 0; xcb_render_pictdepth_next(&depthIt))
        {
            xcb_render_pictdepth_t* pictDepth = depthIt.data;
            auto visualIt = xcb_render_pictdepth_visuals_iterator(pictDepth);
            for (; visualIt.rem > 0; xcb_render_pictvisual_next(&visualIt))
            {
                xcb_render_pictvisual_t* pictVisual = visualIt.data;
                if (pictVisual->visual == visual)
                {
                    if (formatOut)
                        *formatOut = pictVisual->format;
                    if (depthOut)
                        *depthOut = pictDepth->depth;
                    return true;
                }
            }
        }
    }

    return false;
}

xcb_atom_t getInternAtom(xcb_connection_t* conn, const char* atomName)
{
    xcb_intern_atom_cookie_t atomCookie = xcb_intern_atom(conn, 1, strlen(atomName), atomName);
    auto atomReply = wrap_cptr(xcb_intern_atom_reply(conn, atomCookie, nullptr));
    if (!atomReply)
        return 0;
    return atomReply->atom;
}

void setTransparentForMouseEvent(xcb_connection_t* conn, xcb_window_t window)
{
     xcb_rectangle_t *rect = 0;
     int nrect = 0;

     xcb_xfixes_region_t region = xcb_generate_id(conn);
     xcb_xfixes_create_region(conn, region, nrect, rect);
     xcb_xfixes_set_window_shape_region(conn, window, XCB_SHAPE_SK_INPUT, 0, 0, region);
     xcb_xfixes_destroy_region(conn, region);
}

}
