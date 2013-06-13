/**
 * @file events.c
 * @brief X C Bindings VLC video output events handling
 */
/*****************************************************************************
 * Copyright © 2009 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <inttypes.h>
#include <assert.h>

#include <xcb/xcb.h>

#include <vlc_common.h>
#include <vlc_vout_display.h>

#include "events.h"

/**
 * Check for an error
 */
int XCB_error_Check (vout_display_t *vd, xcb_connection_t *conn,
                     const char *str, xcb_void_cookie_t ck)
{
    xcb_generic_error_t *err;

    err = xcb_request_check (conn, ck);
    if (err)
    {
        int code = err->error_code;

        free (err);
        msg_Err (vd, "%s: X11 error %d", str, code);
        assert (code != 0);
        return code;
    }
    return 0;
}

/**
 * Connect to the X server.
 */
static xcb_connection_t *Connect (vlc_object_t *obj, const char *display)
{
    xcb_connection_t *conn = xcb_connect (display, NULL);
    if (xcb_connection_has_error (conn) /*== NULL*/)
    {
        msg_Err (obj, "cannot connect to X server (%s)",
                 (display != NULL) ? display : "default");
        xcb_disconnect (conn);
        return NULL;
    }

    const xcb_setup_t *setup = xcb_get_setup (conn);
    msg_Dbg (obj, "connected to X%"PRIu16".%"PRIu16" server",
             setup->protocol_major_version, setup->protocol_minor_version);
    msg_Dbg (obj, " vendor : %.*s", (int)setup->vendor_len,
             xcb_setup_vendor (setup));
    msg_Dbg (obj, " version: %"PRIu32, setup->release_number);
    return conn;
}

/**
 * (Try to) register to mouse events on a window if needed.
 */
static void RegisterEvents (vlc_object_t *obj, xcb_connection_t *conn,
                            xcb_window_t wnd)
{
    /* Subscribe to parent window resize events */
    uint32_t value = XCB_EVENT_MASK_POINTER_MOTION
                   | XCB_EVENT_MASK_STRUCTURE_NOTIFY;
    xcb_change_window_attributes (conn, wnd, XCB_CW_EVENT_MASK, &value);
    /* Try to subscribe to click events */
    /* (only one X11 client can get them, so might not work) */
    if (var_InheritBool (obj, "mouse-events"))
    {
        value |= XCB_EVENT_MASK_BUTTON_PRESS
               | XCB_EVENT_MASK_BUTTON_RELEASE;
        xcb_change_window_attributes (conn, wnd,
                                      XCB_CW_EVENT_MASK, &value);
    }
}

/**
 * Find screen matching a given root window.
 */
static const xcb_screen_t *FindScreen (vlc_object_t *obj,
                                       xcb_connection_t *conn,
                                       xcb_window_t root)
{
    /* Find the selected screen */
    const xcb_setup_t *setup = xcb_get_setup (conn);
    for (xcb_screen_iterator_t i = xcb_setup_roots_iterator (setup);
         i.rem > 0; xcb_screen_next (&i))
    {
        if (i.data->root == root)
        {
            msg_Dbg (obj, "using screen 0x%"PRIx32, root);
            return i.data;
        }
    }
    msg_Err (obj, "window screen not found");
    return NULL;
}

/**
 * Create a VLC video X window object, connect to the corresponding X server,
 * find the corresponding X server screen.
 */
vout_window_t *XCB_parent_Create (vout_display_t *vd,
                                  xcb_connection_t **restrict pconn,
                                  const xcb_screen_t **restrict pscreen,
                                  uint16_t *restrict pwidth,
                                  uint16_t *restrict pheight)
{
    vout_window_cfg_t cfg = {
        .type = VOUT_WINDOW_TYPE_XID,
        .x = var_InheritInteger (vd, "video-x"),
        .y = var_InheritInteger (vd, "video-y"),
        .width  = vd->cfg->display.width,
        .height = vd->cfg->display.height,
    };

    vout_window_t *wnd = vout_display_NewWindow (vd, &cfg);
    if (wnd == NULL)
    {
        msg_Err (vd, "window not available");
        return NULL;
    }

    xcb_connection_t *conn = Connect (VLC_OBJECT(vd), wnd->display.x11);
    if (conn == NULL)
        goto error;
    *pconn = conn;

    /* Events must be registered before the window geometry is queried, so as
     * to avoid missing impeding resize events. */
    RegisterEvents (VLC_OBJECT(vd), conn, wnd->handle.xid);

    xcb_get_geometry_reply_t *geo =
        xcb_get_geometry_reply (conn, xcb_get_geometry (conn, wnd->handle.xid),
                                NULL);
    if (geo == NULL)
    {
        msg_Err (vd, "window not valid");
        goto error;
    }
    *pwidth = geo->width;
    *pheight = geo->height;

    const xcb_screen_t *screen = FindScreen (VLC_OBJECT(vd), conn, geo->root);
    free (geo);
    if (screen == NULL)
        goto error;
    *pscreen = screen;
    return wnd;

error:
    if (conn != NULL)
        xcb_disconnect (conn);
    vout_display_DeleteWindow (vd, wnd);
    return NULL;
}

/**
 * Create a blank cursor.
 * Note that the pixmaps are leaked (until the X disconnection). Hence, this
 * function should be called no more than once per X connection.
 * @param conn XCB connection
 * @param scr target XCB screen
 */
xcb_cursor_t XCB_cursor_Create (xcb_connection_t *conn,
                                const xcb_screen_t *scr)
{
    xcb_cursor_t cur = xcb_generate_id (conn);
    xcb_pixmap_t pix = xcb_generate_id (conn);

    xcb_create_pixmap (conn, 1, pix, scr->root, 1, 1);
    xcb_create_cursor (conn, cur, pix, pix, 0, 0, 0, 0, 0, 0, 1, 1);
    return cur;
}

/* NOTE: we assume no other thread will be _setting_ our video output events
 * variables. Afterall, only this plugin is supposed to know when these occur.
  * Otherwise, we'd var_OrInteger() and var_NandInteger() functions...
 */

/* FIXME we assume direct mapping between XCB and VLC */
static void HandleButtonPress (vout_display_t *vd,
                               const xcb_button_press_event_t *ev)
{
    vout_display_SendEventMousePressed (vd, ev->detail - 1);
}

static void HandleButtonRelease (vout_display_t *vd,
                                 const xcb_button_release_event_t *ev)
{
    vout_display_SendEventMouseReleased (vd, ev->detail - 1);
}

static void HandleMotionNotify (vout_display_t *vd, xcb_connection_t *conn,
                                const xcb_motion_notify_event_t *ev)
{
    vout_display_place_t place;

    /* show the default cursor */
    xcb_change_window_attributes (conn, ev->event, XCB_CW_CURSOR,
                                  &(uint32_t) { XCB_CURSOR_NONE });
    xcb_flush (conn);

    /* TODO it could be saved */
    vout_display_PlacePicture (&place, &vd->source, vd->cfg, false);

    if (place.width <= 0 || place.height <= 0)
        return;

    const int x = vd->source.i_x_offset +
        (int64_t)(ev->event_x - place.x) * vd->source.i_visible_width / place.width;
    const int y = vd->source.i_y_offset +
        (int64_t)(ev->event_y - place.y) * vd->source.i_visible_height/ place.height;

    vout_display_SendEventMouseMoved (vd, x, y);
}

static void HandleVisibilityNotify (vout_display_t *vd, bool *visible,
                                    const xcb_visibility_notify_event_t *ev)
{
    *visible = ev->state != XCB_VISIBILITY_FULLY_OBSCURED;
    msg_Dbg (vd, "display is %svisible", *visible ? "" : "not ");
}

static void
HandleParentStructure (vout_display_t *vd,
                       const xcb_configure_notify_event_t *ev)
{
    vout_display_SendEventDisplaySize (vd, ev->width, ev->height, vd->cfg->is_fullscreen);
}

/**
 * Process an X11 event.
 */
static int ProcessEvent (vout_display_t *vd, xcb_connection_t *conn,
                         bool *visible, xcb_generic_event_t *ev)
{
    switch (ev->response_type & 0x7f)
    {
        case XCB_BUTTON_PRESS:
            HandleButtonPress (vd, (xcb_button_press_event_t *)ev);
            break;

        case XCB_BUTTON_RELEASE:
            HandleButtonRelease (vd, (xcb_button_release_event_t *)ev);
            break;

        case XCB_MOTION_NOTIFY:
            HandleMotionNotify (vd, conn, (xcb_motion_notify_event_t *)ev);
            break;

        case XCB_VISIBILITY_NOTIFY:
            HandleVisibilityNotify (vd, visible,
                                    (xcb_visibility_notify_event_t *)ev);
            break;

        case XCB_CONFIGURE_NOTIFY:
            HandleParentStructure (vd, (xcb_configure_notify_event_t *)ev);
            break;

        /* FIXME I am not sure it is the right one */
        case XCB_DESTROY_NOTIFY:
            vout_display_SendEventClose (vd);
            break;

        case XCB_MAPPING_NOTIFY:
            break;

        default:
            msg_Dbg (vd, "unhandled event %"PRIu8, ev->response_type);
    }

    free (ev);
    return VLC_SUCCESS;
}

/**
 * Process incoming X events.
 */
int XCB_Manage (vout_display_t *vd, xcb_connection_t *conn, bool *visible)
{
    xcb_generic_event_t *ev;

    while ((ev = xcb_poll_for_event (conn)) != NULL)
        ProcessEvent (vd, conn, visible, ev);

    if (xcb_connection_has_error (conn))
    {
        msg_Err (vd, "X server failure");
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}
