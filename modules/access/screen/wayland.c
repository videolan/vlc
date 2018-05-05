/**
 * @file wayland.c
 * @brief Wayland screenshooter extension module for VLC media player
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

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <unistd.h>

#include <wayland-client.h>
#include "screenshooter-client-protocol.h"

#include <vlc_common.h>
#include <vlc_demux.h>
#include <vlc_fs.h>
#include <vlc_plugin.h>

typedef struct
{
    struct wl_display *display;
    struct wl_output *output;
    struct wl_shm *shm;
    struct screenshooter *screenshooter;
    es_out_id_t *es;

    long pagemask;
    float rate;
    int32_t x; /*< Requested horizontal offset */
    int32_t y; /*< Requested vertical offset */
    int32_t w; /*< Requested width */
    int32_t h; /*< Requested height */
    int32_t width; /*< Actual width */
    int32_t height; /*< Actual height */

    bool done;
    vlc_tick_t start;

    vlc_thread_t thread;
} demux_sys_t;

static bool DisplayError(vlc_object_t *obj, struct wl_display *display)
{
    int val = wl_display_get_error(display);
    if (val == 0)
        return false;

    if (val == EPROTO)
    {
        const struct wl_interface *iface;
        uint32_t id;

        val = wl_display_get_protocol_error(display, &iface, &id);
        msg_Err(obj, "display protocol error %d on %s object %"PRIu32,
                val, iface->name, id);
    }
    else
        msg_Err(obj, "display fatal error: %s", vlc_strerror_c(val));

    return true;
}
#define DisplayError(o,d) DisplayError(VLC_OBJECT(o), (d))

static void output_geometry_cb(void *data, struct wl_output *output, int32_t x,
                               int32_t y, int32_t width, int32_t height,
                               int32_t subpixel, const char *vendor,
                               const char *model, int32_t transform)
{
    demux_t *demux = data;

    msg_Dbg(demux, "output geometry: %s %s %"PRId32"x%"PRId32"mm "
            "@ %"PRId32"x%"PRId32" subpixel: %"PRId32" transform: %"PRId32,
            vendor, model, width, height, x, y, subpixel, transform);
    (void) output;
}

static void output_mode_cb(void *data, struct wl_output *output,
                           uint32_t flags, int32_t width, int32_t height,
                           int32_t refresh)
{
    demux_t *demux = data;
    demux_sys_t *sys = demux->p_sys;

    msg_Dbg(demux, "output mode: 0x%08"PRIX32" %"PRId32"x%"PRId32
            " %"PRId32"mHz%s", flags, width, height, refresh,
            (flags & WL_OUTPUT_MODE_CURRENT) ? " (current)" : "");

    if (!(flags & WL_OUTPUT_MODE_CURRENT))
        return;
    if (width <= sys->x || height <= sys->y)
        return;

    if (sys->es != NULL)
        es_out_Del(demux->out, sys->es);

    es_format_t fmt;

    es_format_Init(&fmt, VIDEO_ES, VLC_CODEC_RGB32);
    fmt.video.i_chroma = VLC_CODEC_RGB32;
    fmt.video.i_bits_per_pixel = 32;
    fmt.video.i_sar_num = fmt.video.i_sar_den = 1;
    fmt.video.i_frame_rate = lroundf(1000.f * sys->rate);
    fmt.video.i_frame_rate_base = 1000;
    fmt.video.i_width = width;

    if (sys->w != 0 && width > sys->w + sys->x)
        fmt.video.i_visible_width = sys->w;
    else
        fmt.video.i_visible_width = width - sys->x;

    if (sys->h != 0 && height > sys->h + sys->y)
        fmt.video.i_visible_height = sys->h;
    else
        fmt.video.i_visible_height = height - sys->y;

    fmt.video.i_height = fmt.video.i_visible_height;

    sys->es = es_out_Add(demux->out, &fmt);
    sys->width = width;
    sys->height = height;
    (void) output;
}

const struct wl_output_listener output_cbs =
{
    output_geometry_cb,
    output_mode_cb,
    NULL,
    NULL,
};

static void screenshooter_done_cb(void *data,
                                  struct screenshooter *screenshooter)
{
    bool *done = data;

    *done = true;
    (void) screenshooter;
}

const struct screenshooter_listener screenshooter_cbs =
{
    screenshooter_done_cb,
};

static block_t *Shoot(demux_t *demux)
{
    demux_sys_t *sys = demux->p_sys;

    int fd = vlc_memfd();
    if (fd == -1)
    {
        msg_Err(demux, "buffer creation error: %s", vlc_strerror_c(errno));
        return NULL;
    }

    /* NOTE: one extra line for overflow if screen-left > 0 */
    uint32_t pitch = 4u * sys->width;
    size_t size = (pitch * (sys->height + 1) + sys->pagemask) & ~sys->pagemask;
    block_t *block = NULL;

    if (ftruncate(fd, size) < 0)
    {
        msg_Err(demux, "buffer allocation error: %s", vlc_strerror_c(errno));
        goto out;
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(sys->shm, fd, size);
    if (pool == NULL)
        goto out;

    struct wl_buffer *buffer;
    buffer = wl_shm_pool_create_buffer(pool, 0, sys->width, sys->height,
                                       pitch, WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
    if (buffer == NULL)
        goto out;

    sys->done = false;
    screenshooter_shoot(sys->screenshooter, sys->output, buffer);

    while (!sys->done)
        wl_display_roundtrip(sys->display);

    wl_buffer_destroy(buffer);
    block = block_File(fd, true);

    if (block != NULL)
    {
        size_t skip = (sys->y * sys->width + sys->x) * 4;

        block->p_buffer += skip;
        block->i_buffer -= skip;
    }

out:
    vlc_close(fd);
    return block;
}

static void cleanup_wl_display_read(void *data)
{
    struct wl_display *display = data;

    wl_display_cancel_read(display);
}

static void *Thread(void *data)
{
    demux_t *demux = data;
    demux_sys_t *sys = demux->p_sys;
    struct wl_display *display = sys->display;
    struct pollfd ufd[1];
    unsigned interval = lroundf(CLOCK_FREQ / (sys->rate * 1000.f));

    int canc = vlc_savecancel();
    vlc_cleanup_push(cleanup_wl_display_read, display);

    ufd[0].fd = wl_display_get_fd(display);
    ufd[0].events = POLLIN;

    for (;;)
    {
        if (DisplayError(demux, display))
            break;

        if (sys->es != NULL)
        {
            block_t *block = Shoot(demux);

            block->i_pts = block->i_dts = vlc_tick_now();
            es_out_SetPCR(demux->out, block->i_pts);
            es_out_Send(demux->out, sys->es, block);
        }

        while (wl_display_prepare_read(display) != 0)
            wl_display_dispatch_pending(display);
        wl_display_flush(display);
        vlc_restorecancel(canc);

        while (poll(ufd, 1, interval) < 0);

        canc = vlc_savecancel();
        wl_display_read_events(display);
        wl_display_dispatch_pending(display);
    }
    vlc_cleanup_pop();
    vlc_restorecancel(canc);
    return NULL;
}

static int Control(demux_t *demux, int query, va_list args)
{
    demux_sys_t *sys = demux->p_sys;

    switch (query)
    {
        case DEMUX_GET_POSITION:
            *va_arg(args, float *) = 0.f;
            break;

        case DEMUX_GET_LENGTH:
            *va_arg(args, vlc_tick_t *) = 0;
            break;

        case DEMUX_GET_TIME:
            *va_arg(args, vlc_tick_t *) = vlc_tick_now() - sys->start;
            break;

        case DEMUX_GET_FPS:
            *va_arg(args, float *) = sys->rate;
            break;

        case DEMUX_CAN_PAUSE:
            *va_arg(args, bool *) = false; /* TODO */
            break;
        //case DEMUX_SET_PAUSE_STATE:
        //    break;

        case DEMUX_GET_PTS_DELAY:
            *va_arg(args, vlc_tick_t *) = VLC_TICK_FROM_MS(
                var_InheritInteger(demux, "live-caching") );
            break;

        case DEMUX_CAN_CONTROL_PACE:
        case DEMUX_CAN_CONTROL_RATE:
        case DEMUX_CAN_SEEK:
            *va_arg(args, bool *) = false;
            break;

        default:
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static void registry_global_cb(void *data, struct wl_registry *registry,
                               uint32_t name, const char *iface, uint32_t vers)
{
    demux_t *demux = data;
    demux_sys_t *sys = demux->p_sys;

    msg_Dbg(demux, "global %3"PRIu32": %s version %"PRIu32, name, iface, vers);

    if (!strcmp(iface, "wl_output"))
        sys->output = wl_registry_bind(registry, name, &wl_output_interface,
                                       1);
    else
    if (!strcmp(iface, "wl_shm"))
        sys->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    else
    if (!strcmp(iface, "screenshooter"))
        sys->screenshooter = wl_registry_bind(registry, name,
                                              &screenshooter_interface, 1);
}

static void registry_global_remove_cb(void *data, struct wl_registry *registry,
                                      uint32_t name)
{
    demux_t *demux = data;

    msg_Dbg(demux, "global remove %3"PRIu32, name);
    (void) registry;
}

static const struct wl_registry_listener registry_cbs =
{
    registry_global_cb,
    registry_global_remove_cb,
};

static int Open(vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    if (demux->out == NULL)
        return VLC_EGENERIC;

    demux_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    /* Connect to the display server */
    char *dpy_name = var_InheritString(demux, "wl-display");
    sys->display = wl_display_connect(dpy_name);
    free(dpy_name);

    if (sys->display == NULL)
    {
        free(sys);
        return VLC_EGENERIC;
    }

    sys->output = NULL;
    sys->shm = NULL;
    sys->screenshooter = NULL;
    sys->es = NULL;
    sys->pagemask = sysconf(_SC_PAGE_SIZE) - 1;
    sys->rate = var_InheritFloat(demux, "screen-fps");
    sys->x = var_InheritInteger(demux, "screen-left");
    sys->y = var_InheritInteger(demux, "screen-top");
    sys->w = var_InheritInteger(demux, "screen-width");
    sys->h = var_InheritInteger(demux, "screen-height");

    if (1000.f * sys->rate <= 0x1.p-30)
        goto error;

    demux->p_sys = sys;

    /* Find the interesting singleton(s) */
    struct wl_registry *registry = wl_display_get_registry(sys->display);
    if (registry == NULL)
        goto error;

    wl_registry_add_listener(registry, &registry_cbs, demux);
    wl_display_roundtrip(sys->display);
    wl_registry_destroy(registry);

    if (sys->output == NULL || sys->shm == NULL || sys->screenshooter == NULL)
    {
        msg_Err(demux, "screenshooter extension not supported");
        goto error;
    }

    wl_output_add_listener(sys->output, &output_cbs, demux);
    screenshooter_add_listener(sys->screenshooter, &screenshooter_cbs,
                               &sys->done);
    wl_display_roundtrip(sys->display);

    if (DisplayError(demux, sys->display))
        goto error;

    /* Initializes demux */
    sys->start = vlc_tick_now();

    if (vlc_clone(&sys->thread, Thread, demux, VLC_THREAD_PRIORITY_INPUT))
        goto error;

    demux->pf_demux = NULL;
    demux->pf_control = Control;
    return VLC_SUCCESS;

error:
    if (sys->screenshooter != NULL)
        screenshooter_destroy(sys->screenshooter);
    if (sys->shm != NULL)
        wl_shm_destroy(sys->shm);
    if (sys->output != NULL)
        wl_output_destroy(sys->output);
    wl_display_disconnect(sys->display);
    free(sys);
    return VLC_EGENERIC;
}

static void Close(vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    demux_sys_t *sys = demux->p_sys;

    vlc_cancel(sys->thread);
    vlc_join(sys->thread, NULL);

    screenshooter_destroy(sys->screenshooter);
    wl_shm_destroy(sys->shm);
    wl_output_destroy(sys->output);
    wl_display_disconnect(sys->display);
    free(sys);
}

#define FPS_TEXT N_("Frame rate")
#define FPS_LONGTEXT N_( \
    "How many times the screen content should be refreshed per second.")

#define LEFT_TEXT N_("Region left column")
#define LEFT_LONGTEXT N_( \
    "Abscissa of the capture region in pixels.")

#define TOP_TEXT N_("Region top row")
#define TOP_LONGTEXT N_( \
    "Ordinate of the capture region in pixels.")

#define WIDTH_TEXT N_("Capture region width")
#define WIDTH_LONGTEXT N_( \
    "Pixel width of the capture region, or 0 for full width")

#define HEIGHT_TEXT N_("Capture region height")
#define HEIGHT_LONGTEXT N_( \
    "Pixel height of the capture region, or 0 for full height")

vlc_module_begin ()
    set_shortname (N_("Screen"))
    set_description (N_("Screen capture (with Wayland)"))
    set_category (CAT_INPUT)
    set_subcategory (SUBCAT_INPUT_ACCESS)
    set_capability ("access", 0)
    set_callbacks (Open, Close)

    /* XXX: VLC core does not support multiple configuration items with the
     * same name. So all default values and ranges must be the same as for XCB
     * for the time being. */
    add_float ("screen-fps", 2.0, FPS_TEXT, FPS_LONGTEXT, true)
    add_integer ("screen-left", 0, LEFT_TEXT, LEFT_LONGTEXT, true)
        change_integer_range (-32768, 32767)
        change_safe ()
    add_integer ("screen-top", 0, TOP_TEXT, TOP_LONGTEXT, true)
        change_integer_range (-32768, 32767)
        change_safe ()
    add_integer ("screen-width", 0, WIDTH_TEXT, WIDTH_LONGTEXT, true)
        change_integer_range (0, 65535)
        change_safe ()
    add_integer ("screen-height", 0, HEIGHT_TEXT, HEIGHT_LONGTEXT, true)
        change_integer_range (0, 65535)
        change_safe ()
    add_shortcut ("screen")
vlc_module_end ()
