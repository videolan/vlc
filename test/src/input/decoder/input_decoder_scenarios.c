/*****************************************************************************
 * input_decoder_scenario.c: testflight for input_decoder state machine
 *****************************************************************************
 * Copyright (C) 2022-2023 VideoLabs
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
#include <vlc_frame.h>
#include <vlc_codec.h>
#include <vlc_filter.h>
#include <vlc_vout_display.h>

#include "input_decoder.h"

static struct scenario_data
{
    vlc_sem_t wait_stop;
    vlc_sem_t wait_ready_to_flush;
    struct vlc_video_context *decoder_vctx;
    bool skip_decoder;
    bool has_reload;
    bool stream_out_sent;
    size_t decoder_image_sent;
    size_t cc_track_idx;
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

static void decoder_i420_800_600_update(decoder_t *dec)
{
    decoder_fixed_size(dec, VLC_CODEC_I420, 800, 600);
    int ret = decoder_UpdateVideoOutput(dec, NULL);
    assert(ret == VLC_SUCCESS);
}

static void decoder_i420_800_600_stop(decoder_t *dec)
{
    decoder_i420_800_600(dec);
    vlc_sem_post(&scenario_data.wait_stop);
}

static const char cc_block_input[] = "cc01_input";
static vlc_frame_t *create_cc_frame(vlc_tick_t ts)
{
    vlc_frame_t *f = vlc_frame_Alloc(sizeof(cc_block_input));
    assert(f != NULL);
    memcpy(f->p_buffer, cc_block_input, sizeof(cc_block_input));
    f->i_dts = f->i_pts = ts;
    f->i_length = VLC_TICK_FROM_MS(20);
    return f;
}

static void decoder_decode_check_cc_common(decoder_t *dec, picture_t *pic)
{
    vlc_tick_t date = pic->date;

    vlc_frame_t *cc = create_cc_frame(date);

    decoder_cc_desc_t desc = {
        .i_608_channels = 1,
        .i_reorder_depth = 4,
    };
    decoder_QueueCc(dec, cc, &desc);
}

static int decoder_decode_check_cc(decoder_t *dec, picture_t *pic)
{
    decoder_decode_check_cc_common(dec, pic);
    picture_Release(pic);

    vlc_sem_post(&scenario_data.wait_ready_to_flush);

    return VLC_SUCCESS;
}

static int decoder_decode_check_cc_queue_video(decoder_t *dec, picture_t *pic)
{
    decoder_decode_check_cc_common(dec, pic);
    decoder_QueueVideo(dec, pic);

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

static int decoder_decode(decoder_t *dec, picture_t *pic)
{
    decoder_QueueVideo(dec, pic);
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

static int sout_filter_wait_cc(sout_stream_t *stream, void *id, block_t *block)
{
    (void)stream; (void)id;
    block_Release(block);
    vlc_fourcc_t *codec = id;
    if (*codec == VLC_CODEC_CEA608)
    {
        scenario_data.stream_out_sent = true;
        vlc_sem_post(&scenario_data.wait_ready_to_flush);
    }
    return VLC_SUCCESS;
}

static void sout_filter_flush(sout_stream_t *stream, void *id)
{
    (void)stream; (void)id;
    assert(scenario_data.stream_out_sent);
    vlc_sem_post(&scenario_data.wait_stop);
}

static vlc_frame_t *packetizer_getcc(decoder_t *dec, decoder_cc_desc_t *cc_desc)
{
    (void)dec;

    cc_desc->i_608_channels = 1;
    cc_desc->i_708_channels = 0;
    cc_desc->i_reorder_depth = 4;

    return create_cc_frame(VLC_TICK_0);
}

static vlc_frame_t *packetizer_getcc_cea608_max(decoder_t *dec, decoder_cc_desc_t *cc_desc)
{
    (void)dec;

    cc_desc->i_608_channels = 0xF;
    cc_desc->i_708_channels = 0;
    cc_desc->i_reorder_depth = 4;

    return create_cc_frame(VLC_TICK_0);
}

static vlc_frame_t *packetizer_getcc_cea708_max(decoder_t *dec, decoder_cc_desc_t *cc_desc)
{
    (void)dec;

    cc_desc->i_608_channels = 0;
    cc_desc->i_708_channels = UINT64_MAX;
    cc_desc->i_reorder_depth = 4;

    return create_cc_frame(VLC_TICK_0);
}

static void on_track_list_changed_check_cea608(
        enum vlc_player_list_action action,
        const struct vlc_player_track *track)
{
    if (action != VLC_PLAYER_LIST_ADDED)
        return;

    if (strcmp(vlc_es_id_GetStrId(track->es_id), "video/0/cc/spu/1") != 0)
        return;

    assert(track->fmt.i_codec == VLC_CODEC_CEA608);
    vlc_sem_post(&scenario_data.wait_stop);
}

static void on_track_list_changed_check_count(
        enum vlc_player_list_action action,
        const struct vlc_player_track *track,
        vlc_fourcc_t codec, unsigned track_max)
{
    if (action != VLC_PLAYER_LIST_ADDED)
        return;

    char buffer[] = "video/0/cc/spu/xx";
    assert(scenario_data.cc_track_idx < 100);
    sprintf(buffer, "video/0/cc/spu/%zu", scenario_data.cc_track_idx);

    if (strcmp(vlc_es_id_GetStrId(track->es_id), buffer) != 0)
        return;

    assert(track->fmt.i_codec == codec);

    scenario_data.cc_track_idx++;

    if (scenario_data.cc_track_idx == track_max)
        vlc_sem_post(&scenario_data.wait_stop);
}

static void on_track_list_changed_check_cea608_count(
        enum vlc_player_list_action action,
        const struct vlc_player_track *track)
{
    on_track_list_changed_check_count(action, track, VLC_CODEC_CEA608, 4);
}

static void on_track_list_changed_check_cea708_count(
        enum vlc_player_list_action action,
        const struct vlc_player_track *track)
{
    on_track_list_changed_check_count(action, track, VLC_CODEC_CEA708, 64);
}

static void player_setup_select_cc(vlc_player_t *player)
{
    vlc_player_SelectTracksByStringIds(player, SPU_ES, "video/0/cc/spu/1");
}

static void cc_decoder_setup_01(decoder_t *dec)
{
    assert(dec->fmt_in->i_codec == VLC_CODEC_CEA608);
    assert(dec->fmt_in->subs.cc.i_channel == 0);
    assert(dec->fmt_in->subs.cc.i_reorder_depth == 4);

    dec->fmt_out.i_codec = VLC_CODEC_TEXT;
}

static int cc_decoder_decode_common(decoder_t *dec, vlc_frame_t *in,
                                    const char *text)
{
    assert(memcmp(in->p_buffer, cc_block_input, sizeof(cc_block_input)) == 0);

    subpicture_t *subpic = decoder_NewSubpicture(dec, NULL);
    assert(subpic != NULL);

    subpic->i_start = in->i_pts;
    subpic->i_stop = subpic->i_start + in->i_length;

    subpicture_region_t *p_region = subpicture_region_NewText();;
    assert(p_region != NULL);
    vlc_spu_regions_push( &subpic->regions, p_region );

    p_region->b_absolute = true;
    p_region->i_x = 0;
    p_region->i_y = 0;
    p_region->p_text = text_segment_New(text);
    assert(p_region->p_text != NULL);

    decoder_QueueSub(dec, subpic);

    return VLC_SUCCESS;
}

static const char cc_block_decoded[] = "cc01_dec";
static int cc_decoder_decode(decoder_t *dec, vlc_frame_t *in)
{
    return cc_decoder_decode_common(dec, in, cc_block_decoded);
}

static void display_prepare_noop(vout_display_t *vd, picture_t *pic)
{
    (void)vd; (void) pic;
}

static void cc_text_renderer_render(filter_t *filter, const subpicture_region_t *region_in)
{
    (void) filter;
    assert(strcmp(region_in->p_text->psz_text, cc_block_decoded) == 0);
    vlc_sem_post(&scenario_data.wait_stop);
}

static vlc_frame_t *packetizer_getcc_cea708_1064(decoder_t *dec, decoder_cc_desc_t *cc_desc)
{
    (void)dec;

    cc_desc->i_608_channels = 0;
    cc_desc->i_708_channels = (1ULL << 9) | (1ULL << 63);
    cc_desc->i_reorder_depth = 4;

    return create_cc_frame(VLC_TICK_0);
}

static void cc_decoder_setup_708_1064(decoder_t *dec)
{
    assert(dec->fmt_in->i_codec == VLC_CODEC_CEA708);
    assert(dec->fmt_in->subs.cc.i_channel == 9
        || dec->fmt_in->subs.cc.i_channel == 63);
    assert(dec->fmt_in->subs.cc.i_reorder_depth == 4);

    dec->fmt_out.i_codec = VLC_CODEC_TEXT;
}

static int cc_decoder_decode_channel(decoder_t *dec, vlc_frame_t *in)
{
    char buf[] = "ccxxx_dec";
    assert(dec->fmt_in->subs.cc.i_channel < 64);
    sprintf(buf, "cc%02u_dec", dec->fmt_in->subs.cc.i_channel + 1);
    return cc_decoder_decode_common(dec, in, buf);
}

static void cc_text_renderer_render_708_1064(filter_t *filter,
                                             const subpicture_region_t *region_in)
{
    (void) filter;
    /* Make sure each tracks are rendered once */
    static unsigned chans[] = { 10, 64 };
    static bool rendered[] = { false, false };

    char buf[] = "ccxx_dec";
    for (size_t i = 0; i < ARRAY_SIZE(chans); ++i)
    {
        unsigned chan = chans[i];
        sprintf(buf, "cc%u_dec", chan);

        if (strcmp(region_in->p_text->psz_text, buf) == 0)
        {
            if (rendered[i])
                return;
            rendered[i] = true;
        }
    }

    for (size_t i = 0; i < ARRAY_SIZE(chans); ++i)
    {
        if (!rendered[i])
            return;
    }

    vlc_sem_post(&scenario_data.wait_stop);
}

static void player_setup_select_ccdec_708_1064(vlc_player_t *player)
{
    vlc_player_SelectTracksByStringIds(player, SPU_ES,
        "video/0/cc/spu/10,video/0/cc/spu/64");
}

static vlc_frame_t *packetizer_getcc_cea608_02(decoder_t *dec, decoder_cc_desc_t *cc_desc)
{
    (void)dec;

    cc_desc->i_608_channels = 1 << 1;
    cc_desc->i_708_channels = 0;
    cc_desc->i_reorder_depth = 4;

    return create_cc_frame(VLC_TICK_0);
}

static void cc_decoder_setup_608(decoder_t *dec)
{
    assert(dec->fmt_in->i_codec == VLC_CODEC_CEA608);

    dec->fmt_out.i_codec = VLC_CODEC_TEXT;
}

static void cc_text_renderer_render_608_02(filter_t *filter,
                                           const subpicture_region_t *region_in)
{
    (void) filter;
    assert(strcmp(region_in->p_text->psz_text, "cc02_dec") == 0);
    vlc_sem_post(&scenario_data.wait_stop);
}

static const vlc_fourcc_t subpicture_chromas[] = {
    VLC_CODEC_RGBA, 0
};

#define source_800_600 "mock://video_track_count=1;length=100000000000;video_width=800;video_height=600"
struct input_decoder_scenario input_decoder_scenarios[] =
{{
    .name = "decoder is flushed",
    .source = source_800_600,
    .decoder_setup = decoder_i420_800_600,
    .decoder_flush = decoder_flush_signal,
    .decoder_decode = decoder_decode_check_cc,
    .interface_setup = interface_setup_select_cc,
},
{
    .name = "video output is flushed",
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
    .name = "stream output is also flushed",
    .source = source_800_600,
    .item_option = ":sout=#" MODULE_STRING,
    .sout_filter_send = sout_filter_send,
    .sout_filter_flush = sout_filter_flush,
    .interface_setup = interface_setup_check_flush,
},
{
    /* Check that releasing a decoder while it is triggering an update
     * of the video output format doesn't lead to a crash. Non-regression
     * test from issue #27532. */
    .name = "releasing a decoder while updating the vout doesn't lead to a crash",
    .source = source_800_600,
    .decoder_setup = decoder_i420_800_600_stop,
    .decoder_decode = decoder_decode_drop,
    .decoder_destroy = decoder_destroy_trigger_update,
},
{
    /* Check that reloading a decoder while it is triggering an update
     * of the video output format doesn't lead to a crash. Non-regression
     * test from issue #27532. */
    .name = "reloading a decoder while updating the vout doesn't lead to a crash",
    .source = source_800_600,
    .decoder_setup = decoder_i420_800_600,
    .decoder_decode = decoder_decode_trigger_reload,
    .decoder_destroy = decoder_destroy_trigger_update,
},
{
    .name = "CC frames are sent to the sout",
    .source = source_800_600,
    .item_option = ":sout=#" MODULE_STRING,
    .packetizer_getcc = packetizer_getcc,
    .sout_filter_send = sout_filter_wait_cc,
    .sout_filter_flush = sout_filter_flush,
    .interface_setup = interface_setup_check_flush,
},
{
    .name = "CC tracks are added",
    .source = source_800_600 ";video_packetized=false",
    .packetizer_getcc = packetizer_getcc,
    .decoder_setup = decoder_i420_800_600,
    .decoder_decode = decoder_decode_drop,
    .on_track_list_changed = on_track_list_changed_check_cea608,
},
{
    /* Check that the CC coming from the packetizer is decoded and rendered:
     * - A valid vout is needed (to blend subtitles into)
     * - Text vlc_frames are passed from the packetizer to the cc decoder to
     *   the text renderer and are checked at each step
     *  - A text renderer is used, this is the last place a text can be checked
     *  before it is rendered (and converted to RGB/YUV)
     */
    .name = "CC coming from the packetizer is decoded and rendered",
    .source = source_800_600 ";video_packetized=false",
    .subpicture_chromas = subpicture_chromas,
    .packetizer_getcc = packetizer_getcc,
    .decoder_setup = decoder_i420_800_600_update,
    .decoder_decode = decoder_decode,
    .cc_decoder_setup = cc_decoder_setup_01,
    .cc_decoder_decode = cc_decoder_decode,
    .display_prepare = display_prepare_noop,
    .text_renderer_render = cc_text_renderer_render,
    .player_setup_before_start = player_setup_select_cc,
},
{
    /* Check that the CC coming from the video decoder is decoded and rendered,
     * cf. the previous scenario for more details. */
    .name = "CC coming from the video decoder is decoded and rendered",
    .source = source_800_600,
    .subpicture_chromas = subpicture_chromas,
    .decoder_setup = decoder_i420_800_600_update,
    .decoder_decode = decoder_decode_check_cc_queue_video,
    .cc_decoder_setup = cc_decoder_setup_01,
    .cc_decoder_decode = cc_decoder_decode,
    .display_prepare = display_prepare_noop,
    .text_renderer_render = cc_text_renderer_render,
    .player_setup_before_start = player_setup_select_cc,
},
{
    .name = "we can create 4 cea608 tracks",
    .source = source_800_600 ";video_packetized=false",
    .packetizer_getcc = packetizer_getcc_cea608_max,
    .decoder_setup = decoder_i420_800_600,
    .decoder_decode = decoder_decode_drop,
    .on_track_list_changed = on_track_list_changed_check_cea608_count,
},
{
    .name = "we can create 64 cea708 tracks",
    .source = source_800_600 ";video_packetized=false",
    .item_option = ":captions=708",
    .packetizer_getcc = packetizer_getcc_cea708_max,
    .decoder_setup = decoder_i420_800_600,
    .decoder_decode = decoder_decode_drop,
    .on_track_list_changed = on_track_list_changed_check_cea708_count,
},
{
    .name = "cea708 10 and 63 tracks can be rendered simultaneously",
    .source = source_800_600 ";video_packetized=false",
    .item_option = ":captions=708",
    .packetizer_getcc = packetizer_getcc_cea708_1064,
    .decoder_setup = decoder_i420_800_600_update,
    .decoder_decode = decoder_decode,
    .cc_decoder_setup = cc_decoder_setup_708_1064,
    .cc_decoder_decode = cc_decoder_decode_channel,
    .display_prepare = display_prepare_noop,
    .text_renderer_render = cc_text_renderer_render_708_1064,
    .player_setup_before_start = player_setup_select_ccdec_708_1064,
},
{
    /* The "--sub-track" option use channel starting with 0 whereas CC tracks
     * indexes start with 1. Ensure channel 1 match with the CC track 2. */
    .name = "A cea608 track can be selected via command line",
    .source = source_800_600 ";video_packetized=false",
    .item_option = ":sub-track=1",
    .packetizer_getcc = packetizer_getcc_cea608_02,
    .decoder_setup = decoder_i420_800_600_update,
    .decoder_decode = decoder_decode,
    .cc_decoder_setup = cc_decoder_setup_608,
    .cc_decoder_decode = cc_decoder_decode_channel,
    .display_prepare = display_prepare_noop,
    .text_renderer_render = cc_text_renderer_render_608_02,
},
};

size_t input_decoder_scenarios_count = ARRAY_SIZE(input_decoder_scenarios);

void input_decoder_scenario_init(void)
{
    scenario_data.decoder_vctx = NULL;
    scenario_data.skip_decoder = false;
    scenario_data.has_reload = false;
    scenario_data.stream_out_sent = false;
    scenario_data.decoder_image_sent = 0;
    scenario_data.cc_track_idx = 1;
    vlc_sem_init(&scenario_data.wait_stop, 0);
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
