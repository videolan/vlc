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

static int decoder_decode_check_cc(decoder_t *dec, picture_t *pic)
{
    vlc_tick_t date = pic->date;
    picture_Release(pic);

    block_t *p_cc = block_Alloc( 1 );
    if (p_cc == NULL)
        return VLC_ENOMEM;

    p_cc->i_dts = p_cc->i_pts = date;

    decoder_cc_desc_t desc = {
        .i_608_channels = 1,
    };
    decoder_QueueCc( dec, p_cc, &desc );

    vlc_sem_post(&scenario_data.wait_ready_to_flush);

    return VLC_SUCCESS;
}
static void decoder_flush_signal(decoder_t *dec)
{
    (void)dec;
    vlc_sem_post(&scenario_data.wait_stop);
}

static void PlayerOnTrackListChanged(vlc_player_t *player,
        enum vlc_player_list_action action,
        const struct vlc_player_track *track,
        void *data)
{
    (void)data;

    if (action != VLC_PLAYER_LIST_ADDED ||
        track->fmt.i_cat != SPU_ES)
        return;

    vlc_player_SelectTrack(player, track, VLC_PLAYER_SELECT_EXCLUSIVE);
}

static void interface_setup_select_cc(intf_thread_t *intf)
{
    vlc_player_t *player = (vlc_player_t *)intf->p_sys;

    static const struct vlc_player_cbs player_cbs =
    {
        .on_track_list_changed = PlayerOnTrackListChanged,
    };

    vlc_player_Lock(player);
    vlc_player_listener_id *listener_id =
        vlc_player_AddListener(player, &player_cbs, NULL);
    vlc_player_Unlock(player);

    vlc_sem_wait(&scenario_data.wait_ready_to_flush);
    vlc_player_Lock(player);
    vlc_player_SetPosition(player, 0);
    vlc_player_Unlock(player);

    vlc_sem_wait(&scenario_data.wait_ready_to_flush);
    vlc_player_Lock(player);
    vlc_player_RemoveListener(player, listener_id);
    vlc_player_Unlock(player);
}

const char source_800_600[] = "mock://video_track_count=1;length=100000000000;video_width=800;video_height=600";
struct input_decoder_scenario input_decoder_scenarios[] =
{{
    .source = source_800_600,
    .decoder_setup = decoder_i420_800_600,
    .decoder_flush = decoder_flush_signal,
    .decoder_decode = decoder_decode_check_cc,
    .interface_setup = interface_setup_select_cc,
}};
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
