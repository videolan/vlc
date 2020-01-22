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
#include <vlc_filter.h>

#include <interface/mmal/mmal.h>
#include <interface/mmal/util/mmal_util.h>
#include <interface/mmal/util/mmal_default_components.h>

#include "mmal_cma.h"
#include "mmal_picture.h"

#include "subpic.h"

/*
 * This seems to be a bit high, but reducing it causes instabilities
 */
#define NUM_EXTRA_BUFFERS 5
//#define NUM_EXTRA_BUFFERS 10
#define NUM_DECODER_BUFFER_HEADERS 30

#define CONVERTER_BUFFERS 4  // Buffers on the output of the converter

#define MMAL_SLICE_HEIGHT 16
#define MMAL_ALIGN_W      32
#define MMAL_ALIGN_H      16

#define MMAL_RESIZE_NAME "mmal-resize"
#define MMAL_RESIZE_TEXT N_("Use mmal resizer rather than hvs.")
#define MMAL_RESIZE_LONGTEXT N_("Use mmal resizer rather than isp. This uses less gpu memory than the ISP but is slower.")

static int OpenDecoder(vlc_object_t *);
static void CloseDecoder(vlc_object_t *);
static int OpenConverter(vlc_object_t *);
static void CloseConverter(vlc_object_t *);

vlc_module_begin()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    set_shortname(N_("MMAL decoder"))
    set_description(N_("MMAL-based decoder plugin for Raspberry Pi"))
    set_capability("video decoder", 90)
    add_shortcut("mmal_decoder")
    add_obsolete_bool("mmal-opaque")
    set_callbacks(OpenDecoder, CloseDecoder)

    add_submodule()
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )
    set_shortname(N_("MMAL resizer"))
    set_description(N_("MMAL resizing conversion filter"))
    add_shortcut("mmal_converter")
    set_capability( "video converter", 900 )
    add_bool(MMAL_RESIZE_NAME, /* default */ false, MMAL_RESIZE_TEXT, MMAL_RESIZE_LONGTEXT, /* advanced option */ false)
    set_callbacks(OpenConverter, CloseConverter)
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

    vcsm_init_type_t vcsm_init_type;

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

static bool is_enc_supported(supported_mmal_enc_t * const support, const MMAL_FOURCC_T fcc)
{
    int i;

    if (fcc == 0)
        return false;
    if (support->n == -1)
        return true;  // Unknown - say OK
    for (i = 0; i < support->n; ++i) {
        if (support->supported.encodings[i] == fcc)
            return true;
    }
    return false;
}

static bool set_and_test_enc_supported(supported_mmal_enc_t * const support, MMAL_PORT_T * port, const MMAL_FOURCC_T fcc)
{
    if (support->n >= 0)
        /* already done */;
    else if (mmal_port_parameter_get(port, (MMAL_PARAMETER_HEADER_T *)&support->supported) != MMAL_SUCCESS)
        support->n = 0;
    else
        support->n = (support->supported.header.size - sizeof(support->supported.header)) /
          sizeof(support->supported.encodings[0]);

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

static MMAL_FOURCC_T pic_to_slice_mmal_fourcc(const MMAL_FOURCC_T fcc)
{
    switch (fcc){
    case MMAL_ENCODING_I420:
        return MMAL_ENCODING_I420_SLICE;
    case MMAL_ENCODING_I422:
        return MMAL_ENCODING_I422_SLICE;
    case MMAL_ENCODING_ARGB:
        return MMAL_ENCODING_ARGB_SLICE;
    case MMAL_ENCODING_RGBA:
        return MMAL_ENCODING_RGBA_SLICE;
    case MMAL_ENCODING_ABGR:
        return MMAL_ENCODING_ABGR_SLICE;
    case MMAL_ENCODING_BGRA:
        return MMAL_ENCODING_BGRA_SLICE;
    case MMAL_ENCODING_RGB16:
        return MMAL_ENCODING_RGB16_SLICE;
    case MMAL_ENCODING_RGB24:
        return MMAL_ENCODING_RGB24_SLICE;
    case MMAL_ENCODING_RGB32:
        return MMAL_ENCODING_RGB32_SLICE;
    case MMAL_ENCODING_BGR16:
        return MMAL_ENCODING_BGR16_SLICE;
    case MMAL_ENCODING_BGR24:
        return MMAL_ENCODING_BGR24_SLICE;
    case MMAL_ENCODING_BGR32:
        return MMAL_ENCODING_BGR32_SLICE;
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

    if (decoder_UpdateVideoFormat(dec) != 0)
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
    ret = decoder_UpdateVideoFormat(dec);
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

    cma_vcsm_exit(sys->vcsm_init_type);

    vlc_mutex_destroy(&sys->pic_lock);
    free(sys);
}

static int OpenDecoder(vlc_object_t *p_this)
{
    decoder_t *dec = (decoder_t*)p_this;
    int ret = VLC_EGENERIC;
    decoder_sys_t *sys;
    MMAL_STATUS_T status;
    const MMAL_FOURCC_T in_fcc = vlc_to_mmal_es_fourcc(dec->fmt_in.i_codec);

    if (!is_enc_supported(&supported_decode_in_enc, in_fcc))
        return VLC_EGENERIC;

    sys = calloc(1, sizeof(decoder_sys_t));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    dec->p_sys = sys;
    vlc_mutex_init(&sys->pic_lock);

    if ((sys->vcsm_init_type = cma_vcsm_init()) == VCSM_INIT_NONE) {
        msg_Err(dec, "VCSM init failed");
        goto fail;
    }
    msg_Info(dec, "VCSM init succeeded: %s", cma_vcsm_init_str(sys->vcsm_init_type));

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

    if (!set_and_test_enc_supported(&supported_decode_in_enc, sys->input, in_fcc)) {
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

// ----------------------------

#define CONV_MAX_LATENCY 1  // In frames

typedef struct pic_fifo_s {
    picture_t * head;
    picture_t * tail;
} pic_fifo_t;

static picture_t * pic_fifo_get(pic_fifo_t * const pf)
{
    picture_t * const pic = pf->head;;
    if (pic != NULL) {
        pf->head = pic->p_next;
        pic->p_next = NULL;
    }
    return pic;
}

static void pic_fifo_release_all(pic_fifo_t * const pf)
{
    picture_t * pic;
    while ((pic = pic_fifo_get(pf)) != NULL) {
        picture_Release(pic);
    }
}

static void pic_fifo_init(pic_fifo_t * const pf)
{
    pf->head = NULL;
    pf->tail = NULL;  // Not strictly needed
}

static void pic_fifo_put(pic_fifo_t * const pf, picture_t * pic)
{
    pic->p_next = NULL;
    if (pf->head == NULL)
        pf->head = pic;
    else
        pf->tail->p_next = pic;
    pf->tail = pic;
}

#define SUBS_MAX 3

typedef enum filter_resizer_e {
    FILTER_RESIZER_RESIZER,
    FILTER_RESIZER_ISP,
} filter_resizer_t;

typedef struct conv_frame_stash_s
{
    mtime_t pts;
    MMAL_BUFFER_HEADER_T * sub_bufs[SUBS_MAX];
} conv_frame_stash_t;

typedef struct
{
    filter_resizer_t resizer_type;
    MMAL_COMPONENT_T *component;
    MMAL_PORT_T *input;
    MMAL_PORT_T *output;
    MMAL_POOL_T *out_pool;  // Free output buffers
    MMAL_POOL_T *in_pool;   // Input pool to get BH for replication

    cma_buf_pool_t * cma_in_pool;
    cma_buf_pool_t * cma_out_pool;

    subpic_reg_stash_t subs[SUBS_MAX];

    pic_fifo_t ret_pics;

    unsigned int pic_n;
    vlc_sem_t sem;
    vlc_mutex_t lock;

    MMAL_STATUS_T err_stream;

    bool needs_copy_in;
    bool is_sliced;
    bool out_fmt_set;
    const char * component_name;
    MMAL_PORT_BH_CB_T in_port_cb_fn;
    MMAL_PORT_BH_CB_T out_port_cb_fn;

    uint64_t frame_seq;
    conv_frame_stash_t stash[16];

    // Slice specific tracking stuff
    struct {
        pic_fifo_t pics;
        unsigned int line;  // Lines filled
    } slice;

    vcsm_init_type_t vcsm_init_type;
} converter_sys_t;


static MMAL_STATUS_T pic_to_format(MMAL_ES_FORMAT_T * const es_fmt, const picture_t * const pic)
{
    unsigned int bpp = (pic->format.i_bits_per_pixel + 7) >> 3;
    MMAL_VIDEO_FORMAT_T * const v_fmt = &es_fmt->es->video;

    es_fmt->type = MMAL_ES_TYPE_VIDEO;
    es_fmt->encoding = vlc_to_mmal_video_fourcc(&pic->format);
    es_fmt->encoding_variant = 0;

    // Fill in crop etc.
    hw_mmal_vlc_fmt_to_mmal_fmt(es_fmt, &pic->format);
    // Override width / height with strides if appropriate
    if (bpp != 0) {
        v_fmt->width = pic->p[0].i_pitch / bpp;
        v_fmt->height = pic->p[0].i_lines;
    }
    return MMAL_SUCCESS;
}


static MMAL_STATUS_T conv_enable_in(filter_t * const p_filter, converter_sys_t * const sys)
{
    MMAL_STATUS_T err = MMAL_SUCCESS;

    if (!sys->input->is_enabled &&
        (err = mmal_port_enable(sys->input, sys->in_port_cb_fn)) != MMAL_SUCCESS)
    {
        msg_Err(p_filter, "Failed to enable input port %s (status=%"PRIx32" %s)",
                sys->input->name, err, mmal_status_to_string(err));
    }
    return err;
}

static MMAL_STATUS_T conv_enable_out(filter_t * const p_filter, converter_sys_t * const sys)
{
    MMAL_STATUS_T err = MMAL_SUCCESS;

    {
        cma_buf_pool_deletez(&sys->cma_out_pool);
    }

    if (!sys->output->is_enabled &&
        (err = mmal_port_enable(sys->output, sys->out_port_cb_fn)) != MMAL_SUCCESS)
    {
        msg_Err(p_filter, "Failed to enable output port %s (status=%"PRIx32" %s)",
                sys->output->name, err, mmal_status_to_string(err));
    }
    return err;
}

static void conv_control_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    filter_t * const p_filter = (filter_t *)port->userdata;

    if (buffer->cmd == MMAL_EVENT_ERROR) {
        MMAL_STATUS_T status = *(uint32_t *)buffer->data;

        converter_sys_t * sys = p_filter->p_sys;
        sys->err_stream = status;

        msg_Err(p_filter, "MMAL error %"PRIx32" \"%s\"", status,
                mmal_status_to_string(status));
    }

    mmal_buffer_header_release(buffer);
}

static void conv_input_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buf)
{
    VLC_UNUSED(port);

    mmal_buffer_header_release(buf);
}

static void conv_out_q_pic(converter_sys_t * const sys, picture_t * const pic)
{
    pic->p_next = NULL;

    vlc_mutex_lock(&sys->lock);
    pic_fifo_put(&sys->ret_pics, pic);
    vlc_mutex_unlock(&sys->lock);

    vlc_sem_post(&sys->sem);
}

static void conv_output_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buf)
{
    filter_t * const p_filter = (filter_t *)port->userdata;
    converter_sys_t * const sys = p_filter->p_sys;

    if (buf->cmd == 0) {
        picture_t * const pic = (picture_t *)buf->user_data;

        if (pic == NULL) {
            msg_Err(p_filter, "Buffer has no attached picture");
        }
        else if (buf->data != NULL && buf->length != 0)
        {
            buf_to_pic_copy_props(pic, buf);

            buf->user_data = NULL;  // Responsability for this pic no longer with buffer
            conv_out_q_pic(sys, pic);
        }
    }

    mmal_buffer_header_release(buf);
}


static void slice_output_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buf)
{
    filter_t * const p_filter = (filter_t *)port->userdata;
    converter_sys_t * const sys = p_filter->p_sys;

    if (buf->cmd != 0)
    {
        mmal_buffer_header_release(buf);
        return;
    }

    if (buf->data != NULL && buf->length != 0)
    {
        // Got slice
        picture_t *pic = sys->slice.pics.head;
        const unsigned int scale_lines = sys->output->format->es->video.height;  // Expected lines of callback

        if (pic == NULL) {
            msg_Err(p_filter, "No output picture");
            goto fail;
        }

        // Copy lines
        // * single plane only - fix for I420
        {
            const unsigned int scale_n = __MIN(scale_lines - sys->slice.line, MMAL_SLICE_HEIGHT);
            const unsigned int pic_lines = pic->p[0].i_lines;
            const unsigned int copy_n = sys->slice.line + scale_n <= pic_lines ? scale_n :
                sys->slice.line >= pic_lines ? 0 :
                    pic_lines - sys->slice.line;

            const unsigned int src_stride = buf->type->video.pitch[0];
            const unsigned int dst_stride = pic->p[0].i_pitch;
            uint8_t *dst = pic->p[0].p_pixels + sys->slice.line * dst_stride;
            const uint8_t *src = buf->data + buf->type->video.offset[0];

            if (src_stride == dst_stride) {
                if (copy_n != 0)
                    memcpy(dst, src, src_stride * copy_n);
            }
            else {
                unsigned int i;
                for (i = 0; i != copy_n; ++i) {
                    memcpy(dst, src, __MIN(dst_stride, src_stride));
                    dst += dst_stride;
                    src += src_stride;
                }
            }
            sys->slice.line += scale_n;
        }

        if ((buf->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END) != 0 || sys->slice.line >= scale_lines) {

            if ((buf->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END) == 0 || sys->slice.line != scale_lines) {
                // Stuff doesn't add up...
                msg_Err(p_filter, "Line count (%d/%d) & EOF disagree (flags=%#x)", sys->slice.line, scale_lines, buf->flags);
                goto fail;
            }
            else {
                sys->slice.line = 0;

                vlc_mutex_lock(&sys->lock);
                pic_fifo_get(&sys->slice.pics);  // Remove head from Q
                vlc_mutex_unlock(&sys->lock);

                buf_to_pic_copy_props(pic, buf);
                conv_out_q_pic(sys, pic);
            }
        }
    }

    // Put back
    buf->user_data = NULL; // Zap here to make sure we can't reuse later
    mmal_buffer_header_reset(buf);

    if (mmal_port_send_buffer(sys->output, buf) != MMAL_SUCCESS) {
        mmal_buffer_header_release(buf);
    }
    return;

fail:
    sys->err_stream = MMAL_EIO;
    vlc_sem_post(&sys->sem);  // If we were waiting then break us out - the flush should fix sem values
}


static void conv_flush(filter_t * p_filter)
{
    converter_sys_t * const sys = p_filter->p_sys;
    unsigned int i;

    if (sys->input != NULL && sys->input->is_enabled)
        mmal_port_disable(sys->input);

    if (sys->output != NULL && sys->output->is_enabled)
        mmal_port_disable(sys->output);

//    cma_buf_pool_deletez(&sys->cma_out_pool);

    // Free up anything we may have already lying around
    // Don't need lock as the above disables should have prevented anything
    // happening in the background

    for (i = 0; i != 16; ++i) {
        conv_frame_stash_t *const stash = sys->stash + i;
        unsigned int sub_no;

        stash->pts = MMAL_TIME_UNKNOWN;
        for (sub_no = 0; sub_no != SUBS_MAX; ++sub_no) {
            if (stash->sub_bufs[sub_no] != NULL) {
                mmal_buffer_header_release(stash->sub_bufs[sub_no]);
                stash->sub_bufs[sub_no] = NULL;
            }
        }
    }

    pic_fifo_release_all(&sys->slice.pics);
    pic_fifo_release_all(&sys->ret_pics);

    // Reset sem values - easiest & most reliable way is to just kill & re-init
    vlc_sem_destroy(&sys->sem);
    vlc_sem_init(&sys->sem, 0);
    sys->pic_n = 0;

    // Reset error status
    sys->err_stream = MMAL_SUCCESS;
}

static void conv_stash_fixup(filter_t * const p_filter, converter_sys_t * const sys, picture_t * const p_pic)
{
    conv_frame_stash_t * const stash = sys->stash + (p_pic->date & 0xf);
    unsigned int sub_no;
    VLC_UNUSED(p_filter);

    p_pic->date = stash->pts;
    for (sub_no = 0; sub_no != SUBS_MAX; ++sub_no) {
        if (stash->sub_bufs[sub_no] != NULL) {
            // **** Do stashed blend
            // **** Aaargh, bother... need to rescale subs too

            mmal_buffer_header_release(stash->sub_bufs[sub_no]);
            stash->sub_bufs[sub_no] = NULL;
        }
    }
}

// Output buffers may contain a pic ref on error or flush
// Free it
static MMAL_BOOL_T out_buffer_pre_release_cb(MMAL_BUFFER_HEADER_T *header, void *userdata)
{
    VLC_UNUSED(userdata);

    picture_t * const pic = header->user_data;
    header->user_data = NULL;

    if (pic != NULL)
        picture_Release(pic);

    return MMAL_FALSE;
}

static MMAL_STATUS_T conv_set_output(filter_t * const p_filter, converter_sys_t * const sys, picture_t * const pic)
{
    MMAL_STATUS_T status;

    sys->output->userdata = (struct MMAL_PORT_USERDATA_T *)p_filter;
    sys->output->format->type = MMAL_ES_TYPE_VIDEO;
    sys->output->format->encoding = vlc_to_mmal_video_fourcc(&p_filter->fmt_out.video);
    sys->output->format->encoding_variant = 0;
    hw_mmal_vlc_fmt_to_mmal_fmt(sys->output->format, &p_filter->fmt_out.video);

    if (pic != NULL)
    {
        // Override default format width/height if we have a pic we need to match
        if ((status = pic_to_format(sys->output->format, pic)) != MMAL_SUCCESS)
        {
            msg_Err(p_filter, "Bad format desc: %4.4s, pic=%p, bits=%d", (const char*)&pic->format.i_chroma, pic, pic->format.i_bits_per_pixel);
            return status;
        }
    }

    if (sys->is_sliced) {
        // Override height for slice
        sys->output->format->es->video.height = MMAL_SLICE_HEIGHT;
    }

    mmal_log_dump_format(sys->output->format);

    status = mmal_port_format_commit(sys->output);
    if (status != MMAL_SUCCESS) {
        msg_Err(p_filter, "Failed to commit format for output port %s (status=%"PRIx32" %s)",
                sys->output->name, status, mmal_status_to_string(status));
        return status;
    }

    sys->output->buffer_num = __MAX(sys->is_sliced ? 16 : 2, sys->output->buffer_num_recommended);
    sys->output->buffer_size = sys->output->buffer_size_recommended;

    if ((status = conv_enable_out(p_filter, sys)) != MMAL_SUCCESS)
        return status;

    return MMAL_SUCCESS;
}

static picture_t *conv_filter(filter_t *p_filter, picture_t *p_pic)
{
    converter_sys_t * const sys = p_filter->p_sys;
    picture_t * ret_pics;
    MMAL_STATUS_T err;
    const uint64_t frame_seq = ++sys->frame_seq;
    conv_frame_stash_t * const stash = sys->stash + (frame_seq & 0xf);
    MMAL_BUFFER_HEADER_T * out_buf = NULL;

    if (sys->err_stream != MMAL_SUCCESS) {
        goto stream_fail;
    }

    // Check pic fmt corresponds to what we have set up
    // ??? ISP may require flush (disable) but actually seems quite happy
    //     without
    if (hw_mmal_vlc_pic_to_mmal_fmt_update(sys->input->format, p_pic))
    {
        msg_Dbg(p_filter, "Reset input port format");
        mmal_port_format_commit(sys->input);
    }

    if (p_pic->context == NULL) {
        // Can't have stashed subpics if not one of our pics
        if (!sys->needs_copy_in)
            msg_Dbg(p_filter, "No context");
    }
    else
    {
        unsigned int sub_no = 0;
        for (sub_no = 0; sub_no != SUBS_MAX; ++sub_no) {
            if ((stash->sub_bufs[sub_no] = hw_mmal_pic_sub_buf_get(p_pic, sub_no)) != NULL) {
                mmal_buffer_header_acquire(stash->sub_bufs[sub_no]);
            }
        }
    }

    if (!sys->out_fmt_set) {
        sys->out_fmt_set = true;

        if (sys->is_sliced) {
            // If zc then we will do stride conversion when we copy to arm side
            // so no need to worry about actual pic dimensions here
            if ((err = conv_set_output(p_filter, sys, NULL)) != MMAL_SUCCESS)
                goto fail;

            sys->out_pool = mmal_port_pool_create(sys->output, sys->output->buffer_num, sys->output->buffer_size);
        }
        else {
            picture_t *pic = filter_NewPicture(p_filter);
            err = conv_set_output(p_filter, sys, pic);
            picture_Release(pic);
            if (err != MMAL_SUCCESS)
                goto fail;

            sys->out_pool = mmal_pool_create(sys->output->buffer_num, 0);
        }

        if (sys->out_pool == NULL) {
            msg_Err(p_filter, "Failed to create output pool");
            goto fail;
        }
    }

    // Reenable stuff if the last thing we did was flush
    if ((err = conv_enable_out(p_filter, sys)) != MMAL_SUCCESS ||
        (err = conv_enable_in(p_filter, sys)) != MMAL_SUCCESS)
        goto fail;

    // We attach pic to buf before stuffing the output port
    // We could attach the pic on output for cma, but it is a lot easier to keep
    // the code common.
    {
        picture_t * const out_pic = filter_NewPicture(p_filter);

        if (out_pic == NULL)
        {
            msg_Err(p_filter, "Failed to alloc required filter output pic");
            goto fail;
        }

        out_pic->format.i_sar_den = p_filter->fmt_out.video.i_sar_den;
        out_pic->format.i_sar_num = p_filter->fmt_out.video.i_sar_num;

        if (sys->is_sliced) {
            vlc_mutex_lock(&sys->lock);
            pic_fifo_put(&sys->slice.pics, out_pic);
            vlc_mutex_unlock(&sys->lock);

            // Poke any returned pic buffers into output
            // In general this should only happen immediately after enable
            while ((out_buf = mmal_queue_get(sys->out_pool->queue)) != NULL)
                mmal_port_send_buffer(sys->output, out_buf);
        }
        else
        {
            // 1 in - 1 out
            if ((out_buf = mmal_queue_wait(sys->out_pool->queue)) == NULL)
            {
                msg_Err(p_filter, "Failed to get output buffer");
                picture_Release(out_pic);
                goto fail;
            }
            mmal_buffer_header_reset(out_buf);

            // Attach out_pic to the buffer & ensure it is freed when the buffer is released
            // On a good send callback the pic will be extracted to avoid this
            out_buf->user_data = out_pic;
            mmal_buffer_header_pre_release_cb_set(out_buf, out_buffer_pre_release_cb, NULL);

            {
                out_buf->data = out_pic->p[0].p_pixels;
                out_buf->alloc_size = out_pic->p[0].i_pitch * out_pic->p[0].i_lines;
                //**** stride ????
            }

            if ((err = mmal_port_send_buffer(sys->output, out_buf)) != MMAL_SUCCESS)
            {
                msg_Err(p_filter, "Send buffer to output failed");
                goto fail;
            }
            out_buf = NULL;
        }
    }


    // Stuff into input
    // We assume the BH is already set up with values reflecting pic date etc.
    stash->pts = p_pic->date;
    {
        MMAL_BUFFER_HEADER_T *const pic_buf = sys->needs_copy_in ?
            hw_mmal_pic_buf_copied(p_pic, sys->in_pool, sys->input, sys->cma_in_pool) :
            hw_mmal_pic_buf_replicated(p_pic, sys->in_pool);

        // Whether or not we extracted the pic_buf we are done with the picture
        picture_Release(p_pic);
        p_pic = NULL;

        if (pic_buf == NULL) {
            msg_Err(p_filter, "Pic has no attached buffer");
            goto fail;
        }

        pic_buf->pts = frame_seq;

        if ((err = mmal_port_send_buffer(sys->input, pic_buf)) != MMAL_SUCCESS)
        {
            msg_Err(p_filter, "Send buffer to input failed");
            mmal_buffer_header_release(pic_buf);
            goto fail;
        }
    }

    // We have a 1 pic latency for everything except the 1st pic which we
    // wait for.
    // This means we get a single static pic out
    if (sys->pic_n++ == 1) {
        return NULL;
    }
    vlc_sem_wait(&sys->sem);

    // Return a single pending buffer
    vlc_mutex_lock(&sys->lock);
    ret_pics = pic_fifo_get(&sys->ret_pics);
    vlc_mutex_unlock(&sys->lock);

    if (sys->err_stream != MMAL_SUCCESS)
        goto stream_fail;

    conv_stash_fixup(p_filter, sys, ret_pics);

    return ret_pics;

stream_fail:
    msg_Err(p_filter, "MMAL error reported by callback");
fail:
    if (out_buf != NULL)
        mmal_buffer_header_release(out_buf);
    if (p_pic != NULL)
        picture_Release(p_pic);
    conv_flush(p_filter);
    return NULL;
}

static void CloseConverter(vlc_object_t * obj)
{
    filter_t * const p_filter = (filter_t *)obj;
    converter_sys_t * const sys = p_filter->p_sys;

    if (sys == NULL)
        return;

    // Disables input & output ports
    conv_flush(p_filter);

    cma_buf_pool_deletez(&sys->cma_in_pool);
    cma_buf_pool_deletez(&sys->cma_out_pool);

    if (sys->component && sys->component->control->is_enabled)
        mmal_port_disable(sys->component->control);

    if (sys->component && sys->component->is_enabled)
        mmal_component_disable(sys->component);

    if (sys->out_pool)
    {
        if (sys->is_sliced)
            mmal_port_pool_destroy(sys->output, sys->out_pool);
        else
            mmal_pool_destroy(sys->out_pool);
    }

    if (sys->in_pool != NULL)
        mmal_pool_destroy(sys->in_pool);

    if (sys->component)
        mmal_component_release(sys->component);

    cma_vcsm_exit(sys->vcsm_init_type);

    vlc_sem_destroy(&sys->sem);
    vlc_mutex_destroy(&sys->lock);

    p_filter->p_sys = NULL;
    free(sys);
}


static MMAL_FOURCC_T filter_enc_in(const video_format_t * const fmt)
{
    if (hw_mmal_chroma_is_mmal(fmt->i_chroma) || fmt->i_chroma == VLC_CODEC_I420)
        return vlc_to_mmal_video_fourcc(fmt);

    if (fmt->i_chroma == VLC_CODEC_I420_10L)
        return MMAL_ENCODING_I420;

    return 0;
}


static int OpenConverter(vlc_object_t * obj)
{
    filter_t * const p_filter = (filter_t *)obj;
    int ret = VLC_EGENERIC;
    converter_sys_t *sys;
    MMAL_STATUS_T status;
    if (p_filter->fmt_out.video.i_chroma == VLC_CODEC_I420)
        return VLC_EGENERIC;
    const MMAL_FOURCC_T enc_out = vlc_to_mmal_video_fourcc(&p_filter->fmt_out.video);
    const MMAL_FOURCC_T enc_in = filter_enc_in(&p_filter->fmt_in.video);
    bool use_resizer;

    // At least in principle we should deal with any mmal format as input
    if (enc_in == 0 || enc_out == 0)
        return VLC_EGENERIC;

    use_resizer = var_InheritBool(p_filter, MMAL_RESIZE_NAME);

retry:
    // ** Make more generic by checking supported encs
    //
    // Must use ISP - HVS can't do this, nor can resizer
    if (enc_in == MMAL_ENCODING_YUVUV64_10) {
        // If resizer selected then just give up
        if (use_resizer)
            return VLC_EGENERIC;
    }
    // Only HVS can deal with SAND30
    if (enc_in == MMAL_ENCODING_YUV10_COL) {
        if (use_resizer)
            return VLC_EGENERIC;
    }

    // Check we have a sliced version of the fourcc if we want the resizer
    if (use_resizer && pic_to_slice_mmal_fourcc(enc_out) == 0) {
        return VLC_EGENERIC;
    }

    sys = calloc(1, sizeof(converter_sys_t));
    if (!sys) {
        ret = VLC_ENOMEM;
        goto fail;
    }
    p_filter->p_sys = sys;

    // Init stuff the we destroy unconditionaly in Close first
    vlc_mutex_init(&sys->lock);
    vlc_sem_init(&sys->sem, 0);
    sys->err_stream = MMAL_SUCCESS;
    pic_fifo_init(&sys->ret_pics);
    pic_fifo_init(&sys->slice.pics);

    sys->needs_copy_in = !hw_mmal_chroma_is_mmal(p_filter->fmt_in.video.i_chroma);
    sys->in_port_cb_fn = conv_input_port_cb;

    if ((sys->vcsm_init_type = cma_vcsm_init()) == VCSM_INIT_NONE) {
        msg_Err(p_filter, "VCSM init failed");
        goto fail;
    }

    if (use_resizer) {
        sys->resizer_type = FILTER_RESIZER_RESIZER;
        sys->is_sliced = true;
        sys->component_name = MMAL_COMPONENT_DEFAULT_RESIZER;
        sys->out_port_cb_fn = slice_output_port_cb;
    }
    else {
        sys->resizer_type = FILTER_RESIZER_ISP;
        sys->is_sliced = false;  // Copy directly into filter picture
        sys->component_name = MMAL_COMPONENT_ISP_RESIZER;
        sys->out_port_cb_fn = conv_output_port_cb;
    }

    status = mmal_component_create(sys->component_name, &sys->component);
    if (status != MMAL_SUCCESS) {
        msg_Err(p_filter, "Failed to create MMAL component %s (status=%"PRIx32" %s)",
                MMAL_COMPONENT_DEFAULT_VIDEO_DECODER, status, mmal_status_to_string(status));
        goto fail;
    }
    sys->output = sys->component->output[0];
    sys->input  = sys->component->input[0];

    sys->component->control->userdata = (struct MMAL_PORT_USERDATA_T *)p_filter;
    status = mmal_port_enable(sys->component->control, conv_control_port_cb);
    if (status != MMAL_SUCCESS) {
        msg_Err(p_filter, "Failed to enable control port %s (status=%"PRIx32" %s)",
                sys->component->control->name, status, mmal_status_to_string(status));
        goto fail;
    }

    if (sys->needs_copy_in &&
        (sys->cma_in_pool = cma_buf_pool_new(2, 2, true, "conv-copy-in")) == NULL)
    {
        msg_Err(p_filter, "Failed to allocate input CMA pool");
        goto fail;
    }

    sys->input->userdata = (struct MMAL_PORT_USERDATA_T *)p_filter;
    sys->input->format->type = MMAL_ES_TYPE_VIDEO;
    sys->input->format->encoding = enc_in;
    sys->input->format->encoding_variant = MMAL_ENCODING_I420;
    hw_mmal_vlc_fmt_to_mmal_fmt(sys->input->format, &p_filter->fmt_in.video);
    port_parameter_set_bool(sys->input, MMAL_PARAMETER_ZERO_COPY, 1);

    mmal_log_dump_format(sys->input->format);

    status = mmal_port_format_commit(sys->input);
    if (status != MMAL_SUCCESS) {
        msg_Err(p_filter, "Failed to commit format for input port %s (status=%"PRIx32" %s)",
                sys->input->name, status, mmal_status_to_string(status));
        goto fail;
    }
    sys->input->buffer_size = sys->input->buffer_size_recommended;
    sys->input->buffer_num = NUM_DECODER_BUFFER_HEADERS;

    if ((status = conv_enable_in(p_filter, sys)) != MMAL_SUCCESS)
        goto fail;

    port_parameter_set_bool(sys->output, MMAL_PARAMETER_ZERO_COPY, sys->is_sliced);

    status = mmal_component_enable(sys->component);
    if (status != MMAL_SUCCESS) {
        msg_Err(p_filter, "Failed to enable component %s (status=%"PRIx32" %s)",
                sys->component->name, status, mmal_status_to_string(status));
        goto fail;
    }

    if ((sys->in_pool = mmal_pool_create(sys->input->buffer_num, 0)) == NULL)
    {
        msg_Err(p_filter, "Failed to create input pool");
        goto fail;
    }

    p_filter->pf_video_filter = conv_filter;
    p_filter->pf_flush = conv_flush;
    // video_drain NIF in filter structure

    return VLC_SUCCESS;

fail:
    CloseConverter(obj);

    if (!use_resizer && status == MMAL_ENOMEM) {
        use_resizer = true;
        msg_Warn(p_filter, "Lack of memory to use HVS/ISP: trying resizer");
        goto retry;
    }

    return ret;
}

