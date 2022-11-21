/*****************************************************************************
 * input_decoder.h: test for input_decoder state machine
 *****************************************************************************
 * Copyright (C) 2022 VideoLabs
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

typedef struct vout_display_t vout_display_t;
typedef struct intf_thread_t intf_thread_t;

struct input_decoder_scenario {
    const char *source;
    const char *sout;
    void (*decoder_setup)(decoder_t *);
    void (*decoder_destroy)(decoder_t *);
    int (*decoder_decode)(decoder_t *, picture_t *);
    void (*decoder_flush)(decoder_t *);
    void (*display_prepare)(vout_display_t *vd, picture_t *pic);
    void (*interface_setup)(intf_thread_t *intf);
    int (*sout_filter_send)(sout_stream_t *stream, void *id, block_t *block);
    void (*sout_filter_flush)(sout_stream_t *stream, void *id);
};


void input_decoder_scenario_init(void);
void input_decoder_scenario_wait(intf_thread_t *intf, struct input_decoder_scenario *scenario);
void input_decoder_scenario_check(struct input_decoder_scenario *scenario);
extern size_t input_decoder_scenarios_count;
extern struct input_decoder_scenario input_decoder_scenarios[];
