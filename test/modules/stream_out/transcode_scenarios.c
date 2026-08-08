/*****************************************************************************
 * transcode_scenario.c: testflight for transcoding pipeline
 *****************************************************************************
 * Copyright (C) 2021-2026 VideoLabs
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

    /* To check the order of frames */
    uint32_t decode_seq;
    vlc_tick_t decode_date;
    uint32_t output_seq;
    uint32_t drained_count;
    bool drain_reported;
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

static void encoder_encode_dummy(encoder_t *enc, picture_t *pic,
                                 vlc_frame_t *out)
{
    (void)enc; (void)pic; (void)out;
    msg_Info(enc, "Encode");
}

#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_(x)

/* Number of frames generated by source_800_600_eos before EOS. */
#define DRAIN_ORDER_SOURCE_FRAME_COUNT 4

/* Number of frames the decoder releases when it is drained. They are encoded
 * right away and pending in the transcode output when the encoder is drained
 * in turn. */
#define DRAIN_ORDER_HELD_FRAME_COUNT 5

/* Number of frames the encoder releases when it is drained, like an encoder
 * flushing the frames it was still holding for its lookahead. */
#define DRAIN_ORDER_DRAINED_FRAME_COUNT 3

static void picture_stamp_destroy(picture_t *pic)
{
    free(pic->p_sys);
}

/* Stamp every picture output by the decoder with the order it was decoded at
 * and the stage it comes from. */
static picture_t *decoder_new_stamped_picture(decoder_t *dec,
                                              enum test_output_origin origin)
{
    struct test_output *stamp = malloc(sizeof *stamp);
    assert(stamp != NULL);
    stamp->seq = scenario_data.decode_seq++;
    stamp->origin = origin;

    const picture_resource_t resource = {
        .p_sys = stamp,
        .pf_destroy = picture_stamp_destroy,
    };
    picture_t *pic = picture_NewFromResource(&dec->fmt_out.video, &resource);
    assert(pic != NULL);
    return pic;
}

static int decoder_decode_stamped(decoder_t *dec, picture_t *pic)
{
    int ret = decoder_UpdateVideoOutput(dec, NULL);
    assert(ret == VLC_SUCCESS);

    picture_t *stamped =
        decoder_new_stamped_picture(dec, TEST_OUTPUT_FROM_DECODE);
    stamped->date = pic->date;
    picture_Release(pic);

    scenario_data.decode_date = stamped->date;
    decoder_QueueVideo(dec, stamped);
    return VLC_SUCCESS;
}

static int decoder_drain_queue_held_frames(decoder_t *dec)
{
    /* Burst the pictures out of the decoder directly to the encoder in
     * one decoder::pf_decode() call. */
    for (size_t i = 0; i < DRAIN_ORDER_HELD_FRAME_COUNT; ++i)
    {
        picture_t *pic =
            decoder_new_stamped_picture(dec, TEST_OUTPUT_FROM_DECODER_DRAIN);
        pic->date = ++scenario_data.decode_date;
        decoder_QueueVideo(dec, pic);
    }
    return VLC_SUCCESS;
}

static void encoder_encode_stamped(encoder_t *enc, picture_t *pic,
                                   vlc_frame_t *out)
{
    const struct test_output *stamp = pic->p_sys;

    /* The stamp must have gone through the transcode pipeline untouched. */
    assert(stamp != NULL);
    assert(out->i_buffer == sizeof *stamp);
    memcpy(out->p_buffer, stamp, sizeof *stamp);
    msg_Info(enc, "Encode %u", stamp->seq);
}

static vlc_frame_t *encoder_drain_held_frames(encoder_t *enc)
{
    /* The encoder is drained one frame at a time, until it reports that it has
     * nothing left. Those frames have no picture to read stamps from, so they
     * continue the numbering of the decoder, which is done releasing pictures at
     * this point. */

    if (scenario_data.drained_count == DRAIN_ORDER_DRAINED_FRAME_COUNT)
        return NULL;

    const struct test_output output = {
        .seq = scenario_data.decode_seq + scenario_data.drained_count,
        .origin = TEST_OUTPUT_FROM_ENCODER_DRAIN,
    };

    vlc_frame_t *out = vlc_frame_Alloc(sizeof output);
    assert(out != NULL);
    memcpy(out->p_buffer, &output, sizeof output);
    msg_Info(enc, "Encode %u, from the drain", output.seq);
    scenario_data.drained_count++;
    return out;
}

static void report_output_in_decode_order(const vlc_frame_t *out)
{
    for (; out != NULL; out = out->p_next)
    {
        struct test_output output;

        assert(!scenario_data.drain_reported);
        assert(out->i_buffer == sizeof output);
        memcpy(&output, out->p_buffer, sizeof output);

        /* Check output order vs decoder order */
        assert(output.seq == scenario_data.output_seq);
        scenario_data.output_seq++;

        /* Ensure the frame order and count to match what we expect in the
         * test. We want every frames to be acknowledged in the right order. */
        switch (output.origin)
        {
            case TEST_OUTPUT_FROM_DECODE:
                assert(output.seq < DRAIN_ORDER_SOURCE_FRAME_COUNT);
                break;
            case TEST_OUTPUT_FROM_DECODER_DRAIN:
                assert(output.seq >= DRAIN_ORDER_SOURCE_FRAME_COUNT);
                assert(output.seq < DRAIN_ORDER_SOURCE_FRAME_COUNT
                                  + DRAIN_ORDER_HELD_FRAME_COUNT);
                break;
            case TEST_OUTPUT_FROM_ENCODER_DRAIN:
                assert(output.seq >= DRAIN_ORDER_SOURCE_FRAME_COUNT
                                   + DRAIN_ORDER_HELD_FRAME_COUNT);
                assert(output.seq < DRAIN_ORDER_SOURCE_FRAME_COUNT
                                  + DRAIN_ORDER_HELD_FRAME_COUNT
                                  + DRAIN_ORDER_DRAINED_FRAME_COUNT);
                scenario_data.drain_reported =
                    output.seq == DRAIN_ORDER_SOURCE_FRAME_COUNT
                                + DRAIN_ORDER_HELD_FRAME_COUNT
                                + DRAIN_ORDER_DRAINED_FRAME_COUNT - 1;
                break;
            default:
                vlc_assert_unreachable();
        }
    }
}

static void encoder_close(encoder_t *enc)
{
    (void)enc;
    scenario_data.encoder_closed = true;
}

static void encoder_close_drain_order(encoder_t *enc)
{
    encoder_close(enc);

    /* The encoder is closed once the whole pipeline has been drained and the
     * output reported, so the scenario checks can run. */
    vlc_sem_post(&scenario_data.wait_stop);
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

/* Same source, but ending after a few frames so that the transcode pipeline
 * is drained by the end of stream instead of by the stop. */
static const char source_800_600_eos[] = "mock://video_track_count=1;video_image_count="
    STRINGIFY(DRAIN_ORDER_SOURCE_FRAME_COUNT) ";video_width=800;video_height=600";
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
},{
    /* The transcode must respect the encoder output order when
     * submitted the drained pictures. */
    .source = source_800_600_eos,
    .sout = "sout=#transcode:output_checker",
    .decoder_setup = decoder_i420_800_600,
    .decoder_decode = decoder_decode_stamped,
    .decoder_drain = decoder_drain_queue_held_frames,
    .encoder_setup = encoder_i420_800_600,
    .encoder_encode = encoder_encode_stamped,
    .encoder_drain = encoder_drain_held_frames,
    .encoder_close = encoder_close_drain_order,
    .report_output = report_output_in_decode_order,
}};
size_t transcode_scenarios_count = ARRAY_SIZE(transcode_scenarios);

void transcode_scenario_init(void)
{
    scenario_data.decoder_vctx = NULL;
    scenario_data.output_frame_count = 0;
    scenario_data.converter_opened = false;
    scenario_data.encoder_opened = false;
    scenario_data.encoder_closed = false;
    scenario_data.decode_seq = 0;
    scenario_data.decode_date = VLC_TICK_INVALID;
    scenario_data.output_seq = 0;
    scenario_data.drained_count = 0;
    scenario_data.drain_reported = false;
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

    if (scenario->encoder_drain != NULL)
    {
        /* Check that everything was output correctly */
        assert(scenario_data.drained_count == DRAIN_ORDER_DRAINED_FRAME_COUNT);
        assert(scenario_data.drain_reported);
        assert(scenario_data.decode_seq == DRAIN_ORDER_SOURCE_FRAME_COUNT
                                         + DRAIN_ORDER_HELD_FRAME_COUNT);

        /* The frames released by the decoder during its drain were still
         * pending when the encoder was drained. They must all have been
         * reported, and the drained frames after them. */
        assert(scenario_data.output_seq == scenario_data.decode_seq
                                         + DRAIN_ORDER_DRAINED_FRAME_COUNT);
    }
}
