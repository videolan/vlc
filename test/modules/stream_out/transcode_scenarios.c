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
#undef __PLUGIN__

#include <vlc_common.h>
#include "transcode.h"

#include <vlc_filter.h>

static struct scenario_data
{
    vlc_sem_t wait_stop;
    struct vlc_video_context *decoder_vctx;
    unsigned encoder_picture_count;
    bool converter_opened;
    bool encoder_opened;
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

const char source_800_600[] = "mock://video_track_count=1;length=100000000000;video_width=800;video_height=600";
struct transcode_scenario transcode_scenarios[] =
{{
    /* Ensure that error are correctly forwarded back to the stream output
     * pipeline. */
    .source = source_800_600,
    .sout = "sout=#error_checker:transcode:dummy",
    .decoder_setup = decoder_i420_800_600,
    .decoder_decode = decoder_decode_error,
    .report_error = wait_error_reported,
}};

size_t transcode_scenarios_count = ARRAY_SIZE(transcode_scenarios);

void transcode_scenario_init(void)
{
    scenario_data.decoder_vctx = NULL;
    scenario_data.encoder_picture_count = 0;
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
}
