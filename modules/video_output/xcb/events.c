/**
 * @file events.c
 * @brief X C Bindings VLC video output events handling
 */
/*****************************************************************************
 * Copyright © 2009 Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
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
#include <vlc_vout_display.h>

#include "xcb_vlc.h"

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

static void HandleMotionNotify (vout_display_t *vd,
                                const xcb_motion_notify_event_t *ev)
{
    vout_display_place_t place;

    /* TODO it could be saved */
    vout_display_PlacePicture (&place, &vd->source, vd->cfg, false);

    if (place.width <= 0 || place.height <= 0)
        return;

    const int x = vd->source.i_x_offset +
        (int64_t)(ev->event_x - place.x) * vd->source.i_visible_width / place.width;
    const int y = vd->source.i_y_offset +
        (int64_t)(ev->event_y - place.y) * vd->source.i_visible_height/ place.height;

    /* TODO show the cursor ? */
    if (x >= vd->source.i_x_offset && x < vd->source.i_x_offset + vd->source.i_visible_width &&
        y >= vd->source.i_y_offset && y < vd->source.i_y_offset + vd->source.i_visible_height)
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
    if (ev->width  != vd->cfg->display.width ||
        ev->height != vd->cfg->display.height)
        vout_display_SendEventDisplaySize (vd, ev->width, ev->height, vd->cfg->is_fullscreen);
}

/**
 * Process an X11 event.
 */
static int ProcessEvent (vout_display_t *vd, bool *visible,
                         xcb_generic_event_t *ev)
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
            HandleMotionNotify (vd, (xcb_motion_notify_event_t *)ev);
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
int ManageEvent (vout_display_t *vd, xcb_connection_t *conn, bool *visible)
{
    xcb_generic_event_t *ev;

    while ((ev = xcb_poll_for_event (conn)) != NULL)
        ProcessEvent (vd, visible, ev);

    if (xcb_connection_has_error (conn))
    {
        msg_Err (vd, "X server failure");
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}
