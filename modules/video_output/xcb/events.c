/**
 * @file events.c
 * @brief X C Bindings VLC video output events handling
 */
/*****************************************************************************
 * Copyright © 2009 Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2.0
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 ****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <inttypes.h>
#include <assert.h>

#include <xcb/xcb.h>

#include <vlc_common.h>
#include <vlc_vout.h>

#include "xcb_vlc.h"

/* NOTE: we assume no other thread will be _setting_ our video output events
 * variables. Afterall, only this plugin is supposed to know when these occur.
  * Otherwise, we'd var_OrInteger() and var_NandInteger() functions...
 */

static void HandleButtonPress (vout_thread_t *vout,
                               xcb_button_press_event_t *ev)
{
    unsigned buttons = var_GetInteger (vout, "mouse-button-down");
    buttons |= (1 << (ev->detail - 1));
    var_SetInteger (vout, "mouse-button-down", buttons);
}

static void HandleButtonRelease (vout_thread_t *vout,
                                 xcb_button_release_event_t *ev)
{
    unsigned buttons = var_GetInteger (vout, "mouse-button-down");
    buttons &= ~(1 << (ev->detail - 1));
    var_SetInteger (vout, "mouse-button-down", buttons);

    switch (ev->detail)
    {
        case 1: /* left mouse button */
            var_SetBool (vout, "mouse-clicked", true);
            var_SetBool (vout->p_libvlc, "intf-popupmenu", false);
            break;
        case 3:
            var_SetBool (vout->p_libvlc, "intf-popupmenu", true);
            break;
    }
}

static void HandleMotionNotify (vout_thread_t *vout,
                                xcb_motion_notify_event_t *ev)
{
    unsigned x, y, width, height;
    int v;

    vout_PlacePicture (vout, vout->output.i_width, vout->output.i_height,
                       &x, &y, &width, &height);
    v = vout->fmt_in.i_x_offset
        + ((ev->event_x - x) * vout->fmt_in.i_visible_width / width);
    if (v < 0)
        v = 0; /* to the left of the picture */
    else if ((unsigned)v > vout->fmt_in.i_width)
        v = vout->fmt_in.i_width; /* to the right of the picture */
    var_SetInteger (vout, "mouse-x", v);

    v = vout->fmt_in.i_y_offset
        + ((ev->event_y - y) * vout->fmt_in.i_visible_height / height);
    if (v < 0)
        v = 0; /* above the picture */
    else if ((unsigned)v > vout->fmt_in.i_height)
        v = vout->fmt_in.i_height; /* below the picture */
    var_SetInteger (vout, "mouse-y", v);
}

/**
 * Process an X11 event.
 */
int ProcessEvent (vout_thread_t *vout, xcb_connection_t *conn,
                  xcb_window_t window, xcb_generic_event_t *ev)
{
    switch (ev->response_type & 0x7f)
    {
        case XCB_BUTTON_PRESS:
            HandleButtonPress (vout, (xcb_button_press_event_t *)ev);
            break;

        case XCB_BUTTON_RELEASE:
            HandleButtonRelease (vout, (xcb_button_release_event_t *)ev);
            break;

        case XCB_MOTION_NOTIFY:
            HandleMotionNotify (vout, (xcb_motion_notify_event_t *)ev);
            break;

        case XCB_CONFIGURE_NOTIFY:
        {
            xcb_configure_notify_event_t *cn =
                (xcb_configure_notify_event_t *)ev;

            assert (cn->window != window);
            HandleParentStructure (vout, conn, window, cn);
            break;
        }

        default:
            msg_Dbg (vout, "unhandled event %"PRIu8, ev->response_type);
    }

    free (ev);
    return VLC_SUCCESS;
}
