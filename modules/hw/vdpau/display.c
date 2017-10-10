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
#include <vlc_picture_pool.h>
#include <vlc_xlib.h>

#include "vlc_vdpau.h"
#include "events.h"

static int Open(vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_shortname(N_("VDPAU"))
    set_description(N_("VDPAU output"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_capability("vout display", 0)
    set_callbacks(Open, Close)

    add_shortcut("vdpau")
vlc_module_end()

struct vout_display_sys_t
{
    xcb_connection_t *conn; /**< XCB connection */
    vout_window_t *embed; /**< parent window */
    vdp_t *vdp; /**< VDPAU back-end */
    picture_t *current; /**< Currently visible picture */

    xcb_window_t window; /**< target window (owned by VDPAU back-end) */
    VdpDevice device; /**< VDPAU device handle */
    VdpPresentationQueueTarget target; /**< VDPAU presentation queue target */
    VdpPresentationQueue queue; /**< VDPAU presentation queue */
    VdpRGBAFormat rgb_fmt; /**< Output surface format */

    picture_pool_t *pool; /**< pictures pool */
};

static void pictureSys_DestroyVDPAU(picture_sys_t *psys)
{
    vdp_output_surface_destroy(psys->vdp, psys->surface);
    vdp_release_x11(psys->vdp);
    free(psys);
}

static void PictureDestroyVDPAU(picture_t *pic)
{
    pictureSys_DestroyVDPAU(pic->p_sys);
    free(pic);
}

static VdpStatus picture_NewVDPAU(vdp_t *vdp, VdpRGBAFormat rgb_fmt,
                                  const video_format_t *restrict fmt,
                                  picture_t **restrict picp)
{
    picture_sys_t *psys = malloc(sizeof (*psys));
    if (unlikely(psys == NULL))
        return VDP_STATUS_RESOURCES;

    psys->vdp = vdp_hold_x11(vdp, &psys->device);

    VdpStatus err = vdp_output_surface_create(psys->vdp, psys->device,
                          rgb_fmt, fmt->i_visible_width, fmt->i_visible_height,
                                              &psys->surface);
    if (err != VDP_STATUS_OK)
    {
        vdp_release_x11(psys->vdp);
        free(psys);
        return err;
    }

    picture_resource_t res = {
        .p_sys = psys,
        .pf_destroy = PictureDestroyVDPAU,
    };

    picture_t *pic = picture_NewFromResource(fmt, &res);
    if (unlikely(pic == NULL))
    {
        pictureSys_DestroyVDPAU(psys);
        return VDP_STATUS_RESOURCES;
    }
    *picp = pic;
    return VDP_STATUS_OK;
}

static picture_pool_t *PoolAlloc(vout_display_t *vd, unsigned requested_count)
{
    vout_display_sys_t *sys = vd->sys;
    picture_t *pics[requested_count];

    unsigned count = 0;
    while (count < requested_count)
    {
        VdpStatus err = picture_NewVDPAU(sys->vdp, sys->rgb_fmt, &vd->fmt,
                                         pics + count);
        if (err != VDP_STATUS_OK)
        {
            msg_Err(vd, "%s creation failure: %s", "output surface",
                    vdp_get_error_string(sys->vdp, err));
            break;
        }
        count++;
    }
    sys->current = NULL;

    if (count == 0)
        return NULL;

    picture_pool_t *pool = picture_pool_New(count, pics);
    if (unlikely(pool == NULL))
        while (count > 0)
            picture_Release(pics[--count]);
    return pool;
}

static void PoolFree(vout_display_t *vd, picture_pool_t *pool)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->current != NULL)
        picture_Release(sys->current);
    picture_pool_Release(pool);
}

static picture_pool_t *Pool(vout_display_t *vd, unsigned requested_count)
{
    vout_display_sys_t *sys = vd->sys;

    if (sys->pool == NULL)
        sys->pool = PoolAlloc(vd, requested_count);
    return sys->pool;
}

static void RenderRegion(vout_display_t *vd, VdpOutputSurface target,
                         const subpicture_t *subpic,
                         const subpicture_region_t *reg)
{
    vout_display_sys_t *sys = vd->sys;
    VdpBitmapSurface surface;
#ifdef WORDS_BIGENDIAN
    VdpRGBAFormat fmt = VDP_RGBA_FORMAT_B8G8R8A8;
#else
    VdpRGBAFormat fmt = VDP_RGBA_FORMAT_R8G8B8A8;
#endif
    VdpStatus err;

    /* Create GPU surface for sub-picture */
    err = vdp_bitmap_surface_create(sys->vdp, sys->device, fmt,
        reg->fmt.i_visible_width, reg->fmt.i_visible_height, VDP_FALSE,
                                    &surface);
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
    VdpRect area = {
        reg->i_x * vd->fmt.i_visible_width
            / subpic->i_original_picture_width,
        reg->i_y * vd->fmt.i_visible_height
            / subpic->i_original_picture_height,
        (reg->i_x + reg->fmt.i_visible_width) * vd->fmt.i_visible_width
            / subpic->i_original_picture_width,
        (reg->i_y + reg->fmt.i_visible_height) * vd->fmt.i_visible_height
            / subpic->i_original_picture_height,
    };
    VdpColor color = { 1.f, 1.f, 1.f,
        reg->i_alpha * subpic->i_alpha / 65535.f };
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

    err = vdp_output_surface_render_bitmap_surface(sys->vdp, target, &area,
                                             surface, NULL, &color, &state, 0);
    if (err != VDP_STATUS_OK)
        msg_Err(vd, "blending failure: %s",
                vdp_get_error_string(sys->vdp, err));

out:/* Destroy GPU surface */
    vdp_bitmap_surface_destroy(sys->vdp, surface);
}

static void Queue(vout_display_t *vd, picture_t *pic, subpicture_t *subpic)
{
    vout_display_sys_t *sys = vd->sys;
    VdpOutputSurface surface = pic->p_sys->surface;
    VdpStatus err;

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
    mtime_t now = mdate();
    VdpTime pts;

    err = vdp_presentation_queue_get_time(sys->vdp, sys->queue, &pts);
    if (err != VDP_STATUS_OK)
    {
        msg_Err(vd, "presentation queue time failure: %s",
                vdp_get_error_string(sys->vdp, err));
        if (err == VDP_STATUS_DISPLAY_PREEMPTED)
            vout_display_SendEventPicturesInvalid(vd);
        return;
    }

    mtime_t delay = pic->date - now;
    if (delay < 0)
        delay = 0; /* core bug: date is not updated during pause */
    if (unlikely(delay > CLOCK_FREQ))
    {   /* We would get stuck if the delay was too long. */
        msg_Dbg(vd, "picture date corrupt: delay of %"PRId64" us", delay);
        delay = CLOCK_FREQ / 50;
    }
    pts += delay * 1000;

    /* Queue picture */
    err = vdp_presentation_queue_display(sys->vdp, sys->queue, surface, 0, 0,
                                         pts);
    if (err != VDP_STATUS_OK)
        msg_Err(vd, "presentation queue display failure: %s",
                vdp_get_error_string(sys->vdp, err));
}

static void Wait(vout_display_t *vd, picture_t *pic, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;

    picture_t *current = sys->current;
    if (current != NULL)
    {
        picture_sys_t *psys = current->p_sys;
        VdpTime pts;
        VdpStatus err;

        err = vdp_presentation_queue_block_until_surface_idle(sys->vdp,
                                              sys->queue, psys->surface, &pts);
        if (err != VDP_STATUS_OK)
        {
            msg_Err(vd, "presentation queue blocking error: %s",
                    vdp_get_error_string(sys->vdp, err));
            picture_Release(pic);
            goto out;
        }
        picture_Release(current);
    }

    sys->current = pic;
out:
    /* We already dealt with the subpicture in the Queue phase, so it's safe to
       delete at this point */
    if (subpicture)
        subpicture_Delete(subpicture);

    /* Drain the event queue. TODO: remove sys->conn completely */
    xcb_generic_event_t *ev;

    while ((ev = xcb_poll_for_event(sys->conn)) != NULL)
        free(ev);
}

static int Control(vout_display_t *vd, int query, va_list ap)
{
    vout_display_sys_t *sys = vd->sys;

    switch (query)
    {
    case VOUT_DISPLAY_RESET_PICTURES:
    {
        msg_Dbg(vd, "resetting pictures");
        if (sys->pool != NULL)
        {
            PoolFree(vd, sys->pool);
            sys->pool = NULL;
        }

        const video_format_t *src= &vd->source;
        video_format_t *fmt = &vd->fmt;
        vout_display_place_t place;

        vout_display_PlacePicture(&place, src, vd->cfg, false);

        fmt->i_width = src->i_width * place.width / src->i_visible_width;
        fmt->i_height = src->i_height * place.height / src->i_visible_height;
        fmt->i_visible_width  = place.width;
        fmt->i_visible_height = place.height;
        fmt->i_x_offset = src->i_x_offset * place.width / src->i_visible_width;
        fmt->i_y_offset = src->i_y_offset * place.height / src->i_visible_height;

        const uint32_t values[] = { place.x, place.y,
                                    place.width, place.height, };
        xcb_configure_window(sys->conn, sys->window,
                             XCB_CONFIG_WINDOW_X|XCB_CONFIG_WINDOW_Y|
                             XCB_CONFIG_WINDOW_WIDTH|XCB_CONFIG_WINDOW_HEIGHT,
                             values);
        break;
    }
    case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
    {
        const vout_display_cfg_t *cfg = va_arg(ap, const vout_display_cfg_t *);
        vout_display_place_t place;

        vout_display_PlacePicture(&place, &vd->source, cfg, false);
        if (place.width  != vd->fmt.i_visible_width
         || place.height != vd->fmt.i_visible_height)
        {
            vout_display_SendEventPicturesInvalid(vd);
            return VLC_SUCCESS;
        }

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
        vout_display_SendEventPicturesInvalid (vd);
        return VLC_SUCCESS;
    default:
        msg_Err(vd, "unknown control request %d", query);
        return VLC_EGENERIC;
    }
    xcb_flush (sys->conn);
    return VLC_SUCCESS;
}

static int xcb_screen_num(xcb_connection_t *conn, const xcb_screen_t *screen)
{
    const xcb_setup_t *setup = xcb_get_setup(conn);
    unsigned snum = 0;

    for (xcb_screen_iterator_t i = xcb_setup_roots_iterator(setup);
         i.rem > 0; xcb_screen_next(&i))
    {
        if (i.data->root == screen->root)
            return snum;
        snum++;
    }
    return -1;
}

static int Open(vlc_object_t *obj)
{
    if (!vlc_xlib_init(obj))
        return VLC_EGENERIC;

    vout_display_t *vd = (vout_display_t *)obj;
    vout_display_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    const xcb_screen_t *screen;
    sys->embed = vlc_xcb_parent_Create(vd, &sys->conn, &screen);
    if (sys->embed == NULL)
    {
        free(sys);
        return VLC_EGENERIC;
    }

    /* Load the VDPAU back-end and create a device instance */
    VdpStatus err = vdp_get_x11(sys->embed->display.x11,
                                xcb_screen_num(sys->conn, screen),
                                &sys->vdp, &sys->device);
    if (err != VDP_STATUS_OK)
    {
        msg_Dbg(obj, "device creation failure: error %d", (int)err);
        xcb_disconnect(sys->conn);
        vout_display_DeleteWindow(vd, sys->embed);
        free(sys);
        return VLC_EGENERIC;
    }

    const char *info;
    if (vdp_get_information_string(sys->vdp, &info) == VDP_STATUS_OK)
        msg_Dbg(vd, "using back-end %s", info);

    /* Check source format */
    video_format_t fmt;
    VdpChromaType chroma;
    VdpYCbCrFormat format;

    video_format_ApplyRotation(&fmt, &vd->fmt);

    if (fmt.i_chroma == VLC_CODEC_VDPAU_VIDEO_420
     || fmt.i_chroma == VLC_CODEC_VDPAU_VIDEO_422
     || fmt.i_chroma == VLC_CODEC_VDPAU_VIDEO_444)
        ;
    else
    if (vlc_fourcc_to_vdp_ycc(fmt.i_chroma, &chroma, &format))
    {
        uint32_t w, h;
        VdpBool ok;

        err = vdp_video_surface_query_capabilities(sys->vdp, sys->device,
                                                   chroma, &ok, &w, &h);
        if (err != VDP_STATUS_OK)
        {
            msg_Err(vd, "%s capabilities query failure: %s", "video surface",
                    vdp_get_error_string(sys->vdp, err));
            goto error;
        }
        if (!ok || w < fmt.i_width || h < fmt.i_height)
        {
            msg_Err(vd, "source video %s not supported", "chroma type");
            goto error;
        }

        err = vdp_video_surface_query_get_put_bits_y_cb_cr_capabilities(
                                   sys->vdp, sys->device, chroma, format, &ok);
        if (err != VDP_STATUS_OK)
        {
            msg_Err(vd, "%s capabilities query failure: %s", "video surface",
                    vdp_get_error_string(sys->vdp, err));
            goto error;
        }
        if (!ok)
        {
            msg_Err(vd, "source video %s not supported", "YCbCr format");
            goto error;
        }
    }
    else
        goto error;

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

    /* Select surface format */
    static const VdpRGBAFormat rgb_fmts[] = {
        VDP_RGBA_FORMAT_R10G10B10A2, VDP_RGBA_FORMAT_B10G10R10A2,
        VDP_RGBA_FORMAT_B8G8R8A8, VDP_RGBA_FORMAT_R8G8B8A8,
    };
    unsigned i;

    for (i = 0; i < sizeof (rgb_fmts) / sizeof (rgb_fmts[0]); i++)
    {
        uint32_t w, h;
        VdpBool ok;

        err = vdp_output_surface_query_capabilities(sys->vdp, sys->device,
                                                    rgb_fmts[i], &ok, &w, &h);
        if (err != VDP_STATUS_OK)
        {
            msg_Err(vd, "%s capabilities query failure: %s", "output surface",
                    vdp_get_error_string(sys->vdp, err));
            continue;
        }
        /* NOTE: Wrong! No warranties that zoom <= 100%! */
        if (!ok || w < fmt.i_width || h < fmt.i_height)
            continue;

        sys->rgb_fmt = rgb_fmts[i];
        msg_Dbg(vd, "using RGBA format %u", sys->rgb_fmt);
        break;
    }
    if (i == sizeof (rgb_fmts) / sizeof (rgb_fmts[0]))
    {
        msg_Err(vd, "no supported output surface format");
        goto error;
    }

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

        vout_display_PlacePicture (&place, &vd->source, vd->cfg, false);
        sys->window = xcb_generate_id(sys->conn);

        xcb_void_cookie_t c =
            xcb_create_window_checked(sys->conn, screen->root_depth,
                sys->window, sys->embed->handle.xid, place.x, place.y,
                place.width, place.height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                screen->root_visual, mask, values);
        if (vlc_xcb_error_Check(vd, sys->conn, "window creation failure", c))
            goto error;
        msg_Dbg(vd, "using X11 window 0x%08"PRIx32, sys->window);
        xcb_map_window(sys->conn, sys->window);
    }

    /* Check bitmap capabilities (for SPU) */
    const vlc_fourcc_t *spu_chromas = NULL;
    {
#ifdef WORDS_BIGENDIAN
        static const vlc_fourcc_t subpicture_chromas[] = { VLC_CODEC_ARGB, 0 };
#else
        static const vlc_fourcc_t subpicture_chromas[] = { VLC_CODEC_RGBA, 0 };
#endif
        uint32_t w, h;
        VdpBool ok;

        err = vdp_bitmap_surface_query_capabilities(sys->vdp, sys->device,
                                        VDP_RGBA_FORMAT_R8G8B8A8, &ok, &w, &h);
        if (err != VDP_STATUS_OK)
        {
            msg_Err(vd, "%s capabilities query failure: %s", "output surface",
                    vdp_get_error_string(sys->vdp, err));
            ok = VDP_FALSE;
        }
        if (ok)
            spu_chromas = subpicture_chromas;
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

    sys->pool = NULL;

    /* */
    vd->sys = sys;
    vd->info.has_pictures_invalid = true;
    vd->info.subpicture_chromas = spu_chromas;
    vd->fmt = fmt;

    vd->pool = Pool;
    vd->prepare = Queue;
    vd->display = Wait;
    vd->control = Control;

    return VLC_SUCCESS;

error:
    vdp_release_x11(sys->vdp);
    xcb_disconnect(sys->conn);
    vout_display_DeleteWindow(vd, sys->embed);
    free(sys);
    return VLC_EGENERIC;
}

static void Close(vlc_object_t *obj)
{
    vout_display_t *vd = (vout_display_t *)obj;
    vout_display_sys_t *sys = vd->sys;

    vdp_presentation_queue_destroy(sys->vdp, sys->queue);
    vdp_presentation_queue_target_destroy(sys->vdp, sys->target);

    if (sys->pool != NULL)
        PoolFree(vd, sys->pool);

    vdp_release_x11(sys->vdp);
    xcb_disconnect(sys->conn);
    vout_display_DeleteWindow(vd, sys->embed);
    free(sys);
}
