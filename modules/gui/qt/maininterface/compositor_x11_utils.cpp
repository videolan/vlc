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
#include <vector>
#include <algorithm>
#include <cstring>

#include <vlc_cxx_helpers.hpp>

#include <xcb/xfixes.h>

#include "compositor_x11_utils.hpp"

namespace vlc {

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

//see https://specifications.freedesktop.org/wm-spec/wm-spec-latest.html#idm45894597940912
bool wmScreenHasCompositor(xcb_connection_t* conn, int screen)
{
     std::string propName("_NET_WM_CM_S");
    propName += std::to_string(screen);

     xcb_atom_t atom = getInternAtom(conn, propName.c_str());
     if (atom == 0)
        return false;

     xcb_get_selection_owner_cookie_t cookie = xcb_get_selection_owner(conn, atom);
     auto reply = wrap_cptr(xcb_get_selection_owner_reply(conn, cookie, nullptr));
     if (!reply)
        return false;

     return reply->owner != 0;
}

//see https://specifications.freedesktop.org/wm-spec/wm-spec-latest.html#idm45894598144416
static std::vector<xcb_atom_t> getNetSupportedList(xcb_connection_t* conn, xcb_window_t rootWindow)
{
    std::vector<xcb_atom_t> netSupportedList;

    xcb_atom_t netSupportedAtom = getInternAtom(conn, "_NET_SUPPORTED");
    if (netSupportedAtom == 0)
       return netSupportedList;

    int offset = 0;
    int remaining = 0;
    do {
       xcb_get_property_cookie_t cookie = xcb_get_property(conn,
            false, rootWindow, netSupportedAtom, XCB_ATOM_ATOM, offset, 1024);
       auto reply = wrap_cptr(xcb_get_property_reply(conn, cookie, NULL));
       if (!reply)
            break;
       if (reply->type == XCB_ATOM_ATOM && reply->format == 32)
       {
            int length = xcb_get_property_value_length(reply.get())/sizeof(xcb_atom_t);
            xcb_atom_t *atoms = (xcb_atom_t *)xcb_get_property_value(reply.get());

            //&atoms[length] -> pointer past the last item
            std::copy(&atoms[0], &atoms[length], std::back_inserter(netSupportedList));
            remaining = reply->bytes_after;
            offset += length;
       }
    } while (remaining > 0);

    return netSupportedList;
}

static xcb_window_t getWindowProperty(xcb_connection_t* conn, xcb_window_t window, xcb_atom_t atom)
{
    xcb_get_property_cookie_t cookie = xcb_get_property(conn,
        false, window, atom, 0, 0, 4096);
    auto reply = wrap_cptr(xcb_get_property_reply(conn, cookie, NULL));
    if (!reply)
       return 0;

    if (xcb_get_property_value_length(reply.get()) == 0)
        return 0;

    return *((xcb_window_t *)xcb_get_property_value(reply.get()));
}

//see https://specifications.freedesktop.org/wm-spec/wm-spec-latest.html#idm45894598113264
static bool supportWMCheck(xcb_connection_t* conn, xcb_window_t rootWindow)
{
    if (rootWindow == 0)
        return false;

    xcb_atom_t atom = getInternAtom(conn, "_NET_SUPPORTING_WM_CHECK");
    xcb_window_t wmWindow = getWindowProperty(conn, rootWindow, atom);
    if (wmWindow == 0)
        return false;

    xcb_window_t wmWindowProp = getWindowProperty(conn, wmWindow, atom);
    return (wmWindow == wmWindowProp);
}

bool wmNetSupport(xcb_connection_t* conn, xcb_window_t rootWindow, xcb_atom_t atom)
{
    if (!supportWMCheck(conn, rootWindow))
        return false;

    std::vector<xcb_atom_t> netSupported = getNetSupportedList(conn, rootWindow);

    auto it = std::find(netSupported.cbegin(), netSupported.cend(), atom);
    return it != netSupported.cend();
}

}
