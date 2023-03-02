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

#define MODULE_STRING "test_input_decoder_mock"

#include <vlc_common.h>
#include <vlc_messages.h>
#include <vlc_player.h>
#include <vlc_interface.h>
#include <vlc_codec.h>
#include <vlc_vout_display.h>

#include "input_decoder.h"

static struct scenario_data
{
    vlc_sem_t wait_stop;
    vlc_sem_t display_prepare_signal;
    vlc_sem_t wait_ready_to_flush;
    struct vlc_video_context *decoder_vctx;
    bool skip_decoder;
    bool has_reload;
    bool stream_out_sent;
    size_t decoder_image_sent;
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

static void decoder_i420_800_600_stop(decoder_t *dec)
{
    decoder_i420_800_600(dec);
    vlc_sem_post(&scenario_data.wait_stop);
}

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

struct picture_watcher_context {
    picture_context_t context;
    vlc_sem_t wait_picture;
    vlc_sem_t wait_prepare;
    vlc_atomic_rc_t rc;
};

static void context_destroy(picture_context_t *context)
{
    struct picture_watcher_context *watcher =
        container_of(context, struct picture_watcher_context, context);

    if (vlc_atomic_rc_dec(&watcher->rc))
        vlc_sem_post(&watcher->wait_picture);
}

static picture_context_t * context_copy(picture_context_t *context)
{
    struct picture_watcher_context *watcher =
        container_of(context, struct picture_watcher_context, context);
    vlc_atomic_rc_inc(&watcher->rc);
    return context;
}

static int decoder_decode_drop(decoder_t *dec, picture_t *pic)
{
    (void)dec;
    picture_Release(pic);
    return VLC_SUCCESS;
}

static int decoder_decode_check_flush_video(decoder_t *dec, picture_t *pic)
{
    if (scenario_data.skip_decoder)
    {
        picture_Release(pic);
        return VLC_SUCCESS;
    }

    /* Only update the output format the first time. */
    if (scenario_data.decoder_image_sent == 0)
    {
        int ret = decoder_UpdateVideoOutput(dec, NULL);
        assert(ret == VLC_SUCCESS);
    }

    /* Workaround: the input decoder needs multiple frame (at least 2
     * currently) to start correctly. We're not testing this, but we're
     * testing the flush here, so provide additional picture at the
     * beginning of the test to avoid issues with this.
     * They must not be linked to any picture context. */
    if (scenario_data.decoder_image_sent < 3)
    {
        msg_Info(dec, "Queueing workaround picture number %zu", scenario_data.decoder_image_sent);
        decoder_QueueVideo(dec, pic);
        scenario_data.decoder_image_sent++;
        return VLC_SUCCESS;
    }

    /* Now the input decoder should be completely started. */

    struct picture_watcher_context context1 = {
        .context.destroy = context_destroy,
        .context.copy = context_copy,
    };
    vlc_sem_init(&context1.wait_picture, 0);
    vlc_atomic_rc_init(&context1.rc);

    picture_t *second_pic = picture_Clone(pic);
    second_pic->b_force = true;
    second_pic->date = VLC_TICK_0;
    second_pic->b_progressive = true;
    second_pic->context = &context1.context;

    msg_Info(dec, "Send first frame from decoder to video output");
    decoder_QueueVideo(dec, second_pic);

    msg_Info(dec, "Wait for the display to prepare the frame");
    vlc_sem_wait(&context1.wait_prepare);

    msg_Info(dec, "Trigger decoder and vout flush");
    vlc_sem_post(&scenario_data.wait_ready_to_flush);

    /* Wait for the picture to be flushed by the vout. */
    msg_Info(dec, "Wait for the picture to be flushed from the vout");
    vlc_sem_wait(&context1.wait_picture);

    /* Reinit the picture context waiter. */

    struct picture_watcher_context context2 = {
        .context.destroy = context_destroy,
        .context.copy = context_copy,
    };
    vlc_sem_init(&context2.wait_picture, 0);
    vlc_atomic_rc_init(&context2.rc);

    picture_t *third_pic = picture_Clone(pic);
    third_pic->b_force = true;
    third_pic->date = VLC_TICK_0 + 1;
    third_pic->b_progressive = true;
    third_pic->context = &context2.context;

    msg_Info(dec, "Re-queue the picture to the vout before decoder::pf_flush is called");
    decoder_QueueVideo(dec, third_pic);

    /* Wait for the picture to be flushed by the input decoder. */
    msg_Info(dec, "Ensure the picture has been discarded by the input decoder");
    vlc_sem_wait(&context2.wait_picture);

    /* Ok, since the picture has been released, we should have decode succeed
     * and we can now check that flush is called. */

    picture_Release(pic);

    msg_Info(dec, "Nothing to do from pf_decode, let pf_flush be called by the input decoder");
    scenario_data.skip_decoder = true;
    return VLC_SUCCESS;
}

static void decoder_flush_signal(decoder_t *dec)
{
    (void)dec;
    vlc_sem_post(&scenario_data.wait_stop);
}

static void* SendUpdateOutput(void *opaque)
{
    decoder_t *dec = opaque;
    decoder_UpdateVideoOutput(dec, NULL);
    return NULL;
}

static void decoder_destroy_trigger_update(decoder_t *dec)
{
    /* Use another thread to ensure we don't double-lock, but
     * at most deadlock instead. */
    vlc_thread_t thread;
    int ret = vlc_clone(&thread, SendUpdateOutput, dec);
    assert(ret == VLC_SUCCESS);
    vlc_join(thread, NULL);
}

static int decoder_decode_trigger_reload(decoder_t *dec, picture_t *pic)
{
    (void)dec;
    picture_Release(pic);

    if (!scenario_data.has_reload)
    {
        vlc_sem_post(&scenario_data.wait_stop);
        return VLCDEC_RELOAD;
    }
    return VLCDEC_SUCCESS;
}

static void display_prepare_signal(vout_display_t *vd, picture_t *pic)
{
    (void)vd;

    if (pic->context == NULL)
        return;

    msg_Info(vd, "Signal that the frame has been prepared from display");
    struct picture_watcher_context *watcher =
        container_of(pic->context, struct picture_watcher_context, context);
    vlc_sem_post(&watcher->wait_prepare);
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

static void interface_setup_check_flush(intf_thread_t *intf)
{
    vlc_player_t *player = (vlc_player_t *)intf->p_sys;
    vlc_sem_wait(&scenario_data.wait_ready_to_flush);

    vlc_player_Lock(player);
    vlc_player_SetPosition(player, 0);
    vlc_player_Unlock(player);
}

static int sout_filter_send(sout_stream_t *stream, void *id, block_t *block)
{
    (void)stream; (void)id;
    block_Release(block);
    scenario_data.stream_out_sent = true;
    vlc_sem_post(&scenario_data.wait_ready_to_flush);
    return VLC_SUCCESS;
}

static void sout_filter_flush(sout_stream_t *stream, void *id)
{
    (void)stream; (void)id;
    assert(scenario_data.stream_out_sent);
    vlc_sem_post(&scenario_data.wait_stop);
}

const char source_800_600[] = "mock://video_track_count=1;length=100000000000;video_width=800;video_height=600";
struct input_decoder_scenario input_decoder_scenarios[] =
{{
    .source = source_800_600,
    .decoder_setup = decoder_i420_800_600,
    .decoder_flush = decoder_flush_signal,
    .decoder_decode = decoder_decode_check_cc,
    .interface_setup = interface_setup_select_cc,
},
{
    .source = source_800_600,
    .decoder_setup = decoder_i420_800_600,
    .decoder_decode = decoder_decode_check_flush_video,
    .decoder_flush = decoder_flush_signal,
    .display_prepare = display_prepare_signal,
    .interface_setup = interface_setup_check_flush,
},
{
    /* Check that stream output is also flushed:
      - the test cannot work if the stream_out filter is not added
      - sout_filter_send(), signal the interface that it can flush
      - the interface change player position to trigger a flush
      - the flush is signaled to the stream_out filter
      - the stream_out filter signal the end of the test */
    .source = source_800_600,
    .sout = "#" MODULE_STRING,
    .sout_filter_send = sout_filter_send,
    .sout_filter_flush = sout_filter_flush,
    .interface_setup = interface_setup_check_flush,
},
{
    /* Check that releasing a decoder while it is triggering an update
     * of the video output format doesn't lead to a crash. Non-regression
     * test from issue #27532. */
    .source = source_800_600,
    .decoder_setup = decoder_i420_800_600_stop,
    .decoder_decode = decoder_decode_drop,
    .decoder_destroy = decoder_destroy_trigger_update,
},
{
    /* Check that reloading a decoder while it is triggering an update
     * of the video output format doesn't lead to a crash. Non-regression
     * test from issue #27532. */
    .source = source_800_600,
    .decoder_setup = decoder_i420_800_600,
    .decoder_decode = decoder_decode_trigger_reload,
    .decoder_destroy = decoder_destroy_trigger_update,
}};
size_t input_decoder_scenarios_count = ARRAY_SIZE(input_decoder_scenarios);

void input_decoder_scenario_init(void)
{
    scenario_data.decoder_vctx = NULL;
    scenario_data.skip_decoder = false;
    scenario_data.has_reload = false;
    scenario_data.stream_out_sent = false;
    scenario_data.decoder_image_sent = 0;
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
