/**
 * @file window.c
 * @brief X C Bindings window provider module for VLC media player
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

#include <stdarg.h>
#include <assert.h>
#ifdef HAVE_POLL_H
# include <poll.h>
#endif
#include <unistd.h> /* gethostname() and sysconf() */
#include <limits.h> /* _POSIX_HOST_NAME_MAX */
#ifdef _WIN32
# include <winsock2.h>
#endif

#include <xcb/xcb.h>
#include <xcb/randr.h>
#ifdef HAVE_XKBCOMMON
# include <xcb/xkb.h>
# include <xkbcommon/xkbcommon-x11.h>
# include "vlc_xkb.h"
#endif
typedef xcb_atom_t Atom;
#include <X11/Xatom.h> /* XA_WM_NAME */

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_actions.h>
#include <vlc_window.h>
#include <vlc_es.h>

typedef struct
{
    xcb_connection_t *conn;
    vlc_thread_t thread;
    vlc_latch_t ready;

    xcb_window_t root;
    xcb_atom_t wm_state;
    xcb_atom_t wm_state_above;
    xcb_atom_t wm_state_below;
    xcb_atom_t wm_state_fullscreen;
    xcb_atom_t motif_wm_hints;

    uint8_t event_randr; /* event id of XCB_RANDR_NOTIFY, or 0 if disabled */
    struct
    {
        xcb_randr_crtc_t crtc;
        int x;
        int y;
        int width;
        int height;
    } *displays;
    int num_displays;
    int current_display;
    xcb_atom_t icc_atom;

#ifdef HAVE_XKBCOMMON
    struct
    {
        struct xkb_context *ctx;
        struct xkb_keymap *map;
        struct xkb_state *state;
        uint8_t base;
    } xkb;
#endif
} vout_window_sys_t;

/** Request the X11 server to internalize a string into an atom */
static inline
xcb_intern_atom_cookie_t intern_string (xcb_connection_t *c, const char *s)
{
    return xcb_intern_atom (c, 0, strlen (s), s);
}

/** Extract the X11 atom from an intern request cookie */
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

#ifdef HAVE_XKBCOMMON
static int InitKeyboard(vlc_window_t *wnd)
{
    vout_window_sys_t *sys = wnd->sys;
    xcb_connection_t *conn = sys->conn;

    int32_t core = xkb_x11_get_core_keyboard_device_id(conn);
    if (core == -1)
        return -1;

    sys->xkb.map = xkb_x11_keymap_new_from_device(sys->xkb.ctx, conn, core,
                                                  XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (unlikely(sys->xkb.map == NULL))
        return -1;

    sys->xkb.state = xkb_x11_state_new_from_device(sys->xkb.map, conn, core);
    if (unlikely(sys->xkb.state == NULL))
    {
        xkb_keymap_unref(sys->xkb.map);
        sys->xkb.map = NULL;
        return -1;
    }
    return 0;
}

static void DeinitKeyboard(vlc_window_t *wnd)
{
    vout_window_sys_t *sys = wnd->sys;

    if (sys->xkb.map == NULL)
        return;

    xkb_state_unref(sys->xkb.state);
    xkb_keymap_unref(sys->xkb.map);
    sys->xkb.map = NULL;
}

static void ProcessKeyboardEvent(vlc_window_t *wnd,
                                 const xcb_generic_event_t *ev)
{
    vout_window_sys_t *sys = wnd->sys;

    switch (ev->pad0)
    {
        case XCB_XKB_NEW_KEYBOARD_NOTIFY:
        case XCB_XKB_MAP_NOTIFY:
            msg_Dbg(wnd, "refreshing keyboard mapping");
            DeinitKeyboard(wnd);
            InitKeyboard(wnd);
            break;

        case XCB_XKB_STATE_NOTIFY:
            if (sys->xkb.map != NULL)
            {
                const xcb_xkb_state_notify_event_t *ne = (void *)ev;

                xkb_state_update_mask(sys->xkb.state, ne->baseMods,
                                      ne->latchedMods, ne->lockedMods,
                                      ne->baseGroup, ne->latchedGroup,
                                      ne->lockedGroup);
            }
            break;
    }
}

static int InitKeyboardExtension(vlc_window_t *wnd)
{
    vout_window_sys_t *sys = wnd->sys;
    xcb_connection_t *conn = sys->conn;
    uint16_t maj, min;

    if (!xkb_x11_setup_xkb_extension(conn, XKB_X11_MIN_MAJOR_XKB_VERSION,
                                     XKB_X11_MIN_MINOR_XKB_VERSION,
                                     XKB_X11_SETUP_XKB_EXTENSION_NO_FLAGS,
                                     &maj, &min, &sys->xkb.base, NULL))
    {
        msg_Err(wnd, "XKeyboard initialization error");
        return 0;
    }

    msg_Dbg(wnd, "XKeyboard v%"PRIu16".%"PRIu16" initialized", maj, min);
    sys->xkb.ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (unlikely(sys->xkb.ctx == NULL))
        return -1;

    /* Events: new KB, keymap change, state (modifiers) change */
    const uint16_t affect = XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY
                          | XCB_XKB_EVENT_TYPE_MAP_NOTIFY
                          | XCB_XKB_EVENT_TYPE_STATE_NOTIFY;
    /* Map event sub-types: everything except key behaviours */
    const uint16_t map_parts = XCB_XKB_MAP_PART_KEY_TYPES
                             | XCB_XKB_MAP_PART_KEY_SYMS
                             | XCB_XKB_MAP_PART_MODIFIER_MAP
                             | XCB_XKB_MAP_PART_EXPLICIT_COMPONENTS
                             | XCB_XKB_MAP_PART_KEY_ACTIONS
                             | XCB_XKB_MAP_PART_VIRTUAL_MODS
                             | XCB_XKB_MAP_PART_VIRTUAL_MOD_MAP;
    static const xcb_xkb_select_events_details_t details =
    {
        /* New keyboard details */
        .affectNewKeyboard = XCB_XKB_NKN_DETAIL_KEYCODES,
        .newKeyboardDetails = XCB_XKB_NKN_DETAIL_KEYCODES,
        /* State event sub-types: as per xkb_state_update_mask() */
#define STATE_PARTS  XCB_XKB_STATE_PART_MODIFIER_BASE \
                   | XCB_XKB_STATE_PART_MODIFIER_LATCH \
                   | XCB_XKB_STATE_PART_MODIFIER_LOCK \
                   | XCB_XKB_STATE_PART_GROUP_BASE \
                   | XCB_XKB_STATE_PART_GROUP_LATCH \
                   | XCB_XKB_STATE_PART_GROUP_LOCK
        .affectState = STATE_PARTS,
        .stateDetails = STATE_PARTS,
    };

    int32_t core = xkb_x11_get_core_keyboard_device_id(conn);
    if (core == -1)
    {
        xkb_context_unref(sys->xkb.ctx);
        sys->xkb.ctx = NULL;
        return -1;
    }

    xcb_xkb_select_events_aux(conn, core, affect, 0, 0, map_parts, map_parts,
                              &details);

    InitKeyboard(wnd);
    return 0;
}

static void DeinitKeyboardExtension(vlc_window_t *wnd)
{
    vout_window_sys_t *sys = wnd->sys;

    if (sys->xkb.ctx == NULL)
        return;

    DeinitKeyboard(wnd);
    xkb_context_unref(sys->xkb.ctx);
}
#else
# define DeinitKeyboardExtension(w) ((void)(w))
#endif

static int InitRandR(vlc_window_t *wnd)
{
    vout_window_sys_t *sys = wnd->sys;
    xcb_connection_t *conn = sys->conn;
    sys->event_randr = 0;

    const xcb_query_extension_reply_t *e =
        xcb_get_extension_data(conn, &xcb_randr_id);
    if (!e || !e->present)
        return VLC_ENOTSUP;

    xcb_randr_query_version_reply_t *v =
        xcb_randr_query_version_reply(conn,
            xcb_randr_query_version(conn, 1, 2), NULL);
    if (v == NULL)
        return VLC_ENOTSUP;

    sys->event_randr = e->first_event + XCB_RANDR_NOTIFY;
    msg_Dbg(wnd, "using X RandR extension v%"PRIu32".%"PRIu32,
            v->major_version, v->minor_version);

    free(v);
    xcb_randr_select_input(conn, sys->root, XCB_RANDR_NOTIFY_MASK_CRTC_CHANGE |
                                            XCB_RANDR_NOTIFY_MASK_RESOURCE_CHANGE);
    return VLC_SUCCESS;
}

static int UpdateDisplays(vlc_window_t *wnd)
{
    vout_window_sys_t *sys = wnd->sys;
    xcb_connection_t *conn = sys->conn;

    xcb_randr_get_screen_resources_reply_t *res =
        xcb_randr_get_screen_resources_reply(conn,
            xcb_randr_get_screen_resources(conn, sys->root), NULL);
    if (res == NULL)
        return VLC_EGENERIC;

    const xcb_randr_output_t *outs = xcb_randr_get_screen_resources_outputs(res);
    sys->num_displays = 0;
    sys->current_display = -1;
    sys->displays = vlc_reallocarray(sys->displays, res->num_outputs,
                                     sizeof(sys->displays[0]));
    if (sys->displays == NULL) {
        free(res);
        return VLC_ENOMEM;
    }

    struct {
        xcb_randr_get_output_info_reply_t *info;
        xcb_randr_get_output_info_cookie_t oic;
        xcb_randr_get_crtc_info_cookie_t cic;
    } *tmp = vlc_reallocarray(NULL, res->num_outputs, sizeof(tmp[0]));
    if (tmp == NULL) {
        free(res);
        return VLC_ENOMEM;
    }

    for (unsigned i = 0; i < res->num_outputs; i++)
        tmp[i].oic = xcb_randr_get_output_info(conn, outs[i], res->config_timestamp);

    for (unsigned i = 0; i < res->num_outputs; i++) {
        tmp[i].info = xcb_randr_get_output_info_reply(conn, tmp[i].oic, NULL);
        if (tmp[i].info) {
            tmp[i].cic = xcb_randr_get_crtc_info(conn, tmp[i].info->crtc,
                                                 res->config_timestamp);
        }
    }

    for (unsigned i = 0; i < res->num_outputs; i++) {
        if (!tmp[i].info)
            continue;
        xcb_randr_get_crtc_info_reply_t *crtc =
            xcb_randr_get_crtc_info_reply(conn, tmp[i].cic, NULL);
        if (crtc) {
            int idx = sys->num_displays++;
            assert(idx < res->num_outputs);
            sys->displays[idx].crtc = tmp[i].info->crtc;
            sys->displays[idx].x = crtc->x;
            sys->displays[idx].y = crtc->y;
            sys->displays[idx].width = crtc->width;
            sys->displays[idx].height = crtc->height;
            free(crtc);
        }
        free(tmp[i].info);
    }

    free(tmp);
    free(res);
    return VLC_SUCCESS;
}

static void ProcessRandREvent(vlc_window_t *wnd, xcb_generic_event_t *gev)
{
    xcb_randr_notify_event_t *ev = (xcb_randr_notify_event_t *) gev;
    vout_window_sys_t *sys = wnd->sys;

    switch (ev->subCode) {
    case XCB_RANDR_NOTIFY_RESOURCE_CHANGE:
        UpdateDisplays(wnd);
        break;
    case XCB_RANDR_NOTIFY_CRTC_CHANGE:
        for (int i = 0; i < sys->num_displays; i++) {
            const xcb_randr_crtc_change_t *cc = &ev->u.cc;
            if (sys->displays[i].crtc != cc->crtc)
                continue;
            sys->displays[i].x = cc->x;
            sys->displays[i].y = cc->y;
            sys->displays[i].width = cc->width;
            sys->displays[i].height = cc->height;
            break;
        }
        break;
    }
}

static void UpdateICCProfile(vlc_window_t *wnd)
{
    vout_window_sys_t *sys = wnd->sys;
    xcb_connection_t *conn = sys->conn;

    if (sys->icc_atom == XCB_ATOM_NONE) {
        vlc_window_ReportICCProfile(wnd, NULL);
        return;
    }

    long max_length = 32 * 1024 * 1024; /* corresponds to 128 MiB */
    xcb_get_property_reply_t *r =
        xcb_get_property_reply(conn,
            xcb_get_property(conn, false, sys->root, sys->icc_atom,
                             XCB_ATOM_CARDINAL, 0, max_length), NULL);
    if (!r || r->response_type == XCB_ATOM_NONE || !r->length || r->bytes_after) {
        vlc_window_ReportICCProfile(wnd, NULL);
        free(r);
        return;
    }

    int len = xcb_get_property_value_length(r);
    struct vlc_icc_profile_t *icc = malloc(sizeof(*icc) + len);
    if (icc) {
        icc->size = len;
        memcpy(icc->data, xcb_get_property_value(r), len);
        vlc_window_ReportICCProfile(wnd, icc);
    }
    free(r);
}

/** Get ID of closest display to a position, or -1 if no displays */
static int DisplayForCoords(vlc_window_t *wnd, int xpos, int ypos)
{
    vout_window_sys_t *sys = wnd->sys;
    if (sys->num_displays == 0)
        return -1;

    int best = 0, idx = -1;
    for (int i = 0; i < sys->num_displays; i++) {
        int xclip = VLC_CLIP(xpos, sys->displays[i].x,
                             sys->displays[i].x + sys->displays[i].width);
        int yclip = VLC_CLIP(xpos, sys->displays[i].y,
                             sys->displays[i].y + sys->displays[i].height);
        int delta = abs(xpos - xclip) + abs(ypos - yclip);
        if (delta == 0)
            return i; /* display contains point */

        if (i == 0 || best < delta) {
            best = delta;
            idx = i;
        }
    }

    return idx;
}

static void DisplayChanged(vlc_window_t *wnd, int new_display)
{
    vout_window_sys_t *sys = wnd->sys;
    xcb_connection_t *conn = sys->conn;

    char icc_prop[32];
    if (new_display > 0) {
        snprintf(icc_prop, sizeof(icc_prop), "_ICC_PROFILE_%d", new_display);
    } else {
        assert(new_display == 0);
        strcpy(icc_prop, "_ICC_PROFILE");
    }

    sys->icc_atom = get_atom(conn, intern_string(conn, icc_prop));
    UpdateICCProfile(wnd);
}

static void UpdateGeometry(vlc_window_t *wnd, unsigned x, unsigned y,
                           unsigned width, unsigned height)
{
    vout_window_sys_t *sys = wnd->sys;
    vlc_window_ReportSize(wnd, width, height);

    /* Detect window center moving across display boundaries */
    unsigned xcenter = x + (width >> 1);
    unsigned ycenter = y + (height >> 1);
    int disp = DisplayForCoords(wnd, xcenter, ycenter);
    if (disp != -1 && disp != sys->current_display) {
        sys->current_display = disp;
        DisplayChanged(wnd, disp);
    }
}

static xcb_cursor_t CursorCreate(xcb_connection_t *conn, xcb_window_t root)
{
    xcb_cursor_t cur = xcb_generate_id(conn);
    xcb_pixmap_t pix = xcb_generate_id(conn);

    xcb_create_pixmap(conn, 1, pix, root, 1, 1);
    xcb_create_cursor(conn, cur, pix, pix, 0, 0, 0, 0, 0, 0, 1, 1);
    return cur;
}

static int ProcessEvent(vlc_window_t *wnd, xcb_generic_event_t *ev)
{
    vout_window_sys_t *sys = wnd->sys;
    int ret = 0;

    switch (ev->response_type & 0x7f)
    {
        case XCB_KEY_PRESS:
        {
#ifdef HAVE_XKBCOMMON
            xcb_key_press_event_t *e = (xcb_key_press_event_t *)ev;
            uint_fast32_t vk = vlc_xkb_get_one(sys->xkb.state, e->detail);

            msg_Dbg(wnd, "key: 0x%08"PRIxFAST32" (X11: 0x%04"PRIx32")",
                    vk, e->detail);
            if (vk == KEY_UNSET)
                break;

            vlc_window_ReportKeyPress(wnd, vk);
#endif
            break;
        }

        case XCB_KEY_RELEASE:
            break;

        /* Note a direct mapping of buttons from XCB to VLC is assumed. */
        case XCB_BUTTON_PRESS:
        {
            xcb_button_release_event_t *bpe = (void *)ev;

            vlc_window_ReportMousePressed(wnd, bpe->detail - 1);
            ret = 1;
            break;
        }

        case XCB_BUTTON_RELEASE:
        {
            xcb_button_release_event_t *bre = (void *)ev;

            vlc_window_ReportMouseReleased(wnd, bre->detail - 1);
            ret = 1;
            break;
        }

        case XCB_MOTION_NOTIFY:
        {
            xcb_motion_notify_event_t *mne = (void *)ev;

            vlc_window_ReportMouseMoved(wnd, mne->event_x, mne->event_y);
            ret = 1;
            break;
        }

        case XCB_CONFIGURE_NOTIFY:
        {
            xcb_configure_notify_event_t *cne = (void *)ev;
            UpdateGeometry(wnd, cne->x, cne->y, cne->width, cne->height);
            break;
        }
        case XCB_DESTROY_NOTIFY:
            vlc_window_ReportClose (wnd);
            break;

        case XCB_UNMAP_NOTIFY:
            break;

        case XCB_MAP_NOTIFY:
            ret = 1;
            break;

        case XCB_REPARENT_NOTIFY:
            break;

        case XCB_MAPPING_NOTIFY:
            break;

        case XCB_PROPERTY_NOTIFY:
        {
            xcb_property_notify_event_t *pne = (void *)ev;
            if (pne->atom == sys->icc_atom)
                UpdateICCProfile(wnd);
            break;
        }

        default:
#ifdef HAVE_XKBCOMMON
            if (sys->xkb.ctx != NULL && ev->response_type == sys->xkb.base)
            {
                ProcessKeyboardEvent(wnd, ev);
                break;
            }
#endif
            if (sys->event_randr && ev->response_type == sys->event_randr)
            {
                ProcessRandREvent(wnd, ev);
                break;
            }

            msg_Dbg (wnd, "unhandled event %"PRIu8, ev->response_type);
    }

    free (ev);
    return ret;
}

/** Background thread for X11 events handling */
static void *Thread (void *data)
{
    vlc_thread_set_name("vlc-window-x11");

    vlc_window_t *wnd = data;
    vout_window_sys_t *p_sys = wnd->sys;
    xcb_connection_t *conn = p_sys->conn;
    struct pollfd ufd = {
        .fd = xcb_get_file_descriptor(conn),
        .events = POLLIN,
    };
    xcb_window_t window = wnd->handle.xid;
    xcb_cursor_t cursor = CursorCreate(conn, p_sys->root); /* blank cursor */
    vlc_tick_t lifetime = VLC_TICK_FROM_MS( var_InheritInteger(wnd, "mouse-hide-timeout") );
    vlc_tick_t deadline = VLC_TICK_MAX;
    xcb_generic_event_t *ev;

    if (ufd.fd == -1)
        return NULL;

    /* Report initial pointer position. */
    xcb_query_pointer_cookie_t qpc = xcb_query_pointer(conn, window);
    /* Report initial window size (for the embedded case). */
    xcb_get_geometry_cookie_t ggc = xcb_get_geometry(conn, window);

    if (p_sys->event_randr) {
        p_sys->displays = NULL;
        UpdateDisplays(wnd);
    }

    xcb_query_pointer_reply_t *qpr = xcb_query_pointer_reply(conn, qpc, NULL);
    if (qpr != NULL) {
        while ((ev = xcb_poll_for_queued_event(conn)) != NULL)
            ProcessEvent(wnd, ev);
        vlc_window_ReportMouseMoved(wnd, qpr->win_x, qpr->win_y);
        free(qpr);
    }

    xcb_get_geometry_reply_t *geo = xcb_get_geometry_reply(conn, ggc, NULL);
    if (geo != NULL) {
        while ((ev = xcb_poll_for_queued_event(conn)) != NULL)
            ProcessEvent(wnd, ev);
        UpdateGeometry(wnd, geo->x, geo->y, geo->width, geo->height);
        free(geo);
    }
    vlc_latch_count_down(&p_sys->ready, 1);

    for (;;)
    {
        int timeout = -1;

        if (deadline != VLC_TICK_MAX)
        {
            vlc_tick_t delay = deadline - vlc_tick_now();
            timeout = (delay > 0) ? MS_FROM_VLC_TICK(delay) : 0;
        }

        int val = poll(&ufd, 1, timeout);

        int canc = vlc_savecancel ();

        if (val == 0)
        {   /* timeout: hide cursor */
            xcb_change_window_attributes(conn, window, XCB_CW_CURSOR, &cursor);
            xcb_flush(conn);
            deadline = VLC_TICK_MAX;
        }
        else
        {
            bool show_cursor = false;

            while ((ev = xcb_poll_for_event (conn)) != NULL)
                show_cursor = ProcessEvent(wnd, ev) || show_cursor;

            if (show_cursor)
            {
                xcb_change_window_attributes(conn, window, XCB_CW_CURSOR,
                                             &(uint32_t){ XCB_CURSOR_NONE });
                xcb_flush(conn);
                deadline = vlc_tick_now() + lifetime;
            }
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

#define NET_WM_STATE_REMOVE 0
#define NET_WM_STATE_ADD    1
#define NET_WM_STATE_TOGGLE 2

/** Changes the EWMH state of the (mapped) window */
static void change_wm_state (vlc_window_t *wnd, bool on, xcb_atom_t state)
{
    vout_window_sys_t *sys = wnd->sys;
    /* From EWMH "_WM_STATE" */
    xcb_client_message_event_t ev = {
         .response_type = XCB_CLIENT_MESSAGE,
         .format = 32,
         .window = wnd->handle.xid,
         .type = sys->wm_state,
    };

    ev.data.data32[0] = on ? NET_WM_STATE_ADD : NET_WM_STATE_REMOVE;
    ev.data.data32[1] = state;
    ev.data.data32[2] = 0;
    ev.data.data32[3] = 1;

    /* From ICCCM "Changing Window State" */
    xcb_send_event (sys->conn, 0, sys->root,
                    XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
                    XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
                    (const char *)&ev);
}

static void Resize(vlc_window_t *wnd, unsigned width, unsigned height)
{
    vout_window_sys_t *sys = wnd->sys;
    xcb_connection_t *conn = sys->conn;
    const uint32_t values[] = { width, height, };

    xcb_configure_window(conn, wnd->handle.xid,
                         XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                         values);
    xcb_flush(conn);
}

static void SetState(vlc_window_t *wnd, unsigned state)
{
    vout_window_sys_t *sys = wnd->sys;
    bool above = (state & VLC_WINDOW_STATE_ABOVE) != 0;
    bool below = (state & VLC_WINDOW_STATE_BELOW) != 0;

    change_wm_state(wnd, above, sys->wm_state_above);
    change_wm_state(wnd, below, sys->wm_state_below);
    xcb_flush(sys->conn);
}

static void UnsetFullscreen(vlc_window_t *wnd)
{
    vout_window_sys_t *sys = wnd->sys;

    change_wm_state(wnd, false, sys->wm_state_fullscreen);
    xcb_flush(sys->conn);
}

static void SetFullscreen(vlc_window_t *wnd, const char *idstr)
{
    vout_window_sys_t *sys = wnd->sys;

    (void) idstr; /* TODO */
    change_wm_state(wnd, true, sys->wm_state_fullscreen);
    xcb_flush(sys->conn);
}

/** Set an X window property from a nul-terminated string */
static inline
void set_string (xcb_connection_t *conn, xcb_window_t window,
                 xcb_atom_t type, xcb_atom_t atom, const char *str)
{
    xcb_change_property (conn, XCB_PROP_MODE_REPLACE, window, atom, type,
                         /* format */ 8, strlen (str), str);
}

/** Set an X window string property */
static inline
void set_ascii_prop (xcb_connection_t *conn, xcb_window_t window,
                     xcb_atom_t atom, const char *value)
{
    set_string (conn, window, XA_STRING, atom, value);
}

static inline
void set_wm_hints (xcb_connection_t *conn, xcb_window_t window)
{
    static const uint32_t wm_hints[8] = {
        3, /* flags: Input, Initial state */
        1, /* input: True */
        1, /* initial state: Normal */
        0, 0, 0, 0, 0, /* Icon */
    };
    xcb_change_property (conn, XCB_PROP_MODE_REPLACE, window, XA_WM_HINTS,
                         XA_WM_HINTS, 32, 8, wm_hints);
}

/** Set the Window ICCCM client machine property */
static inline
void set_hostname_prop (xcb_connection_t *conn, xcb_window_t window)
{
    char *hostname;
#ifndef _WIN32
    long host_name_max = sysconf (_SC_HOST_NAME_MAX);

    if (host_name_max <= 0)
        host_name_max = _POSIX_HOST_NAME_MAX;
#else
    size_t host_name_max = 256;
#endif
    hostname = malloc (host_name_max);
    if (hostname == NULL)
        return;

    if (gethostname (hostname, host_name_max) == 0)
    {
        hostname[host_name_max - 1] = '\0';
        set_ascii_prop (conn, window, XA_WM_CLIENT_MACHINE, hostname);
    }
    free(hostname);
}

static int Enable(vlc_window_t *wnd, const vlc_window_cfg_t *restrict cfg)
{
    vout_window_sys_t *sys = wnd->sys;
    xcb_connection_t *conn = sys->conn;
    xcb_window_t window = wnd->handle.xid;
    xcb_void_cookie_t ck;

    /* Set initial window state */
    if (cfg->is_decorated)
        xcb_delete_property(sys->conn, wnd->handle.xid, sys->motif_wm_hints);
    else
    {
        static const uint32_t motif_wm_hints[5] = { 2, 0, 0, 0, 0 };

        xcb_change_property(sys->conn, XCB_PROP_MODE_REPLACE, wnd->handle.xid,
                            sys->motif_wm_hints, sys->motif_wm_hints, 32,
                            ARRAY_SIZE(motif_wm_hints), motif_wm_hints);
    }

    /* Make the window visible */
    ck = xcb_map_window_checked(conn, window);
    free(xcb_request_check(conn, ck));
    vlc_latch_wait(&sys->ready);
    return VLC_SUCCESS;
}

static void Disable(vlc_window_t *wnd)
{
    vout_window_sys_t *sys = wnd->sys;
    xcb_connection_t *conn = sys->conn;

    xcb_unmap_window(conn, wnd->handle.xid);
    xcb_flush(conn);
}

static void Close(vlc_window_t *);

static const struct vlc_window_operations ops = {
    .enable = Enable,
    .disable = Disable,
    .resize = Resize,
    .set_fullscreen = SetFullscreen,
    .unset_fullscreen = UnsetFullscreen,
    .destroy = Close,
    .set_state = SetState,
};

static int OpenCommon(vlc_window_t *wnd, char *display, xcb_connection_t *conn,
                      xcb_window_t root, xcb_window_t window, uint32_t events,
                      uint32_t evroot)
{
    vout_window_sys_t *sys = vlc_obj_malloc(VLC_OBJECT(wnd), sizeof (*sys));
    if (sys == NULL)
        return VLC_ENOMEM;

    wnd->type = VLC_WINDOW_TYPE_XID;
    wnd->handle.xid = window;
    wnd->display.x11 = display;
    wnd->ops = &ops;
    wnd->sys = sys;

    sys->conn = conn;
    sys->root = root;

#ifdef HAVE_XKBCOMMON
    if (events & XCB_EVENT_MASK_KEY_PRESS)
        InitKeyboardExtension(wnd);
    else
        sys->xkb.ctx = NULL;
#endif

    if (InitRandR(wnd) == VLC_SUCCESS) {
        /* Start tracking ICC profile changes */
        evroot |= XCB_EVENT_MASK_PROPERTY_CHANGE;
        xcb_change_window_attributes(conn, sys->root, XCB_CW_EVENT_MASK, &evroot);
        sys->icc_atom = XCB_ATOM_NONE;
    }

    /* ICCCM */
    /* No cut&paste nor drag&drop, only Window Manager communication. */
    set_ascii_prop(conn, window, XA_WM_NAME,
    /* xgettext: This is a plain ASCII spelling of "VLC media player"
       for the ICCCM window name. This must be pure ASCII.
       The limitation is partially with ICCCM and partially with VLC.
       For Latin script languages, you may need to strip accents.
       For other scripts, you will need to transliterate into Latin. */
                   vlc_pgettext("ASCII", "VLC media player"));

    set_ascii_prop(conn, window, XA_WM_ICON_NAME,
    /* xgettext: This is a plain ASCII spelling of "VLC"
       for the ICCCM window name. This must be pure ASCII. */
                   vlc_pgettext("ASCII", "VLC"));

    set_wm_hints(conn, window);
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, window, XA_WM_CLASS,
                        XA_STRING, 8, 8, "vlc\0Vlc");
    set_hostname_prop(conn, window);

    /* EWMH */
    xcb_intern_atom_cookie_t utf8_string_ck =
        intern_string(conn, "UTF8_STRING");
    xcb_intern_atom_cookie_t net_wm_name_ck =
        intern_string(conn, "_NET_WM_NAME");
    xcb_intern_atom_cookie_t net_wm_icon_name_ck =
        intern_string(conn, "_NET_WM_ICON_NAME");
    xcb_intern_atom_cookie_t wm_window_role_ck =
        intern_string(conn, "WM_WINDOW_ROLE");
    xcb_intern_atom_cookie_t wm_state_ck =
        intern_string(conn, "_NET_WM_STATE");
    xcb_intern_atom_cookie_t wm_state_above_ck =
        intern_string(conn, "_NET_WM_STATE_ABOVE");
    xcb_intern_atom_cookie_t wm_state_below_ck =
        intern_string(conn, "_NET_WM_STATE_BELOW");
    xcb_intern_atom_cookie_t wm_state_fs_ck =
        intern_string(conn, "_NET_WM_STATE_FULLSCREEN");
    xcb_intern_atom_cookie_t motif_wm_hints_ck =
        intern_string(conn, "_MOTIF_WM_HINTS");

    xcb_atom_t utf8 = get_atom(conn, utf8_string_ck);
    xcb_atom_t net_wm_name = get_atom(conn, net_wm_name_ck);
    char *title = var_InheritString(wnd, "video-title");

    if (title != NULL)
    {
        set_string(conn, window, utf8, net_wm_name, title);
        free(title);
    }
    else
        set_string(conn, window, utf8, net_wm_name, _("VLC media player"));

    xcb_atom_t net_wm_icon_name = get_atom(conn, net_wm_icon_name_ck);
    set_string(conn, window, utf8, net_wm_icon_name, _("VLC"));

    xcb_atom_t wm_window_role = get_atom(conn, wm_window_role_ck);
    set_ascii_prop(conn, window, wm_window_role, "vlc-video");

    /* Cache any EWMH atom we may need later */
    sys->wm_state = get_atom(conn, wm_state_ck);
    sys->wm_state_above = get_atom(conn, wm_state_above_ck);
    sys->wm_state_below = get_atom(conn, wm_state_below_ck);
    sys->wm_state_fullscreen = get_atom(conn, wm_state_fs_ck);
    sys->motif_wm_hints = get_atom(conn, motif_wm_hints_ck);

    vlc_latch_init(&sys->ready, 1);
    /* Create the event thread. It will dequeue all events, so any checked
     * request from this thread must be completed at this point. */
    if (vlc_clone(&sys->thread, Thread, wnd))
    {
        DeinitKeyboardExtension(wnd);
        return VLC_ENOMEM;
    }

    return VLC_SUCCESS;
}

/**
 * Create an X11 window.
 */
static int Open(vlc_window_t *wnd)
{
    xcb_generic_error_t *err;
    xcb_void_cookie_t ck;
    int ret = VLC_EGENERIC;

    /* Connect to X */
    char *display = var_InheritString (wnd, "x11-display");
    int snum;

    xcb_connection_t *conn = xcb_connect (display, &snum);
    if (xcb_connection_has_error (conn) /*== NULL*/)
        goto error;

    /* Find configured screen */
    const xcb_setup_t *setup = xcb_get_setup (conn);
    const xcb_screen_t *scr = NULL;
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
        XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_STRUCTURE_NOTIFY,
    };

    if (var_InheritBool(wnd, "keyboard-events"))
        values[1] |= XCB_EVENT_MASK_KEY_PRESS;
    if (var_InheritBool(wnd, "mouse-events"))
        values[1] |= XCB_EVENT_MASK_BUTTON_PRESS
                   | XCB_EVENT_MASK_BUTTON_RELEASE;

    xcb_window_t window = xcb_generate_id (conn);
    ck = xcb_create_window_checked (conn, scr->root_depth, window, scr->root,
                                    0, 0, 1, 1, 0,
                                    XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                    scr->root_visual, mask, values);
    err = xcb_request_check (conn, ck);
    if (err)
    {
        msg_Err (wnd, "creating window: X11 error %d", err->error_code);
        free (err);
        goto error;
    }

    ret = OpenCommon(wnd, display, conn, scr->root, window, values[1], 0);
    if (ret != VLC_SUCCESS)
        goto error;

    return VLC_SUCCESS;

error:
    xcb_disconnect (conn);
    free (display);
    return ret;
}


/**
 * Destroys the X11 window.
 */
static void Close (vlc_window_t *wnd)
{
    vout_window_sys_t *p_sys = wnd->sys;
    xcb_connection_t *conn = p_sys->conn;

    vlc_cancel (p_sys->thread);
    vlc_join (p_sys->thread, NULL);

    DeinitKeyboardExtension(wnd);
    xcb_disconnect (conn);
    free (wnd->display.x11);
}

/*** Embedded drawable support ***/

static int EmEnable(vlc_window_t *wnd, const vlc_window_cfg_t *restrict cfg)
{
    vout_window_sys_t *sys = wnd->sys;

    change_wm_state(wnd, cfg->is_fullscreen, sys->wm_state_fullscreen);
    return VLC_SUCCESS;
}

static vlc_mutex_t serializer = VLC_STATIC_MUTEX;

/** Acquire a drawable */
static int AcquireDrawable (vlc_object_t *obj, xcb_window_t window)
{
    vlc_object_t *vlc = VLC_OBJECT(vlc_object_instance(obj));
    xcb_window_t *used;
    size_t n = 0;

    if (var_Create(vlc, "xid-in-use", VLC_VAR_ADDRESS))
        return VLC_ENOMEM;

    /* Keep a list of busy drawables, so we don't overlap videos if there are
     * more than one video track in the stream. */
    vlc_mutex_lock (&serializer);
    used = var_GetAddress(vlc, "xid-in-use");
    if (used != NULL)
    {
        while (used[n])
        {
            if (used[n] == window)
                goto skip;
            n++;
        }
    }

    used = realloc (used, sizeof (*used) * (n + 2));
    if (used != NULL)
    {
        used[n] = window;
        used[n + 1] = 0;
        var_SetAddress(vlc, "xid-in-use", used);
    }
    else
    {
skip:
        msg_Warn (obj, "X11 drawable 0x%08"PRIx8" is busy", window);
        window = 0;
    }
    vlc_mutex_unlock (&serializer);

    return (window == 0) ? VLC_EGENERIC : VLC_SUCCESS;
}

/** Remove this drawable from the list of busy ones */
static void ReleaseDrawable (vlc_object_t *obj, xcb_window_t window)
{
    vlc_object_t *vlc = VLC_OBJECT(vlc_object_instance(obj));
    xcb_window_t *used;
    size_t n = 0;

    vlc_mutex_lock (&serializer);
    used = var_GetAddress(vlc, "xid-in-use");
    assert (used);
    while (used[n] != window)
    {
        assert (used[n]);
        n++;
    }
    do
        used[n] = used[n + 1];
    while (used[++n]);

    if (!used[0])
        var_SetAddress(vlc, "xid-in-use", NULL);
    else
        used = NULL;

    vlc_mutex_unlock (&serializer);

    free( used );

    /* Variables are reference-counted... */
    var_Destroy(vlc, "xid-in-use");
}

static void EmClose(vlc_window_t *);

static const struct vlc_window_operations em_ops = {
    .enable = EmEnable,
    .set_fullscreen = SetFullscreen,
    .unset_fullscreen = UnsetFullscreen,
    .destroy = EmClose,
    .set_state = SetState,
};

/**
 * Wrap an existing X11 window to embed the video.
 */
static int EmOpen (vlc_window_t *wnd)
{
    int ret = VLC_EGENERIC;
    xcb_window_t window = var_InheritInteger (wnd, "drawable-xid");
    if (window == 0)
        return VLC_EGENERIC;

    if (AcquireDrawable (VLC_OBJECT(wnd), window))
        return VLC_EGENERIC;

    xcb_connection_t *conn = xcb_connect (NULL, NULL);
    if (xcb_connection_has_error (conn))
        goto error;

    /* Subscribe to window events (_before_ the geometry is queried) */
    uint32_t mask = XCB_CW_EVENT_MASK;
    uint32_t value = XCB_EVENT_MASK_POINTER_MOTION
                   | XCB_EVENT_MASK_STRUCTURE_NOTIFY;
    xcb_window_t root;

    xcb_change_window_attributes (conn, window, mask, &value);

    xcb_get_geometry_reply_t *geo =
        xcb_get_geometry_reply (conn, xcb_get_geometry (conn, window), NULL);
    if (geo == NULL)
    {
        msg_Err (wnd, "bad X11 window 0x%08"PRIx8, window);
        goto error;
    }
    root = geo->root;
    free (geo);

    /* Try to subscribe to keyboard and mouse events (only one X11 client can
     * subscribe to input events, so this can fail). */
    if (var_InheritBool(wnd, "keyboard-events"))
        value |= XCB_EVENT_MASK_KEY_PRESS;
    if (var_InheritBool(wnd, "mouse-events"))
        value |= XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE;

    xcb_change_window_attributes(conn, window, XCB_CW_EVENT_MASK, &value);

    uint32_t evroot = (root == window) ? value : 0;
    ret = OpenCommon(wnd, NULL, conn, root, window, value, evroot);
    if (ret != VLC_SUCCESS)
        goto error;

    wnd->ops = &em_ops;
    return VLC_SUCCESS;

error:
    xcb_disconnect (conn);
    ReleaseDrawable (VLC_OBJECT(wnd), window);
    return VLC_EGENERIC;
}

static void EmClose (vlc_window_t *wnd)
{
    xcb_window_t window = wnd->handle.xid;

    Close (wnd);
    ReleaseDrawable (VLC_OBJECT(wnd), window);
}

#define DISPLAY_TEXT N_("X11 display")
#define DISPLAY_LONGTEXT N_( \
    "Video will be rendered with this X11 display. " \
    "If empty, the default display will be used.")

#define XID_TEXT N_("X11 window ID")
#define XID_LONGTEXT N_( \
    "Video will be embedded in this pre-existing window. " \
    "If zero, a new window will be created.")

/*
 * Module descriptor
 */
vlc_module_begin ()
    set_shortname (N_("X window"))
    set_description (N_("X11 video window (XCB)"))
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_capability ("vout window", 10)
    set_callback(Open)

    add_submodule ()
    set_shortname (N_("Drawable"))
    set_description (N_("Embedded window video"))
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_capability ("vout window", 70)
    set_callback(EmOpen)
    add_shortcut ("embed-xid")

    add_string ("x11-display", NULL, DISPLAY_TEXT, DISPLAY_LONGTEXT)
    add_integer ("drawable-xid", 0, XID_TEXT, XID_LONGTEXT)
        change_volatile ()
vlc_module_end ()
