/*****************************************************************************
 * input_decoder.c: test for vlc_input_decoder state machine
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

/* Define a builtin module for mocked parts */
#define MODULE_NAME test_input_decoder_mock
#define MODULE_STRING "test_input_decoder_mock"
#undef VLC_DYNAMIC_PLUGIN

const char vlc_module_name[] = MODULE_STRING;

#include "../../../libvlc/test.h"
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_access.h>
#include <vlc_demux.h>
#include <vlc_codec.h>
#include <vlc_window.h>
#include <vlc_interface.h>
#include <vlc_player.h>
#include <vlc_filter.h>
#include <vlc_threads.h>
#include <vlc_vout_display.h>
#include <vlc_sout.h>

#include <limits.h>

#include "input_decoder.h"
static size_t current_scenario = 0;

static vlc_cond_t player_cond = VLC_STATIC_COND;

static void DecoderDeviceClose(struct vlc_decoder_device *device)
    { VLC_UNUSED(device); }

static const struct vlc_decoder_device_operations decoder_device_ops =
{
    .close = DecoderDeviceClose,
};

static int OpenDecoderDevice(
        struct vlc_decoder_device *device,
        vlc_window_t *window
) {
    VLC_UNUSED(window);
    device->ops = &decoder_device_ops;
    /* Pick any valid one, we'll not use the module which can make use of
     * the private parts. */
    device->type = VLC_DECODER_DEVICE_VAAPI;
    return VLC_SUCCESS;
}

static int DecoderDecode(decoder_t *dec, block_t *block)
{
    if (block == NULL)
        return VLC_SUCCESS;

    const picture_resource_t resource = {
        .p_sys = NULL,
    };
    picture_t *pic = picture_NewFromResource(&dec->fmt_out.video, &resource);
    assert(pic);
    pic->date = block->i_pts;
    pic->b_progressive = true;

    struct input_decoder_scenario *scenario = &input_decoder_scenarios[current_scenario];
    assert(scenario->decoder_decode != NULL);
    int ret = scenario->decoder_decode(dec, pic);
    if (ret != VLCDEC_RELOAD)
        block_Release(block);
    return ret;
}

static void DecoderFlush(decoder_t *dec)
{
    struct input_decoder_scenario *scenario = &input_decoder_scenarios[current_scenario];
    assert(scenario->decoder_flush != NULL);
    return scenario->decoder_flush(dec);
}

static void CloseDecoder(vlc_object_t *obj)
{
    struct input_decoder_scenario *scenario = &input_decoder_scenarios[current_scenario];
    decoder_t *dec = (decoder_t*)obj;

    if (scenario->decoder_destroy != NULL)
        scenario->decoder_destroy(dec);
    struct vlc_video_context *vctx = dec->p_sys;
    if (vctx)
        vlc_video_context_Release(vctx);
}

static int OpenDecoder(vlc_object_t *obj)
{
    decoder_t *dec = (decoder_t*)obj;

    struct vlc_decoder_device *device = decoder_GetDecoderDevice(dec);
    assert(device);
    vlc_decoder_device_Release(device);

    dec->pf_decode = DecoderDecode;
    dec->pf_flush = DecoderFlush;
    es_format_Clean(&dec->fmt_out);
    es_format_Copy(&dec->fmt_out, dec->fmt_in);

    struct input_decoder_scenario *scenario = &input_decoder_scenarios[current_scenario];
    assert(scenario->decoder_setup != NULL);
    scenario->decoder_setup(dec);

    msg_Dbg(obj, "Decoder chroma %4.4s -> %4.4s size %ux%u",
            (const char *)&dec->fmt_in->i_codec,
            (const char *)&dec->fmt_out.i_codec,
            dec->fmt_out.video.i_width, dec->fmt_out.video.i_height);

    return VLC_SUCCESS;
}

static void DisplayPrepare(vout_display_t *vd, picture_t *picture,
        subpicture_t *subpic, vlc_tick_t date)
{
    (void)vd; (void)subpic; (void)date;

    struct input_decoder_scenario *scenario = &input_decoder_scenarios[current_scenario];
    assert(scenario->display_prepare != NULL);
    scenario->display_prepare(vd, picture);
}

static int DisplayControl(vout_display_t *vd, int query)
{
    (void)vd; (void)query;
    return VLC_SUCCESS;
}

static int OpenDisplay(vout_display_t *vd, video_format_t *fmtp, vlc_video_context *context)
{
    (void)fmtp; (void)context;

    static const struct vlc_display_operations ops = {
        .prepare = DisplayPrepare,
        .control = DisplayControl,
    };
    vd->ops = &ops;

    return VLC_SUCCESS;
}

static int OpenWindow(vlc_window_t *wnd)
{
    static const struct vlc_window_operations ops =
    {
        .resize = NULL,
    };
    wnd->ops = &ops;
    return VLC_SUCCESS;
}

static void *SoutFilterAdd(sout_stream_t *stream, const es_format_t *fmt)
{
    (void)stream; (void)fmt;
    void *id = malloc(1);
    assert(id != NULL);
    return id;
}

static void SoutFilterDel(sout_stream_t *stream, void *id)
{
    (void)stream;
    free(id);
}

static int SoutFilterSend(sout_stream_t *stream, void *id, block_t *block)
{
    struct input_decoder_scenario *scenario = &input_decoder_scenarios[current_scenario];
    if (scenario->sout_filter_send != NULL)
        return scenario->sout_filter_send(stream, id, block);
    block_Release(block);
    return VLC_SUCCESS;
}

static void SoutFilterFlush(sout_stream_t *stream, void *id)
{
    struct input_decoder_scenario *scenario = &input_decoder_scenarios[current_scenario];
    if (scenario->sout_filter_flush != NULL)
        scenario->sout_filter_flush(stream, id);
}

static int OpenSoutFilter(vlc_object_t *obj)
{
    sout_stream_t *stream = (sout_stream_t *)obj;
    static const struct sout_stream_operations ops = {
        .add = SoutFilterAdd,
        .del = SoutFilterDel,
        .send = SoutFilterSend,
        .flush = SoutFilterFlush,
    };
    stream->ops = &ops;
    return VLC_SUCCESS;
};

static void on_state_changed(vlc_player_t *player, enum vlc_player_state state, void *opaque)
{
    (void)player; (void)state; (void) opaque;
    vlc_cond_signal(&player_cond);
}

static void play_scenario(intf_thread_t *intf, struct input_decoder_scenario *scenario)
{
    input_decoder_scenario_init();
    input_item_t *media = input_item_New(scenario->source, "dummy");
    assert(media);

    var_Create(intf, "codec", VLC_VAR_STRING);
    var_SetString(intf, "codec", MODULE_STRING);

    if (scenario->sout != NULL)
    {
        char *sout;
        assert(asprintf(&sout, ":sout=%s", scenario->sout) != -1);
        input_item_AddOption(media, sout, VLC_INPUT_OPTION_TRUSTED);
        free(sout);
    }

    vlc_player_t *player = vlc_player_New(&intf->obj,
        VLC_PLAYER_LOCK_NORMAL, NULL, NULL);
    assert(player);

    intf->p_sys = (intf_sys_t *)player;

    static const struct vlc_player_cbs player_cbs = {
        .on_state_changed = on_state_changed,
    };

    vlc_player_Lock(player);
    vlc_player_listener_id *listener =
        vlc_player_AddListener(player, &player_cbs, NULL);
    vlc_player_SetCurrentMedia(player, media);
    vlc_player_Start(player);
    vlc_player_Unlock(player);

    input_decoder_scenario_wait(intf, scenario);

    vlc_player_Lock(player);
    vlc_player_Stop(player);

    while (vlc_player_GetState(player) != VLC_PLAYER_STATE_STOPPED)
        vlc_player_CondWait(player, &player_cond);

    vlc_player_RemoveListener(player, listener);
    vlc_player_Unlock(player);

    input_decoder_scenario_check(scenario);

    vlc_player_Delete(player);
    input_item_Release(media);

    var_Destroy(intf, "codec");
}

static int OpenIntf(vlc_object_t *obj)
{
    intf_thread_t *intf = (intf_thread_t*)obj;

    while (current_scenario < input_decoder_scenarios_count)
    {
        msg_Info(intf, " - Running scenario %zu", current_scenario);
        play_scenario(intf, &input_decoder_scenarios[current_scenario]);
        current_scenario++;
    }

    return VLC_SUCCESS;
}

/**
 * Inject the mocked modules as a static plugin:
 *  - access for triggering the correct decoder
 *  - decoder for generating video format and context
 *  - filter for generating video format and context
 **/
vlc_module_begin()
    set_callback(OpenIntf)
    set_capability("interface", 0)


    add_submodule()
        set_callback(OpenDecoderDevice)
        set_capability("decoder device", 0)

    add_submodule()
        set_callbacks(OpenDecoder, CloseDecoder)
        set_capability("video decoder", INT_MAX)

    add_submodule()
        set_callback(OpenWindow)
        set_capability("vout window", INT_MAX)

    add_submodule()
        set_callback_display(OpenDisplay, 0)

    add_submodule()
        set_callback(OpenSoutFilter)
        set_capability("sout output", 0)

vlc_module_end()

VLC_EXPORT vlc_plugin_cb vlc_static_modules[] = {
    VLC_SYMBOL(vlc_entry),
    NULL
};

int main( int argc, char **argv )
{
    (void)argc; (void)argv;
    test_init();

    const char * const args[] = {
        "-vvv",
        "--vout=" MODULE_STRING,
        "--dec-dev=" MODULE_STRING,
        "--aout=dummy",
        "--text-renderer=dummy",
        "--no-auto-preparse",
        "--no-spu",
        "--no-osd",
    };

    libvlc_instance_t *vlc = libvlc_new(ARRAY_SIZE(args), args);

    libvlc_add_intf(vlc, MODULE_STRING);
    libvlc_playlist_play(vlc);

    libvlc_release(vlc);
    assert(input_decoder_scenarios_count == current_scenario);
    return 0;
}
