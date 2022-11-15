/*****************************************************************************
 * video_output.c: test for the video output pipeline
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
#define MODULE_NAME test_vout_mock
#define MODULE_STRING "test_vout_mock"
#undef __PLUGIN__

static const char dec_dev_arg[] = "--dec-dev=" MODULE_STRING;

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
#include <vlc_vout_display.h>

#include <limits.h>

#include "video_output.h"
static size_t current_scenario;

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

    struct vout_scenario *scenario = &vout_scenarios[current_scenario];
    assert(scenario->decoder_decode != NULL);
    scenario->decoder_decode(dec, block);

    return VLC_SUCCESS;
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
    es_format_Copy(&dec->fmt_out, dec->fmt_in);

    struct vout_scenario *scenario = &vout_scenarios[current_scenario];
    assert(scenario->decoder_setup != NULL);
    scenario->decoder_setup(dec);

    msg_Dbg(obj, "Decoder chroma %4.4s -> %4.4s size %ux%u",
            (const char *)&dec->fmt_in->i_codec,
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

    struct vout_scenario *scenario = &vout_scenarios[current_scenario];
    assert(scenario->converter_setup != NULL);
    scenario->converter_setup(filter);

    static const struct vlc_filter_operations ops = {
        .filter_video = ConverterFilter,
        .close = NULL,
    };
    filter->ops = &ops;

    return VLC_SUCCESS;
}

static int OpenWindow(vlc_window_t *wnd)
{
    static const struct vlc_window_operations ops = {

    };
    wnd->ops = &ops;
    return VLC_SUCCESS;
}

static void Display(vout_display_t *vd, picture_t *picture)
{
    (void) vd;
    (void) picture;
}

static int OpenDisplay(vout_display_t *vd, video_format_t *fmtp,
                       struct vlc_video_context *vctx)
{
    static const struct vlc_display_operations ops =
    {
        .display = Display,
    };
    vd->ops = &ops;

    struct vout_scenario *scenario = &vout_scenarios[current_scenario];
    assert(scenario->display_setup != NULL);
    int ret = scenario->display_setup(vd, fmtp, vctx);

    msg_Dbg(vd, "vout display chroma %4.4s size %ux%u -> %ux%u",
            (const char *)&fmtp->i_chroma,
            fmtp->i_width, fmtp->i_height,
            fmtp->i_width, fmtp->i_height);

    return ret;
}

static void play_scenario(intf_thread_t *intf, struct vout_scenario *scenario)
{
    vout_scenario_init();
    input_item_t *media = input_item_New(scenario->source, "dummy");
    assert(media);

    /* TODO: Codec doesn't seem to have effect in transcode:
     * - add a test that --codec works?
     * - do not use --codec at all here? */

    var_Create(intf, "codec", VLC_VAR_STRING);
    var_SetString(intf, "codec", MODULE_STRING);

    var_Create(intf, "vout", VLC_VAR_STRING);
    var_SetString(intf, "vout", MODULE_STRING);

    var_Create(intf, "window", VLC_VAR_STRING);
    var_SetString(intf, "window", MODULE_STRING);

    vlc_player_t *player = vlc_player_New(&intf->obj,
        VLC_PLAYER_LOCK_NORMAL, NULL, NULL);
    assert(player);

    vlc_player_Lock(player);
    vlc_player_SetCurrentMedia(player, media);
    vlc_player_Start(player);
    vlc_player_Unlock(player);

    vout_scenario_wait(scenario);

    vlc_player_Delete(player);
    input_item_Release(media);

    var_Destroy(intf, "vout");
    var_Destroy(intf, "codec");
}

static int OpenIntf(vlc_object_t *obj)
{
    intf_thread_t *intf = (intf_thread_t*)obj;

    msg_Info(intf, "Starting tests");
    while (current_scenario < vout_scenarios_count)
    {
        msg_Info(intf, " - Running vout scenario %zu", current_scenario);
        play_scenario(intf, &vout_scenarios[current_scenario]);
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
        set_callback(OpenDecoderDevice)
        set_capability("decoder device", 0)

    add_submodule()
        set_callback(OpenFilter)
        set_capability("video filter", 0)

    add_submodule()
        set_callback(OpenConverter)
        set_capability("video converter", INT_MAX)

    add_submodule()
        set_callback(OpenWindow)
        set_capability("vout window", INT_MAX)

    add_submodule()
        set_callback(OpenDisplay)
        set_capability("vout display", 0)

    /* Interface module to avoid casting libvlc_instance_t to object */
    add_submodule()
        set_callback(OpenIntf)
        set_capability("interface", 0)

vlc_module_end()

/* Helper typedef for vlc_static_modules */
typedef int (*vlc_plugin_cb)(vlc_set_cb, void*);


VLC_EXPORT const vlc_plugin_cb vlc_static_modules[];
const vlc_plugin_cb vlc_static_modules[] = {
    VLC_SYMBOL(vlc_entry),
    NULL
};

int main( int argc, char **argv )
{
    (void)argc; (void)argv;
    test_init();

    const char * const args[] = {
        "-vvv", "--vout=dummy", "--aout=dummy", "--text-renderer=dummy",
        "--no-auto-preparse", dec_dev_arg,
        "--no-spu", "--no-osd",
    };

    libvlc_instance_t *vlc = libvlc_new(ARRAY_SIZE(args), args);

    libvlc_add_intf(vlc, MODULE_STRING);
    libvlc_playlist_play(vlc);

    libvlc_release(vlc);
    assert(vout_scenarios_count == current_scenario);
    return 0;
}
