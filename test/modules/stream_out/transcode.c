/*****************************************************************************
 * transcode.c: test for transcoding pipeline
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

/* Define a builtin module for mocked parts */
#define MODULE_NAME test_transcode_mock
#define MODULE_STRING "test_transcode_mock"
#undef __PLUGIN__

const char vlc_module_name[] = MODULE_STRING;

#include "../../libvlc/test.h"
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
#include <vlc_sout.h>
#include <vlc_frame.h>

#include <limits.h>

#include "transcode.h"
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

static int DecoderDecode(decoder_t *dec, vlc_frame_t *frame)
{
    if (frame == NULL)
        return VLC_SUCCESS;

    const picture_resource_t resource = {
        .p_sys = NULL,
    };
    picture_t *pic = picture_NewFromResource(&dec->fmt_out.video, &resource);
    assert(pic);
    pic->date = frame->i_pts;
    vlc_frame_Release(frame);

    struct transcode_scenario *scenario = &transcode_scenarios[current_scenario];
    assert(scenario->decoder_decode != NULL);
    return scenario->decoder_decode(dec, pic);
}

static void CloseDecoder(vlc_object_t *obj)
{
    decoder_t *dec = (decoder_t*)obj;
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
    // Necessary ?
    es_format_Clean(&dec->fmt_out);
    es_format_Copy(&dec->fmt_out, &dec->fmt_in);

    struct transcode_scenario *scenario = &transcode_scenarios[current_scenario];
    assert(scenario->decoder_setup != NULL);
    scenario->decoder_setup(dec);

    msg_Dbg(obj, "Decoder chroma %4.4s -> %4.4s size %ux%u",
            (const char *)&dec->fmt_in.i_codec,
            (const char *)&dec->fmt_out.i_codec,
            dec->fmt_out.video.i_width, dec->fmt_out.video.i_height);

    return VLC_SUCCESS;
}

static int OpenFilter(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;

    static const struct vlc_filter_operations ops = {
        .filter_video = NULL,
        .close = NULL,
    };
    filter->ops = &ops;

    return VLC_SUCCESS;
}

static picture_t *ConverterFilter(filter_t *filter, picture_t *input)
{
    video_format_Clean(&input->format);
    video_format_Copy(&input->format, &filter->fmt_out.video);
    return input;
}

static int OpenConverter(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;

    msg_Dbg(obj, "converter chroma %4.4s -> %4.4s size %ux%u -> %ux%u",
            (const char *)&filter->fmt_in.i_codec,
            (const char *)&filter->fmt_out.i_codec,
            filter->fmt_in.video.i_width, filter->fmt_in.video.i_height,
            filter->fmt_out.video.i_width, filter->fmt_out.video.i_height);

    struct transcode_scenario *scenario = &transcode_scenarios[current_scenario];
    assert(scenario->converter_setup != NULL);
    scenario->converter_setup(filter);

    static const struct vlc_filter_operations ops = {
        .filter_video = ConverterFilter,
        .close = NULL,
    };
    filter->ops = &ops;

    return VLC_SUCCESS;
}

static vlc_frame_t *EncodeVideo(encoder_t *enc, picture_t *pic)
{
    if (pic == NULL)
        return NULL;

    assert(pic->format.i_chroma == enc->fmt_in.video.i_chroma);
    vlc_frame_t *frame = vlc_frame_Alloc(4);

    struct transcode_scenario *scenario = &transcode_scenarios[current_scenario];
    if (scenario->encoder_encode != NULL)
        scenario->encoder_encode(enc, pic);
    return frame;
}

static void CloseEncoder(encoder_t *enc)
{
    struct transcode_scenario *scenario = &transcode_scenarios[current_scenario];
    if (scenario->encoder_close != NULL)
        scenario->encoder_close(enc);
}

static int OpenEncoder(vlc_object_t *obj)
{
    encoder_t *enc = (encoder_t *)obj;
    enc->p_sys = NULL;

    struct transcode_scenario *scenario = &transcode_scenarios[current_scenario];
    assert(scenario->encoder_setup != NULL);
    scenario->encoder_setup(enc);

    msg_Dbg(obj, "Encoder chroma %4.4s -> %4.4s size %ux%u -> %ux%u",
            (const char *)&enc->fmt_in.i_codec,
            (const char *)&enc->fmt_out.i_codec,
            enc->fmt_in.video.i_width, enc->fmt_in.video.i_height,
            enc->fmt_out.video.i_width, enc->fmt_out.video.i_height);

    static const struct vlc_encoder_operations ops =
    {
        .encode_video = EncodeVideo,
        .close = CloseEncoder,
    };
    enc->ops = &ops;

    return VLC_SUCCESS;
}

static int ErrorCheckerSend(sout_stream_t *stream, void *id, vlc_frame_t *f)
{
    struct transcode_scenario *scenario = &transcode_scenarios[current_scenario];

    int ret = sout_StreamIdSend(stream->p_next, id, f);
    if (ret != VLC_SUCCESS)
        scenario->report_error(stream);
    return VLC_SUCCESS;
}

static void* ErrorCheckerAdd(sout_stream_t *stream, const es_format_t *fmt)
    { return sout_StreamIdAdd(stream->p_next, fmt); }

static void ErrorCheckerDel(sout_stream_t *stream, void *id)
    { sout_StreamIdDel(stream->p_next, id); };

static int OpenErrorChecker(vlc_object_t *obj)
{
    sout_stream_t *stream = (sout_stream_t *)obj;
    static const struct sout_stream_operations ops = {
        .add = ErrorCheckerAdd,
        .del = ErrorCheckerDel,
        .send = ErrorCheckerSend,
    };
    stream->ops = &ops;
    return VLC_SUCCESS;
}

static void on_state_changed(vlc_player_t *player, enum vlc_player_state state, void *opaque)
{
    (void)player; (void)state; (void) opaque;
    vlc_cond_signal(&player_cond);
}

static void play_scenario(intf_thread_t *intf, struct transcode_scenario *scenario)
{
    transcode_scenario_init();
    input_item_t *media = input_item_New(scenario->source, "dummy");
    assert(media);

    /* TODO: Codec doesn't seem to have effect in transcode:
     * - add a test that --codec works?
     * - do not use --codec at all here? */
    input_item_AddOption(media, scenario->sout, VLC_INPUT_OPTION_TRUSTED);

    var_Create(intf, "codec", VLC_VAR_STRING);
    var_SetString(intf, "codec", MODULE_STRING);

    var_Create(intf, "sout-transcode-venc", VLC_VAR_STRING);
    var_SetString(intf, "sout-transcode-venc", MODULE_STRING);

    var_Create(intf, "sout-transcode-vcodec", VLC_VAR_STRING);
    var_SetString(intf, "sout-transcode-vcodec", "test");

    vlc_player_t *player = vlc_player_New(&intf->obj,
        VLC_PLAYER_LOCK_NORMAL, NULL, NULL);
    assert(player);

    static const struct vlc_player_cbs player_cbs = {
        .on_state_changed = on_state_changed,
    };

    vlc_player_Lock(player);
    vlc_player_listener_id *listener =
        vlc_player_AddListener(player, &player_cbs, NULL);
    vlc_player_SetCurrentMedia(player, media);
    vlc_player_Start(player);
    vlc_player_Unlock(player);

    transcode_scenario_wait(scenario);

    vlc_player_Lock(player);
    vlc_player_Stop(player);

    while (vlc_player_GetState(player) != VLC_PLAYER_STATE_STOPPED)
        vlc_player_CondWait(player, &player_cond);

    vlc_player_RemoveListener(player, listener);
    vlc_player_Unlock(player);

    transcode_scenario_check(scenario);

    vlc_player_Delete(player);
    input_item_Release(media);

    var_Destroy(intf, "sout-transcode-vcodec");
    var_Destroy(intf, "sout-transcode-venc");
}

static int OpenIntf(vlc_object_t *obj)
{
    intf_thread_t *intf = (intf_thread_t*)obj;

    while (current_scenario < transcode_scenarios_count)
    {
        msg_Info(intf, " - Running transcode scenario %zu", current_scenario);
        play_scenario(intf, &transcode_scenarios[current_scenario]);
        current_scenario++;
    }

    return VLC_SUCCESS;
}

/**
 * Inject the mocked modules as a static plugin:
 *  - access for triggering the correct decoder
 *  - decoder for generating video format and context
 *  - filter for generating video format and context
 *  - encoder to check the previous video format and context
 **/
vlc_module_begin()
    set_callbacks(OpenDecoder, CloseDecoder)
    set_capability("video decoder", INT_MAX)

    add_submodule()
        set_callback(OpenErrorChecker)
        set_capability("sout filter", 0)
        add_shortcut("error_checker")

    add_submodule()
        set_callback(OpenDecoderDevice)
        set_capability("decoder device", 0)

    add_submodule()
        set_callback(OpenFilter)
        set_capability("video filter", 0)

    add_submodule()
        set_callback(OpenConverter)
        set_capability("video converter", INT_MAX)

    add_submodule()
        set_callback(OpenEncoder)
        set_capability("video encoder", 0)

    add_submodule()
        set_callback(OpenIntf)
        set_capability("interface", 0)

vlc_module_end()

/* Helper typedef for vlc_static_modules */
typedef int (*vlc_plugin_cb)(vlc_set_cb, void*);

VLC_EXPORT const vlc_plugin_cb vlc_static_modules[] = {
    VLC_SYMBOL(vlc_entry),
    NULL
};

int main( int argc, char **argv )
{
    (void)argc; (void)argv;
    test_init();

    const char * const args[] = {
        "-vvv", "--vout=dummy", "--aout=dummy", "--text-renderer=dummy",
        "--no-auto-preparse", "--dec-dev=" MODULE_STRING,
    };

    libvlc_instance_t *vlc = libvlc_new(ARRAY_SIZE(args), args);

    libvlc_add_intf(vlc, MODULE_STRING);
    libvlc_playlist_play(vlc);

    libvlc_release(vlc);
    assert(transcode_scenarios_count == current_scenario);
    return 0;
}
