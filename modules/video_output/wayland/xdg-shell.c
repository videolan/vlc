/**
 * @file xdg-shell.c
 * @brief XDG shell surface provider module for VLC media player
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
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <poll.h>

#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include "server-decoration-client-protocol.h"

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_window.h>

static_assert (XDG_SHELL_VERSION_CURRENT == 5, "XDG shell version mismatch");

struct vout_window_sys_t
{
    struct wl_compositor *compositor;
    struct xdg_shell *shell;
    struct xdg_surface *surface;
    struct org_kde_kwin_server_decoration_manager *deco_manager;
    struct org_kde_kwin_server_decoration *deco;

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
            bool fs = va_arg(ap, int);

            if (fs)
                xdg_surface_set_fullscreen(sys->surface, NULL);
            else
                xdg_surface_unset_fullscreen(sys->surface);
            break;
        }

        default:
            msg_Err(wnd, "request %d not implemented", cmd);
            return VLC_EGENERIC;
    }

    wl_display_flush(display);
    return VLC_SUCCESS;
}

static void xdg_surface_configure_cb(void *data, struct xdg_surface *surface,
                                     int32_t width, int32_t height,
                                     struct wl_array *states,
                                     uint32_t serial)
{
    vout_window_t *wnd = data;
    const uint32_t *state;

    msg_Dbg(wnd, "new configuration: %"PRId32"x%"PRId32" (serial: %"PRIu32")",
            width, height, serial);
    wl_array_for_each(state, states)
    {
        msg_Dbg(wnd, " - state 0x%04"PRIX32, *state);
    }

    /* Zero width or zero height means client (we) should choose.
     * DO NOT REPORT those values to video output... */
    if (width != 0 && height != 0)
        vout_window_ReportSize(wnd,  width, height);

    /* TODO: report fullscreen state, not implemented in VLC */
    xdg_surface_ack_configure(surface, serial);
}

static void xdg_surface_close_cb(void *data, struct xdg_surface *surface)
{
    vout_window_t *wnd = data;

    vout_window_ReportClose(wnd);
    (void) surface;
}

static const struct xdg_surface_listener xdg_surface_cbs =
{
    xdg_surface_configure_cb,
    xdg_surface_close_cb,
};

static void xdg_shell_ping_cb(void *data, struct xdg_shell *shell,
                              uint32_t serial)
{
    (void) data;
    xdg_shell_pong(shell, serial);
}

static const struct xdg_shell_listener xdg_shell_cbs =
{
    xdg_shell_ping_cb,
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
    if (!strcmp(iface, "xdg_shell"))
        sys->shell = wl_registry_bind(registry, name, &xdg_shell_interface, 1);
    else
    if (!strcmp(iface, "org_kde_kwin_server_decoration_manager"))
        sys->deco_manager = wl_registry_bind(registry, name,
                         &org_kde_kwin_server_decoration_manager_interface, 1);
}

static void registry_global_remove_cb(void *data, struct wl_registry *registry,
                                      uint32_t name)
{
    vout_window_t *wnd = data;

    msg_Dbg(wnd, "global remove %3"PRIu32, name);
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
    if (cfg->type != VOUT_WINDOW_TYPE_INVALID
     && cfg->type != VOUT_WINDOW_TYPE_WAYLAND)
        return VLC_EGENERIC;

    vout_window_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->compositor = NULL;
    sys->shell = NULL;
    sys->surface = NULL;
    sys->deco_manager = NULL;
    sys->deco = NULL;
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
    struct wl_registry *registry = wl_display_get_registry(display);
    if (registry == NULL)
        goto error;

    wl_registry_add_listener(registry, &registry_cbs, wnd);
    wl_display_roundtrip(display);
    wl_registry_destroy(registry);

    if (sys->compositor == NULL || sys->shell == NULL)
        goto error;

    xdg_shell_use_unstable_version(sys->shell, XDG_SHELL_VERSION_CURRENT);
    xdg_shell_add_listener(sys->shell, &xdg_shell_cbs, NULL);

    /* Create a surface */
    struct wl_surface *surface = wl_compositor_create_surface(sys->compositor);
    if (surface == NULL)
        goto error;

    struct xdg_surface *xdg_surface =
        xdg_shell_get_xdg_surface(sys->shell, surface);
    if (xdg_surface == NULL)
        goto error;

    sys->surface = xdg_surface;

    xdg_surface_add_listener(xdg_surface, &xdg_surface_cbs, wnd);

    char *title = var_InheritString(wnd, "video-title");
    xdg_surface_set_title(xdg_surface,
                          (title != NULL) ? title : _("VLC media player"));
    free(title);

    char *app_id = var_InheritString(wnd, "app-id");
    if (app_id != NULL)
    {
        xdg_surface_set_app_id(xdg_surface, app_id);
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
    else if (deco_mode == ORG_KDE_KWIN_SERVER_DECORATION_MODE_SERVER)
        msg_Err(wnd, "server-side decoration not supported");

    //if (var_InheritBool (wnd, "keyboard-events"))
    //    do_something();

    wl_display_flush(display);

    wnd->type = VOUT_WINDOW_TYPE_WAYLAND;
    wnd->handle.wl = surface;
    wnd->display.wl = display;
    wnd->control = Control;

    vout_window_SetFullScreen(wnd, cfg->is_fullscreen);

    if (vlc_clone(&sys->thread, Thread, wnd, VLC_THREAD_PRIORITY_LOW))
        goto error;

    return VLC_SUCCESS;

error:
    if (sys->deco != NULL)
        org_kde_kwin_server_decoration_destroy(sys->deco);
    if (sys->deco_manager != NULL)
        org_kde_kwin_server_decoration_manager_destroy(sys->deco_manager);
    if (sys->surface != NULL)
        xdg_surface_destroy(sys->surface);
    if (sys->shell != NULL)
        xdg_shell_destroy(sys->shell);
    if (sys->compositor != NULL)
        wl_compositor_destroy(sys->compositor);
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

    if (sys->deco != NULL)
        org_kde_kwin_server_decoration_destroy(sys->deco);
    if (sys->deco_manager != NULL)
        org_kde_kwin_server_decoration_manager_destroy(sys->deco_manager);
    xdg_surface_destroy(sys->surface);
    wl_surface_destroy(wnd->handle.wl);
    xdg_shell_destroy(sys->shell);
    wl_compositor_destroy(sys->compositor);
    wl_display_disconnect(wnd->display.wl);
    free(sys);
}


#define DISPLAY_TEXT N_("Wayland display")
#define DISPLAY_LONGTEXT N_( \
    "Video will be rendered with this Wayland display. " \
    "If empty, the default display will be used.")

vlc_module_begin()
    set_shortname(N_("XDG shell"))
    set_description(N_("XDG shell surface"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_capability("vout window", 20)
    set_callbacks(Open, Close)

    add_string("wl-display", NULL, DISPLAY_TEXT, DISPLAY_LONGTEXT, true)
vlc_module_end()
