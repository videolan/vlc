/*****************************************************************************
 * input_decoder_scenario.c: testflight for input_decoder state machine
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define MODULE_NAME test_input_decoder_mock
#define MODULE_STRING "test_input_decoder_mock"
#undef __PLUGIN__

#include <vlc_common.h>
#include <vlc_messages.h>
#include <vlc_player.h>
#include <vlc_interface.h>
#include <vlc_codec.h>

#include "input_decoder.h"

static struct scenario_data
{
    vlc_sem_t wait_stop;
    vlc_sem_t display_prepare_signal;
    vlc_sem_t wait_ready_to_flush;
    struct vlc_video_context *decoder_vctx;
    bool skip_decoder;
} scenario_data;

struct input_decoder_scenario input_decoder_scenarios[] =
{};
size_t input_decoder_scenarios_count = ARRAY_SIZE(input_decoder_scenarios);

void input_decoder_scenario_init(void)
{
    scenario_data.decoder_vctx = NULL;
    scenario_data.skip_decoder = false;
    vlc_sem_init(&scenario_data.wait_stop, 0);
    vlc_sem_init(&scenario_data.display_prepare_signal, 0);
    vlc_sem_init(&scenario_data.wait_ready_to_flush, 0);
}

void input_decoder_scenario_wait(intf_thread_t *intf, struct input_decoder_scenario *scenario)
{
    if (scenario->interface_setup)
        scenario->interface_setup(intf);

    vlc_sem_wait(&scenario_data.wait_stop);
}

void input_decoder_scenario_check(struct input_decoder_scenario *scenario)
{
    (void)scenario;
}
