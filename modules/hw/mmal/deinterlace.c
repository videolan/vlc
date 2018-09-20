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

#include <stdatomic.h>

#include <vlc_common.h>
#include <vlc_picture_pool.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>

#include "mmal_picture.h"

#include <bcm_host.h>
#include <interface/mmal/mmal.h>
#include <interface/mmal/util/mmal_util.h>
#include <interface/mmal/util/mmal_default_components.h>

#define MIN_NUM_BUFFERS_IN_TRANSIT 2

#define MMAL_DEINTERLACE_QPU "mmal-deinterlace-adv-qpu"
#define MMAL_DEINTERLACE_QPU_TEXT N_("Use QPUs for advanced HD deinterlacing.")
#define MMAL_DEINTERLACE_QPU_LONGTEXT N_("Make use of the QPUs to allow higher quality deinterlacing of HD content.")

static int Open(filter_t *filter);
static void Close(filter_t *filter);

vlc_module_begin()
    set_shortname(N_("MMAL deinterlace"))
    set_description(N_("MMAL-based deinterlace filter plugin"))
    set_capability("video filter", 0)
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VFILTER)
    set_callbacks(Open, Close)
    add_shortcut("deinterlace")
    add_bool(MMAL_DEINTERLACE_QPU, false, MMAL_DEINTERLACE_QPU_TEXT,
                    MMAL_DEINTERLACE_QPU_LONGTEXT, true);
vlc_module_end()

typedef struct
{
    MMAL_COMPONENT_T *component;
    MMAL_PORT_T *input;
    MMAL_PORT_T *output;

    MMAL_QUEUE_T *filtered_pictures;
    vlc_sem_t sem;

    atomic_bool started;

    /* statistics */
    int output_in_transit;
    int input_in_transit;
} filter_sys_t;

static void control_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
static void input_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
static void output_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
static picture_t *deinterlace(filter_t *filter, picture_t *picture);
static void flush(filter_t *filter);

#define MMAL_COMPONENT_DEFAULT_DEINTERLACE "vc.ril.image_fx"

static int Open(filter_t *filter)
{
    int32_t frame_duration = filter->fmt_in.video.i_frame_rate != 0 ?
            vlc_tick_from_samples( filter->fmt_in.video.i_frame_rate_base,
            filter->fmt_in.video.i_frame_rate ) : 0;
    bool use_qpu = var_InheritBool(filter, MMAL_DEINTERLACE_QPU);

    MMAL_PARAMETER_IMAGEFX_PARAMETERS_T imfx_param = {
            { MMAL_PARAMETER_IMAGE_EFFECT_PARAMETERS, sizeof(imfx_param) },
            MMAL_PARAM_IMAGEFX_DEINTERLACE_ADV,
            4,
            { 3, frame_duration, 0, use_qpu }
    };

    int ret = VLC_SUCCESS;
    MMAL_STATUS_T status;
    filter_sys_t *sys;

    msg_Dbg(filter, "Try to open mmal_deinterlace filter. frame_duration: %d, QPU %s!",
            frame_duration, use_qpu ? "used" : "unused");

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

    if (filter->fmt_in.i_codec == VLC_CODEC_MMAL_OPAQUE) {
        MMAL_PARAMETER_BOOLEAN_T zero_copy = {
            { MMAL_PARAMETER_ZERO_COPY, sizeof(MMAL_PARAMETER_BOOLEAN_T) },
            1
        };

        status = mmal_port_parameter_set(sys->input, &zero_copy.hdr);
        if (status != MMAL_SUCCESS) {
           msg_Err(filter, "Failed to set zero copy on port %s (status=%"PRIx32" %s)",
                    sys->input->name, status, mmal_status_to_string(status));
           goto out;
        }
    }

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

    status = mmal_port_format_commit(sys->output);
    if (status != MMAL_SUCCESS) {
        msg_Err(filter, "Failed to commit format for output port %s (status=%"PRIx32" %s)",
                        sys->input->name, status, mmal_status_to_string(status));
        ret = VLC_EGENERIC;
        goto out;
    }

    sys->output->buffer_num = 3;

    if (filter->fmt_in.i_codec == VLC_CODEC_MMAL_OPAQUE) {
        MMAL_PARAMETER_UINT32_T extra_buffers = {
            { MMAL_PARAMETER_EXTRA_BUFFERS, sizeof(MMAL_PARAMETER_UINT32_T) },
            5
        };
        status = mmal_port_parameter_set(sys->output, &extra_buffers.hdr);
        if (status != MMAL_SUCCESS) {
            msg_Err(filter, "Failed to set MMAL_PARAMETER_EXTRA_BUFFERS on output port (status=%"PRIx32" %s)",
                    status, mmal_status_to_string(status));
            goto out;
        }

        MMAL_PARAMETER_BOOLEAN_T zero_copy = {
            { MMAL_PARAMETER_ZERO_COPY, sizeof(MMAL_PARAMETER_BOOLEAN_T) },
            1
        };

        status = mmal_port_parameter_set(sys->output, &zero_copy.hdr);
        if (status != MMAL_SUCCESS) {
           msg_Err(filter, "Failed to set zero copy on port %s (status=%"PRIx32" %s)",
                    sys->output->name, status, mmal_status_to_string(status));
           goto out;
        }
    }

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

    sys->filtered_pictures = mmal_queue_create();

    filter->pf_video_filter = deinterlace;
    filter->pf_flush = flush;

    vlc_sem_init(&sys->sem, 0);

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
    }

    if (sys->filtered_pictures)
        mmal_queue_destroy(sys->filtered_pictures);

    if (sys->component)
        mmal_component_release(sys->component);

    vlc_sem_destroy(&sys->sem);
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

    if (!sys->output->is_enabled) {
        ret = VLC_EGENERIC;
        goto out;
    }

    picture = filter_NewPicture(filter);
    if (!picture) {
        msg_Warn(filter, "Failed to get new picture");
        ret = -1;
        goto out;
    }
    picture->format.i_frame_rate = filter->fmt_out.video.i_frame_rate;
    picture->format.i_frame_rate_base = filter->fmt_out.video.i_frame_rate_base;

    buffer = picture->p_sys->buffer;
    buffer->user_data = picture;
    buffer->cmd = 0;

    mmal_picture_lock(picture);

    status = mmal_port_send_buffer(sys->output, buffer);
    if (status != MMAL_SUCCESS) {
        msg_Err(filter, "Failed to send buffer to output port (status=%"PRIx32" %s)",
                status, mmal_status_to_string(status));
        mmal_buffer_header_release(buffer);
        picture_Release(picture);
        ret = -1;
    } else {
        atomic_fetch_add(&sys->output_in_transit, 1);
        vlc_sem_post(&sys->sem);
    }

out:
    return ret;
}

static void fill_output_port(filter_t *filter)
{
    filter_sys_t *sys = filter->p_sys;
    /* allow at least 2 buffers in transit */
    unsigned max_buffers_in_transit = __MAX(2, MIN_NUM_BUFFERS_IN_TRANSIT);
    int buffers_available = sys->output->buffer_num -
        atomic_load(&sys->output_in_transit) -
        mmal_queue_length(sys->filtered_pictures);
    int buffers_to_send = max_buffers_in_transit - sys->output_in_transit;
    int i;

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
    unsigned i = 0;

    fill_output_port(filter);

    buffer = picture->p_sys->buffer;
    buffer->user_data = picture;
    buffer->pts = picture->date;
    buffer->cmd = 0;

    if (!picture->p_sys->displayed) {
        status = mmal_port_send_buffer(sys->input, buffer);
        if (status != MMAL_SUCCESS) {
            msg_Err(filter, "Failed to send buffer to input port (status=%"PRIx32" %s)",
                    status, mmal_status_to_string(status));
            picture_Release(picture);
        } else {
            picture->p_sys->displayed = true;
            atomic_fetch_add(&sys->input_in_transit, 1);
            vlc_sem_post(&sys->sem);
        }
    } else {
        picture_Release(picture);
    }

    /*
     * Send output buffers
     */
    while(atomic_load(&sys->started) && i < 2) {
        if (buffer = mmal_queue_timedwait(sys->filtered_pictures, 2000)) {
            i++;
            if (!out_picture) {
                out_picture = (picture_t *)buffer->user_data;
                ret = out_picture;
            } else {
                out_picture->p_next = (picture_t *)buffer->user_data;
                out_picture = out_picture->p_next;
            }
            out_picture->date = buffer->pts;
        } else {
            msg_Dbg(filter, "Failed waiting for filtered picture");
            break;
        }
    }
    if (out_picture)
        out_picture->p_next = NULL;

    return ret;
}

static void flush(filter_t *filter)
{
    filter_sys_t *sys = filter->p_sys;
    MMAL_BUFFER_HEADER_T *buffer;

    msg_Dbg(filter, "flush deinterlace filter");

    msg_Dbg(filter, "flush: flush ports (input: %d, output: %d in transit)",
            sys->input_in_transit, sys->output_in_transit);
    mmal_port_flush(sys->output);
    mmal_port_flush(sys->input);

    msg_Dbg(filter, "flush: wait for all buffers to be returned");
    while (atomic_load(&sys->input_in_transit) ||
            atomic_load(&sys->output_in_transit))
        vlc_sem_wait(&sys->sem);

    while ((buffer = mmal_queue_get(sys->filtered_pictures))) {
        picture_t *pic = (picture_t *)buffer->user_data;
        msg_Dbg(filter, "flush: release already filtered pic %p",
                (void *)pic);
        picture_Release(pic);
    }
    atomic_store(&sys->started, false);
    msg_Dbg(filter, "flush: done");
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

    if (picture) {
        picture_Release(picture);
    } else {
        msg_Warn(filter, "Got buffer without picture on input port - OOOPS");
        mmal_buffer_header_release(buffer);
    }

    atomic_fetch_sub(&sys->input_in_transit, 1);
    vlc_sem_post(&sys->sem);
}

static void output_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    filter_t *filter = (filter_t *)port->userdata;
    filter_sys_t *sys = filter->p_sys;
    picture_t *picture;

    if (buffer->cmd == 0) {
        if (buffer->length > 0) {
            atomic_store(&sys->started, true);
            mmal_queue_put(sys->filtered_pictures, buffer);
            picture = (picture_t *)buffer->user_data;
        } else {
            picture = (picture_t *)buffer->user_data;
            picture_Release(picture);
        }

        atomic_fetch_sub(&sys->output_in_transit, 1);
        vlc_sem_post(&sys->sem);
    } else if (buffer->cmd == MMAL_EVENT_FORMAT_CHANGED) {
        msg_Warn(filter, "MMAL_EVENT_FORMAT_CHANGED seen but not handled");
        mmal_buffer_header_release(buffer);
    } else {
        mmal_buffer_header_release(buffer);
    }
}
