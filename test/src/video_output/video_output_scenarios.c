/*****************************************************************************
 * vout_scenario.c: testflight for video output pipeline
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

#define MODULE_NAME test_vout_mock
#define MODULE_STRING "test_vout_mock"
#undef __PLUGIN__

#include <vlc_common.h>
#include "video_output.h"

#include <vlc_filter.h>
#include <vlc_vout_display.h>

static struct scenario_data
{
    vlc_sem_t wait_stop;
    struct vlc_video_context *decoder_vctx;
    unsigned display_picture_count;
    bool converter_opened;
    bool display_opened;

    vlc_fourcc_t display_chroma;
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

static void decoder_rgba_800_600(decoder_t *dec)
    { decoder_fixed_size(dec, VLC_CODEC_RGBA, 800, 600); }

static void decoder_decode_change_chroma(decoder_t *dec, picture_t *pic)
{
    static const vlc_fourcc_t chroma_list[] = {
        VLC_CODEC_RGBA,
        VLC_CODEC_I420,
        VLC_CODEC_NV12,
    };

    size_t index = 0;
    while (index < ARRAY_SIZE(chroma_list))
    {
        index++;
        if (chroma_list[index - 1] == dec->fmt_out.video.i_chroma)
            break;
    }

    /* Limit to the last chroma */
    if (index >= ARRAY_SIZE(chroma_list))
        index = ARRAY_SIZE(chroma_list);

    /* Switch to the new chroma */
    dec->fmt_out.video.i_chroma
        = dec->fmt_out.i_codec
        = chroma_list[index];

    int ret = decoder_UpdateVideoOutput(dec, NULL);
    //assert(ret == VLC_SUCCESS);
    if (ret != VLC_SUCCESS)
    {
        vlc_sem_post(&scenario_data.wait_stop);
        return;
    }

    /* Simulate the chroma change */
    pic->format.i_chroma = chroma_list[index];
    decoder_QueueVideo(dec, pic);
}

static int display_fixed_size(vout_display_t *vd, video_format_t *fmtp,
        struct vlc_video_context *vctx, vlc_fourcc_t chroma,
        unsigned width, unsigned height)
{
    (void)fmtp; (void)vctx;
    msg_Info(vd, "Setting up the display %4.4s: %ux%u",
             (const char *)&chroma, width, height);

    scenario_data.display_opened = true;
    return VLC_SUCCESS;
}

static int display_fail_second_time(vout_display_t *vd, video_format_t *fmtp,
        struct vlc_video_context *vctx, unsigned width, unsigned height)
{
    (void)vctx;

    vlc_fourcc_t chroma = fmtp->i_chroma;
    if (scenario_data.display_opened)
    {
        msg_Info(vd, "Failing the display %4.4s: %ux%u",
                 (const char *)&chroma, width, height);
        return VLC_EGENERIC;
    }

    return display_fixed_size(vd, fmtp, vctx, chroma, 800, 600);
}

static int display_800_600_fail_second_time(
        vout_display_t *vd, video_format_t *fmtp,
        struct vlc_video_context *vctx)
    { return display_fail_second_time(vd, fmtp, vctx, 800, 600); }

const char source_800_600[] = "mock://video_track_count=1;length=100000000000;video_width=800;video_height=600";
struct vout_scenario vout_scenarios[] =
{{
    .source = source_800_600,
    .decoder_setup = decoder_rgba_800_600,
    .decoder_decode = decoder_decode_change_chroma,
    .display_setup = display_800_600_fail_second_time,
}};
size_t vout_scenarios_count = ARRAY_SIZE(vout_scenarios);

void vout_scenario_init(void)
{
    scenario_data.decoder_vctx = NULL;
    scenario_data.display_picture_count = 0;
    scenario_data.converter_opened = false;
    scenario_data.display_opened = false;
    vlc_sem_init(&scenario_data.wait_stop, 0);
}

void vout_scenario_wait(struct vout_scenario *scenario)
{
    vlc_sem_wait(&scenario_data.wait_stop);
    if (scenario->converter_setup != NULL)
        assert(scenario_data.converter_opened);

    if (scenario->display_setup != NULL)
        assert(scenario_data.display_opened);
}
