/*****************************************************************************
 * mmal.c: MMAL-based deinterlace plugin for Raspberry Pi
 *****************************************************************************
 * Copyright © 2014 jusst technologies GmbH
 * $Id$
 *
 * Authors: Julian Scheel <julian@jusst.de>
 *          Dennis Hamester <dennis.hamester@gmail.com>
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

#include <vlc_picture_pool.h>
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>

#include "mmal_picture.h"

#include <bcm_host.h>
#include <interface/mmal/mmal.h>
#include <interface/mmal/util/mmal_util.h>
#include <interface/mmal/util/mmal_default_components.h>

#define NUM_EXTRA_BUFFERS 2
#define MIN_NUM_BUFFERS_IN_TRANSIT 2

static int Open(filter_t *filter);
static void Close(filter_t *filter);

vlc_module_begin()
    set_shortname(N_("MMAL deinterlace"))
    set_description(N_("MMAL-based deinterlace filter plugin"))
    set_capability("video filter2", 10)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VFILTER)
    set_callbacks(Open, Close)
    add_shortcut("deinterlace")
vlc_module_end()

struct filter_sys_t {
    MMAL_COMPONENT_T *component;
    MMAL_PORT_T *input;
    MMAL_POOL_T *input_pool;
    MMAL_PORT_T *output;
    MMAL_POOL_T *output_pool;

    picture_pool_t *picture_pool;
    picture_t **pictures;

    MMAL_QUEUE_T *filtered_pictures;
    vlc_mutex_t mutex;
    vlc_cond_t buffer_cond;

    /* statistics */
    int output_in_transit;
    int input_in_transit;
};

static void control_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
static void input_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
static void output_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
static picture_t *deinterlace(filter_t *filter, picture_t *picture);
static void flush(filter_t *filter);

#define MMAL_COMPONENT_DEFAULT_DEINTERLACE "vc.ril.image_fx"

static int create_picture_pool(filter_t *filter)
{
    picture_pool_configuration_t picture_pool_cfg;
    filter_sys_t *sys = filter->p_sys;
    picture_resource_t picture_res;
    int ret = 0;

    memset(&picture_res, 0, sizeof(picture_resource_t));
    sys->pictures = calloc(sys->output->buffer_num, sizeof(picture_t *));
    if (!sys->pictures) {
        ret = -ENOMEM;
        goto out;
    }

    for (unsigned i = 0; i < sys->output->buffer_num; i++) {
        picture_res.p_sys = calloc(1, sizeof(picture_sys_t));
        if (!picture_res.p_sys) {
            ret = -ENOMEM;
            goto out;
        }

        picture_res.p_sys->owner = (vlc_object_t *)filter;
        picture_res.p_sys->queue = sys->output_pool->queue;
        picture_res.p_sys->mutex = &sys->mutex;

        sys->pictures[i] = picture_NewFromResource(&filter->fmt_out.video,
                &picture_res);
        if (!sys->pictures[i]) {
            free(picture_res.p_sys);
            ret = -ENOMEM;
            goto out;
        }
    }

    memset(&picture_pool_cfg, 0, sizeof(picture_pool_configuration_t));
    picture_pool_cfg.picture_count = sys->output->buffer_num;
    picture_pool_cfg.picture = sys->pictures;
    picture_pool_cfg.lock = mmal_picture_lock;
    picture_pool_cfg.unlock = mmal_picture_unlock;

    sys->picture_pool = picture_pool_NewExtended(&picture_pool_cfg);
    if (!sys->picture_pool) {
        msg_Err(filter, "Failed to create picture pool");
        ret = -ENOMEM;
        goto out;
    }

out:
    return ret;
}

static int Open(filter_t *filter)
{
    int32_t frame_duration = filter->fmt_in.video.i_frame_rate != 0 ?
            (int64_t)1000000 * filter->fmt_in.video.i_frame_rate_base /
            filter->fmt_in.video.i_frame_rate : 0;

    MMAL_PARAMETER_IMAGEFX_PARAMETERS_T imfx_param = {
            { MMAL_PARAMETER_IMAGE_EFFECT_PARAMETERS, sizeof(imfx_param) },
            MMAL_PARAM_IMAGEFX_DEINTERLACE_ADV,
            2,
            { 3, frame_duration }
    };
    MMAL_PARAMETER_UINT32_T extra_buffers;

    int ret = VLC_SUCCESS;
    MMAL_STATUS_T status;
    filter_sys_t *sys;

    msg_Dbg(filter, "Try to open mmal_deinterlace filter. frame_duration: %d!", frame_duration);

    if (filter->fmt_in.video.i_chroma != VLC_CODEC_MMAL_OPAQUE)
        return VLC_EGENERIC;

    if (filter->fmt_out.video.i_chroma != VLC_CODEC_MMAL_OPAQUE)
        return VLC_EGENERIC;

    sys = calloc(1, sizeof(filter_sys_t));
    if (!sys)
        return VLC_ENOMEM;
    filter->p_sys = sys;

    bcm_host_init();

    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_DEINTERLACE, &sys->component);
    if (status != MMAL_SUCCESS) {
        msg_Err(filter, "Failed to create MMAL component %s (status=%"PRIx32" %s)",
                MMAL_COMPONENT_DEFAULT_DEINTERLACE, status, mmal_status_to_string(status));
        ret = VLC_EGENERIC;
        goto out;
    }

    status = mmal_port_parameter_set(sys->component->output[0], &imfx_param.hdr);
    if (status != MMAL_SUCCESS) {
        msg_Err(filter, "Failed to configure MMAL component %s (status=%"PRIx32" %s)",
                MMAL_COMPONENT_DEFAULT_DEINTERLACE, status, mmal_status_to_string(status));
        ret = VLC_EGENERIC;
        goto out;
    }

    sys->component->control->userdata = (struct MMAL_PORT_USERDATA_T *)filter;
    status = mmal_port_enable(sys->component->control, control_port_cb);
    if (status != MMAL_SUCCESS) {
        msg_Err(filter, "Failed to enable control port %s (status=%"PRIx32" %s)",
                sys->component->control->name, status, mmal_status_to_string(status));
        ret = VLC_EGENERIC;
        goto out;
    }

    sys->input = sys->component->input[0];
    sys->input->userdata = (struct MMAL_PORT_USERDATA_T *)filter;
    if (filter->fmt_in.i_codec == VLC_CODEC_MMAL_OPAQUE)
        sys->input->format->encoding = MMAL_ENCODING_OPAQUE;
    sys->input->format->es->video.width = filter->fmt_in.video.i_width;
    sys->input->format->es->video.height = filter->fmt_in.video.i_height;
    sys->input->format->es->video.crop.x = 0;
    sys->input->format->es->video.crop.y = 0;
    sys->input->format->es->video.crop.width = filter->fmt_in.video.i_width;
    sys->input->format->es->video.crop.height = filter->fmt_in.video.i_height;
    sys->input->format->es->video.par.num = filter->fmt_in.video.i_sar_num;
    sys->input->format->es->video.par.den = filter->fmt_in.video.i_sar_den;

    es_format_Copy(&filter->fmt_out, &filter->fmt_in);
    filter->fmt_out.video.i_frame_rate *= 2;

    status = mmal_port_format_commit(sys->input);
    if (status != MMAL_SUCCESS) {
        msg_Err(filter, "Failed to commit format for input port %s (status=%"PRIx32" %s)",
                        sys->input->name, status, mmal_status_to_string(status));
        ret = VLC_EGENERIC;
        goto out;
    }
    sys->input->buffer_size = sys->input->buffer_size_recommended;

    sys->input->buffer_num = sys->input->buffer_num_recommended;
    status = mmal_port_enable(sys->input, input_port_cb);
    if (status != MMAL_SUCCESS) {
        msg_Err(filter, "Failed to enable input port %s (status=%"PRIx32" %s)",
                sys->input->name, status, mmal_status_to_string(status));
        ret = VLC_EGENERIC;
        goto out;
    }

    sys->output = sys->component->output[0];
    sys->output->userdata = (struct MMAL_PORT_USERDATA_T *)filter;
    mmal_format_full_copy(sys->output->format, sys->input->format);

    if (filter->fmt_in.i_codec == VLC_CODEC_MMAL_OPAQUE) {
        extra_buffers.hdr.id = MMAL_PARAMETER_EXTRA_BUFFERS;
        extra_buffers.hdr.size = sizeof(MMAL_PARAMETER_UINT32_T);
        extra_buffers.value = NUM_EXTRA_BUFFERS;
        status = mmal_port_parameter_set(sys->output, &extra_buffers.hdr);
        if (status != MMAL_SUCCESS) {
            msg_Err(filter, "Failed to set MMAL_PARAMETER_EXTRA_BUFFERS on output port (status=%"PRIx32" %s)",
                    status, mmal_status_to_string(status));
            ret = VLC_EGENERIC;
            goto out;
        }
    }

    status = mmal_port_format_commit(sys->output);
    if (status != MMAL_SUCCESS) {
        msg_Err(filter, "Failed to commit format for output port %s (status=%"PRIx32" %s)",
                        sys->input->name, status, mmal_status_to_string(status));
        ret = VLC_EGENERIC;
        goto out;
    }

    sys->output->buffer_num = sys->output->buffer_num_recommended;
    status = mmal_port_enable(sys->output, output_port_cb);
    if (status != MMAL_SUCCESS) {
        msg_Err(filter, "Failed to enable output port %s (status=%"PRIx32" %s)",
                sys->output->name, status, mmal_status_to_string(status));
        ret = VLC_EGENERIC;
        goto out;
    }

    status = mmal_component_enable(sys->component);
    if (status != MMAL_SUCCESS) {
        msg_Err(filter, "Failed to enable component %s (status=%"PRIx32" %s)",
                sys->component->name, status, mmal_status_to_string(status));
        ret = VLC_EGENERIC;
        goto out;
    }

    sys->output_pool = mmal_pool_create(sys->output->buffer_num,
            sys->output->buffer_size);
    if (!sys->output_pool) {
        msg_Err(filter, "Failed to create MMAL pool for %u buffers of size %"PRIu32,
                        sys->output->buffer_num, sys->output->buffer_size);
        goto out;
    }
    sys->input_pool = mmal_pool_create(sys->input->buffer_num, 0);
    sys->filtered_pictures = mmal_queue_create();

    ret = create_picture_pool(filter);
    if (ret != VLC_SUCCESS)
        goto out;

    filter->pf_video_filter = deinterlace;
    filter->pf_video_flush = flush;
    vlc_mutex_init_recursive(&sys->mutex);
    vlc_cond_init(&sys->buffer_cond);

out:
    if (ret != VLC_SUCCESS)
        Close(filter);

    return ret;
}

static void Close(filter_t *filter)
{
    filter_sys_t *sys = filter->p_sys;
    MMAL_BUFFER_HEADER_T *buffer;

    if (!sys)
        return;

    if (sys->component && sys->component->control->is_enabled)
        mmal_port_disable(sys->component->control);

    if (sys->input && sys->input->is_enabled)
        mmal_port_disable(sys->input);

    if (sys->output && sys->output->is_enabled)
        mmal_port_disable(sys->output);

    if (sys->component && sys->component->is_enabled)
        mmal_component_disable(sys->component);

    while ((buffer = mmal_queue_get(sys->filtered_pictures))) {
        picture_t *pic = (picture_t *)buffer->user_data;
        picture_Release(pic);

        buffer->user_data = NULL;
        buffer->alloc_size = 0;
        buffer->data = NULL;
        mmal_buffer_header_release(buffer);
    }

    if (sys->filtered_pictures)
        mmal_queue_destroy(sys->filtered_pictures);

    if (sys->input_pool)
        mmal_pool_destroy(sys->input_pool);

    if (sys->output_pool)
        mmal_pool_destroy(sys->output_pool);

    if (sys->component)
        mmal_component_release(sys->component);

    if (sys->picture_pool)
        picture_pool_Release(sys->picture_pool);

    vlc_mutex_destroy(&sys->mutex);
    vlc_cond_destroy(&sys->buffer_cond);
    free(sys->pictures);
    free(sys);

    bcm_host_deinit();
}

static int send_output_buffer(filter_t *filter)
{
    filter_sys_t *sys = filter->p_sys;
    MMAL_BUFFER_HEADER_T *buffer;
    MMAL_STATUS_T status;
    picture_t *picture;
    int ret = 0;

    picture = picture_pool_Get(sys->picture_pool);
    if (!picture) {
        msg_Warn(filter, "Failed to get new picture");
        ret = -1;
        goto out;
    }
    picture->format.i_frame_rate = filter->fmt_out.video.i_frame_rate;
    picture->format.i_frame_rate_base = filter->fmt_out.video.i_frame_rate_base;

    buffer = picture->p_sys->buffer;
    buffer->cmd = 0;
    buffer->alloc_size = sys->output->buffer_size;

    status = mmal_port_send_buffer(sys->output, buffer);
    if (status != MMAL_SUCCESS) {
        msg_Err(filter, "Failed to send buffer to output port (status=%"PRIx32" %s)",
                status, mmal_status_to_string(status));
        picture_Release(picture);
        ret = -1;
        goto out;
    }
    sys->output_in_transit++;

out:
    return ret;
}

static void fill_output_port(filter_t *filter)
{
    filter_sys_t *sys = filter->p_sys;
    /* allow at least 2 buffers in transit */
    unsigned max_buffers_in_transit = __MAX(sys->output->buffer_num,
            MIN_NUM_BUFFERS_IN_TRANSIT);
    unsigned buffers_available = mmal_queue_length(sys->output_pool->queue);
    unsigned buffers_to_send = max_buffers_in_transit - sys->output_in_transit;
    unsigned i;

    if (buffers_to_send > buffers_available)
        buffers_to_send = buffers_available;

#ifndef NDEBUG
    msg_Dbg(filter, "Send %d buffers to output port (available: %d, in_transit: %d, buffer_num: %d)",
                    buffers_to_send, buffers_available, sys->output_in_transit,
                    sys->output->buffer_num);
#endif
    for (i = 0; i < buffers_to_send; ++i) {
        if (send_output_buffer(filter) < 0)
            break;
    }
}

static picture_t *deinterlace(filter_t *filter, picture_t *picture)
{
    filter_sys_t *sys = filter->p_sys;
    MMAL_BUFFER_HEADER_T *buffer;
    picture_t *out_picture = NULL;
    picture_t *ret = NULL;
    MMAL_STATUS_T status;

    /*
     * Send output buffers
     */
    if (sys->output_pool) {
        int i = 0;
        vlc_mutex_lock(&sys->mutex);
        while((buffer = mmal_queue_get(sys->filtered_pictures))) {
            i++;
            if (!out_picture) {
                out_picture = (picture_t *)buffer->user_data;
                ret = out_picture;
            } else {
                out_picture->p_next = (picture_t *)buffer->user_data;
                out_picture = out_picture->p_next;
            }
            out_picture->date = buffer->pts;
        }
        if (out_picture)
            out_picture->p_next = NULL;
        fill_output_port(filter);
        vlc_mutex_unlock(&sys->mutex);
    }

    /*
     * Process input
     */
    if (!picture)
        return ret;

    vlc_mutex_lock(&sys->mutex);
    buffer = mmal_queue_timedwait(sys->input_pool->queue, 2);
    if (!buffer) {
        msg_Err(filter, "Failed to retrieve buffer header for input picture");
        goto out;
    }

    mmal_buffer_header_reset(buffer);
    buffer->user_data = picture;
    buffer->pts = picture->date;
    buffer->cmd = 0;
    buffer->alloc_size = sys->input->buffer_size;
    buffer->length = sys->input->buffer_size;
    buffer->data = picture->p[0].p_pixels;

    status = mmal_port_send_buffer(sys->input, buffer);
    if (status != MMAL_SUCCESS) {
        msg_Err(filter, "Failed to send buffer to input port (status=%"PRIx32" %s)",
                status, mmal_status_to_string(status));
    }
    sys->input_in_transit++;

out:
    vlc_mutex_unlock(&sys->mutex);
    return ret;
}

static void flush(filter_t *filter)
{
    filter_sys_t *sys = filter->p_sys;
    MMAL_STATUS_T status;

    mmal_port_disable(sys->output);
    mmal_port_disable(sys->input);
    mmal_port_flush(sys->output);
    mmal_port_flush(sys->input);
    status = mmal_port_enable(sys->input, input_port_cb);
    if (status != MMAL_SUCCESS) {
        msg_Err(filter, "Failed to enable input port %s (status=%"PRIx32" %s)",
                sys->input->name, status, mmal_status_to_string(status));
        return;
    }
    status = mmal_port_enable(sys->output, output_port_cb);
    if (status != MMAL_SUCCESS) {
        msg_Err(filter, "Failed to enable output port %s (status=%"PRIx32" %s)",
                sys->output->name, status, mmal_status_to_string(status));
    }

    msg_Dbg(filter, "flush: wait for all buffers to be returned");
    vlc_mutex_lock(&sys->mutex);
    while (sys->input_in_transit || sys->output_in_transit)
        vlc_cond_wait(&sys->buffer_cond, &sys->mutex);
    vlc_mutex_unlock(&sys->mutex);
}

static void control_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    filter_t *filter = (filter_t *)port->userdata;
    MMAL_STATUS_T status;

    if (buffer->cmd == MMAL_EVENT_ERROR) {
        status = *(uint32_t *)buffer->data;
        msg_Err(filter, "MMAL error %"PRIx32" \"%s\"", status,
                mmal_status_to_string(status));
    }

    mmal_buffer_header_release(buffer);
}

static void input_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    picture_t *picture = (picture_t *)buffer->user_data;
    filter_t *filter = (filter_t *)port->userdata;
    filter_sys_t *sys = filter->p_sys;

    buffer->user_data = NULL;
    vlc_mutex_lock(&sys->mutex);
    mmal_buffer_header_release(buffer);
    if (picture)
        picture_Release(picture);
    sys->input_in_transit--;
    vlc_cond_signal(&sys->buffer_cond);
    vlc_mutex_unlock(&sys->mutex);
}

static void output_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    filter_t *filter = (filter_t *)port->userdata;
    filter_sys_t *sys = filter->p_sys;
    picture_t *picture;

    vlc_mutex_lock(&sys->mutex);
    if (buffer->cmd == 0) {
        if (buffer->length > 0) {
            mmal_queue_put(sys->filtered_pictures, buffer);
            fill_output_port(filter);
        } else {
            picture = (picture_t *)buffer->user_data;
            picture_Release(picture);
            buffer->user_data = NULL;
        }
        sys->output_in_transit--;
        vlc_cond_signal(&sys->buffer_cond);
    } else if (buffer->cmd == MMAL_EVENT_FORMAT_CHANGED) {
        msg_Warn(filter, "MMAL_EVENT_FORMAT_CHANGED seen but not handled");
        mmal_buffer_header_release(buffer);
    } else {
        mmal_buffer_header_release(buffer);
    }
    vlc_mutex_unlock(&sys->mutex);
}
