/*****************************************************************************
 * video_output.h: test for the video output pipeline
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

#define MODULE_NAME test_vout_mock
#define MODULE_STRING "test_vout_mock"
#undef __PLUGIN__

#include <vlc_fourcc.h>
#include <vlc_vout_display.h>


#define TEST_FLAG_CONVERTER 0x01
#define TEST_FLAG_FILTER 0x02

struct vout_scenario {
    const char *source;
    void (*decoder_setup)(decoder_t *);
    void (*decoder_decode)(decoder_t *, block_t *);
    int  (*display_setup)(vout_display_t *, video_format_t *,
                          struct vlc_video_context *);
    void (*display_prepare)(vout_display_t *, picture_t *);
    void (*display_display)(vout_display_t *, picture_t *);
    void (*filter_setup)(filter_t *);
    void (*converter_setup)(filter_t *);
};


void vout_scenario_init(void);
void vout_scenario_wait(struct vout_scenario *scenario);
extern size_t vout_scenarios_count;
extern struct vout_scenario vout_scenarios[];
