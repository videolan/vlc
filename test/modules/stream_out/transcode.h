/*****************************************************************************
 * transcode.h: test for transcoding pipeline
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

#define MODULE_NAME test_transcode_mock
#define MODULE_STRING "test_transcode_mock"
#undef __PLUGIN__

#include <vlc_fourcc.h>

#define TEST_FLAG_CONVERTER 0x01
#define TEST_FLAG_FILTER 0x02

struct transcode_scenario {
    const char *source;
    const char *sout;
    void (*decoder_setup)(decoder_t *);
    int (*decoder_decode)(decoder_t *, picture_t *);
    void (*encoder_setup)(encoder_t *);
    void (*encoder_close)(encoder_t *);
    void (*encoder_encode)(encoder_t *, picture_t *);
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
