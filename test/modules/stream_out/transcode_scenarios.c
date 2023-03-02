/*****************************************************************************
 * transcode_scenario.c: testflight for transcoding pipeline
 *****************************************************************************
 * Copyright (C) 2021 VideoLabs
 *
 * Author: Alexandre Janniaux <ajanni@videolabs.io>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
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
# include "config.h"
#endif

#define MODULE_NAME test_transcode_mock
#define MODULE_STRING "test_transcode_mock"
#undef VLC_DYNAMIC_PLUGIN

#include <vlc_common.h>
#include <vlc_frame.h>

#include "transcode.h"

#include <vlc_filter.h>

static struct scenario_data
{
    vlc_sem_t wait_stop;
    struct vlc_video_context *decoder_vctx;
    unsigned output_frame_count;
    bool converter_opened;
    bool encoder_opened;
    bool encoder_closed;
    bool error_reported;
} scenario_data;

static void decoder_fixed_size(decoder_t *dec, vlc_fourcc_t chroma,
        unsigned width, unsigned height)
{
    dec->fmt_out.video.i_chroma
        = dec->fmt_out.i_codec
        = chroma;
    dec->fmt_out.video.i_visible_width
        = dec->fmt_out.video.i_width
        = width;
    dec->fmt_out.video.i_visible_height
        = dec->fmt_out.video.i_height
        = height;
}

static void decoder_i420_800_600(decoder_t *dec)
    { decoder_fixed_size(dec, VLC_CODEC_I420, 800, 600); }

static void decoder_nv12_800_600(decoder_t *dec)
    { decoder_fixed_size(dec, VLC_CODEC_NV12, 800, 600); }

static void decoder_i420_800_600_vctx(decoder_t *dec)
{
    /* We use VLC_VIDEO_CONTEXT_VAAPI here but it could be any other kind of
     * video context type since we prevent the usual plugins from loading. */
    struct vlc_video_context *vctx = vlc_video_context_Create(
            NULL, VLC_VIDEO_CONTEXT_VAAPI, 0, NULL);
    assert(vctx);
    dec->p_sys = vctx;
    decoder_i420_800_600(dec);
}

static int decoder_decode_dummy(decoder_t *dec, picture_t *pic)
{
    int ret = decoder_UpdateVideoOutput(dec, NULL);
    assert(ret == VLC_SUCCESS);
    decoder_QueueVideo(dec, pic);
    return VLC_SUCCESS;
}

/* Picture context implementation */
static void picture_context_destroy(struct picture_context_t *ctx)
    { free(ctx); }

static struct picture_context_t *
picture_context_copy(struct picture_context_t *ctx)
{
    struct picture_context_t *copy = malloc(sizeof *copy);
    copy = ctx;
    copy->vctx = vlc_video_context_Hold(ctx->vctx);
    return copy;
}

static int decoder_decode_vctx(decoder_t *dec, picture_t *pic)
{
    struct vlc_video_context *vctx = dec->p_sys;
    assert(vctx);
    scenario_data.decoder_vctx = vctx;

    int ret = decoder_UpdateVideoOutput(dec, vctx);
    assert(ret == VLC_SUCCESS);

    picture_context_t *context = malloc(sizeof *context);
    assert(context);
    context->destroy = picture_context_destroy;
    context->copy = picture_context_copy;
    context->vctx = vlc_video_context_Hold(vctx);
    pic->context = context;
    pic->format.i_chroma = dec->fmt_out.video.i_chroma;
    decoder_QueueVideo(dec, pic);
    return VLC_SUCCESS;
}

static int decoder_decode_vctx_update(decoder_t *dec, picture_t *pic)
{
    bool should_switch = scenario_data.decoder_vctx != NULL;

    if (should_switch)
    {
        switch (dec->fmt_out.i_codec)
        {
            case VLC_CODEC_I420:
                msg_Dbg(dec, "Switching from I420 to NV12");
                dec->fmt_out.video.i_chroma
                    = dec->fmt_out.i_codec
                    = VLC_CODEC_NV12;
                break;
            default:
                break;
        }
    }
    decoder_decode_vctx(dec, pic);
    return VLC_SUCCESS;
}

static int decoder_decode_error(decoder_t *dec, picture_t *pic)
{
    (void)dec;
    picture_Release(pic);
    return VLC_EGENERIC;
}

static void wait_error_reported(sout_stream_t *stream)
{
    (void)stream;
    vlc_sem_post(&scenario_data.wait_stop);
}

static void encoder_fixed_size(encoder_t *enc, vlc_fourcc_t chroma,
        unsigned width, unsigned height)
{
    assert(!scenario_data.encoder_opened);
    msg_Info(enc, "Setting up the encoder %4.4s: %ux%u",
             (const char *)&chroma, width, height);
    enc->fmt_in.video.i_chroma
        = enc->fmt_in.i_codec
        = chroma;
    enc->fmt_in.video.i_visible_width
        = enc->fmt_in.video.i_width
        = width;
    enc->fmt_in.video.i_visible_height
        = enc->fmt_in.video.i_height
        = height;
    scenario_data.encoder_opened = true;
}

static void encoder_i420_800_600(encoder_t *enc)
    { encoder_fixed_size(enc, VLC_CODEC_I420, 800, 600); }

static void encoder_nv12_800_600(encoder_t *enc)
    { encoder_fixed_size(enc, VLC_CODEC_NV12, 800, 600); }

static void encoder_i420_800_600_vctx(encoder_t *enc)
{
    encoder_fixed_size(enc, VLC_CODEC_I420, 800, 600);
    assert(scenario_data.decoder_vctx != NULL);
    assert(enc->vctx_in == scenario_data.decoder_vctx);
}

#if 0
static void encoder_nv12_800_600_no_vctx(encoder_t *enc)
{
    encoder_fixed_size(enc, VLC_CODEC_NV12, 800, 600);
    assert(enc->vctx_in == NULL);
}

static void encoder_i420_800_600_no_vctx(encoder_t *enc)
{
    encoder_fixed_size(enc, VLC_CODEC_I420, 800, 600);
    assert(enc->vctx_in == NULL);
}
#endif

static void encoder_encode_dummy(encoder_t *enc, picture_t *pic)
{
    (void)enc; (void)pic;
    msg_Info(enc, "Encode");
}

static void encoder_close(encoder_t *enc)
{
    (void)enc;
    scenario_data.encoder_closed = true;
}

static void wait_output_10_frames_reported(const vlc_frame_t *out)
{
    // Count frame output.
    for (; out != NULL; out = out->p_next )
        ++scenario_data.output_frame_count;

    if (scenario_data.output_frame_count == 10)
        vlc_sem_post(&scenario_data.wait_stop);
}

static void wait_output_reported(const vlc_frame_t *out)
{
    (void)out;
    vlc_sem_post(&scenario_data.wait_stop);
}

static void converter_fixed_size(filter_t *filter, vlc_fourcc_t chroma_in,
        vlc_fourcc_t chroma_out, unsigned width, unsigned height)
{
    assert(filter->fmt_in.video.i_width == width);
    assert(filter->fmt_in.video.i_visible_width == width);
    assert(filter->fmt_in.video.i_height == height);
    assert(filter->fmt_in.video.i_visible_height == height);

    assert(filter->fmt_out.video.i_width == width);
    assert(filter->fmt_out.video.i_visible_width == width);
    assert(filter->fmt_out.video.i_height == height);
    assert(filter->fmt_out.video.i_visible_height == height);

    assert(filter->fmt_in.video.i_chroma == chroma_in);
    assert(filter->fmt_out.video.i_chroma == chroma_out);

    scenario_data.converter_opened = true;
}

static void converter_i420_to_nv12_800_600(filter_t *filter)
    { converter_fixed_size(filter, VLC_CODEC_I420, VLC_CODEC_NV12, 800, 600); }

static void converter_nv12_to_i420_800_600(filter_t *filter)
    { converter_fixed_size(filter, VLC_CODEC_NV12, VLC_CODEC_I420, 800, 600); }

static void converter_nv12_to_i420_800_600_vctx(filter_t *filter)
{
    converter_fixed_size(filter, VLC_CODEC_NV12, VLC_CODEC_I420, 800, 600);
    assert(filter->vctx_in == scenario_data.decoder_vctx);
}

static void converter_i420_to_nv12_800_600_vctx(filter_t *filter)
{
    converter_fixed_size(filter, VLC_CODEC_I420, VLC_CODEC_NV12, 800, 600);
    assert(filter->vctx_in == scenario_data.decoder_vctx);
}

const char source_800_600[] = "mock://video_track_count=1;length=100000000000;video_width=800;video_height=600";
struct transcode_scenario transcode_scenarios[] =
{{
    .source = source_800_600,
    .sout = "sout=#transcode:output_checker",
    .decoder_setup = decoder_i420_800_600,
    .decoder_decode = decoder_decode_dummy,
    .encoder_setup = encoder_i420_800_600,
    .encoder_encode = encoder_encode_dummy,
    .encoder_close = encoder_close,
    .report_output = wait_output_reported,
},{
    .source = source_800_600,
    .sout = "sout=#transcode:output_checker",
    .decoder_setup = decoder_nv12_800_600,
    .decoder_decode = decoder_decode_dummy,
    .encoder_setup = encoder_nv12_800_600,
    .encoder_encode = encoder_encode_dummy,
    .encoder_close = encoder_close,
    .report_output = wait_output_reported,
},{
    .source = source_800_600,
    .sout = "sout=#transcode:output_checker",
    .decoder_setup = decoder_i420_800_600,
    .decoder_decode = decoder_decode_dummy,
    .encoder_setup = encoder_nv12_800_600,
    .encoder_encode = encoder_encode_dummy,
    .encoder_close = encoder_close,
    .converter_setup = converter_i420_to_nv12_800_600,
    .report_output = wait_output_reported,
},{
    .source = source_800_600,
    .sout = "sout=#transcode:output_checker",
    .decoder_setup = decoder_nv12_800_600,
    .decoder_decode = decoder_decode_dummy,
    .encoder_setup = encoder_i420_800_600,
    .encoder_encode = encoder_encode_dummy,
    .encoder_close = encoder_close,
    .converter_setup = converter_nv12_to_i420_800_600,
    .report_output = wait_output_reported,
},{
    .source = source_800_600,
    .sout = "sout=#transcode:output_checker",
    .decoder_setup = decoder_i420_800_600_vctx,
    .decoder_decode = decoder_decode_vctx,
    .encoder_setup = encoder_i420_800_600_vctx,
    .encoder_encode = encoder_encode_dummy,
    .encoder_close = encoder_close,
    .report_output = wait_output_reported,
},{
    .source = source_800_600,
    .sout = "sout=#transcode:output_checker",
    .decoder_setup = decoder_i420_800_600_vctx,
    .decoder_decode = decoder_decode_vctx,
    .encoder_setup = encoder_nv12_800_600,
    .encoder_encode = encoder_encode_dummy,
    .encoder_close = encoder_close,
    .converter_setup = converter_i420_to_nv12_800_600,
    .report_output = wait_output_reported,
},{
    /* Make sure fps filter in transcode will forward the video context */
    .source = source_800_600,
    .sout = "sout=#transcode{fps=1}:output_checker",
    .decoder_setup = decoder_i420_800_600_vctx,
    .decoder_decode = decoder_decode_vctx,
    .encoder_setup = encoder_i420_800_600_vctx,
    .encoder_encode = encoder_encode_dummy,
    .encoder_close = encoder_close,
    .report_output = wait_output_reported,
},{
    // - Decoder format with video context
    // - Encoder format request a different chroma
    // - Converter must convert from one to the other
    //   but it doesn't forward any video context
    /* Make sure converter will receive the video context */
    .source = source_800_600,
    .sout = "sout=#transcode:output_checker",
    .decoder_setup = decoder_i420_800_600_vctx,
    .decoder_decode = decoder_decode_vctx,
    .encoder_setup = encoder_nv12_800_600,
    .encoder_encode = encoder_encode_dummy,
    .encoder_close = encoder_close,
    .converter_setup = converter_i420_to_nv12_800_600_vctx,
    .report_output = wait_output_reported,
},{
    /* Make sure a change in format will lead to the addition of a converter.
     * Here, decoder_decode_vctx_update will change format after the first
     * frame. */
    .source = source_800_600,
    .sout = "sout=#transcode:output_checker",
    .decoder_setup = decoder_i420_800_600_vctx,
    .decoder_decode = decoder_decode_vctx_update,
    .encoder_setup = encoder_i420_800_600,
    .encoder_encode = encoder_encode_dummy,
    .encoder_close = encoder_close,
    .converter_setup = converter_nv12_to_i420_800_600_vctx,
    .report_output = wait_output_10_frames_reported,
},{
    /* Ensure that error are correctly forwarded back to the stream output
     * pipeline. */
    .source = source_800_600,
    .sout = "sout=#error_checker:transcode:dummy",
    .decoder_setup = decoder_i420_800_600,
    .decoder_decode = decoder_decode_error,
    .report_error = wait_error_reported,
    .encoder_close = encoder_close,
}};
size_t transcode_scenarios_count = ARRAY_SIZE(transcode_scenarios);

void transcode_scenario_init(void)
{
    scenario_data.decoder_vctx = NULL;
    scenario_data.output_frame_count = 0;
    scenario_data.converter_opened = false;
    scenario_data.encoder_opened = false;
    vlc_sem_init(&scenario_data.wait_stop, 0);
}

void transcode_scenario_wait(struct transcode_scenario *scenario)
{
    (void)scenario;
    vlc_sem_wait(&scenario_data.wait_stop);
}

void transcode_scenario_check(struct transcode_scenario *scenario)
{
    if (scenario->converter_setup != NULL)
        assert(scenario_data.converter_opened);

    if (scenario->encoder_setup != NULL)
        assert(scenario_data.encoder_opened);

    if (scenario_data.encoder_opened && scenario->encoder_close != NULL)
        assert(scenario_data.encoder_closed);
}
