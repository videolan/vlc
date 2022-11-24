/*****************************************************************************
 * captions_to_display.c: test for the captions_to_display feature
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
#define MODULE_NAME test_libvlc_captions_to_display
#define MODULE_STRING "test_libvlc_captions_to_display"
#undef __PLUGIN__

const char vlc_module_name[] = MODULE_STRING;

#include "test.h"
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_codec.h>
#include <vlc_window.h>
#include <vlc_threads.h>

#include <limits.h>

static decoder_t *g_dec;
static vlc_window_t *g_wnd;
static vlc_sem_t g_decoder_started;
static vlc_sem_t g_sem_on_caption;
static vlc_sem_t g_sem_continue_on_caption;
static vlc_sem_t g_sem_prepare_picture;
static vlc_sem_t g_sem_continue_prepare_picture;
static vlc_sem_t g_sem_window_ready;
static const libvlc_event_t *g_caption_event;
static picture_t *g_prepared_picture;

static void CloseDecoder(vlc_object_t *obj)
{
    (void)obj;
}

static int DecoderDecode(decoder_t *dec, vlc_frame_t *block)
{
    if (block == NULL)
        return VLC_SUCCESS;

#if 0
    const picture_resource_t resource = {
        .p_sys = NULL,
    };
    picture_t *pic = picture_NewFromResource(&dec->fmt_out.video, &resource);
    assert(pic);
    pic->date = block->i_pts;
    pic->b_progressive = true;

    // TODO
    pic->captions.size = 1;
    pic->captions.bytes[0] = 0;
#endif

    static vlc_tick_t current_tick = VLC_TICK_INVALID;

    vlc_tick_t previous_tick = current_tick;
    current_tick = block->i_pts;
  
    vlc_frame_Release(block);

    if (previous_tick != VLC_TICK_INVALID)
    {
        msg_Info(dec, "SENDING VSYNC");
        //vlc_window_ReportVsyncReached(g_wnd, vlc_tick_now() + VLC_TICK_FROM_MS(6));
    }
    return VLC_SUCCESS;
}

static int OpenDecoder(vlc_object_t *obj)
{
    decoder_t *dec = (decoder_t*)obj;

    dec->pf_decode = DecoderDecode;
#if 0
    dec->pf_flush = DecoderFlush;
#endif

    es_format_Clean(&dec->fmt_out);
    es_format_Copy(&dec->fmt_out, dec->fmt_in);

    g_dec = dec;
    int ret = decoder_UpdateVideoOutput(dec, NULL);
    assert(ret == VLC_SUCCESS);
    vlc_sem_post(&g_decoder_started);
    return VLC_SUCCESS;
}

static void DisplayPrepare(vout_display_t *vd, picture_t *picture,
        subpicture_t *subpic, vlc_tick_t date)
{
    (void)vd; (void)picture; (void)subpic; (void)date;
    if (g_prepared_picture != NULL)
        picture_Release(g_prepared_picture);
    g_prepared_picture = picture_Hold(picture);

    vlc_sem_post(&g_sem_prepare_picture);
    vlc_sem_wait(&g_sem_continue_prepare_picture);
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

    g_wnd = wnd;
    vlc_window_ReportSize(wnd, 800, 600);
    vlc_sem_post(&g_sem_window_ready);
    return VLC_SUCCESS;
}


/**
 * Inject the mocked modules as a static plugin:
 *  - decoder for generating pictures with captions
 **/
vlc_module_begin()
    set_callbacks(OpenDecoder, CloseDecoder)
    set_capability("video decoder", INT_MAX)

    add_submodule()
        set_callback(OpenWindow)
        set_capability("vout window", INT_MAX)

    add_submodule()
        set_callback_display(OpenDisplay, INT_MAX)

vlc_module_end()

/* Helper typedef for vlc_static_modules */
typedef int (*vlc_plugin_cb)(vlc_set_cb, void*);

VLC_EXPORT vlc_plugin_cb vlc_static_modules[] = {
    VLC_SYMBOL(vlc_entry),
    NULL
};

static void on_caption(const libvlc_event_t *event, void *opaque)
{
    (void)event; (void)opaque;
    vlc_sem_post(&g_sem_on_caption);
    g_caption_event = event;
    vlc_sem_wait(&g_sem_continue_on_caption);
}

int main(void)
{
    vlc_sem_init(&g_decoder_started, 0);
    vlc_sem_init(&g_sem_on_caption, 0);
    vlc_sem_init(&g_sem_continue_on_caption, 0);
    vlc_sem_init(&g_sem_prepare_picture, 0);
    vlc_sem_init(&g_sem_continue_prepare_picture, 0);
    vlc_sem_init(&g_sem_window_ready, 0);

    test_init();

    const char *args[] = { "-vv" };
    libvlc_instance_t *vlc = libvlc_new (ARRAY_SIZE(args), args);
    assert (vlc != NULL);


    const char source_800_600[] = "mock://video_track_count=1;length=100000000000;video_width=800;video_height=600";
    libvlc_media_t *md = libvlc_media_new_location(source_800_600);
    assert (md != NULL);

    libvlc_media_player_t *mp = libvlc_media_player_new (vlc);
    assert (mp != NULL);

    libvlc_media_player_set_media(mp, md);
    
    libvlc_event_manager_t *em = libvlc_media_player_event_manager(mp);
    assert(em != NULL);

    // TODO
    libvlc_event_attach(em, libvlc_CaptionsToDisplay, on_caption, NULL);

    libvlc_media_player_play(mp);

    /* Wait for the decoder and window*/
    fprintf(stderr, "Waiting decoder...\n");
    vlc_sem_wait(&g_decoder_started);
    fprintf(stderr, "Waiting window...\n");
    vlc_sem_wait(&g_sem_window_ready);

    /* Ok, now we can trigger the decoder ourselves */
    for (char i=1; i<10; ++i)
    {
        fprintf(stderr, "Sending frame number %d...\n", (int)i);
        const picture_resource_t resource =
            { .p_sys = NULL, };

        picture_t *pic = picture_NewFromResource(&g_dec->fmt_out.video, &resource);
        assert(pic);
        pic->date = VLC_TICK_0;
        pic->b_progressive = true;
        pic->b_force = true;
        pic->captions.size = i;
        memset(pic->captions.bytes, i, i);

        decoder_QueueVideo(g_dec, pic);
        vlc_window_ReportVsyncReached(g_wnd, vlc_tick_now() + VLC_TICK_FROM_MS(6));

        /* vout_display_t::prepare barrier */
        fprintf(stderr, "Waiting the prepare picture\n");
        vlc_sem_wait(&g_sem_prepare_picture);

        fprintf(stderr, "CAPTIONS SIZE: %zu\n", g_prepared_picture->captions.size);
        /* Here we check that the picture matches what we expect in the test
         * to ensure the test will run correctly. */
        assert(g_prepared_picture->captions.size == (size_t)i);

        /* Free the vout_display_t::prepare */
        vlc_sem_post(&g_sem_continue_prepare_picture);

        /* on_caption barrier */
        fprintf(stderr, "Waiting the picture caption event\n");
        vlc_sem_wait(&g_sem_on_caption);

        /* Here we check the result of the test. */
        assert(g_caption_event->u.captions_to_display.i_cc == (size_t)i);

        /* Free the on_caption event */
        vlc_sem_post(&g_sem_continue_on_caption);
    }

    libvlc_event_detach(em, libvlc_CaptionsToDisplay, on_caption, NULL);

    libvlc_media_release (md);

    libvlc_media_player_stop_async (mp);
    libvlc_media_player_release (mp);
    libvlc_release (vlc);

    return 0;
}
