/*****************************************************************************
 * mmal.c: MMAL-based decoder plugin for Raspberry Pi
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_threads.h>

#include <bcm_host.h>
#include <interface/mmal/mmal.h>
#include <interface/mmal/util/mmal_util.h>
#include <interface/mmal/util/mmal_default_components.h>

/* This value must match the define in video_output/mmal.c
 * Think twice before changing this. Incorrect values cause havoc.
 */
#define NUM_ACTUAL_OPAQUE_BUFFERS 40

#define NUM_EXTRA_BUFFERS 10
#define NUM_OPAQUE_BUFFERS 20

#define MIN_NUM_BUFFERS_IN_TRANSIT 2

#define MMAL_ZEROCOPY_NAME "mmal-zerocopy"
#define MMAL_ZEROCOPY_TEXT N_("Decode frames directly into RPI VideoCore instead of host memory.")
#define MMAL_ZEROCOPY_LONGTEXT N_("Decode frames directly into RPI VideoCore instead of host memory. This option must only be used with the MMAL video output plugin.")

static int OpenDecoder(decoder_t *dec);
static void CloseDecoder(decoder_t *dec);

vlc_module_begin()
    set_shortname(N_("MMAL decoder"))
    set_description(N_("MMAL-based decoder plugin for Raspberry Pi"))
    set_capability("decoder", 90)
    add_shortcut("mmal_decoder")
    add_bool(MMAL_ZEROCOPY_NAME, true, MMAL_ZEROCOPY_TEXT, MMAL_ZEROCOPY_LONGTEXT, false)
    set_callbacks(OpenDecoder, CloseDecoder)
vlc_module_end()

struct decoder_sys_t {
    bool opaque;
    MMAL_COMPONENT_T *component;
    MMAL_PORT_T *input;
    MMAL_POOL_T *input_pool;
    MMAL_PORT_T *output;
    MMAL_POOL_T *output_pool;
    MMAL_ES_FORMAT_T *output_format;
    MMAL_QUEUE_T *decoded_pictures;
    vlc_mutex_t mutex;
};

/* Utilities */
static int change_output_format(decoder_t *dec);
static int send_output_buffer(decoder_t *dec);
static void fill_output_port(decoder_t *dec);

/* VLC decoder callback */
static picture_t *decode(decoder_t *dec, block_t **block);

/* MMAL callbacks */
static void control_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
static void input_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
static void output_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);

static int OpenDecoder(decoder_t *dec)
{
    int ret = VLC_SUCCESS;
    decoder_sys_t *sys;
    MMAL_PARAMETER_BOOLEAN_T error_concealment;
    MMAL_PARAMETER_UINT32_T extra_buffers;
    MMAL_STATUS_T status;

    if (dec->fmt_in.i_cat != VIDEO_ES)
        return VLC_EGENERIC;

    if (dec->fmt_in.i_codec != VLC_CODEC_MPGV &&
            dec->fmt_in.i_codec != VLC_CODEC_H264)
        return VLC_EGENERIC;

    sys = calloc(1, sizeof(decoder_sys_t));
    if (!sys) {
        ret = VLC_ENOMEM;
        goto out;
    }
    dec->p_sys = sys;
    dec->b_need_packetized = true;

    sys->opaque = var_InheritBool(dec, MMAL_ZEROCOPY_NAME);
    bcm_host_init();

    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_DECODER, &sys->component);
    if (status != MMAL_SUCCESS) {
        msg_Err(dec, "Failed to create MMAL component %s (status=%"PRIx32" %s)",
                MMAL_COMPONENT_DEFAULT_VIDEO_DECODER, status, mmal_status_to_string(status));
        ret = VLC_EGENERIC;
        goto out;
    }

    sys->component->control->userdata = (struct MMAL_PORT_USERDATA_T *)dec;
    status = mmal_port_enable(sys->component->control, control_port_cb);
    if (status != MMAL_SUCCESS) {
        msg_Err(dec, "Failed to enable control port %s (status=%"PRIx32" %s)",
                sys->component->control->name, status, mmal_status_to_string(status));
        ret = VLC_EGENERIC;
        goto out;
    }

    sys->input = sys->component->input[0];
    sys->input->userdata = (struct MMAL_PORT_USERDATA_T *)dec;
    if (dec->fmt_in.i_codec == VLC_CODEC_MPGV)
        sys->input->format->encoding = MMAL_ENCODING_MP2V;
    else
        sys->input->format->encoding = MMAL_ENCODING_H264;

    if (dec->fmt_in.i_codec == VLC_CODEC_H264) {
        if (dec->fmt_in.i_extra > 0) {
            status = mmal_format_extradata_alloc(sys->input->format,
                    dec->fmt_in.i_extra);
            if (status == MMAL_SUCCESS) {
                memcpy(sys->input->format->extradata, dec->fmt_in.p_extra,
                        dec->fmt_in.i_extra);
                sys->input->format->extradata_size = dec->fmt_in.i_extra;
            } else {
                msg_Err(dec, "Failed to allocate extra format data on input port %s (status=%"PRIx32" %s)",
                        sys->input->name, status, mmal_status_to_string(status));
            }
        } else {
            error_concealment.hdr.id = MMAL_PARAMETER_VIDEO_DECODE_ERROR_CONCEALMENT;
            error_concealment.hdr.size = sizeof(MMAL_PARAMETER_BOOLEAN_T);
            error_concealment.enable = MMAL_FALSE;
            status = mmal_port_parameter_set(sys->input, &error_concealment.hdr);
            if (status != MMAL_SUCCESS)
                msg_Err(dec, "Failed to disable error concealment (status=%"PRIx32" %s)",
                        status, mmal_status_to_string(status));
        }
    }

    status = mmal_port_format_commit(sys->input);
    if (status != MMAL_SUCCESS) {
        msg_Err(dec, "Failed to commit format for input port %s (status=%"PRIx32" %s)",
                sys->input->name, status, mmal_status_to_string(status));
        ret = VLC_EGENERIC;
        goto out;
    }
    sys->input->buffer_size = sys->input->buffer_size_recommended;
    sys->input->buffer_num = __MAX(sys->input->buffer_num_recommended, 80);

    status = mmal_port_enable(sys->input, input_port_cb);
    if (status != MMAL_SUCCESS) {
        msg_Err(dec, "Failed to enable input port %s (status=%"PRIx32" %s)",
                sys->input->name, status, mmal_status_to_string(status));
        ret = VLC_EGENERIC;
        goto out;
    }

    sys->output = sys->component->output[0];
    sys->output->userdata = (struct MMAL_PORT_USERDATA_T *)dec;

    if (sys->opaque) {
        extra_buffers.hdr.id = MMAL_PARAMETER_EXTRA_BUFFERS;
        extra_buffers.hdr.size = sizeof(MMAL_PARAMETER_UINT32_T);
        extra_buffers.value = NUM_ACTUAL_OPAQUE_BUFFERS - NUM_OPAQUE_BUFFERS;
        status = mmal_port_parameter_set(sys->output, &extra_buffers.hdr);
        if (status != MMAL_SUCCESS) {
            msg_Err(dec, "Failed to set MMAL_PARAMETER_EXTRA_BUFFERS on output port (status=%"PRIx32" %s)",
                    status, mmal_status_to_string(status));
            ret = VLC_EGENERIC;
            goto out;
        }
    }

    status = mmal_port_enable(sys->output, output_port_cb);
    if (status != MMAL_SUCCESS) {
        msg_Err(dec, "Failed to enable output port %s (status=%"PRIx32" %s)",
                sys->output->name, status, mmal_status_to_string(status));
        ret = VLC_EGENERIC;
        goto out;
    }

    status = mmal_component_enable(sys->component);
    if (status != MMAL_SUCCESS) {
        msg_Err(dec, "Failed to enable component %s (status=%"PRIx32" %s)",
                sys->component->name, status, mmal_status_to_string(status));
        ret = VLC_EGENERIC;
        goto out;
    }

    sys->input_pool = mmal_pool_create(sys->input->buffer_num, 0);
    sys->decoded_pictures = mmal_queue_create();

    if (sys->opaque) {
        dec->fmt_out.i_codec = VLC_CODEC_MMAL_OPAQUE;
        dec->fmt_out.video.i_chroma = VLC_CODEC_MMAL_OPAQUE;
    } else {
        dec->fmt_out.i_codec = VLC_CODEC_I420;
        dec->fmt_out.video.i_chroma = VLC_CODEC_I420;
    }

    dec->fmt_out.i_cat = VIDEO_ES;
    dec->pf_decode_video = decode;

    vlc_mutex_init_recursive(&sys->mutex);

out:
    if (ret != VLC_SUCCESS)
        CloseDecoder(dec);

    return ret;
}

static void CloseDecoder(decoder_t *dec)
{
    decoder_sys_t *sys = dec->p_sys;
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

    if (sys->input_pool)
        mmal_pool_destroy(sys->input_pool);

    if (sys->output_format)
        mmal_format_free(sys->output_format);

    /* Free pictures which are decoded but have not yet been sent
     * out to the core */
    while (buffer = mmal_queue_get(sys->decoded_pictures)) {
        picture_t *pic = (picture_t *)buffer->user_data;
        picture_Release(pic);

        buffer->user_data = NULL;
        buffer->alloc_size = 0;
        buffer->data = NULL;
        mmal_buffer_header_release(buffer);
    }

    if (sys->decoded_pictures)
        mmal_queue_destroy(sys->decoded_pictures);

    if (sys->output_pool)
        mmal_pool_destroy(sys->output_pool);

    if (sys->component)
        mmal_component_release(sys->component);

    free(sys);

    bcm_host_deinit();
}

static int change_output_format(decoder_t *dec)
{
    decoder_sys_t *sys = dec->p_sys;
    MMAL_STATUS_T status;
    int pool_size;
    int ret = 0;

    status = mmal_port_disable(sys->output);
    if (status != MMAL_SUCCESS) {
        msg_Err(dec, "Failed to disable output port (status=%"PRIx32" %s)",
                status, mmal_status_to_string(status));
        ret = -1;
        goto out;
    }

    mmal_format_full_copy(sys->output->format, sys->output_format);
    status = mmal_port_format_commit(sys->output);
    if (status != MMAL_SUCCESS) {
        msg_Err(dec, "Failed to commit output format (status=%"PRIx32" %s)",
                status, mmal_status_to_string(status));
        ret = -1;
        goto out;
    }

    if (sys->opaque) {
        sys->output->buffer_num = NUM_ACTUAL_OPAQUE_BUFFERS;
        pool_size = sys->output->buffer_num_recommended + NUM_EXTRA_BUFFERS;
    } else {
        sys->output->buffer_num = __MAX(sys->output->buffer_num_recommended,
                MIN_NUM_BUFFERS_IN_TRANSIT);
        pool_size = sys->output->buffer_num + NUM_EXTRA_BUFFERS;
    }

    sys->output->buffer_size = sys->output->buffer_size_recommended;

    status = mmal_port_enable(sys->output, output_port_cb);
    if (status != MMAL_SUCCESS) {
        msg_Err(dec, "Failed to enable output port (status=%"PRIx32" %s)",
                status, mmal_status_to_string(status));
        ret = -1;
        goto out;
    }

    if (!sys->output_pool) {
        sys->output_pool = mmal_pool_create(pool_size, 0);
        msg_Dbg(dec, "Created output pool with %d pictures", sys->output_pool->headers_num);

        dec->i_extra_picture_buffers = sys->output_pool->headers_num;

        if (dec->fmt_in.i_codec == VLC_CODEC_H264)
            dec->i_extra_picture_buffers -= 16;
        else
            dec->i_extra_picture_buffers -= 4;

        dec->i_extra_picture_buffers = __MAX(dec->i_extra_picture_buffers,
                MIN_NUM_BUFFERS_IN_TRANSIT + 1);
        msg_Dbg(dec, "Request %d extra pictures", dec->i_extra_picture_buffers);
    }

    dec->fmt_out.video.i_width = sys->output->format->es->video.width;
    dec->fmt_out.video.i_height = sys->output->format->es->video.height;
    dec->fmt_out.video.i_x_offset = sys->output->format->es->video.crop.x;
    dec->fmt_out.video.i_y_offset = sys->output->format->es->video.crop.y;
    dec->fmt_out.video.i_visible_width = sys->output->format->es->video.crop.width;
    dec->fmt_out.video.i_visible_height = sys->output->format->es->video.crop.height;
    dec->fmt_out.video.i_sar_num = sys->output->format->es->video.par.num;
    dec->fmt_out.video.i_sar_den = sys->output->format->es->video.par.den;
    dec->fmt_out.video.i_frame_rate = sys->output->format->es->video.frame_rate.num;
    dec->fmt_out.video.i_frame_rate_base = sys->output->format->es->video.frame_rate.den;


out:
    mmal_format_free(sys->output_format);
    sys->output_format = NULL;

    return ret;
}

static int send_output_buffer(decoder_t *dec)
{
    decoder_sys_t *sys = dec->p_sys;
    MMAL_BUFFER_HEADER_T *buffer;
    picture_t *picture;
    MMAL_STATUS_T status;
    int ret = 0;

    buffer = mmal_queue_get(sys->output_pool->queue);
    if (!buffer) {
        msg_Warn(dec, "Failed to get new buffer");
        ret = -1;
        goto out;
    }

    picture = decoder_NewPicture(dec);
    if (!picture) {
        msg_Warn(dec, "Failed to get new picture");
        mmal_buffer_header_release(buffer);
        ret = -1;
        goto out;
    }

    mmal_buffer_header_reset(buffer);
    buffer->user_data = picture;
    buffer->cmd = 0;
    buffer->alloc_size = sys->output->buffer_size;
    buffer->data = picture->p[0].p_pixels;

    status = mmal_port_send_buffer(sys->output, buffer);
    if (status != MMAL_SUCCESS) {
        msg_Err(dec, "Failed to send buffer to output port (status=%"PRIx32" %s)",
                status, mmal_status_to_string(status));
        mmal_buffer_header_release(buffer);
        decoder_DeletePicture(dec, picture);
        ret = -1;
        goto out;
    }

out:
    return ret;
}

static void fill_output_port(decoder_t *dec)
{
    decoder_sys_t *sys = dec->p_sys;
    /* allow at least 2 buffers in transit */
    unsigned max_buffers_in_transit = __MAX(sys->output->buffer_num_recommended,
            MIN_NUM_BUFFERS_IN_TRANSIT);
    unsigned buffers_available = mmal_queue_length(sys->output_pool->queue);
    unsigned buffers_in_transit = sys->output_pool->headers_num - buffers_available -
        mmal_queue_length(sys->decoded_pictures);
    unsigned buffers_to_send = max_buffers_in_transit - buffers_in_transit;
    unsigned i;

    if (buffers_to_send > buffers_available)
        buffers_to_send = buffers_available;

#ifndef NDEBUG
    msg_Dbg(dec, "Send %d buffers to output port (available: %d, in_transit: %d, buffer_num: %d)",
                    buffers_to_send, buffers_available, buffers_in_transit,
                    sys->output->buffer_num);
#endif
    for (i = 0; i < buffers_to_send; ++i)
        if (send_output_buffer(dec) < 0)
            break;
}

static picture_t *decode(decoder_t *dec, block_t **pblock)
{
    decoder_sys_t *sys = dec->p_sys;
    block_t *block;
    MMAL_BUFFER_HEADER_T *buffer;
    uint32_t len;
    uint32_t flags = 0;
    MMAL_STATUS_T status;
    picture_t *ret = NULL;

    /*
     * Configure output port if necessary
     */
    if (sys->output_format) {
        vlc_mutex_lock(&sys->mutex);
        if (change_output_format(dec) < 0)
            msg_Err(dec, "Failed to change output port format");
        vlc_mutex_unlock(&sys->mutex);
    }

    /*
     * Send output buffers
     */
    if (sys->output_pool) {
        vlc_mutex_lock(&sys->mutex);
        buffer = mmal_queue_get(sys->decoded_pictures);
        if (buffer) {
            ret = (picture_t *)buffer->user_data;
            ret->date = buffer->pts;

            buffer->user_data = NULL;
            buffer->alloc_size = 0;
            buffer->data = NULL;
            mmal_buffer_header_release(buffer);
        }

        fill_output_port(dec);
        vlc_mutex_unlock(&sys->mutex);
    }

    if (ret)
        goto out;

    /*
     * Process input
     */
    if (!pblock)
        goto out;

    block = *pblock;

    if (!block)
        goto out;

    *pblock = NULL;

    if (block->i_flags & BLOCK_FLAG_CORRUPTED)
        flags |= MMAL_BUFFER_HEADER_FLAG_CORRUPTED;

    if (block->i_flags & BLOCK_FLAG_DISCONTINUITY)
        flags |= MMAL_BUFFER_HEADER_FLAG_DISCONTINUITY;

    vlc_mutex_lock(&sys->mutex);
    while (block->i_buffer > 0) {
        buffer = mmal_queue_timedwait(sys->input_pool->queue, 2);
        if (!buffer) {
            msg_Err(dec, "Failed to retrieve buffer header for input data");
            break;
        }
        mmal_buffer_header_reset(buffer);
        buffer->cmd = 0;
        buffer->pts = block->i_pts;
        buffer->dts = block->i_dts;
        buffer->alloc_size = sys->input->buffer_size;

        len = block->i_buffer;
        if (len > buffer->alloc_size)
            len = buffer->alloc_size;

        buffer->data = block->p_buffer;
        block->p_buffer += len;
        block->i_buffer -= len;
        buffer->length = len;
        if (block->i_buffer == 0)
            buffer->user_data = block;
        buffer->flags = flags;

        status = mmal_port_send_buffer(sys->input, buffer);
        if (status != MMAL_SUCCESS) {
            msg_Err(dec, "Failed to send buffer to input port (status=%"PRIx32" %s)",
                    status, mmal_status_to_string(status));
            break;
        }
    }
    vlc_mutex_unlock(&sys->mutex);

out:
    return ret;
}

static void control_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    decoder_t *dec = (decoder_t *)port->userdata;
    MMAL_STATUS_T status;

    if (buffer->cmd == MMAL_EVENT_ERROR) {
        status = *(uint32_t *)buffer->data;
        msg_Err(dec, "MMAL error %"PRIx32" \"%s\"", status,
                mmal_status_to_string(status));
    }

    mmal_buffer_header_release(buffer);
}

static void input_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    block_t *block = (block_t *)buffer->user_data;
    decoder_t *dec = (decoder_t *)port->userdata;
    decoder_sys_t *sys = dec->p_sys;
    buffer->user_data = NULL;

    mmal_buffer_header_release(buffer);
    vlc_mutex_lock(&sys->mutex);
    if (block)
        block_Release(block);
    vlc_mutex_unlock(&sys->mutex);
}

static void output_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    decoder_t *dec = (decoder_t *)port->userdata;
    decoder_sys_t *sys = dec->p_sys;
    picture_t *picture;
    MMAL_EVENT_FORMAT_CHANGED_T *fmt;
    MMAL_ES_FORMAT_T *format;

    vlc_mutex_lock(&sys->mutex);
    if (buffer->cmd == 0) {
        if (buffer->length > 0) {
            mmal_queue_put(sys->decoded_pictures, buffer);
            fill_output_port(dec);
        } else {
            picture = (picture_t *)buffer->user_data;
            decoder_DeletePicture(dec, picture);
            buffer->user_data = NULL;
            buffer->alloc_size = 0;
            buffer->data = NULL;
            mmal_buffer_header_release(buffer);
        }
    } else if (buffer->cmd == MMAL_EVENT_FORMAT_CHANGED) {
        fmt = mmal_event_format_changed_get(buffer);

        format = mmal_format_alloc();
        mmal_format_full_copy(format, fmt->format);

        if (sys->opaque)
            format->encoding = MMAL_ENCODING_OPAQUE;

        sys->output_format = format;

        mmal_buffer_header_release(buffer);
    } else {
        mmal_buffer_header_release(buffer);
    }
    vlc_mutex_unlock(&sys->mutex);
}
