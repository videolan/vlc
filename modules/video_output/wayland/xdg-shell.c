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
#include <wayland-cursor.h>
#ifdef XDG_SHELL
# include "xdg-shell-client-protocol.h"
# include "xdg-decoration-client-protocol.h"
#else
# define xdg_wm_base wl_shell
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_window.h>

#include "input.h"
#include "output.h"

typedef struct
{
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct xdg_wm_base *wm_base;
    struct xdg_surface *surface;
    struct xdg_toplevel *toplevel;
#ifdef XDG_SHELL
    struct zxdg_decoration_manager_v1 *deco_manager;
    struct zxdg_toplevel_decoration_v1 *deco;
#endif

    uint32_t default_output;

    struct
    {
        unsigned width;
        unsigned height;
    } set;
    struct
    {
        unsigned width;
        unsigned height;
        struct
        {
            unsigned width;
            unsigned height;
            bool fullscreen;
        } latch;
        bool configured;
    } wm;

    struct output_list *outputs;
    struct wl_list seats;
    struct wl_cursor_theme *cursor_theme;
    struct wl_cursor *cursor;
    struct wl_surface *cursor_surface;

    vlc_mutex_t lock;
    vlc_cond_t cond_configured;
    vlc_thread_t thread;
} vout_window_sys_t;

static void cleanup_wl_display_read(void *data)
{
    struct wl_display *display = data;

    wl_display_cancel_read(display);
}

/** Background thread for Wayland shell events handling */
static void *Thread(void *data)
{
    vout_window_t *wnd = data;
    vout_window_sys_t *sys = wnd->sys;
    struct wl_display *display = wnd->display.wl;
    struct pollfd ufd[1];

    int canc = vlc_savecancel();
    vlc_cleanup_push(cleanup_wl_display_read, display);

    ufd[0].fd = wl_display_get_fd(display);
    ufd[0].events = POLLIN;

    for (;;)
    {
        int timeout;

        while (wl_display_prepare_read(display) != 0)
            wl_display_dispatch_pending(display);

        wl_display_flush(display);
        timeout = seat_next_timeout(&sys->seats);
        vlc_restorecancel(canc);

        int val = poll(ufd, 1, timeout);

        canc = vlc_savecancel();
        if (val == 0)
            seat_timeout(&sys->seats);

        wl_display_read_events(display);
        wl_display_dispatch_pending(display);
    }
    vlc_assert_unreachable();
    vlc_cleanup_pop();
    //vlc_restorecancel(canc);
    //return NULL;
}

static void ReportSize(vout_window_t *wnd)
{
    vout_window_sys_t *sys = wnd->sys;
    /* Zero wm.width or zero wm.height means the client should choose.
     * DO NOT REPORT those values to video output... */
    unsigned width = sys->wm.width ? sys->wm.width : sys->set.width;
    unsigned height = sys->wm.height ? sys->wm.height : sys->set.height;

    vout_window_ReportSize(wnd, width, height);
    xdg_surface_set_window_geometry(sys->surface, 0, 0, width, height);
}

static void Resize(vout_window_t *wnd, unsigned width, unsigned height)
{
    vout_window_sys_t *sys = wnd->sys;

#ifdef XDG_SHELL
    /* The minimum size must be smaller or equal to the maximum size
     * at _all_ times. This gets a bit cumbersome. */
    xdg_toplevel_set_min_size(sys->toplevel, 0, 0);
    xdg_toplevel_set_max_size(sys->toplevel, width, height);
    xdg_toplevel_set_min_size(sys->toplevel, width, height);
#endif

    vlc_mutex_lock(&sys->lock);
    sys->set.width = width;
    sys->set.height = height;
    ReportSize(wnd);
    vlc_mutex_unlock(&sys->lock);
    wl_display_flush(wnd->display.wl);
}

static void Close(vout_window_t *);

static void UnsetFullscreen(vout_window_t *wnd)
{
    vout_window_sys_t *sys = wnd->sys;

    xdg_toplevel_unset_fullscreen(sys->toplevel);
    wl_display_flush(wnd->display.wl);
}

static void SetFullscreen(vout_window_t *wnd, const char *idstr)
{
    vout_window_sys_t *sys = wnd->sys;
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

    wl_display_flush(wnd->display.wl);
}

#ifdef XDG_SHELL
static void SetDecoration(vout_window_t *wnd, bool decorated)
{
    vout_window_sys_t *sys = wnd->sys;
    const uint_fast32_t deco_mode = decorated
        ? ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE
        : ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;

    if (sys->deco != NULL)
        zxdg_toplevel_decoration_v1_set_mode(sys->deco, deco_mode);
    else
    if (deco_mode != ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE)
        msg_Err(wnd, "server-side decoration not supported");
}
#endif

static int Enable(vout_window_t *wnd, const vout_window_cfg_t *restrict cfg)
{
    vout_window_sys_t *sys = wnd->sys;
    struct wl_display *display = wnd->display.wl;

    if (cfg->is_fullscreen)
        xdg_toplevel_set_fullscreen(sys->toplevel, NULL);
    else
        xdg_toplevel_unset_fullscreen(sys->toplevel);

#ifdef XDG_SHELL
    SetDecoration(wnd, cfg->is_decorated);
#else
    if (cfg->is_decorated)
        return VLC_EGENERIC;
#endif
    vout_window_SetSize(wnd, cfg->width, cfg->height);
    wl_surface_commit(wnd->handle.wl);
    wl_display_flush(display);

#ifdef XDG_SHELL
    vlc_mutex_lock(&sys->lock);
    while (!sys->wm.configured)
        vlc_cond_wait(&sys->cond_configured, &sys->lock);
    vlc_mutex_unlock(&sys->lock);
#endif

    return VLC_SUCCESS;
}

static const struct vout_window_operations ops = {
    .enable = Enable,
    .resize = Resize,
    .destroy = Close,
    .unset_fullscreen = UnsetFullscreen,
    .set_fullscreen = SetFullscreen,
};

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
    sys->wm.latch.width = width;
    sys->wm.latch.height = height;
    sys->wm.latch.fullscreen = false;

    wl_array_for_each(state, states)
    {
        msg_Dbg(wnd, " - state 0x%04"PRIX32, *state);

        switch (*state)
        {
            case XDG_TOPLEVEL_STATE_FULLSCREEN:
                sys->wm.latch.fullscreen = true;
                break;
        }
    }

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

    vlc_mutex_lock(&sys->lock);
    sys->wm.width = sys->wm.latch.width;
    sys->wm.height = sys->wm.latch.height;
    ReportSize(wnd);
    vlc_mutex_unlock(&sys->lock);

    if (sys->wm.latch.fullscreen)
        vout_window_ReportFullscreen(wnd, NULL);
    else
        vout_window_ReportWindowed(wnd);

    xdg_surface_ack_configure(surface, serial);

    vlc_mutex_lock(&sys->lock);
    sys->wm.configured = true;
    vlc_cond_signal(&sys->cond_configured);
    vlc_mutex_unlock(&sys->lock);
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

static void xdg_toplevel_decoration_configure_cb(void *data,
                                      struct zxdg_toplevel_decoration_v1 *deco,
                                                 uint32_t mode)
{
    vout_window_t *wnd = data;

    msg_Dbg(wnd, "new decoration mode: %"PRIu32, mode);
    (void) deco;
}

static const struct zxdg_toplevel_decoration_v1_listener
                                                  xdg_toplevel_decoration_cbs =
{
    xdg_toplevel_decoration_configure_cb,
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

static void register_wl_compositor(void *data, struct wl_registry *registry,
                                   uint32_t name, uint32_t vers)
{
    vout_window_t *wnd = data;
    vout_window_sys_t *sys = wnd->sys;

    if (likely(sys->compositor == NULL))
        sys->compositor = wl_registry_bind(registry, name,
                                           &wl_compositor_interface, vers);
}

static void register_wl_output(void *data, struct wl_registry *registry,
                               uint32_t name, uint32_t vers)
{
    vout_window_t *wnd = data;
    vout_window_sys_t *sys = wnd->sys;

    output_create(sys->outputs, registry, name, vers);
}

static void register_wl_seat(void *data, struct wl_registry *registry,
                             uint32_t name, uint32_t vers)
{
    vout_window_t *wnd = data;
    vout_window_sys_t *sys = wnd->sys;

    seat_create(wnd, registry, name, vers, &sys->seats);
}

#ifndef XDG_SHELL
static void register_wl_shell(void *data, struct wl_registry *registry,
                              uint32_t name, uint32_t vers)
{
    vout_window_t *wnd = data;
    vout_window_sys_t *sys = wnd->sys;

    if (likely(sys->wm_base == NULL))
        sys->wm_base = wl_registry_bind(registry, name, &wl_shell_interface,
                                        vers);
}
#endif

static void register_wl_shm(void *data, struct wl_registry *registry,
                            uint32_t name, uint32_t vers)
{
    vout_window_t *wnd = data;
    vout_window_sys_t *sys = wnd->sys;

    if (likely(sys->shm == NULL))
        sys->shm = wl_registry_bind(registry, name, &wl_shm_interface, vers);
}

#ifdef XDG_SHELL
static void register_xdg_wm_base(void *data, struct wl_registry *registry,
                                 uint32_t name, uint32_t vers)
{
    vout_window_t *wnd = data;
    vout_window_sys_t *sys = wnd->sys;

    if (likely(sys->wm_base == NULL))
        sys->wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface,
                                        vers);
}

static void register_xdg_decoration_manager(void *data,
                                            struct wl_registry *registry,
                                            uint32_t name, uint32_t vers)
{
    vout_window_t *wnd = data;
    vout_window_sys_t *sys = wnd->sys;

    if (likely(sys->deco_manager == NULL))
        sys->deco_manager = wl_registry_bind(registry, name,
                                  &zxdg_decoration_manager_v1_interface, vers);
}
#endif

struct registry_handler
{
    const char *iface;
    void (*global)(void *, struct wl_registry *, uint32_t, uint32_t);
    uint32_t max_version;
};

static const struct registry_handler global_handlers[] =
{
     { "wl_compositor", register_wl_compositor, 2 },
     { "wl_output", register_wl_output, 1},
     { "wl_seat", register_wl_seat, UINT32_C(-1) },
#ifndef XDG_SHELL
     { "wl_shell", register_wl_shell, 1 },
#endif
     { "wl_shm", register_wl_shm, 1 },
#ifdef XDG_SHELL
     { "xdg_wm_base", register_xdg_wm_base, 1 },
     { "zxdg_decoration_manager_v1", register_xdg_decoration_manager, 1 },
#endif
};

static int rghcmp(const void *a, const void *b)
{
    const char *iface = a;
    const struct registry_handler *handler = b;

    return strcmp(iface, handler->iface);
}

static void registry_global_cb(void *data, struct wl_registry *registry,
                               uint32_t name, const char *iface, uint32_t vers)
{
    vout_window_t *wnd = data;
    const struct registry_handler *h;

    msg_Dbg(wnd, "global %3"PRIu32": %s version %"PRIu32, name, iface, vers);

    h = bsearch(iface, global_handlers, ARRAY_SIZE(global_handlers),
                sizeof (*h), rghcmp);
    if (h != NULL)
    {
        uint32_t version = (vers < h->max_version) ? vers : h->max_version;

        h->global(data, registry, name, version);
    }
}

static void registry_global_remove_cb(void *data, struct wl_registry *registry,
                                      uint32_t name)
{
    vout_window_t *wnd = data;
    vout_window_sys_t *sys = wnd->sys;

    msg_Dbg(wnd, "global remove %3"PRIu32, name);

    if (seat_destroy_one(&sys->seats, name) == 0)
        return;
    if (output_destroy_by_name(sys->outputs, name) == 0)
        return;

    (void) registry;
}

static const struct wl_registry_listener registry_cbs =
{
    registry_global_cb,
    registry_global_remove_cb,
};

struct wl_surface *window_get_cursor(vout_window_t *wnd, int32_t *restrict hsx,
                                     int32_t *restrict hsy)
{
    vout_window_sys_t *sys = wnd->sys;

    if (unlikely(sys->cursor == NULL))
        return NULL;

    assert(sys->cursor->image_count > 0);

    /* TODO? animated cursor (more than one image) */
    struct wl_cursor_image *img = sys->cursor->images[0];
    struct wl_surface *surface = sys->cursor_surface;

    if (likely(surface != NULL))
    {
        wl_surface_attach(surface, wl_cursor_image_get_buffer(img), 0, 0);
        wl_surface_damage(surface, 0, 0, img->width, img->height);
        wl_surface_commit(surface);
    }

    *hsx = img->hotspot_x;
    *hsy = img->hotspot_y;
    return surface;
}

/**
 * Creates a Wayland shell surface.
 */
static int Open(vout_window_t *wnd)
{
    vout_window_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->compositor = NULL;
    sys->shm = NULL;
    sys->wm_base = NULL;
    sys->surface = NULL;
    sys->toplevel = NULL;
    sys->cursor_theme = NULL;
    sys->cursor = NULL;
#ifdef XDG_SHELL
    sys->deco_manager = NULL;
    sys->deco = NULL;
#endif
    sys->default_output = var_InheritInteger(wnd, "wl-output");
    sys->wm.width = 0;
    sys->wm.height = 0;
    sys->wm.latch.width = 0;
    sys->wm.latch.height = 0;
    sys->wm.latch.fullscreen = false;
    sys->wm.configured = false;
    sys->set.width = 0;
    sys->set.height = 0;
    sys->outputs = output_list_create(wnd);
    wl_list_init(&sys->seats);
    sys->cursor_theme = NULL;
    sys->cursor_surface = NULL;
    vlc_mutex_init(&sys->lock);
    vlc_cond_init(&sys->cond_configured);
    wnd->sys = sys;
    wnd->handle.wl = NULL;

    /* Connect to the display server */
    char *dpy_name = var_InheritString(wnd, "wl-display");
    struct wl_display *display = wl_display_connect(dpy_name);

    free(dpy_name);

    if (display == NULL)
    {
        output_list_destroy(sys->outputs);
        free(sys);
        return VLC_EGENERIC;
    }

    /* Find the interesting singleton(s) */
    sys->registry = wl_display_get_registry(display);
    if (sys->registry == NULL)
        goto error;

    wl_registry_add_listener(sys->registry, &registry_cbs, wnd);
    wl_display_roundtrip(display); /* complete registry enumeration */

    if (sys->compositor == NULL || sys->wm_base == NULL)
        goto error;

    /* Create a surface */
    struct wl_surface *surface = wl_compositor_create_surface(sys->compositor);
    if (surface == NULL)
        goto error;

    xdg_wm_base_add_listener(sys->wm_base, &xdg_wm_base_cbs, NULL);

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

    if (sys->shm != NULL)
    {
        sys->cursor_theme = wl_cursor_theme_load(NULL, 32, sys->shm);
        if (sys->cursor_theme != NULL)
            sys->cursor = wl_cursor_theme_get_cursor(sys->cursor_theme,
                                                     "left_ptr");

        sys->cursor_surface = wl_compositor_create_surface(sys->compositor);
    }
    if (sys->cursor == NULL)
        msg_Err(wnd, "failed to load cursor");

#ifdef XDG_SHELL
    if (sys->deco_manager != NULL)
        sys->deco = zxdg_decoration_manager_v1_get_toplevel_decoration(
                                                  sys->deco_manager, toplevel);
    if (sys->deco != NULL)
        zxdg_toplevel_decoration_v1_add_listener(sys->deco,
                                                 &xdg_toplevel_decoration_cbs,
                                                 wnd);
#endif

    wnd->type = VOUT_WINDOW_TYPE_WAYLAND;
    wnd->handle.wl = surface;
    wnd->display.wl = display;
    wnd->ops = &ops;

    if (vlc_clone(&sys->thread, Thread, wnd, VLC_THREAD_PRIORITY_LOW))
        goto error;

    return VLC_SUCCESS;

error:
    seat_destroy_all(&sys->seats);
    output_list_destroy(sys->outputs);
#ifdef XDG_SHELL
    if (sys->deco != NULL)
        zxdg_toplevel_decoration_v1_destroy(sys->deco);
    if (sys->deco_manager != NULL)
        zxdg_decoration_manager_v1_destroy(sys->deco_manager);
#endif
    if (sys->cursor_surface != NULL)
        wl_surface_destroy(sys->cursor_surface);
    if (sys->cursor_theme != NULL)
        wl_cursor_theme_destroy(sys->cursor_theme);
    if (sys->toplevel != NULL)
        xdg_toplevel_destroy(sys->toplevel);
    if (sys->surface != NULL)
        xdg_surface_destroy(sys->surface);
    if (sys->wm_base != NULL)
        xdg_wm_base_destroy(sys->wm_base);
    if (wnd->handle.wl != NULL)
        wl_surface_destroy(wnd->handle.wl);
    if (sys->shm != NULL)
        wl_shm_destroy(sys->shm);
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
    output_list_destroy(sys->outputs);
#ifdef XDG_SHELL
    if (sys->deco != NULL)
        zxdg_toplevel_decoration_v1_destroy(sys->deco);
    if (sys->deco_manager != NULL)
        zxdg_decoration_manager_v1_destroy(sys->deco_manager);
#endif
    if (sys->cursor_surface != NULL)
        wl_surface_destroy(sys->cursor_surface);
    if (sys->cursor_theme != NULL)
        wl_cursor_theme_destroy(sys->cursor_theme);
    xdg_toplevel_destroy(sys->toplevel);
    xdg_surface_destroy(sys->surface);
    xdg_wm_base_destroy(sys->wm_base);
    wl_surface_destroy(wnd->handle.wl);
    if (sys->shm != NULL)
        wl_shm_destroy(sys->shm);
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
    set_shortname(N_("XDG shell"))
    set_description(N_("XDG shell surface"))
#else
    set_shortname(N_("WL shell"))
    set_description(N_("Wayland shell surface"))
#endif
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
#ifdef XDG_SHELL
    set_capability("vout window", 20)
#else
    set_capability("vout window", 10)
#endif
    set_callback(Open)

    add_string("wl-display", NULL, DISPLAY_TEXT, DISPLAY_LONGTEXT, true)
    add_integer("wl-output", 0, OUTPUT_TEXT, OUTPUT_LONGTEXT, true)
        change_integer_range(0, UINT32_MAX)
        change_volatile()
vlc_module_end()
