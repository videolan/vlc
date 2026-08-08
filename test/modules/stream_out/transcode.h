/*****************************************************************************
 * transcode.h: test for transcoding pipeline
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

#include <vlc_fourcc.h>

#define TEST_FLAG_CONVERTER 0x01
#define TEST_FLAG_FILTER 0x02

/* Which stage a picture was released from, so that the stream output can
 * tell the frames pending in the encoder output from the frames that the
 * encoder only released when it got drained. */
enum test_output_origin
{
    TEST_OUTPUT_FROM_DECODE,
    TEST_OUTPUT_FROM_DECODER_DRAIN,
    TEST_OUTPUT_FROM_ENCODER_DRAIN,
};

/* Stamp the decoder puts on every picture it releases, in picture_t.p_sys,
 * and that the encoder copies as-is into the frame it produces from it. */
struct test_output
{
    uint32_t seq;
    enum test_output_origin origin;
};

struct transcode_scenario {
    const char *source;
    const char *sout;
    void (*decoder_setup)(decoder_t *);
    int (*decoder_decode)(decoder_t *, picture_t *);
    int (*decoder_drain)(decoder_t *);
    void (*encoder_setup)(encoder_t *);
    void (*encoder_close)(encoder_t *);
    void (*encoder_encode)(encoder_t *, picture_t *, vlc_frame_t *);
    vlc_frame_t *(*encoder_drain)(encoder_t *);
    void (*filter_setup)(filter_t *);
    void (*converter_setup)(filter_t *);
    void (*report_error)(sout_stream_t *);
    void (*report_output)(const vlc_frame_t *);
};


void transcode_scenario_init(void);
void transcode_scenario_wait(struct transcode_scenario *scenario);
void transcode_scenario_check(struct transcode_scenario *scenario);
extern size_t transcode_scenarios_count;
extern struct transcode_scenario transcode_scenarios[];
