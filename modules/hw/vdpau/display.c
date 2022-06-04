/**
 * @file display.c
 * @brief VDPAU video display module for VLC media player
 */
/*****************************************************************************
 * Copyright © 2009-2013 Rémi Denis-Courmont
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

#include <stdlib.h>
#include <assert.h>

#include <xcb/xcb.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_xlib.h>

#include "vlc_vdpau.h"
#include "events.h"

static int Open(vout_display_t *vd, video_format_t *fmtp, vlc_video_context *context);
static void Close(vout_display_t *vd);

vlc_module_begin()
    set_shortname(N_("VDPAU"))
    set_description(N_("VDPAU output"))
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_callback_display(Open, 0)

    add_shortcut("vdpau")
vlc_module_end()

typedef struct vout_display_sys_t
{
    xcb_connection_t *conn; /**< XCB connection */
    vdp_t *vdp; /**< VDPAU back-end */
    picture_t *current; /**< Currently visible picture */

    xcb_window_t window; /**< target window (owned by VDPAU back-end) */
    VdpDevice device; /**< VDPAU device handle */
    VdpPresentationQueueTarget target; /**< VDPAU presentation queue target */
    VdpPresentationQueue queue; /**< VDPAU presentation queue */

    unsigned width;
    unsigned height;

    vlc_fourcc_t spu_formats[3];
} vout_display_sys_t;

static void RenderRegion(vout_display_t *vd, VdpOutputSurface target,
                         const subpicture_t *subpic,
                         const subpicture_region_t *reg)
{
    vout_display_sys_t *sys = vd->sys;
    VdpBitmapSurface surface;
    VdpRGBAFormat fmt;
    VdpStatus err;

    switch (reg->fmt.i_chroma) {
#ifdef WORDS_BIGENDIAN
        case VLC_CODEC_ARGB:
            fmt = VDP_RGBA_FORMAT_B8G8R8A8;
            break;
#else
        case VLC_CODEC_RGBA:
            fmt = VDP_RGBA_FORMAT_R8G8B8A8;
            break;
        case VLC_CODEC_BGRA:
            fmt = VDP_RGBA_FORMAT_B8G8R8A8;
            break;
#endif
        default:
            vlc_assert_unreachable();
    }

    /* Create GPU surface for sub-picture */
    err = vdp_bitmap_surface_create(sys->vdp, sys->device, fmt,
                                    reg->fmt.i_width, reg->fmt.i_height,
                                    VDP_FALSE, &surface);
    if (err != VDP_STATUS_OK)
    {
        msg_Err(vd, "%s creation failure: %s", "bitmap surface",
                vdp_get_error_string(sys->vdp, err));
        return;
    }

    /* Upload sub-picture to GPU surface */
    picture_t *pic = reg->p_picture;
    const void *data = pic->p[0].p_pixels;
    uint32_t pitch = pic->p[0].i_pitch;

    err = vdp_bitmap_surface_put_bits_native(sys->vdp, surface, &data, &pitch,
                                             NULL);
    if (err != VDP_STATUS_OK)
    {
        msg_Err(vd, "subpicture upload failure: %s",
                vdp_get_error_string(sys->vdp, err));
        goto out;
    }

    /* Render onto main surface */
    VdpRect dst_area = {
        reg->i_x * sys->width
            / subpic->i_original_picture_width,
        reg->i_y * sys->height
            / subpic->i_original_picture_height,
        (reg->i_x + reg->fmt.i_visible_width) * sys->width
            / subpic->i_original_picture_width,
        (reg->i_y + reg->fmt.i_visible_height) * sys->height
            / subpic->i_original_picture_height,
    };
    VdpRect src_area = {
        reg->fmt.i_x_offset,
        reg->fmt.i_y_offset,
        reg->fmt.i_x_offset + reg->fmt.i_visible_width,
        reg->fmt.i_y_offset + reg->fmt.i_visible_height,
    };
    VdpColor color = { 1.f, 1.f, 1.f,
        reg->i_alpha * subpic->i_alpha / 65025.f };
    VdpOutputSurfaceRenderBlendState state = {
        .struct_version = VDP_OUTPUT_SURFACE_RENDER_BLEND_STATE_VERSION,
        .blend_factor_source_color =
            VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_SRC_ALPHA,
        .blend_factor_destination_color =
            VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .blend_factor_source_alpha =
            VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ZERO,
        .blend_factor_destination_alpha =
            VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE,
        .blend_equation_color = VDP_OUTPUT_SURFACE_RENDER_BLEND_EQUATION_ADD,
        .blend_equation_alpha = VDP_OUTPUT_SURFACE_RENDER_BLEND_EQUATION_ADD,
        .blend_constant = { 0.f, 0.f, 0.f, 0.f },
    };

    err = vdp_output_surface_render_bitmap_surface(sys->vdp, target, &dst_area,
                                                   surface, &src_area, &color,
                                                   &state, 0);
    if (err != VDP_STATUS_OK)
        msg_Err(vd, "blending failure: %s",
                vdp_get_error_string(sys->vdp, err));

out:/* Destroy GPU surface */
    vdp_bitmap_surface_destroy(sys->vdp, surface);
}

static void Queue(vout_display_t *vd, picture_t *pic, subpicture_t *subpic,
                  vlc_tick_t date)
{
    vout_display_sys_t *sys = vd->sys;
    vlc_vdp_output_surface_t *p_sys = pic->p_sys;
    VdpOutputSurface surface = p_sys->surface;
    VdpStatus err;

    vlc_xcb_Manage(vd->obj.logger, sys->conn);

    VdpPresentationQueueStatus status;
    VdpTime ts;
    err = vdp_presentation_queue_query_surface_status(sys->vdp, sys->queue,
                                                      surface, &status, &ts);
    if (err == VDP_STATUS_OK && status != VDP_PRESENTATION_QUEUE_STATUS_IDLE)
        msg_Dbg(vd, "surface status: %u", status);

    if (subpic != NULL)
        for (subpicture_region_t *r = subpic->p_region; r != NULL;
             r = r->p_next)
            RenderRegion(vd, surface, subpic, r);

    /* Compute picture presentation time */
    vlc_tick_t now = vlc_tick_now();
    VdpTime pts;

    err = vdp_presentation_queue_get_time(sys->vdp, sys->queue, &pts);
    if (err != VDP_STATUS_OK)
    {
        msg_Err(vd, "presentation queue time failure: %s",
                vdp_get_error_string(sys->vdp, err));
        return;
    }

    vlc_tick_t delay = date - now;
    if (delay < 0)
        delay = 0; /* core bug: date is not updated during pause */
    if (unlikely(delay > VLC_TICK_FROM_SEC(1)))
    {   /* We would get stuck if the delay was too long. */
        msg_Dbg(vd, "picture date corrupt: delay of %"PRId64" us", delay);
        delay = VLC_TICK_FROM_MS(20);
    }
    pts += MS_FROM_VLC_TICK(delay);

    /* Queue picture */
    err = vdp_presentation_queue_display(sys->vdp, sys->queue, surface, 0, 0,
                                         pts);
    if (err != VDP_STATUS_OK)
        msg_Err(vd, "presentation queue display failure: %s",
                vdp_get_error_string(sys->vdp, err));
}

static void Wait(vout_display_t *vd, picture_t *pic)
{
    vout_display_sys_t *sys = vd->sys;
    xcb_generic_event_t *ev;

    picture_t *current = sys->current;
    if (current != NULL)
    {
        vlc_vdp_output_surface_t *psys = current->p_sys;
        VdpTime pts;
        VdpStatus err;

        err = vdp_presentation_queue_block_until_surface_idle(sys->vdp,
                                              sys->queue, psys->surface, &pts);
        if (err != VDP_STATUS_OK)
        {
            msg_Err(vd, "presentation queue blocking error: %s",
                    vdp_get_error_string(sys->vdp, err));
            goto out;
        }
        picture_Release(current);
    }

    sys->current = picture_Hold(pic);
out:
    /* Drain the event queue. TODO: remove sys->conn completely */

    while ((ev = xcb_poll_for_event(sys->conn)) != NULL)
        free(ev);
}

static int ResetPictures(vout_display_t *vd, video_format_t *fmt)
{
    vout_display_sys_t *sys = vd->sys;
    const video_format_t *src= vd->source;
    vout_display_place_t place;

    msg_Dbg(vd, "resetting pictures");
    vout_display_PlacePicture(&place, src, &vd->cfg->display);

    fmt->i_width = src->i_width * place.width / src->i_visible_width;
    fmt->i_height = src->i_height * place.height / src->i_visible_height;
    sys->width = fmt->i_visible_width = place.width;
    sys->height = fmt->i_visible_height = place.height;
    fmt->i_x_offset = src->i_x_offset * place.width / src->i_visible_width;
    fmt->i_y_offset = src->i_y_offset * place.height / src->i_visible_height;

    const uint32_t values[] = { place.x, place.y,
                                place.width, place.height, };
    xcb_configure_window(sys->conn, sys->window,
                            XCB_CONFIG_WINDOW_X|XCB_CONFIG_WINDOW_Y|
                            XCB_CONFIG_WINDOW_WIDTH|XCB_CONFIG_WINDOW_HEIGHT,
                            values);
    xcb_flush (sys->conn);
    return VLC_SUCCESS;
}

static int Control(vout_display_t *vd, int query)
{
    vout_display_sys_t *sys = vd->sys;

    switch (query)
    {
    case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
    {
        vout_display_place_t place;

        vout_display_PlacePicture(&place, vd->source, &vd->cfg->display);
        if (place.width  != vd->fmt->i_visible_width
         || place.height != vd->fmt->i_visible_height)
            return VLC_EGENERIC;

        const uint32_t values[] = { place.x, place.y,
                                    place.width, place.height, };
        xcb_configure_window(sys->conn, sys->window,
                             XCB_CONFIG_WINDOW_X|XCB_CONFIG_WINDOW_Y|
                             XCB_CONFIG_WINDOW_WIDTH|XCB_CONFIG_WINDOW_HEIGHT,
                             values);
        break;
    }
    case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
    case VOUT_DISPLAY_CHANGE_ZOOM:
    case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
    case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
        return VLC_EGENERIC;
    default:
        msg_Err(vd, "unknown control request %d", query);
        return VLC_EGENERIC;
    }
    xcb_flush (sys->conn);
    return VLC_SUCCESS;
}

static const struct vlc_display_operations ops = {
    .close = Close,
    .prepare = Queue,
    .display = Wait,
    .control = Control,
    .reset_pictures = ResetPictures,
};

static int Open(vout_display_t *vd,
                video_format_t *fmtp, vlc_video_context *context)
{
    if (fmtp->i_chroma != VLC_CODEC_VDPAU_VIDEO)
        return VLC_ENOTSUP;

    vout_display_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    struct vlc_logger *log = vd->obj.logger;
    const xcb_screen_t *screen;
    int ret = vlc_xcb_parent_Create(log, vd->cfg->window, &sys->conn, &screen);
    if (ret != VLC_SUCCESS)
    {
        free(sys);
        return ret;
    }

    vlc_decoder_device *dec_device = context ? vlc_video_context_HoldDevice(context) : NULL;
    if (dec_device == NULL)
        goto error;

    vdpau_decoder_device_t *vdpau_decoder = GetVDPAUOpaqueDevice(dec_device);
    if (vdpau_decoder == NULL)
    {
        vlc_decoder_device_Release(dec_device);
        goto error;
    }

    // get the vdp/device from the decoder device, it is always matching the same
    // window configuration use to create the XCB connection that was used to
    // create the decoder device
    sys->vdp    = vdpau_decoder->vdp;
    sys->device = vdpau_decoder->device;

    vlc_decoder_device_Release(dec_device);

    /* Check source format */
    video_format_t fmt;
    VdpStatus err;

    video_format_ApplyRotation(&fmt, fmtp);

    /* Check video mixer capabilities */
    {
        uint32_t min, max;

        err = vdp_video_mixer_query_parameter_value_range(sys->vdp,
                    sys->device, VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_WIDTH,
                                                          &min, &max);
        if (err != VDP_STATUS_OK)
        {
            msg_Err(vd, "%s capabilities query failure: %s",
                    "video mixer surface width",
                    vdp_get_error_string(sys->vdp, err));
            goto error;
        }
        if (min > fmt.i_width || fmt.i_width > max)
        {
            msg_Err(vd, "source video %s not supported", "width");
            goto error;
        }

        err = vdp_video_mixer_query_parameter_value_range(sys->vdp,
                   sys->device, VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_HEIGHT,
                                                          &min, &max);
        if (err != VDP_STATUS_OK)
        {
            msg_Err(vd, "%s capabilities query failure: %s",
                    "video mixer surface height",
                    vdp_get_error_string(sys->vdp, err));
            goto error;
        }
        if (min > fmt.i_height || fmt.i_height > max)
        {
            msg_Err(vd, "source video %s not supported", "height");
            goto error;
        }
    }
    fmt.i_chroma = VLC_CODEC_VDPAU_OUTPUT;

    sys->width = fmtp->i_visible_width;
    sys->height = fmtp->i_visible_height;
    /* VDPAU-X11 requires a window dedicated to the back-end */
    {
        xcb_pixmap_t pix = xcb_generate_id(sys->conn);
        xcb_create_pixmap(sys->conn, screen->root_depth, pix,
                          screen->root, 1, 1);

        uint32_t mask =
            XCB_CW_BACK_PIXMAP | XCB_CW_BACK_PIXEL |
            XCB_CW_BORDER_PIXMAP | XCB_CW_BORDER_PIXEL |
            XCB_CW_EVENT_MASK | XCB_CW_COLORMAP;
        const uint32_t values[] = {
            pix, screen->black_pixel, pix, screen->black_pixel,
            XCB_EVENT_MASK_VISIBILITY_CHANGE, screen->default_colormap
        };
        vout_display_place_t place;

        vout_display_PlacePicture(&place, vd->source, &vd->cfg->display);
        sys->window = xcb_generate_id(sys->conn);

        xcb_void_cookie_t c =
            xcb_create_window_checked(sys->conn, screen->root_depth,
                sys->window, vd->cfg->window->handle.xid, place.x, place.y,
                place.width, place.height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                screen->root_visual, mask, values);
        if (vlc_xcb_error_Check(log, sys->conn, "window creation failure", c))
            goto error;
        msg_Dbg(vd, "using X11 window 0x%08"PRIx32, sys->window);
        xcb_map_window(sys->conn, sys->window);
    }

    /* Initialize VDPAU queue */
    err = vdp_presentation_queue_target_create_x11(sys->vdp, sys->device,
                                                   sys->window, &sys->target);
    if (err != VDP_STATUS_OK)
    {
        msg_Err(vd, "%s creation failure: %s", "presentation queue target",
                vdp_get_error_string(sys->vdp, err));
        goto error;
    }

    err = vdp_presentation_queue_create(sys->vdp, sys->device, sys->target,
                                        &sys->queue);
    if (err != VDP_STATUS_OK)
    {
        msg_Err(vd, "%s creation failure: %s", "presentation queue",
                vdp_get_error_string(sys->vdp, err));
        vdp_presentation_queue_target_destroy(sys->vdp, sys->target);
        goto error;
    }

    /* Check bitmap capabilities (for SPU) */
    {
        uint32_t w, h;
        VdpBool ok;
        unsigned int n = 0;

        err = vdp_bitmap_surface_query_capabilities(sys->vdp, sys->device,
                                                    VDP_RGBA_FORMAT_R8G8B8A8,
                                                    &ok, &w, &h);
        if (err == VDP_STATUS_OK && ok == VDP_TRUE)
#ifdef WORDS_BIGENDIAN
            sys->spu_formats[n++] = VLC_CODEC_ABGR;
#else
            sys->spu_formats[n++] = VLC_CODEC_RGBA;
#endif
        if (n > 0) {
            sys->spu_formats[n] = 0;
            vd->info.subpicture_chromas = sys->spu_formats;
        }

        err = vdp_bitmap_surface_query_capabilities(sys->vdp, sys->device,
                                                    VDP_RGBA_FORMAT_B8G8R8A8,
                                                    &ok, &w, &h);
        if (err == VDP_STATUS_OK && ok == VDP_TRUE)
#ifdef WORDS_BIGENDIAN
            sys->spu_formats[n++] = VLC_CODEC_ARGB;
#else
            sys->spu_formats[n++] = VLC_CODEC_BGRA;
#endif
        if (n > 0) {
            sys->spu_formats[n] = 0;
            vd->info.subpicture_chromas = sys->spu_formats;
        }
    }

    /* */
    sys->current = NULL;
    vd->sys = sys;
    *fmtp = fmt;

    vd->ops = &ops;

    return VLC_SUCCESS;

error:
    xcb_disconnect(sys->conn);
    free(sys);
    return VLC_EGENERIC;
}

static void Close(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    vdp_presentation_queue_destroy(sys->vdp, sys->queue);
    vdp_presentation_queue_target_destroy(sys->vdp, sys->target);

    if (sys->current != NULL)
        picture_Release(sys->current);

    xcb_disconnect(sys->conn);
    free(sys);
}
