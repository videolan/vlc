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
#include <poll.h>
#include <unistd.h> /* gethostname() */
#include <limits.h> /* HOST_NAME_MAX */

#include <xcb/xcb.h>
typedef xcb_atom_t Atom;
#include <X11/Xatom.h> /* XA_WM_NAME */

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_window.h>

#include "xcb_vlc.h"

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
    set_capability ("xwindow", 10)
    set_callbacks (Open, Close)

    add_string ("x11-display", NULL, NULL,
                DISPLAY_TEXT, DISPLAY_LONGTEXT, true)
vlc_module_end ()

static int Control (vout_window_t *, int, va_list ap);
static void *Thread (void *);

struct vout_window_sys_t
{
    xcb_connection_t *conn;
    key_handler_t *keys;
    vlc_thread_t thread;

    xcb_window_t root;
    xcb_atom_t wm_state;
    xcb_atom_t wm_state_above;
    /*xcb_atom_t wmstate_fullscreen;*/
};

static inline
void set_string (xcb_connection_t *conn, xcb_window_t window,
                 xcb_atom_t type, xcb_atom_t atom, const char *str)
{
    xcb_change_property (conn, XCB_PROP_MODE_REPLACE, window, atom, type,
                         /* format */ 8, strlen (str), str);
}

static inline
void set_ascii_prop (xcb_connection_t *conn, xcb_window_t window,
                     xcb_atom_t atom, const char *value)
{
    set_string (conn, window, atom, XA_STRING, value);
}

static inline
void set_hostname_prop (xcb_connection_t *conn, xcb_window_t window)
{
    char hostname[HOST_NAME_MAX];

    if (gethostname (hostname, sizeof (hostname)) == 0)
    {
        hostname[sizeof (hostname) - 1] = '\0';
        set_ascii_prop (conn, window, XA_WM_CLIENT_MACHINE, hostname);
    }
}

static inline
xcb_intern_atom_cookie_t intern_string (xcb_connection_t *c, const char *s)
{
    return xcb_intern_atom (c, 0, strlen (s), s);
}

static
xcb_atom_t get_atom (xcb_connection_t *conn, xcb_intern_atom_cookie_t ck)
{
    xcb_intern_atom_reply_t *reply;
    xcb_atom_t atom;

    reply = xcb_intern_atom_reply (conn, ck, NULL);
    if (reply == NULL)
        return 0;

    atom = reply->atom;
    free (reply);
    return atom;
}

#define NET_WM_STATE_REMOVE 0
#define NET_WM_STATE_ADD    1
#define NET_WM_STATE_TOGGLE 2

/**
 * Create an X11 window.
 */
static int Open (vlc_object_t *obj)
{
    vout_window_t *wnd = (vout_window_t *)obj;
    vout_window_sys_t *p_sys = malloc (sizeof (*p_sys));
    xcb_generic_error_t *err;
    xcb_void_cookie_t ck;

    if (p_sys == NULL)
        return VLC_ENOMEM;

    /* Connect to X */
    char *display = var_CreateGetNonEmptyString (wnd, "x11-display");
    int snum;

    xcb_connection_t *conn = xcb_connect (display, &snum);
    free (display);
    if (xcb_connection_has_error (conn) /*== NULL*/)
        goto error;

    /* Find configured screen */
    const xcb_setup_t *setup = xcb_get_setup (conn);
    xcb_screen_t *scr = NULL;
    for (xcb_screen_iterator_t i = xcb_setup_roots_iterator (setup);
         i.rem > 0; xcb_screen_next (&i))
    {
        if (snum == 0)
        {
            scr = i.data;
            break;
        }
        snum--;
    }
    if (scr == NULL)
    {
        msg_Err (wnd, "bad X11 screen number");
        goto error;
    }

    /* Create window */
    const uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t values[2] = {
        /* XCB_CW_BACK_PIXEL */
        scr->black_pixel,
        /* XCB_CW_EVENT_MASK */
        XCB_EVENT_MASK_KEY_PRESS,
    };

    xcb_window_t window = xcb_generate_id (conn);
    ck = xcb_create_window_checked (conn, scr->root_depth, window, scr->root,
                                    0, 0, wnd->width, wnd->height, 0,
                                    XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                    scr->root_visual, mask, values);
    err = xcb_request_check (conn, ck);
    if (err)
    {
        msg_Err (wnd, "creating window: X11 error %d", err->error_code);
        goto error;
    }

    wnd->handle.xid = window;
    wnd->p_sys = p_sys;
    wnd->control = Control;

    p_sys->conn = conn;
    p_sys->keys = CreateKeyHandler (obj, conn);
    p_sys->root = scr->root;

    /* ICCCM
     * No cut&paste nor drag&drop, only Window Manager communication. */
    /* Plain ASCII localization of VLC for ICCCM window name */
    set_ascii_prop (conn, window, XA_WM_NAME,
                  vlc_pgettext ("ASCII", "VLC media player"));
    set_ascii_prop (conn, window, XA_WM_ICON_NAME,
                    vlc_pgettext ("ASCII", "VLC"));
    xcb_change_property (conn, XCB_PROP_MODE_REPLACE, window, XA_WM_CLASS,
                         XA_STRING, 8, 8, "vlc\0Vlc");
    set_hostname_prop (conn, window);

    /* EWMH */
    xcb_intern_atom_cookie_t utf8_string_ck
        = intern_string (conn, "UTF8_STRING");;
    xcb_intern_atom_cookie_t net_wm_name_ck
        = intern_string (conn, "_NET_WM_NAME");
    xcb_intern_atom_cookie_t net_wm_icon_name_ck
        = intern_string (conn, "_NET_WM_ICON_NAME");

    xcb_atom_t utf8 = get_atom (conn, utf8_string_ck);

    xcb_atom_t net_wm_name = get_atom (conn, net_wm_name_ck);
    char *title = var_CreateGetNonEmptyString (wnd, "video-title");
    if (title)
    {
        set_string (conn, window, utf8, net_wm_name, title);
        free (title);
    }
    else
        set_string (conn, window, utf8, net_wm_name, _("VLC media player"));

    xcb_atom_t net_wm_icon_name = get_atom (conn, net_wm_icon_name_ck);
    set_string (conn, window, utf8, net_wm_icon_name, _("VLC"));

    /* Cache any EWMH atom we may need later */
    xcb_intern_atom_cookie_t wm_state_ck, wm_state_above_ck;

    wm_state_ck = intern_string (conn, "_NET_WM_STATE");
    wm_state_above_ck = intern_string (conn, "_NET_WM_STATE_ABOVE");

    p_sys->wm_state = get_atom (conn, wm_state_ck);
    p_sys->wm_state_above = get_atom (conn, wm_state_above_ck);

    /* Create the event thread. It will dequeue all events, so any checked
     * request from this thread must be completed at this point. */
    if ((p_sys->keys != NULL)
     && vlc_clone (&p_sys->thread, Thread, wnd, VLC_THREAD_PRIORITY_LOW))
        DestroyKeyHandler (p_sys->keys);

    /* Make sure the window is ready */
    xcb_map_window (conn, window);
    xcb_flush (conn);

    return VLC_SUCCESS;

error:
    xcb_disconnect (conn);
    free (p_sys);
    return VLC_EGENERIC;
}


/**
 * Destroys the X11 window.
 */
static void Close (vlc_object_t *obj)
{
    vout_window_t *wnd = (vout_window_t *)obj;
    vout_window_sys_t *p_sys = wnd->p_sys;
    xcb_connection_t *conn = p_sys->conn;
    xcb_window_t window = wnd->handle.xid;

    if (p_sys->keys)
    {
        vlc_cancel (p_sys->thread);
        vlc_join (p_sys->thread, NULL);
        DestroyKeyHandler (p_sys->keys);
    }
    xcb_unmap_window (conn, window);
    xcb_destroy_window (conn, window);
    xcb_disconnect (conn);
    free (p_sys);
}


static void *Thread (void *data)
{
    vout_window_t *wnd = data;
    vout_window_sys_t *p_sys = wnd->p_sys;
    xcb_connection_t *conn = p_sys->conn;

    int fd = xcb_get_file_descriptor (conn);
    if (fd == -1)
        return NULL;

    for (;;)
    {
        xcb_generic_event_t *ev;
        struct pollfd ufd = { .fd = fd, .events = POLLIN, };

        poll (&ufd, 1, -1);

        int canc = vlc_savecancel ();
        while ((ev = xcb_poll_for_event (conn)) != NULL)
        {
            if (ProcessKeyEvent (p_sys->keys, ev) == 0)
                continue;
            msg_Dbg (wnd, "unhandled event: %"PRIu8, ev->response_type);
            free (ev);
        }
        vlc_restorecancel (canc);

        if (xcb_connection_has_error (conn))
        {
            msg_Err (wnd, "X server failure");
            break;
        }
    }
    return NULL;
}

#include <vlc_vout.h>

static int Control (vout_window_t *wnd, int cmd, va_list ap)
{
    vout_window_sys_t *p_sys = wnd->p_sys;
    xcb_connection_t *conn = p_sys->conn;

    switch (cmd)
    {
        case VOUT_SET_SIZE:
        {
            unsigned width = va_arg (ap, unsigned);
            unsigned height = va_arg (ap, unsigned);
            const uint32_t values[] = { width, height, };

            xcb_configure_window (conn, wnd->handle.xid,
                                  XCB_CONFIG_WINDOW_WIDTH |
                                  XCB_CONFIG_WINDOW_HEIGHT, values);
            xcb_flush (conn);
            break;
        }

        case VOUT_SET_STAY_ON_TOP:
        {   /* From EWMH "_WM_STATE" */
            xcb_client_message_event_t ev = {
                .response_type = XCB_CLIENT_MESSAGE,
                .format = 32,
                .window = wnd->handle.xid,
                .type = p_sys->wm_state,
            };
            bool on = va_arg (ap, int);

            ev.data.data32[0] = on ? NET_WM_STATE_ADD : NET_WM_STATE_REMOVE;
            ev.data.data32[1] = p_sys->wm_state_above;
            ev.data.data32[2] = 0;
            ev.data.data32[3] = 1;

            /* From ICCCM "Changing Window State" */
            xcb_send_event (p_sys->conn, 0, p_sys->root,
                            XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
                            XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
                            (const char *)&ev);
            xcb_flush (p_sys->conn);
            return VLC_SUCCESS;
        }

        default:
            msg_Err (wnd, "request %d not implemented", cmd);
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

