/*****************************************************************************
 * inhibit.c: test for the video output screensaver inhibition
 *****************************************************************************
 * Copyright (C) 2026 VideoLabs
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

/**
 * The vout window is created once per vout thread, and the vout is recycled
 * from one input to the next one. Check that switching to another media while
 * the playback is paused, which is what a playlist does when the user selects
 * another item, restores the inhibition of the screensaver.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* Define a builtin module for mocked parts */
#define MODULE_NAME test_vout_inhibit
#undef VLC_DYNAMIC_PLUGIN

#include "../../libvlc/test.h"
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_fourcc.h>
#include <vlc_frame.h>
#include <vlc_inhibit.h>
#include <vlc_interface.h>
#include <vlc_player.h>
#include <vlc_vout_display.h>
#include <vlc_window.h>

#include <limits.h>

#include "../lib/libvlc_internal.h"

const char vlc_module_name[] = MODULE_STRING;

static const char dec_dev_arg[] = "--dec-dev=" MODULE_STRING;

static const char source_800_600[] =
    "mock://video_track_count=1;length=100000000000;video_width=800;video_height=600";

static struct
{
    vlc_mutex_t lock;
    vlc_cond_t wait;

    /* Number of inhibiter instances created, used to check whether the
     * inhibiter was recycled or recreated. */
    size_t inhibit_count;
    /* Flags of the last inhibition request. */
    unsigned inhibit_flags;
    /* Number of display instances created, one per media
     * (decoder_UpdateVideoOutput) here. */
    size_t display_count;
    /* Number of pictures sent to the display. */
    size_t picture_count;
} ctx = {
    .lock = VLC_STATIC_MUTEX,
    .wait = VLC_STATIC_COND,
    .inhibit_count = 0,
    .inhibit_flags = VLC_INHIBIT_NONE,
    .display_count = 0,
    .picture_count = 0,
};

/* Wait for a state change from the vout, or abort the test if it never
 * happens. This simplify the writing of the test by helping writing it
 * in a synchronous way. */
#define WAIT_UNTIL(condition) do { \
    vlc_tick_t deadline_ = vlc_tick_now() + VLC_TICK_FROM_SEC(30); \
    vlc_mutex_lock(&ctx.lock); \
    while (!(condition)) \
        assert(vlc_cond_timedwait(&ctx.wait, &ctx.lock, deadline_) == 0 \
               && "timed out waiting for: " #condition); \
    vlc_mutex_unlock(&ctx.lock); \
} while (0)

/* Inhibit module implementation */

static void Inhibit(vlc_inhibit_t *ih, unsigned flags)
{
    msg_Info(ih, "inhibition flags: 0x%x", flags);

    vlc_mutex_lock(&ctx.lock);
    ctx.inhibit_flags = flags;
    vlc_cond_broadcast(&ctx.wait);
    vlc_mutex_unlock(&ctx.lock);
}

static int OpenInhibit(vlc_object_t *obj)
{
    vlc_inhibit_t *ih = (vlc_inhibit_t *)obj;
    ih->inhibit = Inhibit;

    vlc_mutex_lock(&ctx.lock);
    ctx.inhibit_count++;
    vlc_mutex_unlock(&ctx.lock);

    return VLC_SUCCESS;
}

/* Decoder mock implementation */

static int DecoderDecode(decoder_t *dec, vlc_frame_t *frame)
{
    int i_ret = VLC_SUCCESS;

    if (frame == NULL)
        return VLC_SUCCESS;

    i_ret = decoder_UpdateVideoOutput(dec, NULL);
    if (i_ret != VLC_SUCCESS)
        goto error;

    picture_t *pic = picture_NewFromFormat(&dec->fmt_out.video);
    if (pic == NULL)
    {
        i_ret = VLC_ENOMEM;
        goto error;
    }

    pic->date = frame->i_pts;
    pic->b_progressive = true;
    decoder_QueueVideo(dec, pic);

error:
    vlc_frame_Release(frame);
    return i_ret;
}

static int OpenDecoder(vlc_object_t *obj)
{
    decoder_t *dec = (decoder_t *)obj;

    es_format_Clean(&dec->fmt_out);
    es_format_Copy(&dec->fmt_out, dec->fmt_in);
    dec->fmt_out.i_codec
        = dec->fmt_out.video.i_chroma
        = VLC_CODEC_I420;
    dec->fmt_out.video.i_visible_width
        = dec->fmt_out.video.i_width
        = 800;
    dec->fmt_out.video.i_visible_height
        = dec->fmt_out.video.i_height
        = 600;

    dec->pf_decode = DecoderDecode;
    return VLC_SUCCESS;
}

/* Window mock implementation */

static int OpenWindow(vlc_window_t *wnd)
{
    static const struct vlc_window_operations ops =
    {

    };
    wnd->ops = &ops;
    return VLC_SUCCESS;
}

/* Display mock implementation */

static void Display(vout_display_t *vd, picture_t *picture)
{
    VLC_UNUSED(vd); VLC_UNUSED(picture);

    vlc_mutex_lock(&ctx.lock);
    ctx.picture_count++;
    vlc_cond_broadcast(&ctx.wait);
    vlc_mutex_unlock(&ctx.lock);
}

static int OpenDisplay(vout_display_t *vd, video_format_t *fmtp,
                       struct vlc_video_context *vctx)
{
    VLC_UNUSED(fmtp); VLC_UNUSED(vctx);

    static const struct vlc_display_operations ops =
    {
        .display = Display,
    };
    vd->ops = &ops;

    vlc_mutex_lock(&ctx.lock);
    ctx.display_count++;
    vlc_cond_broadcast(&ctx.wait);
    vlc_mutex_unlock(&ctx.lock);

    return VLC_SUCCESS;
}

/* Interface implementation, which actually starts the test */
static int OpenIntf(vlc_object_t *obj)
{
    intf_thread_t *intf = (intf_thread_t *)obj;

    var_Create(intf, "codec", VLC_VAR_STRING);
    var_SetString(intf, "codec", MODULE_STRING);

    var_Create(intf, "vout", VLC_VAR_STRING);
    var_SetString(intf, "vout", MODULE_STRING);

    var_Create(intf, "window", VLC_VAR_STRING);
    var_SetString(intf, "window", MODULE_STRING);

    input_item_t *media1 = input_item_New(source_800_600, "media1");
    assert(media1 != NULL);
    input_item_t *media2 = input_item_New(source_800_600, "media2");
    assert(media2 != NULL);

    vlc_player_t *player = vlc_player_New(&intf->obj, VLC_PLAYER_LOCK_NORMAL);
    assert(player != NULL);

    msg_Info(intf, "Starting the first media");
    vlc_player_Lock(player);
    vlc_player_SetCurrentMedia(player, media1);
    vlc_player_Start(player);
    vlc_player_Unlock(player);

    /* Enabling the window of the vout inhibits the screensaver. Wait for a
     * picture so that the decoder is known to be attached to the vout, which
     * is required for the pause below to reach it. */
    WAIT_UNTIL(ctx.picture_count > 0 && ctx.inhibit_flags == VLC_INHIBIT_VIDEO);

    vlc_mutex_lock(&ctx.lock);
    assert(ctx.display_count == 1);
    vlc_mutex_unlock(&ctx.lock);

    msg_Info(intf, "Pausing the first media");
    vlc_player_Lock(player);
    vlc_player_Pause(player);
    vlc_player_Unlock(player);

    /* Pausing the playback releases the inhibition. */
    WAIT_UNTIL(ctx.inhibit_flags == VLC_INHIBIT_NONE);

    /* Switch to another media without resuming first, like a playlist does
     * when the user selects another item while paused. The vout is recycled
     * with its window still enabled. */
    msg_Info(intf, "Switching to the second media while paused");
    vlc_player_Lock(player);
    vlc_player_SetCurrentMedia(player, media2);
    vlc_player_Unlock(player);

    /* The window is enabled again before the display of the second media is
     * created, so the inhibition state is settled once it shows up. */
    WAIT_UNTIL(ctx.display_count == 2);

    vlc_mutex_lock(&ctx.lock);
    /* The window, and thus the inhibiter, must have been reused. */
    assert(ctx.inhibit_count == 1);
    /* The second media is playing, the screensaver must be inhibited again. */
    assert(ctx.inhibit_flags == VLC_INHIBIT_VIDEO);
    vlc_mutex_unlock(&ctx.lock);

    vlc_player_Delete(player);
    input_item_Release(media2);
    input_item_Release(media1);

    var_Destroy(intf, "window");
    var_Destroy(intf, "vout");
    var_Destroy(intf, "codec");

    return VLC_SUCCESS;
}

/**
 * Inject the mocked modules as a static plugin:
 *  - decoder generating pictures for the video output
 *  - window and display to run a video output without any display server
 *  - inhibit to monitor the screensaver inhibition requests
 **/
vlc_module_begin()
    set_callback(OpenDecoder)
    set_capability("video decoder", INT_MAX)

    add_submodule()
        set_callback(OpenWindow)
        set_capability("vout window", INT_MAX)

    add_submodule()
        set_callback_display(OpenDisplay, 0)

    add_submodule()
        set_callback(OpenInhibit)
        set_capability("inhibit", INT_MAX)

    /* Interface module to start the test */
    add_submodule()
        set_callback(OpenIntf)
        set_capability("interface", 0)

vlc_module_end()

VLC_EXPORT const vlc_plugin_cb vlc_static_modules[] = {
    VLC_SYMBOL(vlc_entry),
    NULL
};

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    test_init();

    const char * const args[] = {
        "-vvv", "--aout=dummy", "--text-renderer=dummy",
        "--no-auto-preparse", "--no-hw-dec",
        "--no-spu", "--no-osd",
    };

    libvlc_instance_t *vlc = libvlc_new(ARRAY_SIZE(args), args);
    assert(vlc != NULL);

    libvlc_InternalAddIntf(vlc->p_libvlc_int, MODULE_STRING);
    libvlc_InternalPlay(vlc->p_libvlc_int);

    libvlc_release(vlc);
    return 0;
}
