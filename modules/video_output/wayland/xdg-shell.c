/**
 * @file xdg-shell.c
 * @brief Desktop shell surface provider module for VLC media player
 */
/*****************************************************************************
 * Copyright © 2014, 2017 Rémi Denis-Courmont
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

#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>

#include <wayland-client.h>
#ifdef XDG_SHELL
#ifndef XDG_SHELL_UNSTABLE_VERSION
#include "xdg-shell-client-protocol.h"
#else
#include "xdg-shell-unstable-v6-client-protocol.h"
# define xdg_wm_base zxdg_shell_v6
# define xdg_wm_base_interface zxdg_shell_v6_interface
# define xdg_wm_base_listener zxdg_shell_v6_listener
# define xdg_wm_base_add_listener zxdg_shell_v6_add_listener
# define xdg_wm_base_destroy zxdg_shell_v6_destroy
# define xdg_wm_base_get_xdg_surface zxdg_shell_v6_get_xdg_surface
# define xdg_wm_base_pong zxdg_shell_v6_pong
# define xdg_surface zxdg_surface_v6
# define xdg_surface_listener zxdg_surface_v6_listener
# define xdg_surface_add_listener zxdg_surface_v6_add_listener
# define xdg_surface_destroy zxdg_surface_v6_destroy
# define xdg_surface_get_toplevel zxdg_surface_v6_get_toplevel
# define xdg_surface_set_window_geometry zxdg_surface_v6_set_window_geometry
# define xdg_surface_ack_configure zxdg_surface_v6_ack_configure
# define xdg_toplevel zxdg_toplevel_v6
# define xdg_toplevel_listener zxdg_toplevel_v6_listener
# define xdg_toplevel_add_listener zxdg_toplevel_v6_add_listener
# define xdg_toplevel_destroy zxdg_toplevel_v6_destroy
# define xdg_toplevel_set_title zxdg_toplevel_v6_set_title
# define xdg_toplevel_set_app_id zxdg_toplevel_v6_set_app_id
# define xdg_toplevel_set_fullscreen zxdg_toplevel_v6_set_fullscreen
# define xdg_toplevel_unset_fullscreen zxdg_toplevel_v6_unset_fullscreen
# define XDG_TOPLEVEL_STATE_FULLSCREEN ZXDG_TOPLEVEL_V6_STATE_FULLSCREEN
#endif
#else
# define xdg_wm_base wl_shell
# define xdg_wm_base_interface wl_shell_interface
# define xdg_wm_base_add_listener(s, l, q) (void)0
# define xdg_wm_base_destroy wl_shell_destroy
# define xdg_wm_base_get_xdg_surface wl_shell_get_shell_surface
# define xdg_wm_base_pong wl_shell_pong
# define xdg_surface wl_shell_surface
# define xdg_surface_listener wl_shell_surface_listener
# define xdg_surface_add_listener wl_shell_surface_add_listener
# define xdg_surface_destroy wl_shell_surface_destroy
# define xdg_surface_get_toplevel(s) (s)
# define xdg_surface_set_window_geometry(s,x,y,w,h) (void)0
# define xdg_toplevel wl_shell_surface
# define xdg_toplevel_add_listener(s, l, q) (void)0
# define xdg_toplevel_destroy(s) (void)0
# define xdg_toplevel_set_title wl_shell_surface_set_title
# define xdg_toplevel_set_app_id(s, i) (void)0
# define xdg_toplevel_set_fullscreen(s, o) \
         wl_shell_surface_set_fullscreen(s, 1, 0, o)
# define xdg_toplevel_unset_fullscreen wl_shell_surface_set_toplevel
#endif
#include "server-decoration-client-protocol.h"

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_window.h>

#include "input.h"

struct vout_window_sys_t
{
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct xdg_wm_base *wm_base;
    struct xdg_surface *surface;
    struct xdg_toplevel *toplevel;
    struct org_kde_kwin_server_decoration_manager *deco_manager;
    struct org_kde_kwin_server_decoration *deco;

    uint32_t default_output;

    unsigned width;
    unsigned height;
    bool fullscreen;

    struct wl_list seats;

    vlc_thread_t thread;
};

static void cleanup_wl_display_read(void *data)
{
    struct wl_display *display = data;

    wl_display_cancel_read(display);
}

/** Background thread for Wayland shell events handling */
static void *Thread(void *data)
{
    vout_window_t *wnd = data;
    struct wl_display *display = wnd->display.wl;
    struct pollfd ufd[1];

    int canc = vlc_savecancel();
    vlc_cleanup_push(cleanup_wl_display_read, display);

    ufd[0].fd = wl_display_get_fd(display);
    ufd[0].events = POLLIN;

    for (;;)
    {
        while (wl_display_prepare_read(display) != 0)
            wl_display_dispatch_pending(display);

        wl_display_flush(display);
        vlc_restorecancel(canc);

        while (poll(ufd, 1, -1) < 0);

        canc = vlc_savecancel();
        wl_display_read_events(display);
        wl_display_dispatch_pending(display);
    }
    vlc_assert_unreachable();
    vlc_cleanup_pop();
    //vlc_restorecancel(canc);
    //return NULL;
}

struct device_data
{
    uint32_t name;
    struct vout_window_t *window;
};

static int Control(vout_window_t *wnd, int cmd, va_list ap)
{
    vout_window_sys_t *sys = wnd->sys;
    struct wl_display *display = wnd->display.wl;

    switch (cmd)
    {
        case VOUT_WINDOW_SET_STATE:
            return VLC_EGENERIC;

        case VOUT_WINDOW_SET_SIZE:
        {
            unsigned width = va_arg(ap, unsigned);
            unsigned height = va_arg(ap, unsigned);

            /* Unlike X11, the client basically gets to choose its size, which
             * is the size of the buffer attached to the surface.
             * Note that it is unspecified who "wins" in case of a race
             * (e.g. if trying to resize the window, and changing the zoom
             * at the same time). With X11, the race is arbitrated by the X11
             * server. With Wayland, it is arbitrated in the client windowing
             * code. In this case, it is arbitrated by the window core code.
             */
            vout_window_ReportSize(wnd, width, height);
            xdg_surface_set_window_geometry(sys->surface, 0, 0, width, height);
            break;
        }

        case VOUT_WINDOW_SET_FULLSCREEN:
        {
            const char *idstr = va_arg(ap, const char *);
            struct wl_output *output = NULL;

            if (idstr != NULL)
            {
                char *end;
                unsigned long name = strtoul(idstr, &end, 10);

                assert(*end == '\0' && name <= UINT32_MAX);
                output = wl_registry_bind(sys->registry, name,
                                          &wl_output_interface, 1);
            }
            else
            if (sys->default_output != 0)
                output = wl_registry_bind(sys->registry, sys->default_output,
                                          &wl_output_interface, 1);

            xdg_toplevel_set_fullscreen(sys->toplevel, output);

            if (output != NULL)
                wl_output_destroy(output);
            break;
        }

        case VOUT_WINDOW_UNSET_FULLSCREEN:
            xdg_toplevel_unset_fullscreen(sys->toplevel);
            break;

        default:
            msg_Err(wnd, "request %d not implemented", cmd);
            return VLC_EGENERIC;
    }

    wl_display_flush(display);
    return VLC_SUCCESS;
}

#ifdef XDG_SHELL
static void xdg_toplevel_configure_cb(void *data,
                                      struct xdg_toplevel *toplevel,
                                      int32_t width, int32_t height,
                                      struct wl_array *states)
{
    vout_window_t *wnd = data;
    vout_window_sys_t *sys = wnd->sys;
    const uint32_t *state;

    msg_Dbg(wnd, "new configuration: %"PRId32"x%"PRId32, width, height);

    sys->fullscreen = false;
    wl_array_for_each(state, states)
    {
        msg_Dbg(wnd, " - state 0x%04"PRIX32, *state);

        switch (*state)
        {
            case XDG_TOPLEVEL_STATE_FULLSCREEN:
                sys->fullscreen = true;
                break;
        }
    }

    /* Zero width or zero height means client (we) should choose.
     * DO NOT REPORT those values to video output... */
    if (width != 0)
        sys->width = width;
    if (height != 0)
        sys->height = height;

    (void) toplevel;
}

static void xdg_toplevel_close_cb(void *data, struct xdg_toplevel *toplevel)
{
    vout_window_t *wnd = data;

    vout_window_ReportClose(wnd);
    (void) toplevel;
}

static const struct xdg_toplevel_listener xdg_toplevel_cbs =
{
    xdg_toplevel_configure_cb,
    xdg_toplevel_close_cb,
};

static void xdg_surface_configure_cb(void *data, struct xdg_surface *surface,
                                     uint32_t serial)
{
    vout_window_t *wnd = data;
    vout_window_sys_t *sys = wnd->sys;

    vout_window_ReportSize(wnd, sys->width, sys->height);

    if (sys->fullscreen)
        vout_window_ReportFullscreen(wnd, NULL);
    else
        vout_window_ReportWindowed(wnd);

    xdg_surface_set_window_geometry(surface, 0, 0, sys->width, sys->height);
    xdg_surface_ack_configure(surface, serial);
}

static const struct xdg_surface_listener xdg_surface_cbs =
{
    xdg_surface_configure_cb,
};

static void xdg_wm_base_ping_cb(void *data, struct xdg_wm_base *wm_base,
                                uint32_t serial)
{
    (void) data;
    xdg_wm_base_pong(wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_cbs =
{
    xdg_wm_base_ping_cb,
};
#else
static void wl_shell_surface_configure_cb(void *data,
                                          struct wl_shell_surface *toplevel,
                                          uint32_t edges,
                                          int32_t width, int32_t height)
{
    vout_window_t *wnd = data;

    msg_Dbg(wnd, "new configuration: %"PRId32"x%"PRId32, width, height);
    vout_window_ReportSize(wnd, width, height);
    (void) toplevel; (void) edges;
}

static void wl_shell_surface_ping_cb(void *data,
                                     struct wl_shell_surface *surface,
                                     uint32_t serial)
{
    (void) data;
    wl_shell_surface_pong(surface, serial);
}

static void wl_shell_surface_popup_done_cb(void *data,
                                           struct wl_shell_surface *surface)
{
    (void) data; (void) surface;
}

static const struct wl_shell_surface_listener wl_shell_surface_cbs =
{
    wl_shell_surface_ping_cb,
    wl_shell_surface_configure_cb,
    wl_shell_surface_popup_done_cb,
};
#define xdg_surface_cbs wl_shell_surface_cbs
#endif

static void output_geometry_cb(void *data, struct wl_output *output,
                               int32_t x, int32_t y, int32_t w, int32_t h,
                               int32_t sp, const char *make, const char *model,
                               int32_t transform)
{
    struct device_data *dd = data;
    struct vout_window_t *wnd = dd->window;
    char idstr[11];
    char *name;

    msg_Dbg(wnd, "output %"PRIu32" geometry: %"PRId32"x%"PRId32"mm"
            "+%"PRId32"+%"PRId32", subpixel %"PRId32", transform %"PRId32,
            dd->name, w, h, x, y, sp, transform);

    sprintf(idstr, "%"PRIu32, dd->name);
    if (likely(asprintf(&name, "%s - %s", make, model) >= 0))
    {
        vout_window_ReportOutputDevice(wnd, idstr, name);
        free(name);
    }
    (void) output;
}

static void output_mode_cb(void *data, struct wl_output *output,
                           uint32_t flags, int32_t w, int32_t h, int32_t vr)
{
    struct device_data *dd = data;
    struct vout_window_t *wnd = dd->window;
    div_t d = div(vr, 1000);

    msg_Dbg(wnd, "output %"PRIu32" mode: 0x%"PRIx32" %"PRId32"x%"PRId32
            ", %d.%03d Hz", dd->name, flags, w, h, d.quot, d.rem);
    (void) output;
}

static void output_done_cb(void *data, struct wl_output *output)
{
    wl_output_destroy(output);
    free(data);
}

static void output_scale_cb(void *data, struct wl_output *output, int32_t f)
{
    struct device_data *dd = data;
    struct vout_window_t *wnd = dd->window;

    msg_Dbg(wnd, "output %"PRIu32" scale: %"PRId32, dd->name, f);
    (void) output;
}

static const struct wl_output_listener output_cbs =
{
    output_geometry_cb,
    output_mode_cb,
    output_done_cb,
    output_scale_cb,
};

static void registry_global_cb(void *data, struct wl_registry *registry,
                               uint32_t name, const char *iface, uint32_t vers)
{
    vout_window_t *wnd = data;
    vout_window_sys_t *sys = wnd->sys;

    msg_Dbg(wnd, "global %3"PRIu32": %s version %"PRIu32, name, iface, vers);

    if (!strcmp(iface, "wl_compositor"))
        sys->compositor = wl_registry_bind(registry, name,
                                           &wl_compositor_interface,
                                           (vers < 2) ? vers : 2);
    else
    if (!strcmp(iface, "wl_output") && vers >= 2)
    {
        struct device_data *dd = malloc(sizeof (*dd));
        if (unlikely(dd == NULL))
            return;

        struct wl_output *output = wl_registry_bind(registry, name,
                                                    &wl_output_interface, 2);
        if (unlikely(output == NULL))
            return;

        dd->name = name;
        dd->window = wnd;
        wl_output_add_listener(output, &output_cbs, dd);
    }
    else
#ifdef XDG_SHELL
# ifndef XDG_SHELL_UNSTABLE_VERSION
    if (!strcmp(iface, "xdg_wm_base"))
# else
    if (!strcmp(iface, "zxdg_shell_v6"))
# endif
#else
    if (!strcmp(iface, "wl_shell"))
#endif
        sys->wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface,
                                        1);
    else
    if (!strcmp(iface, "wl_seat"))
        seat_create(wnd, registry, name, vers, &sys->seats);
    else
    if (!strcmp(iface, "org_kde_kwin_server_decoration_manager"))
        sys->deco_manager = wl_registry_bind(registry, name,
                         &org_kde_kwin_server_decoration_manager_interface, 1);
}

static void registry_global_remove_cb(void *data, struct wl_registry *registry,
                                      uint32_t name)
{
    vout_window_t *wnd = data;
    char idstr[11];

    msg_Dbg(wnd, "global remove %3"PRIu32, name);

    /* If the global was an output, this will remove it. Otherwise, no-op. */
    sprintf(idstr, "%"PRIu32, name);
    vout_window_ReportOutputDevice(wnd, idstr, NULL);

    (void) registry;
}

static const struct wl_registry_listener registry_cbs =
{
    registry_global_cb,
    registry_global_remove_cb,
};

/**
 * Creates a Wayland shell surface.
 */
static int Open(vout_window_t *wnd, const vout_window_cfg_t *cfg)
{
    vout_window_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->compositor = NULL;
    sys->wm_base = NULL;
    sys->surface = NULL;
    sys->toplevel = NULL;
    sys->deco_manager = NULL;
    sys->deco = NULL;
    sys->default_output = var_InheritInteger(wnd, "wl-output");
    sys->width = cfg->width;
    sys->height = cfg->height;
    sys->fullscreen = false;
    wl_list_init(&sys->seats);
    wnd->sys = sys;

    /* Connect to the display server */
    char *dpy_name = var_InheritString(wnd, "wl-display");
    struct wl_display *display = wl_display_connect(dpy_name);

    free(dpy_name);

    if (display == NULL)
    {
        free(sys);
        return VLC_EGENERIC;
    }

    /* Find the interesting singleton(s) */
    sys->registry = wl_display_get_registry(display);
    if (sys->registry == NULL)
        goto error;

    wl_registry_add_listener(sys->registry, &registry_cbs, wnd);
    wl_display_roundtrip(display); /* complete registry enumeration */
    wl_display_roundtrip(display); /* complete devices enumeration */

    if (sys->compositor == NULL || sys->wm_base == NULL)
        goto error;

    xdg_wm_base_add_listener(sys->wm_base, &xdg_wm_base_cbs, NULL);

    /* Create a surface */
    struct wl_surface *surface = wl_compositor_create_surface(sys->compositor);
    if (surface == NULL)
        goto error;

    struct xdg_surface *xdg_surface =
        xdg_wm_base_get_xdg_surface(sys->wm_base, surface);
    if (xdg_surface == NULL)
        goto error;

    sys->surface = xdg_surface;
    xdg_surface_add_listener(xdg_surface, &xdg_surface_cbs, wnd);

    struct xdg_toplevel *toplevel = xdg_surface_get_toplevel(xdg_surface);
    if (toplevel == NULL)
        goto error;

    sys->toplevel = toplevel;
    xdg_toplevel_add_listener(toplevel, &xdg_toplevel_cbs, wnd);

    char *title = var_InheritString(wnd, "video-title");
    xdg_toplevel_set_title(toplevel,
                           (title != NULL) ? title : _("VLC media player"));
    free(title);

    char *app_id = var_InheritString(wnd, "app-id");
    if (app_id != NULL)
    {
        xdg_toplevel_set_app_id(toplevel, app_id);
        free(app_id);
    }

    xdg_surface_set_window_geometry(xdg_surface, 0, 0,
                                    cfg->width, cfg->height);
    vout_window_ReportSize(wnd, cfg->width, cfg->height);

    const uint_fast32_t deco_mode =
        var_InheritBool(wnd, "video-deco")
            ? ORG_KDE_KWIN_SERVER_DECORATION_MODE_SERVER
            : ORG_KDE_KWIN_SERVER_DECORATION_MODE_CLIENT;

    if (sys->deco_manager != NULL)
        sys->deco = org_kde_kwin_server_decoration_manager_create(
                                                   sys->deco_manager, surface);
    if (sys->deco != NULL)
        org_kde_kwin_server_decoration_request_mode(sys->deco, deco_mode);
    else
    if (deco_mode != ORG_KDE_KWIN_SERVER_DECORATION_MODE_CLIENT)
    {
        msg_Err(wnd, "server-side decoration not supported");
        goto error;
    }

    //if (var_InheritBool (wnd, "keyboard-events"))
    //    do_something();

    wl_display_flush(display);

    wnd->type = VOUT_WINDOW_TYPE_WAYLAND;
    wnd->handle.wl = surface;
    wnd->display.wl = display;
    wnd->control = Control;

    if (cfg->is_fullscreen)
        vout_window_SetFullScreen(wnd, NULL);

    if (vlc_clone(&sys->thread, Thread, wnd, VLC_THREAD_PRIORITY_LOW))
        goto error;

#ifdef XDG_SHELL_UNSTABLE_VERSION
    msg_Warn(wnd, "using XDG shell unstable version %d",
             XDG_SHELL_UNSTABLE_VERSION);
    msg_Info(wnd, "The window manager needs an update.");
#endif
    return VLC_SUCCESS;

error:
    seat_destroy_all(&sys->seats);
    if (sys->deco != NULL)
        org_kde_kwin_server_decoration_destroy(sys->deco);
    if (sys->deco_manager != NULL)
        org_kde_kwin_server_decoration_manager_destroy(sys->deco_manager);
    if (sys->toplevel != NULL)
        xdg_toplevel_destroy(sys->toplevel);
    if (sys->surface != NULL)
        xdg_surface_destroy(sys->surface);
    if (sys->wm_base != NULL)
        xdg_wm_base_destroy(sys->wm_base);
    if (sys->compositor != NULL)
        wl_compositor_destroy(sys->compositor);
    if (sys->registry != NULL)
        wl_registry_destroy(sys->registry);
    wl_display_disconnect(display);
    free(sys);
    return VLC_EGENERIC;
}

/**
 * Destroys a XDG shell surface.
 */
static void Close(vout_window_t *wnd)
{
    vout_window_sys_t *sys = wnd->sys;

    vlc_cancel(sys->thread);
    vlc_join(sys->thread, NULL);

    seat_destroy_all(&sys->seats);
    if (sys->deco != NULL)
        org_kde_kwin_server_decoration_destroy(sys->deco);
    if (sys->deco_manager != NULL)
        org_kde_kwin_server_decoration_manager_destroy(sys->deco_manager);
    xdg_toplevel_destroy(sys->toplevel);
    xdg_surface_destroy(sys->surface);
    wl_surface_destroy(wnd->handle.wl);
    xdg_wm_base_destroy(sys->wm_base);
    wl_compositor_destroy(sys->compositor);
    wl_registry_destroy(sys->registry);
    wl_display_disconnect(wnd->display.wl);
    free(sys);
}


#define DISPLAY_TEXT N_("Wayland display")
#define DISPLAY_LONGTEXT N_( \
    "Video will be rendered with this Wayland display. " \
    "If empty, the default display will be used.")

#define OUTPUT_TEXT N_("Fullscreen output")
#define OUTPUT_LONGTEXT N_( \
    "Fullscreen mode with use the output with this name by default.")

vlc_module_begin()
#ifdef XDG_SHELL
# ifndef XDG_SHELL_UNSTABLE_VERSION
    set_shortname(N_("XDG shell"))
    set_description(N_("XDG shell surface"))
# else
    set_shortname(N_("XDG shell v6"))
    set_description(N_("XDG shell (unstable version 6) surface"))
# endif
#else
    set_shortname(N_("WL shell"))
    set_description(N_("Wayland shell surface"))
#endif
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
#ifdef XDG_SHELL
# ifndef XDG_SHELL_UNSTABLE_VERSION
    set_capability("vout window", 20)
# else
    set_capability("vout window", 19)
# endif
#else
    set_capability("vout window", 10)
#endif
    set_callbacks(Open, Close)

    add_string("wl-display", NULL, DISPLAY_TEXT, DISPLAY_LONGTEXT, true)
    add_integer("wl-output", 0, OUTPUT_TEXT, OUTPUT_LONGTEXT, true)
        change_integer_range(0, UINT32_MAX)
        change_volatile()
vlc_module_end()
