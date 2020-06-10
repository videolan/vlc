/*
 * media_player.c - libvlc smoke test
 *
 */

/**********************************************************************
 *  Copyright (C) 2007 Rémi Denis-Courmont.                           *
 *  This program is free software; you can redistribute and/or modify *
 *  it under the terms of the GNU General Public License as published *
 *  by the Free Software Foundation; version 2 of the license, or (at *
 *  your option) any later version.                                   *
 *                                                                    *
 *  This program is distributed in the hope that it will be useful,   *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of    *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *  See the GNU General Public License for more details.              *
 *                                                                    *
 *  You should have received a copy of the GNU General Public License *
 *  along with this program; if not, you can get it from:             *
 *  http://www.gnu.org/copyleft/gpl.html                              *
 **********************************************************************/

#include "test.h"
#include <vlc_common.h>

struct event_ctx
{
    vlc_sem_t sem_ev;
    vlc_sem_t sem_done;
    const struct libvlc_event_t *ev;
};

static void event_ctx_init(struct event_ctx *ctx)
{
    vlc_sem_init(&ctx->sem_ev, 0);
    vlc_sem_init(&ctx->sem_done, 0);
    ctx->ev = NULL;
}

static const struct libvlc_event_t *even_ctx_wait_event(struct event_ctx *ctx)
{
    vlc_sem_wait(&ctx->sem_ev);
    assert(ctx->ev != NULL);
    return ctx->ev;
}

static void event_ctx_release(struct event_ctx *ctx)
{
    assert(ctx->ev != NULL);
    ctx->ev = NULL;
    vlc_sem_post(&ctx->sem_done);
}

static void even_ctx_wait(struct event_ctx *ctx)
{
    even_ctx_wait_event(ctx);
    event_ctx_release(ctx);
}

static void on_event(const struct libvlc_event_t *event, void *data)
{
    struct event_ctx *ctx = data;

    assert(ctx->ev == NULL);
    ctx->ev = event;

    vlc_sem_post(&ctx->sem_ev);

    vlc_sem_wait(&ctx->sem_done);

    assert(ctx->ev == NULL);
}

static void play_and_wait(libvlc_media_player_t *mp)
{
    libvlc_event_manager_t *em = libvlc_media_player_event_manager(mp);

    struct event_ctx ctx;
    event_ctx_init(&ctx);

    int res;
    res = libvlc_event_attach(em, libvlc_MediaPlayerPlaying, on_event, &ctx);
    assert(!res);

    libvlc_media_player_play(mp);

    test_log("Waiting for playing\n");
    even_ctx_wait(&ctx);

    libvlc_event_detach(em, libvlc_MediaPlayerPlaying, on_event, &ctx);
}

static void pause_and_wait(libvlc_media_player_t *mp)
{
    libvlc_event_manager_t *em = libvlc_media_player_event_manager(mp);

    struct event_ctx ctx;
    event_ctx_init(&ctx);

    int res;
    res = libvlc_event_attach(em, libvlc_MediaPlayerPaused, on_event, &ctx);
    assert(!res);

    assert(libvlc_media_player_get_state(mp) == libvlc_Playing);

    libvlc_media_player_set_pause(mp, true);

    test_log("Waiting for pause\n");
    even_ctx_wait(&ctx);

    assert(libvlc_media_player_get_state(mp) == libvlc_Paused);

    libvlc_event_detach(em, libvlc_MediaPlayerPaused, on_event, &ctx);
}

/* Test a bunch of A/V properties. This most does nothing since the current
 * test file contains a dummy audio track. This is a smoke test. */
static void test_audio_video(libvlc_media_player_t *mp)
{
    bool fs = libvlc_get_fullscreen(mp);
    libvlc_set_fullscreen(mp, true);
    assert(libvlc_get_fullscreen(mp));
    libvlc_set_fullscreen(mp, false);
    assert(!libvlc_get_fullscreen(mp));
    libvlc_toggle_fullscreen(mp);
    assert(libvlc_get_fullscreen(mp));
    libvlc_toggle_fullscreen(mp);
    assert(!libvlc_get_fullscreen(mp));
    libvlc_set_fullscreen(mp, fs);
    assert(libvlc_get_fullscreen(mp) == fs);

    assert(libvlc_video_get_scale(mp) == 0.); /* default */
    libvlc_video_set_scale(mp, 0.); /* no-op */
    libvlc_video_set_scale(mp, 2.5);
    assert(libvlc_video_get_scale(mp) == 2.5);
    libvlc_video_set_scale(mp, 0.);
    libvlc_video_set_scale(mp, 0.); /* no-op */
    assert(libvlc_video_get_scale(mp) == 0.);

    libvlc_audio_output_device_t *aouts = libvlc_audio_output_device_enum(mp);
    for (libvlc_audio_output_device_t *e = aouts; e != NULL; e = e->p_next)
    {
        libvlc_audio_output_device_set( mp, NULL, e->psz_device );
    }
    libvlc_audio_output_device_list_release( aouts );
}

static void test_role(libvlc_media_player_t *mp)
{
    int role;

    /* Test default value */
    assert(libvlc_media_player_get_role(mp) == libvlc_role_Video);

    for (role = 0; libvlc_media_player_set_role(mp, role) == 0; role++)
        assert(libvlc_media_player_get_role(mp) == role);

    assert(role > libvlc_role_Last);
}

static void test_media_player_set_media(const char** argv, int argc)
{
    const char * file = test_default_sample;

    test_log ("Testing set_media\n");

    libvlc_instance_t *vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    libvlc_media_t *md = libvlc_media_new_location (vlc, file);
    assert (md != NULL);

    libvlc_media_player_t *mp = libvlc_media_player_new (vlc);
    assert (mp != NULL);

    libvlc_media_player_set_media (mp, md);

    libvlc_media_release (md);

    play_and_wait(mp);

    libvlc_media_player_stop_async (mp);
    libvlc_media_player_release (mp);
    libvlc_release (vlc);
}

static void test_media_player_play_stop(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_t *md;
    libvlc_media_player_t *mi;
    const char * file = test_default_sample;

    test_log ("Testing play and pause of %s\n", file);

    vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    md = libvlc_media_new_location (vlc, file);
    assert (md != NULL);

    mi = libvlc_media_player_new_from_media (md);
    assert (mi != NULL);

    libvlc_media_release (md);

    play_and_wait(mi);

    libvlc_media_player_stop_async (mi);
    libvlc_media_player_release (mi);
    libvlc_release (vlc);
}

static void test_media_player_pause_stop(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_t *md;
    libvlc_media_player_t *mi;
    const char * file = test_default_sample;

    test_log ("Testing pause and stop of %s\n", file);

    vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    md = libvlc_media_new_location (vlc, file);
    assert (md != NULL);

    mi = libvlc_media_player_new_from_media (md);
    assert (mi != NULL);

    libvlc_media_release (md);

    test_audio_video(mi);
    test_role(mi);

    play_and_wait(mi);
    test_audio_video(mi);

    pause_and_wait(mi);
    test_audio_video(mi);

    libvlc_media_player_stop_async (mi);
    test_audio_video(mi);

    libvlc_media_player_release (mi);
    libvlc_release (vlc);
}


int main (void)
{
    test_init();

    test_media_player_set_media (test_defaults_args, test_defaults_nargs);
    test_media_player_play_stop (test_defaults_args, test_defaults_nargs);
    test_media_player_pause_stop (test_defaults_args, test_defaults_nargs);
//    test_media_player_tracks (test_defaults_args, test_defaults_nargs);

    return 0;
}
