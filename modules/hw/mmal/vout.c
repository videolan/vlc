/*****************************************************************************
 * mmal.c: MMAL-based vout plugin for Raspberry Pi
 *****************************************************************************
 * Copyright © 2014 jusst technologies GmbH
 * $Id$
 *
 * Authors: Dennis Hamester <dennis.hamester@gmail.com>
 *          Julian Scheel <julian@jusst.de>
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
#include "config.h"
#endif

#include <math.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_threads.h>
#include <vlc_vout_display.h>

#include <bcm_host.h>
#include <interface/mmal/mmal.h>
#include <interface/mmal/util/mmal_util.h>
#include <interface/mmal/util/mmal_default_components.h>
#include <interface/vmcs_host/vc_tvservice.h>
#include <interface/vmcs_host/vc_dispmanx.h>

/* This value must match the define in codec/mmal.c
 * Think twice before changing this. Incorrect values cause havoc.
 */
#define NUM_ACTUAL_OPAQUE_BUFFERS 40

#define MAX_BUFFERS_IN_TRANSIT 2
#define VC_TV_MAX_MODE_IDS 127

#define MMAL_LAYER_NAME "mmal-layer"
#define MMAL_LAYER_TEXT N_("VideoCore layer where the video is displayed.")
#define MMAL_LAYER_LONGTEXT N_("VideoCore layer where the video is displayed. Subpictures are displayed directly above and a black background directly below.")

#define MMAL_ADJUST_REFRESHRATE_NAME "mmal-adjust-refreshrate"
#define MMAL_ADJUST_REFRESHRATE_TEXT N_("Adjust HDMI refresh rate to the video.")
#define MMAL_ADJUST_REFRESHRATE_LONGTEXT N_("Adjust HDMI refresh rate to the video.")

static int Open(vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_shortname(N_("MMAL vout"))
    set_description(N_("MMAL-based vout plugin for Raspberry Pi"))
    set_capability("vout display", 90)
    add_shortcut("mmal_vout")
    add_integer(MMAL_LAYER_NAME, 1, MMAL_LAYER_TEXT, MMAL_LAYER_LONGTEXT, false)
    add_bool(MMAL_ADJUST_REFRESHRATE_NAME, false, MMAL_ADJUST_REFRESHRATE_TEXT,
                    MMAL_ADJUST_REFRESHRATE_LONGTEXT, false)
    set_callbacks(Open, Close)
vlc_module_end()

struct dmx_region_t {
    struct dmx_region_t *next;
    picture_t *picture;
    VC_RECT_T bmp_rect;
    VC_RECT_T src_rect;
    VC_RECT_T dst_rect;
    VC_DISPMANX_ALPHA_T alpha;
    DISPMANX_ELEMENT_HANDLE_T element;
    DISPMANX_RESOURCE_HANDLE_T resource;
    int32_t pos_x;
    int32_t pos_y;
};

struct vout_display_sys_t {
    picture_t **pictures;
    picture_pool_t *picture_pool;

    MMAL_COMPONENT_T *component;
    MMAL_PORT_T *input;
    MMAL_POOL_T *pool;
    struct dmx_region_t *dmx_region;
    plane_t planes[3];

    vlc_mutex_t buffer_mutex;
    vlc_mutex_t manage_mutex;
    vlc_cond_t buffer_cond;
    uint32_t buffer_size;
    int buffers_in_transit;
    unsigned num_buffers;

    DISPMANX_DISPLAY_HANDLE_T dmx_handle;
    DISPMANX_ELEMENT_HANDLE_T bkg_element;
    DISPMANX_RESOURCE_HANDLE_T bkg_resource;
    unsigned display_width;
    unsigned display_height;
    bool need_configure_display;

    int layer;
    bool opaque;
};

struct picture_sys_t {
    vout_display_t *vd;
    MMAL_BUFFER_HEADER_T *buffer;
    bool displayed;
};

static const vlc_fourcc_t subpicture_chromas[] = {
    VLC_CODEC_RGBA,
    0
};

/* Utility functions */
static inline uint32_t align(uint32_t x, uint32_t y);
static int configure_display(vout_display_t *vd, const vout_display_cfg_t *cfg,
                const video_format_t *fmt);

/* VLC vout display callbacks */
static picture_pool_t *vd_pool(vout_display_t *vd, unsigned count);
static void vd_display(vout_display_t *vd, picture_t *picture,
                subpicture_t *subpicture);
static int vd_control(vout_display_t *vd, int query, va_list args);
static void vd_manage(vout_display_t *vd);

/* VLC picture pool */
static int picture_lock(picture_t *picture);
static void picture_unlock(picture_t *picture);

/* MMAL callbacks */
static void control_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
static void input_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);

/* TV service */
static int query_resolution(vout_display_t *vd, unsigned *width, unsigned *height);
static void tvservice_cb(void *callback_data, uint32_t reason, uint32_t param1,
                uint32_t param2);
static void adjust_refresh_rate(vout_display_t *vd);

/* DispManX */
static void display_subpicture(vout_display_t *vd, subpicture_t *subpicture);
static void close_dmx(vout_display_t *vd);
static struct dmx_region_t *dmx_region_new(vout_display_t *vd,
                DISPMANX_UPDATE_HANDLE_T update, subpicture_region_t *region);
static void dmx_region_update(struct dmx_region_t *dmx_region,
                DISPMANX_UPDATE_HANDLE_T update, picture_t *picture);
static void dmx_region_delete(struct dmx_region_t *dmx_region,
                DISPMANX_UPDATE_HANDLE_T update);
static void show_background(vout_display_t *vd, bool enable);

static int Open(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys;
    uint32_t buffer_pitch, buffer_height;
    vout_display_place_t place;
    MMAL_DISPLAYREGION_T display_region;
    uint32_t offsets[3];
    MMAL_STATUS_T status;
    int ret = VLC_SUCCESS;
    unsigned i;

    sys = calloc(1, sizeof(struct vout_display_sys_t));
    if (!sys)
        return VLC_ENOMEM;
    vd->sys = sys;

    sys->layer = var_InheritInteger(vd, MMAL_LAYER_NAME);
    bcm_host_init();

    vd->info.has_hide_mouse = true;
    sys->opaque = vd->fmt.i_chroma == VLC_CODEC_MMAL_OPAQUE;

    if (!sys->opaque)
        vd->fmt.i_chroma = VLC_CODEC_I420;
    vd->fmt.i_sar_num = vd->source.i_sar_num;
    vd->fmt.i_sar_den = vd->source.i_sar_den;

    buffer_pitch = align(vd->fmt.i_width, 32);
    buffer_height = align(vd->fmt.i_height, 16);
    sys->buffer_size = 3 * buffer_pitch * buffer_height / 2;

    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER, &sys->component);
    if (status != MMAL_SUCCESS) {
        msg_Err(vd, "Failed to create MMAL component %s (status=%"PRIx32" %s)",
                        MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER, status, mmal_status_to_string(status));
        ret = VLC_EGENERIC;
        goto out;
    }

    sys->component->control->userdata = (struct MMAL_PORT_USERDATA_T *)vd;
    status = mmal_port_enable(sys->component->control, control_port_cb);
    if (status != MMAL_SUCCESS) {
        msg_Err(vd, "Failed to enable control port %s (status=%"PRIx32" %s)",
                        sys->component->control->name, status, mmal_status_to_string(status));
        ret = VLC_EGENERIC;
        goto out;
    }

    sys->input = sys->component->input[0];
    sys->input->userdata = (struct MMAL_PORT_USERDATA_T *)vd;
    if (sys->opaque)
        sys->input->format->encoding = MMAL_ENCODING_OPAQUE;
    else
        sys->input->format->encoding = MMAL_ENCODING_I420;
    sys->input->format->es->video.width = vd->fmt.i_width;
    sys->input->format->es->video.height = vd->fmt.i_height;
    sys->input->format->es->video.crop.x = 0;
    sys->input->format->es->video.crop.y = 0;
    sys->input->format->es->video.crop.width = vd->fmt.i_width;
    sys->input->format->es->video.crop.height = vd->fmt.i_height;
    sys->input->format->es->video.par.num = vd->source.i_sar_num;
    sys->input->format->es->video.par.den = vd->source.i_sar_den;

    status = mmal_port_format_commit(sys->input);
    if (status != MMAL_SUCCESS) {
        msg_Err(vd, "Failed to commit format for input port %s (status=%"PRIx32" %s)",
                        sys->input->name, status, mmal_status_to_string(status));
        ret = VLC_EGENERIC;
        goto out;
    }
    sys->input->buffer_size = sys->input->buffer_size_recommended;

    vout_display_PlacePicture(&place, &vd->source, vd->cfg, false);
    display_region.hdr.id = MMAL_PARAMETER_DISPLAYREGION;
    display_region.hdr.size = sizeof(MMAL_DISPLAYREGION_T);
    display_region.fullscreen = MMAL_FALSE;
    display_region.src_rect.x = vd->fmt.i_x_offset;
    display_region.src_rect.y = vd->fmt.i_y_offset;
    display_region.src_rect.width = vd->fmt.i_visible_width;
    display_region.src_rect.height = vd->fmt.i_visible_height;
    display_region.dest_rect.x = place.x;
    display_region.dest_rect.y = place.y;
    display_region.dest_rect.width = place.width;
    display_region.dest_rect.height = place.height;
    display_region.layer = sys->layer;
    display_region.set = MMAL_DISPLAY_SET_FULLSCREEN | MMAL_DISPLAY_SET_SRC_RECT |
            MMAL_DISPLAY_SET_DEST_RECT | MMAL_DISPLAY_SET_LAYER;
    status = mmal_port_parameter_set(sys->input, &display_region.hdr);
    if (status != MMAL_SUCCESS) {
        msg_Err(vd, "Failed to set display region (status=%"PRIx32" %s)",
                        status, mmal_status_to_string(status));
        ret = VLC_EGENERIC;
        goto out;
    }

    offsets[0] = 0;
    for (i = 0; i < 3; ++i) {
        sys->planes[i].i_lines = buffer_height;
        sys->planes[i].i_pitch = buffer_pitch;
        sys->planes[i].i_visible_lines = vd->fmt.i_visible_height;
        sys->planes[i].i_visible_pitch = vd->fmt.i_visible_width;

        if (i > 0) {
            offsets[i] = offsets[i - 1] + sys->planes[i - 1].i_pitch * sys->planes[i - 1].i_lines;
            sys->planes[i].i_lines /= 2;
            sys->planes[i].i_pitch /= 2;
            sys->planes[i].i_visible_lines /= 2;
            sys->planes[i].i_visible_pitch /= 2;
        }

        sys->planes[i].p_pixels = (uint8_t *)offsets[i];
    }

    vlc_mutex_init(&sys->buffer_mutex);
    vlc_cond_init(&sys->buffer_cond);
    vlc_mutex_init(&sys->manage_mutex);

    vd->pool = vd_pool;
    vd->display = vd_display;
    vd->control = vd_control;
    vd->manage = vd_manage;

    vc_tv_register_callback(tvservice_cb, vd);

    if (query_resolution(vd, &sys->display_width, &sys->display_height) >= 0) {
        vout_display_SendEventDisplaySize(vd, sys->display_width, sys->display_height,
                        vd->cfg->is_fullscreen);
    } else {
        sys->display_width = vd->cfg->display.width;
        sys->display_height = vd->cfg->display.height;
    }

    sys->dmx_handle = vc_dispmanx_display_open(0);
    vd->info.subpicture_chromas = subpicture_chromas;

out:
    if (ret != VLC_SUCCESS)
        Close(object);

    return ret;
}

static void Close(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys = vd->sys;
    unsigned i;

    vc_tv_unregister_callback_full(tvservice_cb, vd);

    if (sys->dmx_handle)
        close_dmx(vd);

    if (sys->component && sys->component->control->is_enabled)
        mmal_port_disable(sys->component->control);

    if (sys->input && sys->input->is_enabled)
        mmal_port_disable(sys->input);

    if (sys->component && sys->component->is_enabled)
        mmal_component_disable(sys->component);

    if (sys->pool)
        mmal_pool_destroy(sys->pool);

    if (sys->component)
        mmal_component_release(sys->component);

    if (sys->picture_pool)
        picture_pool_Delete(sys->picture_pool);
    else
        for (i = 0; i < sys->num_buffers; ++i)
            if (sys->pictures[i])
                picture_Release(sys->pictures[i]);

    vlc_mutex_destroy(&sys->buffer_mutex);
    vlc_cond_destroy(&sys->buffer_cond);
    vlc_mutex_destroy(&sys->manage_mutex);

    free(sys);

    bcm_host_deinit();
}

static inline uint32_t align(uint32_t x, uint32_t y) {
    uint32_t mod = x % y;
    if (mod == 0)
        return x;
    else
        return x + y - mod;
}

static int configure_display(vout_display_t *vd, const vout_display_cfg_t *cfg,
                const video_format_t *fmt)
{
    vout_display_sys_t *sys = vd->sys;
    vout_display_place_t place;
    MMAL_DISPLAYREGION_T display_region;
    MMAL_STATUS_T status;

    if (!cfg && !fmt)
        return -EINVAL;

    if (fmt) {
        sys->input->format->es->video.par.num = fmt->i_sar_num;
        sys->input->format->es->video.par.den = fmt->i_sar_den;

        status = mmal_port_format_commit(sys->input);
        if (status != MMAL_SUCCESS) {
            msg_Err(vd, "Failed to commit format for input port %s (status=%"PRIx32" %s)",
                            sys->input->name, status, mmal_status_to_string(status));
            return -EINVAL;
        }
    } else {
        fmt = &vd->fmt;
    }

    if (!cfg)
        cfg = vd->cfg;

    vout_display_PlacePicture(&place, fmt, cfg, false);

    display_region.hdr.id = MMAL_PARAMETER_DISPLAYREGION;
    display_region.hdr.size = sizeof(MMAL_DISPLAYREGION_T);
    display_region.fullscreen = MMAL_FALSE;
    display_region.src_rect.x = fmt->i_x_offset;
    display_region.src_rect.y = fmt->i_y_offset;
    display_region.src_rect.width = fmt->i_visible_width;
    display_region.src_rect.height = fmt->i_visible_height;
    display_region.dest_rect.x = place.x;
    display_region.dest_rect.y = place.y;
    display_region.dest_rect.width = place.width;
    display_region.dest_rect.height = place.height;
    display_region.layer = sys->layer;
    display_region.set = MMAL_DISPLAY_SET_FULLSCREEN | MMAL_DISPLAY_SET_SRC_RECT |
            MMAL_DISPLAY_SET_DEST_RECT | MMAL_DISPLAY_SET_LAYER;
    status = mmal_port_parameter_set(sys->input, &display_region.hdr);
    if (status != MMAL_SUCCESS) {
        msg_Err(vd, "Failed to set display region (status=%"PRIx32" %s)",
                        status, mmal_status_to_string(status));
        return -EINVAL;
    }

    show_background(vd, cfg->is_fullscreen);
    if (var_InheritBool(vd, MMAL_ADJUST_REFRESHRATE_NAME))
        adjust_refresh_rate(vd);

    if (fmt != &vd->fmt)
        memcpy(&vd->fmt, fmt, sizeof(video_format_t));

    return 0;
}

static picture_pool_t *vd_pool(vout_display_t *vd, unsigned count)
{
    vout_display_sys_t *sys = vd->sys;
    picture_resource_t picture_res;
    picture_pool_configuration_t picture_pool_cfg;
    video_format_t fmt = vd->fmt;
    MMAL_STATUS_T status;
    unsigned i;

    if (sys->picture_pool) {
        if (sys->num_buffers < count)
            msg_Warn(vd, "Picture pool with %u pictures requested, but we already have one with %u pictures",
                            count, sys->num_buffers);

        goto out;
    }

    if (sys->opaque) {
        if (count <= NUM_ACTUAL_OPAQUE_BUFFERS)
            count = NUM_ACTUAL_OPAQUE_BUFFERS;
        else
            msg_Err(vd, "More picture (%u) than NUM_ACTUAL_OPAQUE_BUFFERS (%d) requested. Expect errors",
                            count, NUM_ACTUAL_OPAQUE_BUFFERS);
    }

    if (count < sys->input->buffer_num_recommended)
        count = sys->input->buffer_num_recommended;

#ifndef NDEBUG
    msg_Dbg(vd, "Creating picture pool with %u pictures", count);
#endif

    sys->input->buffer_num = count;
    status = mmal_port_enable(sys->input, input_port_cb);
    if (status != MMAL_SUCCESS) {
        msg_Err(vd, "Failed to enable input port %s (status=%"PRIx32" %s)",
                        sys->input->name, status, mmal_status_to_string(status));
        goto out;
    }

    status = mmal_component_enable(sys->component);
    if (status != MMAL_SUCCESS) {
        msg_Err(vd, "Failed to enable component %s (status=%"PRIx32" %s)",
                        sys->component->name, status, mmal_status_to_string(status));
        goto out;
    }

    sys->pool = mmal_pool_create_with_allocator(count, sys->input->buffer_size,
                    sys->input,
                    (mmal_pool_allocator_alloc_t)mmal_port_payload_alloc,
                    (mmal_pool_allocator_free_t)mmal_port_payload_free);
    if (!sys->pool) {
        msg_Err(vd, "Failed to create MMAL pool for %u buffers of size %"PRIu32,
                        count, sys->input->buffer_size);
        goto out;
    }
    sys->num_buffers = count;

    memset(&picture_res, 0, sizeof(picture_resource_t));
    sys->pictures = calloc(sys->num_buffers, sizeof(picture_t *));
    for (i = 0; i < sys->num_buffers; ++i) {
        picture_res.p_sys = malloc(sizeof(picture_sys_t));
        picture_res.p_sys->vd = vd;
        picture_res.p_sys->buffer = NULL;

        sys->pictures[i] = picture_NewFromResource(&fmt, &picture_res);
        if (!sys->pictures[i]) {
            msg_Err(vd, "Failed to create picture");
            free(picture_res.p_sys);
            goto out;
        }
    }

    memset(&picture_pool_cfg, 0, sizeof(picture_pool_configuration_t));
    picture_pool_cfg.picture_count = sys->num_buffers;
    picture_pool_cfg.picture = sys->pictures;
    picture_pool_cfg.lock = picture_lock;
    picture_pool_cfg.unlock = picture_unlock;

    sys->picture_pool = picture_pool_NewExtended(&picture_pool_cfg);
    if (!sys->picture_pool) {
        msg_Err(vd, "Failed to create picture pool");
        goto out;
    }

out:
    return sys->picture_pool;
}

static void vd_display(vout_display_t *vd, picture_t *picture,
                subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;
    picture_sys_t *pic_sys = picture->p_sys;
    MMAL_BUFFER_HEADER_T *buffer = pic_sys->buffer;
    MMAL_STATUS_T status;

    if (!pic_sys->displayed || !sys->opaque) {
        buffer->cmd = 0;
        buffer->length = sys->input->buffer_size;

        vlc_mutex_lock(&sys->buffer_mutex);
        while (sys->buffers_in_transit >= MAX_BUFFERS_IN_TRANSIT)
            vlc_cond_wait(&sys->buffer_cond, &sys->buffer_mutex);

        status = mmal_port_send_buffer(sys->input, buffer);
        if (status == MMAL_SUCCESS)
            ++sys->buffers_in_transit;

        vlc_mutex_unlock(&sys->buffer_mutex);
        if (status != MMAL_SUCCESS) {
            msg_Err(vd, "Failed to send buffer to input port. Frame dropped");
            picture_Release(picture);
        }

        pic_sys->displayed = true;
    } else {
        picture_Release(picture);
    }

    display_subpicture(vd, subpicture);

    if (subpicture)
        subpicture_Delete(subpicture);
}

static int vd_control(vout_display_t *vd, int query, va_list args)
{
    vout_display_sys_t *sys = vd->sys;
    vout_display_cfg_t cfg;
    const vout_display_cfg_t *tmp_cfg;
    video_format_t fmt;
    const video_format_t *tmp_fmt;
    int ret = VLC_EGENERIC;

    switch (query) {
        case VOUT_DISPLAY_HIDE_MOUSE:
        case VOUT_DISPLAY_CHANGE_WINDOW_STATE:
            ret = VLC_SUCCESS;
            break;

        case VOUT_DISPLAY_CHANGE_FULLSCREEN:
            tmp_cfg = va_arg(args, const vout_display_cfg_t *);
            vout_display_SendEventDisplaySize(vd, sys->display_width,
                            sys->display_height, tmp_cfg->is_fullscreen);
            ret = VLC_SUCCESS;
            break;

        case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
            tmp_cfg = va_arg(args, const vout_display_cfg_t *);
            if (tmp_cfg->display.width == sys->display_width &&
                            tmp_cfg->display.height == sys->display_height) {
                cfg = *vd->cfg;
                cfg.display.width = sys->display_width;
                cfg.display.height = sys->display_height;
                if (configure_display(vd, &cfg, NULL) >= 0)
                    ret = VLC_SUCCESS;
            }
            break;

        case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
            cfg = *vd->cfg;
            tmp_cfg = va_arg(args, const vout_display_cfg_t *);
            cfg.is_display_filled = tmp_cfg->is_display_filled;
            if (configure_display(vd, &cfg, NULL) >= 0)
                ret = VLC_SUCCESS;
            break;

        case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
            fmt = vd->fmt;
            tmp_fmt = va_arg(args, const video_format_t *);
            fmt.i_sar_num = tmp_fmt->i_sar_num;
            fmt.i_sar_den = tmp_fmt->i_sar_den;
            if (configure_display(vd, NULL, &fmt) >= 0)
                ret = VLC_SUCCESS;
            break;

        case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
            fmt = vd->fmt;
            tmp_fmt = va_arg(args, const video_format_t *);
            fmt.i_x_offset = tmp_fmt->i_x_offset;
            fmt.i_y_offset = tmp_fmt->i_y_offset;
            fmt.i_visible_width = tmp_fmt->i_visible_width;
            fmt.i_visible_height = tmp_fmt->i_visible_height;
            if (configure_display(vd, NULL, &fmt) >= 0)
                ret = VLC_SUCCESS;
            break;

        case VOUT_DISPLAY_CHANGE_ZOOM:
        case VOUT_DISPLAY_RESET_PICTURES:
        case VOUT_DISPLAY_GET_OPENGL:
            msg_Warn(vd, "Unsupported control query %d", query);
            break;

        default:
            msg_Warn(vd, "Unknown control query %d", query);
            break;
    }

    return ret;
}

static void vd_manage(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    unsigned width, height;

    vlc_mutex_lock(&sys->manage_mutex);

    if (sys->need_configure_display) {
        close_dmx(vd);
        sys->dmx_handle = vc_dispmanx_display_open(0);

        if (query_resolution(vd, &width, &height) >= 0) {
            sys->display_width = width;
            sys->display_height = height;
            vout_display_SendEventDisplaySize(vd, width, height, vd->cfg->is_fullscreen);
        }

        sys->need_configure_display = false;
    }

    vlc_mutex_unlock(&sys->manage_mutex);
}

static int picture_lock(picture_t *picture)
{
    vout_display_t *vd = picture->p_sys->vd;
    vout_display_sys_t *sys = vd->sys;
    picture_sys_t *pic_sys = picture->p_sys;

    MMAL_BUFFER_HEADER_T *buffer = mmal_queue_wait(sys->pool->queue);
    if (!buffer)
        return VLC_EGENERIC;

    vlc_mutex_lock(&sys->buffer_mutex);

    mmal_buffer_header_reset(buffer);
    buffer->user_data = picture;
    pic_sys->buffer = buffer;

    memcpy(picture->p, sys->planes, sizeof(sys->planes));
    picture->p[0].p_pixels = buffer->data;
    picture->p[1].p_pixels += (ptrdiff_t)buffer->data;
    picture->p[2].p_pixels += (ptrdiff_t)buffer->data;

    pic_sys->displayed = false;

    vlc_mutex_unlock(&sys->buffer_mutex);

    return VLC_SUCCESS;
}

static void picture_unlock(picture_t *picture)
{
    picture_sys_t *pic_sys = picture->p_sys;
    vout_display_t *vd = pic_sys->vd;
    vout_display_sys_t *sys = vd->sys;
    MMAL_BUFFER_HEADER_T *buffer = pic_sys->buffer;

    vlc_mutex_lock(&sys->buffer_mutex);

    pic_sys->buffer = NULL;
    if (buffer) {
        buffer->user_data = NULL;
        mmal_buffer_header_release(buffer);
    }

    vlc_mutex_unlock(&sys->buffer_mutex);
}

static void control_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    vout_display_t *vd = (vout_display_t *)port->userdata;
    MMAL_STATUS_T status;

    if (buffer->cmd == MMAL_EVENT_ERROR) {
        status = *(uint32_t *)buffer->data;
        msg_Err(vd, "MMAL error %"PRIx32" \"%s\"", status, mmal_status_to_string(status));
    }

    mmal_buffer_header_release(buffer);
}

static void input_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    vout_display_t *vd = (vout_display_t *)port->userdata;
    vout_display_sys_t *sys = vd->sys;
    picture_t *picture = (picture_t *)buffer->user_data;

    vlc_mutex_lock(&sys->buffer_mutex);
    --sys->buffers_in_transit;
    vlc_cond_signal(&sys->buffer_cond);
    vlc_mutex_unlock(&sys->buffer_mutex);

    if (picture)
        picture_Release(picture);
}

static int query_resolution(vout_display_t *vd, unsigned *width, unsigned *height)
{
    TV_DISPLAY_STATE_T display_state;
    int ret = 0;

    if (vc_tv_get_display_state(&display_state) == 0) {
        if (display_state.state & 0xFF) {
            *width = display_state.display.hdmi.width;
            *height = display_state.display.hdmi.height;
        } else if (display_state.state & 0xFF00) {
            *width = display_state.display.sdtv.width;
            *height = display_state.display.sdtv.height;
        } else {
            msg_Warn(vd, "Invalid display state %"PRIx32, display_state.state);
            ret = -1;
        }
    } else {
        msg_Warn(vd, "Failed to query display resolution");
        ret = -1;
    }

    return ret;
}

static void tvservice_cb(void *callback_data, uint32_t reason, uint32_t param1, uint32_t param2)
{
    VLC_UNUSED(reason);
    VLC_UNUSED(param1);
    VLC_UNUSED(param2);

    vout_display_t *vd = (vout_display_t *)callback_data;
    vout_display_sys_t *sys = vd->sys;

    vlc_mutex_lock(&sys->manage_mutex);
    sys->need_configure_display = true;
    vlc_mutex_unlock(&sys->manage_mutex);
}

static void adjust_refresh_rate(vout_display_t *vd)
{
    TV_DISPLAY_STATE_T display_state;
    TV_SUPPORTED_MODE_NEW_T supported_modes[VC_TV_MAX_MODE_IDS];
    int num_modes;
    double frame_rate = (double)vd->fmt.i_frame_rate / vd->fmt.i_frame_rate_base;
    int best_id = -1;
    double best_score, score;
    int i;

    vc_tv_get_display_state(&display_state);
    if(display_state.display.hdmi.mode != HDMI_MODE_OFF) {
        num_modes = vc_tv_hdmi_get_supported_modes_new(display_state.display.hdmi.group,
                        supported_modes, VC_TV_MAX_MODE_IDS, NULL, NULL);

        for(i = 0; i < num_modes; ++i) {
            if(supported_modes[i].width != display_state.display.hdmi.width ||
                            supported_modes[i].height != display_state.display.hdmi.height ||
                            supported_modes[i].scan_mode != display_state.display.hdmi.scan_mode)
                continue;

            score = fmod(supported_modes[i].frame_rate, frame_rate);
            if((best_id < 0) || (score < best_score)) {
                best_id = i;
                best_score = score;
            }
        }

        if((best_id >= 0) && (display_state.display.hdmi.frame_rate != supported_modes[best_id].frame_rate)) {
            msg_Info(vd, "Setting HDMI refresh rate to %"PRIu32,
                            supported_modes[best_id].frame_rate);
            vc_tv_hdmi_power_on_explicit_new(HDMI_MODE_HDMI,
                            supported_modes[best_id].group,
                            supported_modes[best_id].code);
        }
    }
}

static void display_subpicture(vout_display_t *vd, subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;
    struct dmx_region_t **dmx_region = &sys->dmx_region;
    struct dmx_region_t *unused_dmx_region;
    DISPMANX_UPDATE_HANDLE_T update = 0;
    picture_t *picture;
    video_format_t *fmt;
    struct dmx_region_t *dmx_region_next;

    if(subpicture) {
        subpicture_region_t *region = subpicture->p_region;
        while(region) {
            picture = region->p_picture;
            fmt = &region->fmt;

            if(!*dmx_region) {
                if(!update)
                    update = vc_dispmanx_update_start(10);
                *dmx_region = dmx_region_new(vd, update, region);
            } else if(((*dmx_region)->bmp_rect.width != (int32_t)fmt->i_visible_width) ||
                    ((*dmx_region)->bmp_rect.height != (int32_t)fmt->i_visible_height) ||
                    ((*dmx_region)->pos_x != region->i_x) ||
                    ((*dmx_region)->pos_y != region->i_y) ||
                    ((*dmx_region)->alpha.opacity != (uint32_t)region->i_alpha)) {
                dmx_region_next = (*dmx_region)->next;
                if(!update)
                    update = vc_dispmanx_update_start(10);
                dmx_region_delete(*dmx_region, update);
                *dmx_region = dmx_region_new(vd, update, region);
                (*dmx_region)->next = dmx_region_next;
            } else if((*dmx_region)->picture != picture) {
                if(!update)
                    update = vc_dispmanx_update_start(10);
                dmx_region_update(*dmx_region, update, picture);
            }

            dmx_region = &(*dmx_region)->next;
            region = region->p_next;
        }
    }

    /* Remove remaining regions */
    unused_dmx_region = *dmx_region;
    while(unused_dmx_region) {
        dmx_region_next = unused_dmx_region->next;
        if(!update)
            update = vc_dispmanx_update_start(10);
        dmx_region_delete(unused_dmx_region, update);
        unused_dmx_region = dmx_region_next;
    }
    *dmx_region = NULL;

    if(update)
        vc_dispmanx_update_submit_sync(update);
}

static void close_dmx(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    DISPMANX_UPDATE_HANDLE_T update = vc_dispmanx_update_start(10);
    struct dmx_region_t *dmx_region = sys->dmx_region;
    struct dmx_region_t *dmx_region_next;

    while(dmx_region) {
        dmx_region_next = dmx_region->next;
        dmx_region_delete(dmx_region, update);
        dmx_region = dmx_region_next;
    }

    vc_dispmanx_update_submit_sync(update);
    sys->dmx_region = NULL;

    show_background(vd, false);

    vc_dispmanx_display_close(sys->dmx_handle);
    sys->dmx_handle = DISPMANX_NO_HANDLE;
}

static struct dmx_region_t *dmx_region_new(vout_display_t *vd,
                DISPMANX_UPDATE_HANDLE_T update, subpicture_region_t *region)
{
    vout_display_sys_t *sys = vd->sys;
    video_format_t *fmt = &region->fmt;
    struct dmx_region_t *dmx_region = malloc(sizeof(struct dmx_region_t));
    uint32_t image_handle;

    dmx_region->pos_x = region->i_x;
    dmx_region->pos_y = region->i_y;

    vc_dispmanx_rect_set(&dmx_region->bmp_rect, 0, 0, fmt->i_visible_width,
                    fmt->i_visible_height);
    vc_dispmanx_rect_set(&dmx_region->src_rect, 0, 0, fmt->i_visible_width << 16,
                    fmt->i_visible_height << 16);
    vc_dispmanx_rect_set(&dmx_region->dst_rect, region->i_x, region->i_y,
                    fmt->i_visible_width, fmt->i_visible_height);

    dmx_region->resource = vc_dispmanx_resource_create(VC_IMAGE_RGBA32,
                    dmx_region->bmp_rect.width | (region->p_picture->p[0].i_pitch << 16),
                    dmx_region->bmp_rect.height | (dmx_region->bmp_rect.height << 16),
                    &image_handle);
    vc_dispmanx_resource_write_data(dmx_region->resource, VC_IMAGE_RGBA32,
                    region->p_picture->p[0].i_pitch,
                    region->p_picture->p[0].p_pixels, &dmx_region->bmp_rect);

    dmx_region->alpha.flags = DISPMANX_FLAGS_ALPHA_FROM_SOURCE | DISPMANX_FLAGS_ALPHA_MIX;
    dmx_region->alpha.opacity = region->i_alpha;
    dmx_region->alpha.mask = DISPMANX_NO_HANDLE;
    dmx_region->element = vc_dispmanx_element_add(update, sys->dmx_handle,
                    sys->layer + 1, &dmx_region->dst_rect, dmx_region->resource,
                    &dmx_region->src_rect, DISPMANX_PROTECTION_NONE,
                    &dmx_region->alpha, NULL, VC_IMAGE_ROT0);

    dmx_region->next = NULL;
    dmx_region->picture = region->p_picture;

    return dmx_region;
}

static void dmx_region_update(struct dmx_region_t *dmx_region,
                DISPMANX_UPDATE_HANDLE_T update, picture_t *picture)
{
    vc_dispmanx_resource_write_data(dmx_region->resource, VC_IMAGE_RGBA32,
                    picture->p[0].i_pitch, picture->p[0].p_pixels, &dmx_region->bmp_rect);
    vc_dispmanx_element_change_source(update, dmx_region->element, dmx_region->resource);
    dmx_region->picture = picture;
}

static void dmx_region_delete(struct dmx_region_t *dmx_region,
                DISPMANX_UPDATE_HANDLE_T update)
{
    vc_dispmanx_element_remove(update, dmx_region->element);
    vc_dispmanx_resource_delete(dmx_region->resource);
    free(dmx_region);
}

static void show_background(vout_display_t *vd, bool enable)
{
    vout_display_sys_t *sys = vd->sys;
    uint32_t image_ptr, color = 0xFF000000;
    VC_RECT_T dst_rect, src_rect;
    DISPMANX_UPDATE_HANDLE_T update;

    if (enable && !sys->bkg_element) {
        sys->bkg_resource = vc_dispmanx_resource_create(VC_IMAGE_RGBA32, 1, 1,
                        &image_ptr);
        vc_dispmanx_rect_set(&dst_rect, 0, 0, 1, 1);
        vc_dispmanx_resource_write_data(sys->bkg_resource, VC_IMAGE_RGBA32,
                        sizeof(color), &color, &dst_rect);
        vc_dispmanx_rect_set(&src_rect, 0, 0, 1 << 16, 1 << 16);
        vc_dispmanx_rect_set(&dst_rect, 0, 0, 0, 0);
        update = vc_dispmanx_update_start(0);
        sys->bkg_element = vc_dispmanx_element_add(update, sys->dmx_handle,
                        sys->layer - 1, &dst_rect, sys->bkg_resource, &src_rect,
                        DISPMANX_PROTECTION_NONE, NULL, NULL, VC_IMAGE_ROT0);
        vc_dispmanx_update_submit_sync(update);
    } else if (!enable && sys->bkg_element) {
        update = vc_dispmanx_update_start(0);
        vc_dispmanx_element_remove(update, sys->bkg_element);
        vc_dispmanx_resource_delete(sys->bkg_resource);
        vc_dispmanx_update_submit_sync(update);
        sys->bkg_element = DISPMANX_NO_HANDLE;
        sys->bkg_resource = DISPMANX_NO_HANDLE;
    }
}
