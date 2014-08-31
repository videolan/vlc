/**
 * @file shm.c
 * @brief Wayland shared memory video output module for VLC media player
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
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <wayland-client.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_picture_pool.h>

#define MAX_PICTURES 4

struct vout_display_sys_t
{
    vout_window_t *embed; /* VLC window */
    struct wl_shm *shm;
    struct wl_shm_pool *shm_pool;

    picture_pool_t *pool; /* picture pool */
    unsigned char *base;
    size_t length;
    int fd;

    int x;
    int y;
};

static void PictureDestroy(picture_t *pic)
{
    struct wl_buffer *buf = (struct wl_buffer *)pic->p_sys;

    wl_buffer_destroy(buf);
}

static void buffer_release_cb(void *data, struct wl_buffer *buffer)
{
    picture_t *pic = data;

    picture_Release(pic);
    (void) buffer;
}

static const struct wl_buffer_listener buffer_cbs =
{
    buffer_release_cb,
};

static picture_pool_t *Pool(vout_display_t *vd, unsigned req)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->pool != NULL)
        return sys->pool;

    if (req > MAX_PICTURES)
        req = MAX_PICTURES;

    vout_display_place_t place;

    vout_display_PlacePicture(&place, &vd->source, vd->cfg, false);

    /* We need one extra line to cover for horizontal crop offset */
    unsigned stride = 4 * ((vd->fmt.i_width + 31) & ~31);
    unsigned lines = (vd->fmt.i_height + 31 + 1) & ~31;
    const long pagemask = sysconf(_SC_PAGE_SIZE) - 1;
    size_t picsize = ((stride * lines) + pagemask) & ~pagemask;

    sys->length = picsize * req;

    if (ftruncate(sys->fd, sys->length))
    {
        msg_Err(vd, "cannot allocate buffers: %s", vlc_strerror_c(errno));
        return NULL;
    }

    sys->base = mmap(NULL, sys->length, PROT_READ|PROT_WRITE, MAP_SHARED,
                     sys->fd, 0);
    if (sys->base == MAP_FAILED)
    {
        msg_Err(vd, "cannot map buffers: %s", vlc_strerror_c(errno));
        goto error;
    }

    sys->shm_pool = wl_shm_create_pool(sys->shm, sys->fd, sys->length);
    if (sys->shm_pool == NULL)
        goto error;

    picture_t *pics[MAX_PICTURES];
    picture_resource_t res = {
        .pf_destroy = PictureDestroy,
        .p = {
            [0] = {
                .i_lines = lines,
                .i_pitch = stride,
            },
        },
    };
    size_t offset = 4 * vd->fmt.i_x_offset + stride * vd->fmt.i_y_offset;
    unsigned width = vd->fmt.i_visible_width;
    unsigned height = vd->fmt.i_visible_height;
    unsigned count = 0;

    while (count < req)
    {
        struct wl_buffer *buf;

        buf = wl_shm_pool_create_buffer(sys->shm_pool, offset, width, height,
                                        stride, WL_SHM_FORMAT_XRGB8888);
        if (buf == NULL)
            break;

        res.p_sys = (picture_sys_t *)buf;
        res.p[0].p_pixels = sys->base + offset;
        offset += picsize;

        picture_t *pic = picture_NewFromResource(&vd->fmt, &res);
        if (unlikely(pic == NULL))
        {
            wl_buffer_destroy(buf);
            break;
        }

        wl_buffer_add_listener(buf, &buffer_cbs, pic);
        pics[count++] = pic;
    }

    if (count == 0)
    {
        wl_shm_pool_destroy(sys->shm_pool);
        munmap(sys->base, sys->length);
        goto error;
    }

    wl_display_flush(sys->embed->display.wl);

    sys->pool = picture_pool_New (count, pics);
    if (unlikely(sys->pool == NULL))
    {
        while (count > 0)
            picture_Release(pics[--count]);
        wl_shm_pool_destroy(sys->shm_pool);
        goto error;
    }
    return sys->pool;

error:
    if (sys->base != MAP_FAILED)
        munmap(sys->base, sys->length);
    ftruncate(sys->fd, 0); /* "free" memory */
    return NULL;
}

static void Prepare(vout_display_t *vd, picture_t *pic, subpicture_t *subpic)
{
    vout_display_sys_t *sys = vd->sys;
    struct wl_display *display = sys->embed->display.wl;
    struct wl_surface *surface = sys->embed->handle.wl;
    struct wl_buffer *buf = (struct wl_buffer *)pic->p_sys;

    wl_surface_attach(surface, buf, sys->x, sys->y);
    wl_surface_damage(surface, 0, 0,
                      vd->fmt.i_visible_width, vd->fmt.i_visible_height);
    wl_display_flush(display);

    sys->x = 0;
    sys->y = 0;

    (void) subpic;
}

static void Display(vout_display_t *vd, picture_t *pic, subpicture_t *subpic)
{
    vout_display_sys_t *sys = vd->sys;
    struct wl_display *display = sys->embed->display.wl;
    struct wl_surface *surface = sys->embed->handle.wl;

    wl_surface_commit(surface);
    // FIXME: deadlocks here
    wl_display_roundtrip(display);

    (void) pic; (void) subpic;
}

static void ResetPictures(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->pool == NULL)
        return;

    picture_pool_Delete(sys->pool);
    wl_shm_pool_destroy(sys->shm_pool);
    munmap(sys->base, sys->length);

    sys->pool = NULL;
}

static int Control(vout_display_t *vd, int query, va_list ap)
{
    vout_display_sys_t *sys = vd->sys;

    switch (query)
    {
        case VOUT_DISPLAY_HIDE_MOUSE:
            /* TODO */
            return VLC_EGENERIC;

        case VOUT_DISPLAY_RESET_PICTURES:
        {
            vout_display_place_t place;
            video_format_t src;

            sys->x -= vd->fmt.i_x_offset;
            sys->y -= vd->fmt.i_y_offset;

            vout_display_PlacePicture(&place, &vd->source, vd->cfg, false);
            video_format_ApplyRotation(&src, &vd->source);

            vd->fmt.i_width  = src.i_width  * place.width
                                            / src.i_visible_width;
            vd->fmt.i_height = src.i_height * place.height
                                            / src.i_visible_height;
            vd->fmt.i_visible_width  = place.width;
            vd->fmt.i_visible_height = place.height;
            vd->fmt.i_x_offset = src.i_x_offset * place.width
                                                / src.i_visible_width;
            vd->fmt.i_y_offset = src.i_y_offset * place.height
                                                / src.i_visible_height;

            sys->x += vd->fmt.i_x_offset;
            sys->y += vd->fmt.i_y_offset;

            ResetPictures(vd);
            break;
        }

        case VOUT_DISPLAY_CHANGE_FULLSCREEN:
        {
            const vout_display_cfg_t *cfg =
                va_arg(ap, const vout_display_cfg_t *);
            return vout_window_SetFullScreen(sys->embed, cfg->is_fullscreen);
        }

        case VOUT_DISPLAY_CHANGE_WINDOW_STATE:
        {
            unsigned state = va_arg(ap, unsigned);
            return vout_window_SetState(sys->embed, state);
        }

        case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
        {
            const vout_display_cfg_t *cfg =
                va_arg(ap, const vout_display_cfg_t *);
            const bool forced = va_arg(ap, int);

            if (forced)
            {
                vout_display_SendEventDisplaySize(vd, cfg->display.width,
                                                  cfg->display.height,
                                                  vd->cfg->is_fullscreen);
                return VLC_EGENERIC;
            }

            vout_display_place_t place;
            vout_display_PlacePicture(&place, &vd->source, cfg, false);

            if (place.width == vd->fmt.i_visible_width
             && place.height == vd->fmt.i_visible_height)
                break;
            /* fall through */
        }
        case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
        case VOUT_DISPLAY_CHANGE_ZOOM:
        case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
        case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
             vout_display_SendEventPicturesInvalid(vd);
             break;

        default:
             msg_Err(vd, "unknown request %d", query);
             return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static void registry_global_cb(void *data, struct wl_registry *registry,
                               uint32_t name, const char *iface, uint32_t vers)
{
    vout_display_t *vd = data;
    vout_display_sys_t *sys = vd->sys;

    msg_Dbg(vd, "global %3"PRIu32": %s version %"PRIu32, name, iface, vers);

    if (!strcmp(iface, "wl_shm"))
        sys->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
}

static void registry_global_remove_cb(void *data, struct wl_registry *registry,
                                      uint32_t name)
{
    vout_display_t *vd = data;

    msg_Dbg(vd, "global remove %3"PRIu32, name);
    (void) registry;
}

static const struct wl_registry_listener registry_cbs =
{
    registry_global_cb,
    registry_global_remove_cb,
};

static int Open(vlc_object_t *obj)
{
    vout_display_t *vd = (vout_display_t *)obj;
    vout_display_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    vd->sys = sys;
    sys->embed = NULL;
    sys->shm = NULL;
    sys->pool = NULL;
    sys->fd = -1;
    sys->x = 0;
    sys->y = 0;

    char bufpath[] = "/tmp/"PACKAGE_NAME"XXXXXX";
    sys->fd = mkostemp(bufpath, O_CLOEXEC);
    if (sys->fd == -1)
    {
        msg_Err(vd, "cannot create buffers: %s", vlc_strerror_c(errno));
        goto error;
    }
    unlink(bufpath);

    /* Get window */
    vout_window_cfg_t wcfg = {
        .type = VOUT_WINDOW_TYPE_WAYLAND,
        .width  = vd->cfg->display.width,
        .height = vd->cfg->display.height,
    };
    sys->embed = vout_display_NewWindow(vd, &wcfg);
    if (sys->embed == NULL)
        goto error;

    struct wl_display *display = sys->embed->display.wl;
    struct wl_registry *registry = wl_display_get_registry(display);
    if (registry == NULL)
        goto error;

    wl_registry_add_listener(registry, &registry_cbs, vd);
    wl_display_roundtrip(display);
    wl_registry_destroy(registry);

    if (sys->shm == NULL)
        goto error;

    /* Determine our pixel format */
    video_format_t fmt_pic;

    video_format_ApplyRotation(&fmt_pic, &vd->fmt);
    fmt_pic.i_chroma = VLC_CODEC_RGB32;

    vd->info.has_pictures_invalid = true;
    vd->info.has_event_thread = true;

    vd->fmt = fmt_pic;
    vd->pool = Pool;
    vd->prepare = Prepare;
    vd->display = Display;
    vd->control = Control;
    vd->manage = NULL;

    bool is_fullscreen = vd->cfg->is_fullscreen;
    if (is_fullscreen && vout_window_SetFullScreen(sys->embed, true))
        is_fullscreen = false;
    vout_display_SendEventFullscreen(vd, is_fullscreen);
    vout_display_SendEventDisplaySize(vd, vd->cfg->display.width,
                                      vd->cfg->display.height, is_fullscreen);
    return VLC_SUCCESS;

error:
    if (sys->embed != NULL)
        vout_display_DeleteWindow(vd, sys->embed);
    if (sys->fd != -1)
        close(sys->fd);
    free(sys);
    return VLC_EGENERIC;
}

static void Close(vlc_object_t *obj)
{
    vout_display_t *vd = (vout_display_t *)obj;
    vout_display_sys_t *sys = vd->sys;

    ResetPictures(vd);

    wl_shm_destroy(sys->shm);
    vout_display_DeleteWindow(vd, sys->embed);
    close(sys->fd);
    free(sys);
}

vlc_module_begin()
    set_shortname(N_("WL SHM"))
    set_description(N_("Wayland shared memory video output"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_capability("vout display", 120)
    set_callbacks(Open, Close)
    add_shortcut("wl")
vlc_module_end()
