/*****************************************************************************
 * mmal.c: MMAL-based decoder plugin for Raspberry Pi
 *****************************************************************************
 * Copyright © 2014 jusst technologies GmbH
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

#include <stdatomic.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_threads.h>

#include <bcm_host.h>
#include <interface/mmal/mmal.h>
#include <interface/mmal/util/mmal_util.h>
#include <interface/mmal/util/mmal_default_components.h>

#include "mmal_picture.h"

/*
 * This seems to be a bit high, but reducing it causes instabilities
 */
#define NUM_EXTRA_BUFFERS 5
#define NUM_DECODER_BUFFER_HEADERS 30

#define MIN_NUM_BUFFERS_IN_TRANSIT 2

#define MMAL_OPAQUE_NAME "mmal-opaque"
#define MMAL_OPAQUE_TEXT N_("Decode frames directly into RPI VideoCore instead of host memory.")
#define MMAL_OPAQUE_LONGTEXT N_("Decode frames directly into RPI VideoCore instead of host memory. This option must only be used with the MMAL video output plugin.")

static int OpenDecoder(decoder_t *dec);
static void CloseDecoder(decoder_t *dec);

vlc_module_begin()
    set_shortname(N_("MMAL decoder"))
    set_description(N_("MMAL-based decoder plugin for Raspberry Pi"))
    set_capability("video decoder", 90)
    add_shortcut("mmal_decoder")
    add_bool(MMAL_OPAQUE_NAME, true, MMAL_OPAQUE_TEXT, MMAL_OPAQUE_LONGTEXT, false)
    set_callbacks(OpenDecoder, CloseDecoder)
vlc_module_end()

typedef struct
{
    bool opaque;
    MMAL_COMPONENT_T *component;
    MMAL_PORT_T *input;
    MMAL_POOL_T *input_pool;
    MMAL_PORT_T *output;
    MMAL_POOL_T *output_pool; /* only used for non-opaque mode */
    MMAL_ES_FORMAT_T *output_format;
    vlc_sem_t sem;

    bool b_top_field_first;
    bool b_progressive;

    /* statistics */
    int output_in_transit;
    int input_in_transit;
    atomic_bool started;
} decoder_sys_t;

/* Utilities */
static int change_output_format(decoder_t *dec);
static int send_output_buffer(decoder_t *dec);
static void fill_output_port(decoder_t *dec);

/* VLC decoder callback */
static int decode(decoder_t *dec, block_t *block);
static void flush_decoder(decoder_t *dec);

/* MMAL callbacks */
static void control_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
static void input_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
static void output_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);

static int OpenDecoder(decoder_t *dec)
{
    int ret = VLC_SUCCESS;
    decoder_sys_t *sys;
    MMAL_PARAMETER_UINT32_T extra_buffers;
    MMAL_STATUS_T status;

    if (dec->fmt_in.i_codec != VLC_CODEC_MPGV &&
            dec->fmt_in.i_codec != VLC_CODEC_H264)
        return VLC_EGENERIC;

    sys = calloc(1, sizeof(decoder_sys_t));
    if (!sys) {
        ret = VLC_ENOMEM;
        goto out;
    }
    dec->p_sys = sys;

    sys->opaque = var_InheritBool(dec, MMAL_OPAQUE_NAME);
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
    sys->input->buffer_num = sys->input->buffer_num_recommended;

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
        extra_buffers.value = NUM_EXTRA_BUFFERS;
        status = mmal_port_parameter_set(sys->output, &extra_buffers.hdr);
        if (status != MMAL_SUCCESS) {
            msg_Err(dec, "Failed to set MMAL_PARAMETER_EXTRA_BUFFERS on output port (status=%"PRIx32" %s)",
                    status, mmal_status_to_string(status));
            ret = VLC_EGENERIC;
            goto out;
        }

        msg_Dbg(dec, "Activate zero-copy for output port");
        MMAL_PARAMETER_BOOLEAN_T zero_copy = {
            { MMAL_PARAMETER_ZERO_COPY, sizeof(MMAL_PARAMETER_BOOLEAN_T) },
            1
        };

        status = mmal_port_parameter_set(sys->output, &zero_copy.hdr);
        if (status != MMAL_SUCCESS) {
           msg_Err(dec, "Failed to set zero copy on port %s (status=%"PRIx32" %s)",
                    sys->output->name, status, mmal_status_to_string(status));
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

    if (sys->opaque) {
        dec->fmt_out.i_codec = VLC_CODEC_MMAL_OPAQUE;
        dec->fmt_out.video.i_chroma = VLC_CODEC_MMAL_OPAQUE;
    } else {
        dec->fmt_out.i_codec = VLC_CODEC_I420;
        dec->fmt_out.video.i_chroma = VLC_CODEC_I420;
    }

    dec->pf_decode = decode;
    dec->pf_flush  = flush_decoder;

    vlc_sem_init(&sys->sem, 0);

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

    if (sys->output_pool)
        mmal_pool_destroy(sys->output_pool);

    if (sys->component)
        mmal_component_release(sys->component);

    vlc_sem_destroy(&sys->sem);
    free(sys);

    bcm_host_deinit();
}

static int change_output_format(decoder_t *dec)
{
    MMAL_PARAMETER_VIDEO_INTERLACE_TYPE_T interlace_type;
    decoder_sys_t *sys = dec->p_sys;
    MMAL_STATUS_T status;
    int pool_size;
    int ret = 0;

    if (atomic_load(&sys->started)) {
        mmal_format_full_copy(sys->output->format, sys->output_format);
        status = mmal_port_format_commit(sys->output);
        if (status != MMAL_SUCCESS) {
            msg_Err(dec, "Failed to commit output format (status=%"PRIx32" %s)",
                    status, mmal_status_to_string(status));
            ret = -1;
            goto port_reset;
        }
        goto apply_fmt;
    }

port_reset:
    msg_Dbg(dec, "%s: Do full port reset", __func__);
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
        sys->output->buffer_num = NUM_DECODER_BUFFER_HEADERS;
        pool_size = NUM_DECODER_BUFFER_HEADERS;
    } else {
        sys->output->buffer_num = __MAX(sys->output->buffer_num_recommended,
                MIN_NUM_BUFFERS_IN_TRANSIT);
        pool_size = sys->output->buffer_num;
    }

    sys->output->buffer_size = sys->output->buffer_size_recommended;

    status = mmal_port_enable(sys->output, output_port_cb);
    if (status != MMAL_SUCCESS) {
        msg_Err(dec, "Failed to enable output port (status=%"PRIx32" %s)",
                status, mmal_status_to_string(status));
        ret = -1;
        goto out;
    }

    if (!atomic_load(&sys->started)) {
        if (!sys->opaque) {
            sys->output_pool = mmal_port_pool_create(sys->output, pool_size, 0);
            msg_Dbg(dec, "Created output pool with %d pictures", sys->output_pool->headers_num);
        }

        atomic_store(&sys->started, true);

        /* we need one picture from vout for each buffer header on the output
         * port */
        dec->i_extra_picture_buffers = pool_size;

        /* remove what VLC core reserves as it is part of the pool_size
         * already */
        if (dec->fmt_in.i_codec == VLC_CODEC_H264)
            dec->i_extra_picture_buffers -= 19;
        else
            dec->i_extra_picture_buffers -= 3;

        msg_Dbg(dec, "Request %d extra pictures", dec->i_extra_picture_buffers);
    }

apply_fmt:
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

    /* Query interlaced type */
    interlace_type.hdr.id = MMAL_PARAMETER_VIDEO_INTERLACE_TYPE;
    interlace_type.hdr.size = sizeof(MMAL_PARAMETER_VIDEO_INTERLACE_TYPE_T);
    status = mmal_port_parameter_get(sys->output, &interlace_type.hdr);
    if (status != MMAL_SUCCESS) {
        msg_Warn(dec, "Failed to query interlace type from decoder output port (status=%"PRIx32" %s)",
                status, mmal_status_to_string(status));
    } else {
        sys->b_progressive = (interlace_type.eMode == MMAL_InterlaceProgressive);
        sys->b_top_field_first = sys->b_progressive ? true :
            (interlace_type.eMode == MMAL_InterlaceFieldsInterleavedUpperFirst);
        msg_Dbg(dec, "Detected %s%s video (%d)",
                sys->b_progressive ? "progressive" : "interlaced",
                sys->b_progressive ? "" : (sys->b_top_field_first ? " tff" : " bff"),
                interlace_type.eMode);
    }

out:
    mmal_format_free(sys->output_format);
    sys->output_format = NULL;

    return ret;
}

static int send_output_buffer(decoder_t *dec)
{
    decoder_sys_t *sys = dec->p_sys;
    MMAL_BUFFER_HEADER_T *buffer;
    picture_sys_t *p_sys;
    picture_t *picture = NULL;
    MMAL_STATUS_T status;
    unsigned buffer_size = 0;
    int ret = 0;

    if (!sys->output->is_enabled)
        return VLC_EGENERIC;

    /* If local output pool is allocated, use it - this is only the case for
     * non-opaque modes */
    if (sys->output_pool) {
        buffer = mmal_queue_get(sys->output_pool->queue);
        if (!buffer) {
            msg_Warn(dec, "Failed to get new buffer");
            return VLC_EGENERIC;
        }
    }

    if (!decoder_UpdateVideoFormat(dec))
        picture = decoder_NewPicture(dec);
    if (!picture) {
        msg_Warn(dec, "Failed to get new picture");
        ret = -1;
        goto err;
    }

    p_sys = picture->p_sys;
    for (int i = 0; i < picture->i_planes; i++)
        buffer_size += picture->p[i].i_lines * picture->p[i].i_pitch;

    if (sys->output_pool) {
        mmal_buffer_header_reset(buffer);
        buffer->alloc_size = sys->output->buffer_size;
        if (buffer_size < sys->output->buffer_size) {
            msg_Err(dec, "Retrieved picture with too small data block (%d < %d)",
                    buffer_size, sys->output->buffer_size);
            ret = VLC_EGENERIC;
            goto err;
        }

        if (!sys->opaque)
            buffer->data = picture->p[0].p_pixels;
    } else {
        buffer = p_sys->buffer;
        if (!buffer) {
            msg_Warn(dec, "Picture has no buffer attached");
            picture_Release(picture);
            return VLC_EGENERIC;
        }
        buffer->data = p_sys->buffer->data;
    }
    buffer->user_data = picture;
    buffer->cmd = 0;

    status = mmal_port_send_buffer(sys->output, buffer);
    if (status != MMAL_SUCCESS) {
        msg_Err(dec, "Failed to send buffer to output port (status=%"PRIx32" %s)",
                status, mmal_status_to_string(status));
        ret = -1;
        goto err;
    }
    atomic_fetch_add(&sys->output_in_transit, 1);

    return ret;

err:
    if (picture)
        picture_Release(picture);
    if (sys->output_pool && buffer) {
        buffer->data = NULL;
        mmal_buffer_header_release(buffer);
    }
    return ret;
}

static void fill_output_port(decoder_t *dec)
{
    decoder_sys_t *sys = dec->p_sys;

    unsigned max_buffers_in_transit = 0;
    int buffers_available = 0;
    int buffers_to_send = 0;
    int i;

    if (sys->output_pool) {
        max_buffers_in_transit = __MAX(sys->output_pool->headers_num,
                MIN_NUM_BUFFERS_IN_TRANSIT);
        buffers_available = mmal_queue_length(sys->output_pool->queue);
    } else {
        max_buffers_in_transit = NUM_DECODER_BUFFER_HEADERS;
        buffers_available = NUM_DECODER_BUFFER_HEADERS - atomic_load(&sys->output_in_transit);
    }
    buffers_to_send = max_buffers_in_transit - atomic_load(&sys->output_in_transit);

    if (buffers_to_send > buffers_available)
        buffers_to_send = buffers_available;

#ifndef NDEBUG
    msg_Dbg(dec, "Send %d buffers to output port (available: %d, "
                    "in_transit: %d, buffer_num: %d)",
                    buffers_to_send, buffers_available,
                    atomic_load(&sys->output_in_transit),
                    sys->output->buffer_num);
#endif
    for (i = 0; i < buffers_to_send; ++i)
        if (send_output_buffer(dec) < 0)
            break;
}

static void flush_decoder(decoder_t *dec)
{
    decoder_sys_t *sys = dec->p_sys;
    MMAL_BUFFER_HEADER_T *buffer;
    MMAL_STATUS_T status;

    msg_Dbg(dec, "Flushing decoder ports...");
    mmal_port_flush(sys->output);
    mmal_port_flush(sys->input);

    while (atomic_load(&sys->output_in_transit) ||
           atomic_load(&sys->input_in_transit))
        vlc_sem_wait(&sys->sem);
}

static int decode(decoder_t *dec, block_t *block)
{
    decoder_sys_t *sys = dec->p_sys;
    MMAL_BUFFER_HEADER_T *buffer;
    bool need_flush = false;
    uint32_t len;
    uint32_t flags = 0;
    MMAL_STATUS_T status;

    /*
     * Configure output port if necessary
     */
    if (sys->output_format) {
        if (change_output_format(dec) < 0)
            msg_Err(dec, "Failed to change output port format");
    }

    if (!block)
        goto out;

    /*
     * Check whether full flush is required
     */
    if (block && block->i_flags & BLOCK_FLAG_DISCONTINUITY) {
        flush_decoder(dec);
        block_Release(block);
        return VLCDEC_SUCCESS;
    }

    if (atomic_load(&sys->started))
        fill_output_port(dec);

    /*
     * Process input
     */

    if (block->i_flags & BLOCK_FLAG_CORRUPTED)
        flags |= MMAL_BUFFER_HEADER_FLAG_CORRUPTED;

    while (block && block->i_buffer > 0) {
        buffer = mmal_queue_timedwait(sys->input_pool->queue, 100);
        if (!buffer) {
            msg_Err(dec, "Failed to retrieve buffer header for input data");
            need_flush = true;
            break;
        }
        mmal_buffer_header_reset(buffer);
        buffer->cmd = 0;
        buffer->pts = block->i_pts != 0 ? block->i_pts : block->i_dts;
        buffer->dts = block->i_dts;
        buffer->alloc_size = sys->input->buffer_size;

        len = block->i_buffer;
        if (len > buffer->alloc_size)
            len = buffer->alloc_size;

        buffer->data = block->p_buffer;
        block->p_buffer += len;
        block->i_buffer -= len;
        buffer->length = len;
        if (block->i_buffer == 0) {
            buffer->user_data = block;
            block = NULL;
        }
        buffer->flags = flags;

        status = mmal_port_send_buffer(sys->input, buffer);
        if (status != MMAL_SUCCESS) {
            msg_Err(dec, "Failed to send buffer to input port (status=%"PRIx32" %s)",
                    status, mmal_status_to_string(status));
            break;
        }
        atomic_fetch_add(&sys->input_in_transit, 1);
    }

out:
    if (need_flush)
        flush_decoder(dec);

    return VLCDEC_SUCCESS;
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
    if (block)
        block_Release(block);
    atomic_fetch_sub(&sys->input_in_transit, 1);
    vlc_sem_post(&sys->sem);
}

static void output_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    decoder_t *dec = (decoder_t *)port->userdata;
    decoder_sys_t *sys = dec->p_sys;
    picture_t *picture;
    MMAL_EVENT_FORMAT_CHANGED_T *fmt;
    MMAL_ES_FORMAT_T *format;

    if (buffer->cmd == 0) {
        picture = (picture_t *)buffer->user_data;
        if (buffer->length > 0) {
            picture->date = buffer->pts;
            picture->b_progressive = sys->b_progressive;
            picture->b_top_field_first = sys->b_top_field_first;
            decoder_QueueVideo(dec, picture);
        } else {
            picture_Release(picture);
            if (sys->output_pool) {
                buffer->user_data = NULL;
                buffer->alloc_size = 0;
                buffer->data = NULL;
                mmal_buffer_header_release(buffer);
            }
        }
        atomic_fetch_sub(&sys->output_in_transit, 1);
        vlc_sem_post(&sys->sem);
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
}
