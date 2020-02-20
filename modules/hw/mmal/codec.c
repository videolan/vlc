/*****************************************************************************
 * mmal.c: MMAL-based decoder plugin for Raspberry Pi
 *****************************************************************************
 * Copyright © 2014 jusst technologies GmbH
 *
 * Authors: Dennis Hamester <dennis.hamester@gmail.com>
 *          Julian Scheel <julian@jusst.de>
 *          John Cox <jc@kynesim.co.uk>
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

#include <interface/mmal/mmal.h>
#include <interface/mmal/util/mmal_util.h>
#include <interface/mmal/util/mmal_default_components.h>

#include "mmal_picture.h"

/*
 * This seems to be a bit high, but reducing it causes instabilities
 */
#define NUM_EXTRA_BUFFERS 5
//#define NUM_EXTRA_BUFFERS 10

static int OpenDecoder(vlc_object_t *);
static void CloseDecoder(vlc_object_t *);

vlc_module_begin()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    set_shortname(N_("MMAL decoder"))
    set_description(N_("MMAL-based decoder plugin for Raspberry Pi"))
    set_capability("video decoder", 90)
    add_shortcut("mmal_decoder")
    add_obsolete_bool("mmal-opaque")
    set_callbacks(OpenDecoder, CloseDecoder)
vlc_module_end()

typedef struct
{
    MMAL_COMPONENT_T *component;
    MMAL_PORT_T *input;
    MMAL_POOL_T *input_pool;
    MMAL_PORT_T *output;
    hw_mmal_port_pool_ref_t *ppr;
    MMAL_ES_FORMAT_T *output_format;

    MMAL_STATUS_T err_stream;
    bool b_top_field_first;
    bool b_progressive;

    bool b_flushed;

    vlc_video_context *vctx;

    // Lock to avoid pic update & allocate happenening simultainiously
    // * We should be able to arrange life s.t. this isn't needed
    //   but while we are confused apply belt & braces
    vlc_mutex_t pic_lock;

    /* statistics */
    bool started;
} decoder_sys_t;


typedef struct supported_mmal_enc_s {
    struct {
       MMAL_PARAMETER_HEADER_T header;
       MMAL_FOURCC_T encodings[64];
    } supported;
    int n;
} supported_mmal_enc_t;

#define SUPPORTED_MMAL_ENC_INIT \
{ \
    {{MMAL_PARAMETER_SUPPORTED_ENCODINGS, sizeof(((supported_mmal_enc_t *)0)->supported)}, {0}}, \
    -1 \
}

static supported_mmal_enc_t supported_decode_in_enc = SUPPORTED_MMAL_ENC_INIT;

static bool is_enc_supported(const supported_mmal_enc_t * const support, const MMAL_FOURCC_T fcc)
{
    int i;

    if (support->n == -1)
        return true;  // Unknown - say OK
    for (i = 0; i < support->n; ++i) {
        if (support->supported.encodings[i] == fcc)
            return true;
    }
    return false;
}

static bool set_and_test_enc_supported(vlc_object_t *obj, supported_mmal_enc_t * const support,
                                       MMAL_PORT_T * port, const MMAL_FOURCC_T fcc)
{
    if (support->n == -1)
    {
        if (mmal_port_parameter_get(port, (MMAL_PARAMETER_HEADER_T *)&support->supported) != MMAL_SUCCESS) {
            support->n = 0;
            msg_Err(obj, "Failed to get the supported codecs");
            return false;
        }

        support->n = (support->supported.header.size - sizeof(support->supported.header)) /
                     sizeof(support->supported.encodings[0]);
        for (int i=0; i<support->n; i++)
            msg_Dbg(obj, "%4.4s supported", (const char*)&support->supported.encodings[i]);
    }

    return is_enc_supported(support, fcc);
}

static MMAL_FOURCC_T vlc_to_mmal_es_fourcc(const unsigned int fcc)
{
    switch (fcc){
    case VLC_CODEC_MJPG:
        return MMAL_ENCODING_MJPEG;
    case VLC_CODEC_MP1V:
        return MMAL_ENCODING_MP1V;
    case VLC_CODEC_MPGV:
    case VLC_CODEC_MP2V:
        return MMAL_ENCODING_MP2V;
    case VLC_CODEC_H263:
        return MMAL_ENCODING_H263;
    case VLC_CODEC_MP4V:
        return MMAL_ENCODING_MP4V;
    case VLC_CODEC_H264:
        return MMAL_ENCODING_H264;
    case VLC_CODEC_VP6:
        return MMAL_ENCODING_VP6;
    case VLC_CODEC_VP8:
        return MMAL_ENCODING_VP8;
    case VLC_CODEC_WMV1:
        return MMAL_ENCODING_WMV1;
    case VLC_CODEC_WMV2:
        return MMAL_ENCODING_WMV2;
    case VLC_CODEC_WMV3:
        return MMAL_ENCODING_WMV3;
    case VLC_CODEC_VC1:
        return MMAL_ENCODING_WVC1;
    case VLC_CODEC_THEORA:
        return MMAL_ENCODING_THEORA;
    default:
        break;
    }
    return 0;
}

// Buffer either attached to pic or released
static picture_t * alloc_opaque_pic(decoder_t * const dec, MMAL_BUFFER_HEADER_T * const buf)
{
    decoder_sys_t *const dec_sys = dec->p_sys;

    vlc_mutex_lock(&dec_sys->pic_lock);
    picture_t * const pic = decoder_NewPicture(dec);
    vlc_mutex_unlock(&dec_sys->pic_lock);

    if (pic == NULL)
        goto fail1;

    if (buf->length == 0) {
        msg_Err(dec, "Empty buffer");
        goto fail2;
    }

    if ((pic->context = hw_mmal_gen_context(buf, dec_sys->ppr)) == NULL)
        goto fail2;

    buf_to_pic_copy_props(pic, buf);

    return pic;

fail2:
    picture_Release(pic);
fail1:
    // Recycle rather than release to avoid buffer starvation if NewPic fails
    hw_mmal_port_pool_ref_recycle(dec_sys->ppr, buf);
    return NULL;
}

static void control_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    decoder_t *dec = (decoder_t *)port->userdata;
    MMAL_STATUS_T status;

    if (buffer->cmd == MMAL_EVENT_ERROR) {
        status = *(uint32_t *)buffer->data;
        decoder_sys_t * const sys = dec->p_sys;
        sys->err_stream = status;
        msg_Err(dec, "MMAL error %"PRIx32" \"%s\"", status,
                mmal_status_to_string(status));
    }

    mmal_buffer_header_release(buffer);
}

static void input_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    block_t * const block = (block_t *)buffer->user_data;

    (void)port;  // Unused

    mmal_buffer_header_reset(buffer);
    mmal_buffer_header_release(buffer);

    if (block != NULL)
        block_Release(block);
}

static void decoder_output_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    decoder_t * const dec = (decoder_t *)port->userdata;

    if (buffer->cmd == 0 && buffer->length != 0)
    {
        picture_t *pic = alloc_opaque_pic(dec, buffer);
        if (pic == NULL)
            msg_Err(dec, "Failed to allocate new picture");
        else
            decoder_QueueVideo(dec, pic);
        // Buffer released or attached to pic - do not release again
        return;
    }

    if (buffer->cmd == MMAL_EVENT_FORMAT_CHANGED)
    {
        decoder_sys_t * const sys = dec->p_sys;
        MMAL_EVENT_FORMAT_CHANGED_T * const fmt = mmal_event_format_changed_get(buffer);
        MMAL_ES_FORMAT_T * const format = mmal_format_alloc();

        if (format == NULL)
            msg_Err(dec, "Failed to allocate new format");
        else
        {
            mmal_format_full_copy(format, fmt->format);
            format->encoding = MMAL_ENCODING_OPAQUE;

            if (sys->output_format != NULL)
                mmal_format_free(sys->output_format);

            sys->output_format = format;
        }
    }
    else if (buffer->cmd != 0) {
        msg_Warn(dec, "Unexpected output cb event: %4.4s", (const char*)&buffer->cmd);
    }

    // If we get here then we were flushing (cmd == 0 && len == 0) or
    // that was an EVENT - in either case we want to release the buffer
    // back to its pool rather than recycle it.
    mmal_buffer_header_reset(buffer);
    buffer->user_data = NULL;
    mmal_buffer_header_release(buffer);
}



static void fill_output_port(decoder_t *dec)
{
    decoder_sys_t *sys = dec->p_sys;

    if (decoder_UpdateVideoOutput(dec, sys->vctx) != 0)
    {
        // If we have a new format don't bother stuffing the buffer
        // We should get a reset RSN
        return;
    }

    hw_mmal_port_pool_ref_fill(sys->ppr);
}

static int change_output_format(decoder_t *dec)
{
    MMAL_PARAMETER_VIDEO_INTERLACE_TYPE_T interlace_type;
    decoder_sys_t * const sys = dec->p_sys;
    MMAL_STATUS_T status;
    int ret = 0;

    if (sys->started) {
        mmal_format_full_copy(sys->output->format, sys->output_format);
        status = mmal_port_format_commit(sys->output);
        if (status == MMAL_SUCCESS)
            goto apply_fmt;

        msg_Err(dec, "Failed to commit output format (status=%"PRIx32" %s)",
                status, mmal_status_to_string(status));
        ret = -1;
    }

    status = mmal_port_disable(sys->output);
    if (status != MMAL_SUCCESS) {
        msg_Err(dec, "Failed to disable output port (status=%"PRIx32" %s)",
                status, mmal_status_to_string(status));
        return -1;
    }

    mmal_format_full_copy(sys->output->format, sys->output_format);
    status = mmal_port_format_commit(sys->output);
    if (status != MMAL_SUCCESS) {
        msg_Err(dec, "Failed to commit output format (status=%"PRIx32" %s)",
                status, mmal_status_to_string(status));
        return -1;
    }

    sys->output->buffer_num = NUM_DECODER_BUFFER_HEADERS;
    sys->output->buffer_size = sys->output->buffer_size_recommended;

    status = mmal_port_enable(sys->output, decoder_output_cb);
    if (status != MMAL_SUCCESS) {
        msg_Err(dec, "Failed to enable output port (status=%"PRIx32" %s)",
                status, mmal_status_to_string(status));
        return -1;
    }

    if (!sys->started) {
        sys->started = true;

        /* we need one picture from vout for each buffer header on the output
         * port */
        dec->i_extra_picture_buffers = 10;
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
    }

    // Tell the rest of the world we have changed format
    vlc_mutex_lock(&sys->pic_lock);
    ret = decoder_UpdateVideoOutput(dec, sys->vctx);
    vlc_mutex_unlock(&sys->pic_lock);

    return ret;
}

static MMAL_STATUS_T
set_extradata_and_commit(decoder_t * const dec, decoder_sys_t * const sys)
{
    MMAL_STATUS_T status;

    status = mmal_port_format_commit(sys->input);
    if (status != MMAL_SUCCESS) {
        msg_Err(dec, "Failed to commit format for input port %s (status=%"PRIx32" %s)",
                sys->input->name, status, mmal_status_to_string(status));
    }
    return status;
}

static MMAL_STATUS_T decoder_send_extradata(decoder_t * const dec, decoder_sys_t *const sys)
{
    if (dec->fmt_in.i_codec == VLC_CODEC_H264 &&
        dec->fmt_in.i_extra > 0)
    {
        MMAL_BUFFER_HEADER_T * const buf = mmal_queue_wait(sys->input_pool->queue);
        MMAL_STATUS_T status;

        mmal_buffer_header_reset(buf);
        buf->cmd = 0;
        buf->user_data = NULL;
        buf->alloc_size = sys->input->buffer_size;
        buf->length = dec->fmt_in.i_extra;
        buf->data = dec->fmt_in.p_extra;
        buf->flags = MMAL_BUFFER_HEADER_FLAG_CONFIG;

        status = mmal_port_send_buffer(sys->input, buf);
        if (status != MMAL_SUCCESS) {
            msg_Err(dec, "Failed to send extradata buffer to input port (status=%"PRIx32" %s)",
                    status, mmal_status_to_string(status));
            return status;
        }
    }

    return MMAL_SUCCESS;
}

static void flush_decoder(decoder_t *dec)
{
    decoder_sys_t *const sys = dec->p_sys;

    if (!sys->b_flushed) {
        mmal_port_disable(sys->input);
        mmal_port_disable(sys->output);
        // We can leave the input disabled, but we want the output enabled
        // in order to sink any buffers returning from other modules
        mmal_port_enable(sys->output, decoder_output_cb);
        sys->b_flushed = true;
    }
}

static int decode(decoder_t *dec, block_t *block)
{
    decoder_sys_t *sys = dec->p_sys;
    MMAL_BUFFER_HEADER_T *buffer;
    uint32_t len;
    uint32_t flags = 0;
    MMAL_STATUS_T status;

    if (sys->err_stream != MMAL_SUCCESS) {
        msg_Err(dec, "MMAL error reported by ctrl");
        flush_decoder(dec);
        return VLCDEC_ECRITICAL;  /// I think they are all fatal
    }

    /*
     * Configure output port if necessary
     */
    if (sys->output_format) {
        if (change_output_format(dec) < 0)
            msg_Err(dec, "Failed to change output port format");
        mmal_format_free(sys->output_format);
        sys->output_format = NULL;
    }

    if (block == NULL)
        return VLCDEC_SUCCESS;

    /*
     * Check whether full flush is required
     */
    if (block->i_flags & BLOCK_FLAG_DISCONTINUITY) {
        flush_decoder(dec);
    }

    if (block->i_buffer == 0)
    {
        block_Release(block);
        return VLCDEC_SUCCESS;
    }

    // Reenable stuff if the last thing we did was flush
    if (!sys->output->is_enabled &&
        (status = mmal_port_enable(sys->output, decoder_output_cb)) != MMAL_SUCCESS)
    {
        msg_Err(dec, "Output port enable failed");
        goto fail;
    }

    if (!sys->input->is_enabled)
    {
        if ((status = set_extradata_and_commit(dec, sys)) != MMAL_SUCCESS)
            goto fail;

        if ((status = mmal_port_enable(sys->input, input_port_cb)) != MMAL_SUCCESS)
        {
            msg_Err(dec, "Input port enable failed");
            goto fail;
        }

        if ((status = decoder_send_extradata(dec, sys)) != MMAL_SUCCESS)
            goto fail;
    }

    // *** We cannot get a picture to put the result in 'till we have
    // reported the size & the output stages have been set up
    if (sys->started)
        fill_output_port(dec);

    /*
     * Process input
     */

    if (block->i_flags & BLOCK_FLAG_CORRUPTED)
        flags |= MMAL_BUFFER_HEADER_FLAG_CORRUPTED;

    while (block != NULL)
    {
        buffer = mmal_queue_wait(sys->input_pool->queue);
        if (!buffer) {
            msg_Err(dec, "Failed to retrieve buffer header for input data");
            goto fail;
        }

        mmal_buffer_header_reset(buffer);
        buffer->cmd = 0;
        buffer->pts = block->i_pts != VLC_TICK_INVALID ? block->i_pts :
            block->i_dts != VLC_TICK_INVALID ? block->i_dts : MMAL_TIME_UNKNOWN;
        buffer->dts = block->i_dts;
        buffer->alloc_size = sys->input->buffer_size;
        buffer->user_data = NULL;

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
            goto fail;
        }

        // Reset flushed flag once we have sent a buf
        sys->b_flushed = false;
    }
    return VLCDEC_SUCCESS;

fail:
    flush_decoder(dec);
    return VLCDEC_ECRITICAL;

}

static void CloseDecoder(vlc_object_t *p_this)
{
    decoder_t *dec = (decoder_t*)p_this;
    decoder_sys_t *sys = dec->p_sys;

    if (!sys)
        return;

    if (sys->component != NULL) {
        if (sys->input->is_enabled)
            mmal_port_disable(sys->input);

        if (sys->output->is_enabled)
            mmal_port_disable(sys->output);

        if (sys->component->control->is_enabled)
            mmal_port_disable(sys->component->control);

        if (sys->component->is_enabled)
            mmal_component_disable(sys->component);

        mmal_component_release(sys->component);
    }

    if (sys->input_pool != NULL)
        mmal_pool_destroy(sys->input_pool);

    if (sys->output_format != NULL)
        mmal_format_free(sys->output_format);

    hw_mmal_port_pool_ref_release(sys->ppr, false);

    if (sys->vctx)
        vlc_video_context_Release(sys->vctx);

    free(sys);
}

static int OpenDecoder(vlc_object_t *p_this)
{
    decoder_t *dec = (decoder_t*)p_this;
    int ret = VLC_EGENERIC;
    decoder_sys_t *sys;
    MMAL_STATUS_T status;
    const MMAL_FOURCC_T in_fcc = vlc_to_mmal_es_fourcc(dec->fmt_in.i_codec);
    if (in_fcc == 0) {
        msg_Dbg(p_this, "codec %4.4s not supported", (const char*)&dec->fmt_in.i_codec);
        return VLC_EGENERIC;
    }

    if (!is_enc_supported(&supported_decode_in_enc, in_fcc)) {
        msg_Dbg(p_this, "codec %4.4s (MMAL %4.4s) not supported",
                (const char*)&dec->fmt_in.i_codec, (const char*)&in_fcc);
        return VLC_EGENERIC;
    }

    sys = calloc(1, sizeof(decoder_sys_t));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    vlc_decoder_device *dec_dev = decoder_GetDecoderDevice(dec);
    mmal_decoder_device_t *devsys = GetMMALDeviceOpaque(dec_dev);
    if (devsys == NULL)
    {
        msg_Err(dec, "Could not find a MMAL decoder device");
        return VLC_EGENERIC;
    }
    sys->vctx = vlc_video_context_Create(dec_dev, VLC_VIDEO_CONTEXT_MMAL, 0, NULL);
    vlc_decoder_device_Release(dec_dev);

    dec->p_sys = sys;
    vlc_mutex_init(&sys->pic_lock);

    sys->err_stream = MMAL_SUCCESS;

    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_DECODER, &sys->component);
    if (status != MMAL_SUCCESS) {
        msg_Err(dec, "Failed to create MMAL component %s (status=%"PRIx32" %s)",
                MMAL_COMPONENT_DEFAULT_VIDEO_DECODER, status, mmal_status_to_string(status));
        goto fail;
    }

    sys->input = sys->component->input[0];
    sys->output = sys->component->output[0];

    sys->input->userdata = (struct MMAL_PORT_USERDATA_T *)dec;
    sys->input->format->encoding = in_fcc;

    if (!set_and_test_enc_supported(p_this, &supported_decode_in_enc, sys->input, in_fcc)) {
        msg_Warn(p_this, "codec %4.4s not supported", (const char*)&dec->fmt_in.i_codec);
        goto fail;
    }

    sys->component->control->userdata = (struct MMAL_PORT_USERDATA_T *)dec;
    status = mmal_port_enable(sys->component->control, control_port_cb);
    if (status != MMAL_SUCCESS) {
        msg_Err(dec, "Failed to enable control port %s (status=%"PRIx32" %s)",
                sys->component->control->name, status, mmal_status_to_string(status));
        goto fail;
    }

    if ((status = set_extradata_and_commit(dec, sys)) != MMAL_SUCCESS)
        goto fail;

    sys->input->buffer_size = sys->input->buffer_size_recommended;
    sys->input->buffer_num = sys->input->buffer_num_recommended;

    status = mmal_port_enable(sys->input, input_port_cb);
    if (status != MMAL_SUCCESS) {
        msg_Err(dec, "Failed to enable input port %s (status=%"PRIx32" %s)",
                sys->input->name, status, mmal_status_to_string(status));
        goto fail;
    }

    if ((status = hw_mmal_opaque_output(VLC_OBJECT(dec), &sys->ppr,
                                        sys->output, NUM_EXTRA_BUFFERS, decoder_output_cb)) != MMAL_SUCCESS)
        goto fail;

    status = mmal_component_enable(sys->component);
    if (status != MMAL_SUCCESS) {
        msg_Err(dec, "Failed to enable component %s (status=%"PRIx32" %s)",
                sys->component->name, status, mmal_status_to_string(status));
        goto fail;
    }

    if ((sys->input_pool = mmal_pool_create(sys->input->buffer_num, 0)) == NULL)
    {
        msg_Err(dec, "Failed to create input pool");
        goto fail;
    }

    sys->b_flushed = true;
    dec->fmt_out.i_codec = VLC_CODEC_MMAL_OPAQUE;
    dec->fmt_out.video.i_chroma = VLC_CODEC_MMAL_OPAQUE;

    if ((status = decoder_send_extradata(dec, sys)) != MMAL_SUCCESS)
        goto fail;

    dec->pf_decode = decode;
    dec->pf_flush  = flush_decoder;

    return 0;

fail:
    CloseDecoder(p_this);
    return ret;
}
