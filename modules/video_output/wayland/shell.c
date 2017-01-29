/**
 * @file shell.c
 * @brief Wayland shell surface provider module for VLC media player
 */
/*****************************************************************************
 * Copyright © 2014 Rémi Denis-Courmont
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_window.h>

struct vout_window_sys_t
{
    struct wl_compositor *compositor;
    struct wl_output *output;
    struct wl_shell *shell;
    struct wl_shell_surface *shell_surface;

    uint32_t top_width;
    uint32_t top_height;
    uint32_t fs_width;
    uint32_t fs_height;
    bool fullscreen;

    vlc_mutex_t lock;
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
            unsigned width = va_arg (ap, unsigned);
            unsigned height = va_arg (ap, unsigned);

            vlc_mutex_lock(&sys->lock);
            sys->top_width = width;
            sys->top_height = height;

            if (!sys->fullscreen)
                vout_window_ReportSize(wnd, width, height);
            vlc_mutex_unlock(&sys->lock);
            break;
        }
        case VOUT_WINDOW_SET_FULLSCREEN:
        {
            bool fs = va_arg(ap, int);

            if (fs && sys->output != NULL)
            {
                wl_shell_surface_set_fullscreen(sys->shell_surface, 1, 0,
                                                sys->output);
                vlc_mutex_lock(&sys->lock);
                sys->fullscreen = true;
                vout_window_ReportSize(wnd, sys->fs_width, sys->fs_height);
                vlc_mutex_unlock(&sys->lock);
            }
            else
            {
                wl_shell_surface_set_toplevel(sys->shell_surface);

                vlc_mutex_lock(&sys->lock);
                sys->fullscreen = false;
                vout_window_ReportSize(wnd, sys->top_width, sys->top_height);
                vlc_mutex_unlock(&sys->lock);
            }
            break;
        }

        default:
            msg_Err(wnd, "request %d not implemented", cmd);
            return VLC_EGENERIC;
    }

    wl_display_flush(display);
    return VLC_SUCCESS;
}

static void output_geometry_cb(void *data, struct wl_output *output, int32_t x,
                               int32_t y, int32_t width, int32_t height,
                               int32_t subpixel, const char *vendor,
                               const char *model, int32_t transform)
{
    vout_window_t *wnd = data;

    msg_Dbg(wnd, "output geometry: %s %s %"PRId32"x%"PRId32"mm "
            "@ %"PRId32"x%"PRId32" subpixel: %"PRId32" transform: %"PRId32,
            vendor, model, width, height, x, y, subpixel, transform);
    (void) output;
}

static void output_mode_cb(void *data, struct wl_output *output,
                           uint32_t flags, int32_t width, int32_t height,
                           int32_t refresh)
{
    vout_window_t *wnd = data;
    vout_window_sys_t *sys = wnd->sys;

    msg_Dbg(wnd, "output mode: 0x%08"PRIX32" %"PRId32"x%"PRId32
            " %"PRId32"mHz%s", flags, width, height, refresh,
            (flags & WL_OUTPUT_MODE_CURRENT) ? " (current)" : "");

    if (!(flags & WL_OUTPUT_MODE_CURRENT))
        return;

    vlc_mutex_lock(&sys->lock);
    sys->fs_width = width;
    sys->fs_height = height;

    if (sys->fullscreen)
        vout_window_ReportSize(wnd, width, height);
    vlc_mutex_unlock(&sys->lock);

    (void) output;
}

const struct wl_output_listener output_cbs =
{
    output_geometry_cb,
    output_mode_cb,
    NULL,
    NULL,
};

static void shell_surface_ping_cb(void *data,
                                  struct wl_shell_surface *shell_surface,
                                  uint32_t serial)
{
    (void) data;
    wl_shell_surface_pong(shell_surface, serial);
}

static void shell_surface_configure_cb(void *data,
                                       struct wl_shell_surface *shell_surface,
                                       uint32_t edges,
                                       int32_t width, int32_t height)
{
    vout_window_t *wnd = data;
    vout_window_sys_t *sys = wnd->sys;

    msg_Dbg(wnd, "new configuration: %"PRId32"x%"PRId32, width, height);
    vlc_mutex_lock(&sys->lock);
    sys->top_width = width;
    sys->top_height = height;

    if (!sys->fullscreen)
        vout_window_ReportSize(wnd,  width, height);
    vlc_mutex_unlock(&sys->lock);

    (void) shell_surface;
    (void) edges;
}

static void shell_surface_popup_done_cb(void *data,
                                        struct wl_shell_surface *shell_surface)
{
    (void) data; (void) shell_surface;
}

static const struct wl_shell_surface_listener shell_surface_cbs =
{
    shell_surface_ping_cb,
    shell_surface_configure_cb,
    shell_surface_popup_done_cb,
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
    if (!strcmp(iface, "wl_output"))
        sys->output = wl_registry_bind(registry, name, &wl_output_interface,
                                       1);
    else
    if (!strcmp(iface, "wl_shell"))
        sys->shell = wl_registry_bind(registry, name, &wl_shell_interface, 1);
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
    sys->output = NULL;
    sys->shell = NULL;
    sys->shell_surface = NULL;
    sys->top_width = cfg->width;
    sys->top_height = cfg->height;
    sys->fs_width = cfg->width;
    sys->fs_height = cfg->height;
    sys->fullscreen = false;
    vlc_mutex_init(&sys->lock);
    wnd->sys = sys;

    /* Connect to the display server */
    char *dpy_name = var_InheritString(wnd, "wl-display");
    struct wl_display *display = wl_display_connect(dpy_name);

    free(dpy_name);

    if (display == NULL)
    {
        vlc_mutex_destroy(&sys->lock);
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

    if (sys->output != NULL)
        wl_output_add_listener(sys->output, &output_cbs, wnd);

    /* Create a surface */
    struct wl_surface *surface = wl_compositor_create_surface(sys->compositor);
    if (surface == NULL)
        goto error;

    struct wl_shell_surface *shell_surface =
        wl_shell_get_shell_surface(sys->shell, surface);
    if (shell_surface == NULL)
        goto error;

    sys->shell_surface = shell_surface;

    wl_shell_surface_add_listener(shell_surface, &shell_surface_cbs, wnd);
    wl_shell_surface_set_class(shell_surface, PACKAGE_NAME);
    wl_shell_surface_set_toplevel(shell_surface);

    char *title = var_InheritString(wnd, "video-title");
    wl_shell_surface_set_title(shell_surface, title ? title
                                                    : _("VLC media player"));
    free(title);

    //if (var_InheritBool (wnd, "keyboard-events"))
    //    do_something();

    wl_display_flush(display);

    wnd->type = VOUT_WINDOW_TYPE_WAYLAND;
    wnd->handle.wl = surface;
    wnd->display.wl = display;
    wnd->control = Control;

    if (vlc_clone (&sys->thread, Thread, wnd, VLC_THREAD_PRIORITY_LOW))
        goto error;

    vout_window_ReportSize(wnd, cfg->width, cfg->height);
    vout_window_SetFullScreen(wnd, cfg->is_fullscreen);
    return VLC_SUCCESS;

error:
    if (sys->shell_surface != NULL)
        wl_shell_surface_destroy(sys->shell_surface);
    if (sys->shell != NULL)
        wl_shell_destroy(sys->shell);
    if (sys->output != NULL)
        wl_output_destroy(sys->output);
    if (sys->compositor != NULL)
        wl_compositor_destroy(sys->compositor);
    wl_display_disconnect(display);
    vlc_mutex_destroy(&sys->lock);
    free(sys);
    return VLC_EGENERIC;
}

/**
 * Destroys a Wayland shell surface.
 */
static void Close(vout_window_t *wnd)
{
    vout_window_sys_t *sys = wnd->sys;

    vlc_cancel(sys->thread);
    vlc_join(sys->thread, NULL);

    wl_shell_surface_destroy(sys->shell_surface);
    wl_surface_destroy(wnd->handle.wl);
    wl_shell_destroy(sys->shell);
    if (sys->output != NULL)
        wl_output_destroy(sys->output);
    wl_compositor_destroy(sys->compositor);
    wl_display_disconnect(wnd->display.wl);
    vlc_mutex_destroy(&sys->lock);
    free(sys);
}


#define DISPLAY_TEXT N_("Wayland display")
#define DISPLAY_LONGTEXT N_( \
    "Video will be rendered with this Wayland display. " \
    "If empty, the default display will be used.")

vlc_module_begin ()
    set_shortname (N_("WL shell"))
    set_description (N_("Wayland shell surface"))
    set_category (CAT_VIDEO)
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_capability ("vout window", 10)
    set_callbacks (Open, Close)

    add_string ("wl-display", NULL, DISPLAY_TEXT, DISPLAY_LONGTEXT, true)
vlc_module_end ()
