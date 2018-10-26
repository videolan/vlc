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
#include <stdatomic.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_threads.h>
#include <vlc_vout_display.h>

#include "mmal_picture.h"

#include <bcm_host.h>
#include <interface/mmal/mmal.h>
#include <interface/mmal/util/mmal_util.h>
#include <interface/mmal/util/mmal_default_components.h>
#include <interface/vmcs_host/vc_tvservice.h>
#include <interface/vmcs_host/vc_dispmanx.h>

#define MAX_BUFFERS_IN_TRANSIT 1
#define VC_TV_MAX_MODE_IDS 127

#define MMAL_LAYER_NAME "mmal-layer"
#define MMAL_LAYER_TEXT N_("VideoCore layer where the video is displayed.")
#define MMAL_LAYER_LONGTEXT N_("VideoCore layer where the video is displayed. Subpictures are displayed directly above and a black background directly below.")

#define MMAL_BLANK_BACKGROUND_NAME "mmal-blank-background"
#define MMAL_BLANK_BACKGROUND_TEXT N_("Blank screen below video.")
#define MMAL_BLANK_BACKGROUND_LONGTEXT N_("Render blank screen below video. " \
        "Increases VideoCore load.")

#define MMAL_ADJUST_REFRESHRATE_NAME "mmal-adjust-refreshrate"
#define MMAL_ADJUST_REFRESHRATE_TEXT N_("Adjust HDMI refresh rate to the video.")
#define MMAL_ADJUST_REFRESHRATE_LONGTEXT N_("Adjust HDMI refresh rate to the video.")

#define MMAL_NATIVE_INTERLACED "mmal-native-interlaced"
#define MMAL_NATIVE_INTERLACE_TEXT N_("Force interlaced video mode.")
#define MMAL_NATIVE_INTERLACE_LONGTEXT N_("Force the HDMI output into an " \
        "interlaced video mode for interlaced video content.")

/* Ideal rendering phase target is at rough 25% of frame duration */
#define PHASE_OFFSET_TARGET ((double)0.25)
#define PHASE_CHECK_INTERVAL 100

static int Open(vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_shortname(N_("MMAL vout"))
    set_description(N_("MMAL-based vout plugin for Raspberry Pi"))
    set_capability("vout display", 90)
    add_shortcut("mmal_vout")
    add_integer(MMAL_LAYER_NAME, 1, MMAL_LAYER_TEXT, MMAL_LAYER_LONGTEXT, false)
    add_bool(MMAL_BLANK_BACKGROUND_NAME, true, MMAL_BLANK_BACKGROUND_TEXT,
                    MMAL_BLANK_BACKGROUND_LONGTEXT, true);
    add_bool(MMAL_ADJUST_REFRESHRATE_NAME, false, MMAL_ADJUST_REFRESHRATE_TEXT,
                    MMAL_ADJUST_REFRESHRATE_LONGTEXT, false)
    add_bool(MMAL_NATIVE_INTERLACED, false, MMAL_NATIVE_INTERLACE_TEXT,
                    MMAL_NATIVE_INTERLACE_LONGTEXT, false)
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
    vlc_cond_t buffer_cond;
    vlc_mutex_t buffer_mutex;
    vlc_mutex_t manage_mutex;

    plane_t planes[3]; /* Depending on video format up to 3 planes are used */
    picture_t **pictures; /* Actual list of alloced pictures passed into picture_pool */
    picture_pool_t *picture_pool;

    MMAL_COMPONENT_T *component;
    MMAL_PORT_T *input;
    MMAL_POOL_T *pool; /* mmal buffer headers, used for pushing pictures to component*/
    struct dmx_region_t *dmx_region;
    int i_planes; /* Number of actually used planes, 1 for opaque, 3 for i420 */

    uint32_t buffer_size; /* size of actual mmal buffers */
    int buffers_in_transit; /* number of buffers currently pushed to mmal component */
    unsigned num_buffers; /* number of buffers allocated at mmal port */

    DISPMANX_DISPLAY_HANDLE_T dmx_handle;
    DISPMANX_ELEMENT_HANDLE_T bkg_element;
    DISPMANX_RESOURCE_HANDLE_T bkg_resource;
    unsigned display_width;
    unsigned display_height;

    int i_frame_rate_base; /* cached framerate to detect changes for rate adjustment */
    int i_frame_rate;

    int next_phase_check; /* lowpass for phase check frequency */
    int phase_offset; /* currently applied offset to presentation time in ns */
    int layer; /* the dispman layer (z-index) used for video rendering */

    bool need_configure_display; /* indicates a required display reconfigure to main thread */
    bool adjust_refresh_rate;
    bool native_interlaced;
    bool b_top_field_first; /* cached interlaced settings to detect changes for native mode */
    bool b_progressive;
    bool opaque; /* indicated use of opaque picture format (zerocopy) */
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
static void vd_prepare(vout_display_t *vd, picture_t *picture,
                subpicture_t *subpicture);
static void vd_display(vout_display_t *vd, picture_t *picture,
                subpicture_t *subpicture);
static int vd_control(vout_display_t *vd, int query, va_list args);
static void vd_manage(vout_display_t *vd);

/* MMAL callbacks */
static void control_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
static void input_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);

/* TV service */
static int query_resolution(vout_display_t *vd, unsigned *width, unsigned *height);
static void tvservice_cb(void *callback_data, uint32_t reason, uint32_t param1,
                uint32_t param2);
static void adjust_refresh_rate(vout_display_t *vd, const video_format_t *fmt);
static int set_latency_target(vout_display_t *vd, bool enable);

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
static void maintain_phase_sync(vout_display_t *vd);

static int Open(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys;
    uint32_t buffer_pitch, buffer_height;
    vout_display_place_t place;
    MMAL_DISPLAYREGION_T display_region;
    MMAL_STATUS_T status;
    int ret = VLC_SUCCESS;
    unsigned i;

    if (vout_display_IsWindowed(vd))
        return VLC_EGENERIC;

    sys = calloc(1, sizeof(struct vout_display_sys_t));
    if (!sys)
        return VLC_ENOMEM;
    vd->sys = sys;

    sys->layer = var_InheritInteger(vd, MMAL_LAYER_NAME);
    bcm_host_init();

    sys->opaque = vd->fmt.i_chroma == VLC_CODEC_MMAL_OPAQUE;

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

    if (sys->opaque) {
        sys->input->format->encoding = MMAL_ENCODING_OPAQUE;
        sys->i_planes = 1;
        sys->buffer_size = sys->input->buffer_size_recommended;
    } else {
        sys->input->format->encoding = MMAL_ENCODING_I420;
        vd->fmt.i_chroma = VLC_CODEC_I420;
        buffer_pitch = align(vd->fmt.i_width, 32);
        buffer_height = align(vd->fmt.i_height, 16);
        sys->i_planes = 3;
        sys->buffer_size = 3 * buffer_pitch * buffer_height / 2;
    }

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

    for (i = 0; i < sys->i_planes; ++i) {
        sys->planes[i].i_lines = buffer_height;
        sys->planes[i].i_pitch = buffer_pitch;
        sys->planes[i].i_visible_lines = vd->fmt.i_visible_height;
        sys->planes[i].i_visible_pitch = vd->fmt.i_visible_width;

        if (i > 0) {
            sys->planes[i].i_lines /= 2;
            sys->planes[i].i_pitch /= 2;
            sys->planes[i].i_visible_lines /= 2;
            sys->planes[i].i_visible_pitch /= 2;
        }
    }

    vlc_mutex_init(&sys->buffer_mutex);
    vlc_cond_init(&sys->buffer_cond);
    vlc_mutex_init(&sys->manage_mutex);

    vd->pool = vd_pool;
    vd->prepare = vd_prepare;
    vd->display = vd_display;
    vd->control = vd_control;

    vc_tv_register_callback(tvservice_cb, vd);

    if (query_resolution(vd, &sys->display_width, &sys->display_height) >= 0) {
        vout_window_ReportSize(vd->cfg->window,
                               sys->display_width, sys->display_height);
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
    char response[20]; /* answer is hvs_update_fields=%1d */
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
        mmal_port_pool_destroy(sys->input, sys->pool);

    if (sys->component)
        mmal_component_release(sys->component);

    if (sys->picture_pool)
        picture_pool_Release(sys->picture_pool);
    else
        for (i = 0; i < sys->num_buffers; ++i)
            if (sys->pictures[i]) {
                mmal_buffer_header_release(sys->pictures[i]->p_sys->buffer);
                picture_Release(sys->pictures[i]);
            }

    vlc_mutex_destroy(&sys->buffer_mutex);
    vlc_cond_destroy(&sys->buffer_cond);
    vlc_mutex_destroy(&sys->manage_mutex);

    if (sys->native_interlaced) {
        if (vc_gencmd(response, sizeof(response), "hvs_update_fields 0") < 0 ||
                response[18] != '0')
            msg_Warn(vd, "Could not reset hvs field mode");
    }

    free(sys->pictures);
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
        fmt = &vd->source;
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

    show_background(vd, var_InheritBool(vd, MMAL_BLANK_BACKGROUND_NAME));
    sys->adjust_refresh_rate = var_InheritBool(vd, MMAL_ADJUST_REFRESHRATE_NAME);
    sys->native_interlaced = var_InheritBool(vd, MMAL_NATIVE_INTERLACED);
    if (sys->adjust_refresh_rate) {
        adjust_refresh_rate(vd, fmt);
        set_latency_target(vd, true);
    }

    return 0;
}

static picture_pool_t *vd_pool(vout_display_t *vd, unsigned count)
{
    vout_display_sys_t *sys = vd->sys;
    picture_resource_t picture_res;
    picture_pool_configuration_t picture_pool_cfg;
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

        MMAL_PARAMETER_BOOLEAN_T zero_copy = {
            { MMAL_PARAMETER_ZERO_COPY, sizeof(MMAL_PARAMETER_BOOLEAN_T) },
            1
        };

        status = mmal_port_parameter_set(sys->input, &zero_copy.hdr);
        if (status != MMAL_SUCCESS) {
           msg_Err(vd, "Failed to set zero copy on port %s (status=%"PRIx32" %s)",
                    sys->input->name, status, mmal_status_to_string(status));
           goto out;
        }
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

    sys->num_buffers = count;
    sys->pool = mmal_port_pool_create(sys->input, sys->num_buffers,
            sys->input->buffer_size);
    if (!sys->pool) {
        msg_Err(vd, "Failed to create MMAL pool for %u buffers of size %"PRIu32,
                        count, sys->input->buffer_size);
        goto out;
    }

    memset(&picture_res, 0, sizeof(picture_resource_t));
    sys->pictures = calloc(sys->num_buffers, sizeof(picture_t *));
    for (i = 0; i < sys->num_buffers; ++i) {
        picture_res.p_sys = calloc(1, sizeof(picture_sys_t));
        picture_res.p_sys->owner = (vlc_object_t *)vd;
        picture_res.p_sys->buffer = mmal_queue_get(sys->pool->queue);

        sys->pictures[i] = picture_NewFromResource(&vd->fmt, &picture_res);
        if (!sys->pictures[i]) {
            msg_Err(vd, "Failed to create picture");
            free(picture_res.p_sys);
            goto out;
        }

        sys->pictures[i]->i_planes = sys->i_planes;
        memcpy(sys->pictures[i]->p, sys->planes, sys->i_planes * sizeof(plane_t));
    }

    memset(&picture_pool_cfg, 0, sizeof(picture_pool_configuration_t));
    picture_pool_cfg.picture_count = sys->num_buffers;
    picture_pool_cfg.picture = sys->pictures;
    picture_pool_cfg.lock = mmal_picture_lock;

    sys->picture_pool = picture_pool_NewExtended(&picture_pool_cfg);
    if (!sys->picture_pool) {
        msg_Err(vd, "Failed to create picture pool");
        goto out;
    }

out:
    return sys->picture_pool;
}

static void vd_prepare(vout_display_t *vd, picture_t *picture,
                       subpicture_t *subpicture, vlc_tick_t date)
{
    vd_manage(vd);
    VLC_UNUSED(date);
    vout_display_sys_t *sys = vd->sys;
    picture_sys_t *pic_sys = picture->p_sys;

    if (!sys->adjust_refresh_rate || pic_sys->displayed)
        return;

    /* Apply the required phase_offset to the picture, so that vd_display()
     * will be called at the corrected time from the core */
    picture->date += sys->phase_offset;
}

static void vd_display(vout_display_t *vd, picture_t *picture,
                subpicture_t *subpicture)
{
    vout_display_sys_t *sys = vd->sys;
    picture_sys_t *pic_sys = picture->p_sys;
    MMAL_BUFFER_HEADER_T *buffer = pic_sys->buffer;
    MMAL_STATUS_T status;

    if (picture->format.i_frame_rate != sys->i_frame_rate ||
        picture->format.i_frame_rate_base != sys->i_frame_rate_base ||
        picture->b_progressive != sys->b_progressive ||
        picture->b_top_field_first != sys->b_top_field_first) {
        sys->b_top_field_first = picture->b_top_field_first;
        sys->b_progressive = picture->b_progressive;
        sys->i_frame_rate = picture->format.i_frame_rate;
        sys->i_frame_rate_base = picture->format.i_frame_rate_base;
        configure_display(vd, NULL, &picture->format);
    }

    if (!pic_sys->displayed || !sys->opaque) {
        buffer->cmd = 0;
        buffer->length = sys->input->buffer_size;
        buffer->user_data = picture_Hold(picture);

        status = mmal_port_send_buffer(sys->input, buffer);
        if (status == MMAL_SUCCESS)
            atomic_fetch_add(&sys->buffers_in_transit, 1);

        if (status != MMAL_SUCCESS) {
            msg_Err(vd, "Failed to send buffer to input port. Frame dropped");
            picture_Release(picture);
        }

        pic_sys->displayed = true;
    }

    display_subpicture(vd, subpicture);

    if (subpicture)
        subpicture_Delete(subpicture);

    if (sys->next_phase_check == 0 && sys->adjust_refresh_rate)
        maintain_phase_sync(vd);
    sys->next_phase_check = (sys->next_phase_check + 1) % PHASE_CHECK_INTERVAL;

    if (sys->opaque) {
        vlc_mutex_lock(&sys->buffer_mutex);
        while (atomic_load(&sys->buffers_in_transit) >= MAX_BUFFERS_IN_TRANSIT)
            vlc_cond_wait(&sys->buffer_cond, &sys->buffer_mutex);
        vlc_mutex_unlock(&sys->buffer_mutex);
    }
}

static int vd_control(vout_display_t *vd, int query, va_list args)
{
    vout_display_sys_t *sys = vd->sys;
    vout_display_cfg_t cfg;
    const vout_display_cfg_t *tmp_cfg;
    int ret = VLC_EGENERIC;

    switch (query) {
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

        case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
        case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
            if (configure_display(vd, NULL, &vd->source) >= 0)
                ret = VLC_SUCCESS;
            break;

        case VOUT_DISPLAY_RESET_PICTURES:
            vlc_assert_unreachable();
        case VOUT_DISPLAY_CHANGE_ZOOM:
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
            vout_window_ReportSize(vd->cfg->window, width, height);
        }

        sys->need_configure_display = false;
    }

    vlc_mutex_unlock(&sys->manage_mutex);
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

    if (picture)
        picture_Release(picture);

    vlc_mutex_lock(&sys->buffer_mutex);
    atomic_fetch_sub(&sys->buffers_in_transit, 1);
    vlc_cond_signal(&sys->buffer_cond);
    vlc_mutex_unlock(&sys->buffer_mutex);
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

static int set_latency_target(vout_display_t *vd, bool enable)
{
    vout_display_sys_t *sys = vd->sys;
    MMAL_STATUS_T status;

    MMAL_PARAMETER_AUDIO_LATENCY_TARGET_T latency_target = {
        .hdr = { MMAL_PARAMETER_AUDIO_LATENCY_TARGET, sizeof(latency_target) },
        .enable = enable ? MMAL_TRUE : MMAL_FALSE,
        .filter = 2,
        .target = 4000,
        .shift = 3,
        .speed_factor = -135,
        .inter_factor = 500,
        .adj_cap = 20
    };

    status = mmal_port_parameter_set(sys->input, &latency_target.hdr);
    if (status != MMAL_SUCCESS) {
        msg_Err(vd, "Failed to configure latency target on input port %s (status=%"PRIx32" %s)",
                        sys->input->name, status, mmal_status_to_string(status));
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static void adjust_refresh_rate(vout_display_t *vd, const video_format_t *fmt)
{
    vout_display_sys_t *sys = vd->sys;
    TV_DISPLAY_STATE_T display_state;
    TV_SUPPORTED_MODE_NEW_T supported_modes[VC_TV_MAX_MODE_IDS];
    char response[20]; /* answer is hvs_update_fields=%1d */
    int num_modes;
    double frame_rate = (double)fmt->i_frame_rate / fmt->i_frame_rate_base;
    int best_id = -1;
    double best_score, score;
    int i;

    vc_tv_get_display_state(&display_state);
    if(display_state.display.hdmi.mode != HDMI_MODE_OFF) {
        num_modes = vc_tv_hdmi_get_supported_modes_new(display_state.display.hdmi.group,
                        supported_modes, VC_TV_MAX_MODE_IDS, NULL, NULL);

        for (i = 0; i < num_modes; ++i) {
            TV_SUPPORTED_MODE_NEW_T *mode = &supported_modes[i];
            if (!sys->native_interlaced) {
                if (mode->width != display_state.display.hdmi.width ||
                                mode->height != display_state.display.hdmi.height ||
                                mode->scan_mode == HDMI_INTERLACED)
                    continue;
            } else {
                if (mode->width != vd->fmt.i_visible_width ||
                        mode->height != vd->fmt.i_visible_height)
                    continue;
                if (mode->scan_mode != sys->b_progressive ? HDMI_NONINTERLACED : HDMI_INTERLACED)
                    continue;
            }

            score = fmod(supported_modes[i].frame_rate, frame_rate);
            if((best_id < 0) || (score < best_score)) {
                best_id = i;
                best_score = score;
            }
        }

        if((best_id >= 0) && (display_state.display.hdmi.mode != supported_modes[best_id].code)) {
            msg_Info(vd, "Setting HDMI refresh rate to %"PRIu32,
                            supported_modes[best_id].frame_rate);
            vc_tv_hdmi_power_on_explicit_new(HDMI_MODE_HDMI,
                            supported_modes[best_id].group,
                            supported_modes[best_id].code);
        }

        if (sys->native_interlaced &&
                supported_modes[best_id].scan_mode == HDMI_INTERLACED) {
            char hvs_mode = sys->b_top_field_first ? '1' : '2';
            if (vc_gencmd(response, sizeof(response), "hvs_update_fields %c",
                    hvs_mode) != 0 || response[18] != hvs_mode)
                msg_Warn(vd, "Could not set hvs field mode");
            else
                msg_Info(vd, "Configured hvs field mode for interlaced %s playback",
                        sys->b_top_field_first ? "tff" : "bff");
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

static void maintain_phase_sync(vout_display_t *vd)
{
    MMAL_PARAMETER_VIDEO_RENDER_STATS_T render_stats = {
        .hdr = { MMAL_PARAMETER_VIDEO_RENDER_STATS, sizeof(render_stats) },
    };
    int32_t frame_duration = CLOCK_FREQ /
        ((double)vd->sys->i_frame_rate /
        vd->sys->i_frame_rate_base);
    vout_display_sys_t *sys = vd->sys;
    int32_t phase_offset;
    MMAL_STATUS_T status;

    status = mmal_port_parameter_get(sys->input, &render_stats.hdr);
    if (status != MMAL_SUCCESS) {
        msg_Err(vd, "Failed to read render stats on control port %s (status=%"PRIx32" %s)",
                        sys->input->name, status, mmal_status_to_string(status));
        return;
    }

    if (render_stats.valid) {
#ifndef NDEBUG
        msg_Dbg(vd, "render_stats: match: %u, period: %u ms, phase: %u ms, hvs: %u",
                render_stats.match, render_stats.period / 1000, render_stats.phase / 1000,
                render_stats.hvs_status);
#endif

        if (render_stats.phase > 0.1 * frame_duration &&
                render_stats.phase < 0.75 * frame_duration)
            return;

        phase_offset = frame_duration * PHASE_OFFSET_TARGET - render_stats.phase;
        if (phase_offset < 0)
            phase_offset += frame_duration;
        else
            phase_offset %= frame_duration;

        sys->phase_offset += phase_offset;
        sys->phase_offset %= frame_duration;
        msg_Dbg(vd, "Apply phase offset of %"PRId32" ms (total offset %"PRId32" ms)",
                phase_offset / 1000, sys->phase_offset / 1000);

        /* Reset the latency target, so that it does not get confused
         * by the jump in the offset */
        set_latency_target(vd, false);
        set_latency_target(vd, true);
    }
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
