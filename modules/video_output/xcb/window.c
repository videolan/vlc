/**
 * @file window.c
 * @brief X C Bindings window provider module for VLC media player
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

#include <stdarg.h>
#include <assert.h>

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_window.h>

#define DISPLAY_TEXT N_("X11 display")
#define DISPLAY_LONGTEXT N_( \
    "X11 hardware display to use. By default VLC will " \
    "use the value of the DISPLAY environment variable.")

static int  Open (vlc_object_t *);
static void Close (vlc_object_t *);

/*
 * Module descriptor
 */
vlc_module_begin ()
    set_shortname (N_("XCB window"))
    set_description (N_("(Experimental) XCB video window"))
    set_category (CAT_VIDEO)
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_capability ("vout_window", 10)
    set_callbacks (Open, Close)

    add_string ("x11-display", NULL, NULL,
                DISPLAY_TEXT, DISPLAY_LONGTEXT, true)
vlc_module_end ()

static int Control (vout_window_t *, int, va_list ap);

/**
 * Create an X11 window.
 */
static int Open (vlc_object_t *obj)
{
    vout_window_t *wnd = (vout_window_t *)obj;
    xcb_generic_error_t *err;
    xcb_void_cookie_t ck;

    /* Connect to X */
    char *display = var_CreateGetNonEmptyString (wnd, "x11-display");
    int snum;

    xcb_connection_t *conn = xcb_connect (display, &snum);
    free (display);
    if (xcb_connection_has_error (conn) /*== NULL*/)
        goto error;

    /* Create window */
    xcb_screen_t *scr = xcb_aux_get_screen (conn, snum);

    xcb_window_t window = xcb_generate_id (conn);
    ck = xcb_create_window_checked (conn, scr->root_depth, window, scr->root,
                                    0, 0, wnd->width, wnd->height, 0,
                                    XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                    scr->root_visual, 0, NULL);
    err = xcb_request_check (conn, ck);
    if (err)
    {
        msg_Err (wnd, "creating window: X11 error %d", err->error_code);
        goto error;
    }

    /* Make sure the window is ready */
    xcb_map_window (conn, window);
    xcb_flush (conn);

    wnd->handle = (void *)(intptr_t)window;
    wnd->p_sys = conn;
    wnd->control = Control;
    return VLC_SUCCESS;

error:
    xcb_disconnect (conn);
    return VLC_EGENERIC;
}


/**
 * Destroys the X11 window.
 */
static void Close (vlc_object_t *obj)
{
    vout_window_t *wnd = (vout_window_t *)obj;
    xcb_connection_t *conn = wnd->p_sys;
    xcb_window_t window = (uintptr_t)wnd->handle;

    xcb_unmap_window (conn, window);
    xcb_destroy_window (conn, window);
    xcb_disconnect (conn);
}


static int Control (vout_window_t *wnd, int cmd, va_list ap)
{
    msg_Err (wnd, "request %d not implemented", cmd);
    (void)ap;
    return VLC_EGENERIC;
}

