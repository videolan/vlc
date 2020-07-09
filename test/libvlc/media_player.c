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

struct track
{
    const char *id;
    bool toselect;
    bool selected;
    bool added;
    bool removed;
};

static const libvlc_track_type_t types[] =
    { libvlc_track_audio, libvlc_track_video, libvlc_track_text };

static struct track *get_track_from_id(struct track *tracks, const char *id)
{
    while (tracks->id != NULL)
    {
        if (strcmp(tracks->id, id) == 0)
            return tracks;
        tracks++;
    }
    return NULL;
}

static bool tracks_check_all_events(struct track *const *tracks, bool removed)
{
    for (size_t i = 0; i < ARRAY_SIZE(types); ++i)
    {
        libvlc_track_type_t type = types[i];
        const struct track *ttracks = tracks[type];

        while (ttracks->id != NULL)
        {
            if (removed)
            {
                if (!ttracks->removed)
                    return false;
            }
            else
            {
                if (!ttracks->added || ttracks->toselect != ttracks->selected)
                    return false;
            }
            ttracks++;
        }
    }

    return true;
}

static void test_media_player_tracks(const char** argv, int argc)
{
    test_log ("Testing tracks\n");

    const char *file = "mock://video_track_count=3;audio_track_count=3;sub_track_count=3";

    struct track atracks[] = {
        { "audio/0", false, false, false, false },
        { "audio/1", false, false, false, false },
        { "audio/2", true, false, false, false },
        { NULL, false, false, false, false },
    };
    static const char atracks_ids[] = "audio/2";

    struct track vtracks[] = {
        { "video/0", false, false, false, false },
        { "video/1", true, false, false, false },
        { "video/2", false, false, false, false },
        { NULL, false, false, false, false },
    };
    static const char vtracks_ids[] = "video/1";

    struct track stracks[] = {
        { "spu/0", true, false, false, false },
        { "spu/1", true, false, false, false },
        { "spu/2", false, false, false, false },
        { NULL, false, false, false, false },
    };
    static const char stracks_ids[] = "spu/0,spu/1";

    struct track *alltracks[] = {
        [libvlc_track_audio] = atracks,
        [libvlc_track_video] = vtracks,
        [libvlc_track_text] = stracks,
    };
    static const size_t alltracks_count[] = {
        [libvlc_track_audio] = 3,
        [libvlc_track_video] = 3,
        [libvlc_track_text] = 3,
    };

    /* Avoid leaks from various dlopen... */
    const char *new_argv[argc+1];
    for (int i = 0; i < argc; ++i)
        new_argv[i] = argv[i];
    new_argv[argc++] = "--codec=araw,rawvideo,subsdec,none";

    /* Load the mock media */
    libvlc_instance_t *vlc = libvlc_new (argc, new_argv);
    assert (vlc != NULL);
    libvlc_media_t *md = libvlc_media_new_location (vlc, file);
    assert (md != NULL);
    libvlc_media_player_t *mp = libvlc_media_player_new (vlc);
    assert (mp != NULL);
    libvlc_media_player_set_media (mp, md);
    libvlc_media_release (md);

    /* Confiture track selection before first play */
    libvlc_media_player_select_tracks_by_ids(mp, libvlc_track_audio, atracks_ids);
    libvlc_media_player_select_tracks_by_ids(mp, libvlc_track_video, vtracks_ids);
    libvlc_media_player_select_tracks_by_ids(mp, libvlc_track_text, stracks_ids);

    libvlc_media_track_t *libtrack;
    libvlc_event_manager_t *em = libvlc_media_player_event_manager(mp);
    struct event_ctx ctx;
    event_ctx_init(&ctx);

    int res;
    res = libvlc_event_attach(em, libvlc_MediaPlayerESAdded, on_event, &ctx);
    assert(!res);
    res = libvlc_event_attach(em, libvlc_MediaPlayerESDeleted, on_event, &ctx);
    assert(!res);
    res = libvlc_event_attach(em, libvlc_MediaPlayerESSelected, on_event, &ctx);
    assert(!res);

    libvlc_media_player_play (mp);

    /* Check that all tracks are added and selected according to
     * libvlc_media_player_select_tracks_by_ids */
    while (!tracks_check_all_events(alltracks, false))
    {
        const struct libvlc_event_t *ev = even_ctx_wait_event(&ctx);
        switch (ev->type)
        {
            case libvlc_MediaPlayerESAdded:
            {
                libvlc_track_type_t type = ev->u.media_player_es_changed.i_type;
                struct track *track =
                    get_track_from_id(alltracks[type],
                                      ev->u.media_player_es_changed.psz_id);
                assert(track);
                track->added = true;
                break;
            }
            case libvlc_MediaPlayerESSelected:
            {
                libvlc_track_type_t type = ev->u.media_player_es_selection_changed.i_type;
                const char *selected_id =
                    ev->u.media_player_es_selection_changed.psz_selected_id;
                const char *unselected_id =
                    ev->u.media_player_es_selection_changed.psz_unselected_id;

                assert(selected_id);
                assert(!unselected_id);

                struct track *track = get_track_from_id(alltracks[type],
                                                        selected_id);
                assert(track);
                assert(track->toselect);
                track->selected = true;
                break;
            }
            default:
                assert(!"Event not expected");
        }

        event_ctx_release(&ctx);
    }

    /* Compare with the tracklists */
    for (size_t i = 0; i < ARRAY_SIZE(types); ++i)
    {
        libvlc_track_type_t type = types[i];
        size_t track_count = alltracks_count[i];
        struct track *tracks = alltracks[i];

        libvlc_media_tracklist_t *tracklist =
            libvlc_media_player_get_tracklist(mp, type);
        assert(tracklist);
        assert(libvlc_media_tracklist_count(tracklist) == track_count);

        for (size_t j = 0; j < track_count; ++j)
        {
            struct track *track = &tracks[j];
            libtrack = libvlc_media_tracklist_at(tracklist, j);
            assert(libtrack);

            assert(strcmp(libtrack->psz_id, track->id) == 0);
            assert(libtrack->selected == track->selected);
        }
        libvlc_media_tracklist_delete(tracklist);
    }

    /* Select (replace) a new audio track */
    libtrack = libvlc_media_player_get_track_from_id(mp, "audio/0");
    assert(libtrack);
    libvlc_media_player_select_track(mp, libvlc_track_audio, libtrack);
    libvlc_media_track_release(libtrack);
    atracks[0].toselect = true;
    atracks[2].toselect = false;

    /* Add a new video track */
    libvlc_media_tracklist_t *tracklist =
        libvlc_media_player_get_tracklist(mp, libvlc_track_video);
    assert(tracklist);

    libvlc_media_track_t *vtrack1 =
        libvlc_media_track_hold(libvlc_media_tracklist_at(tracklist, 1));
    libvlc_media_track_t *vtrack2 =
        libvlc_media_track_hold(libvlc_media_tracklist_at(tracklist, 2));
    libvlc_media_tracklist_delete(tracklist);

    const libvlc_media_track_t *selecttracks[] = {
        vtrack1, vtrack2,
    };
    assert(vtrack1->selected); /* video/1 already selected */
    assert(!vtrack2->selected); /* select video/2 */

    libvlc_media_player_select_tracks(mp, libvlc_track_video, selecttracks, 2);
    vtracks[2].toselect = true;

    libvlc_media_track_release(vtrack1);
    libvlc_media_track_release(vtrack2);

    /* Unselect all spu tracks */
    libvlc_media_player_select_track(mp, libvlc_track_text, NULL);
    stracks[0].toselect = stracks[1].toselect = false;

    /* Check that all tracks are added and selected according to previous
     * changes. */
    while (!tracks_check_all_events(alltracks, false))
    {
        const struct libvlc_event_t *ev = even_ctx_wait_event(&ctx);
        assert(ev->type == libvlc_MediaPlayerESSelected);

        libvlc_track_type_t type = ev->u.media_player_es_selection_changed.i_type;
        const char *selected_id =
            ev->u.media_player_es_selection_changed.psz_selected_id;
        const char *unselected_id =
            ev->u.media_player_es_selection_changed.psz_unselected_id;

        if (unselected_id)
        {
            struct track *track =
                get_track_from_id(alltracks[type], unselected_id);
            assert(!track->toselect);
            assert(track->selected);
            track->selected = false;
        }

        if (selected_id)
        {
            struct track *track =
                get_track_from_id(alltracks[type], selected_id);
            assert(track->toselect);
            assert(!track->selected);
            track->selected = true;
        }

        event_ctx_release(&ctx);
    }

    libvlc_media_player_stop_async (mp);

    /* Check that all tracks are removed */
    while (!tracks_check_all_events(alltracks, true))
    {
        const struct libvlc_event_t *ev = even_ctx_wait_event(&ctx);

        if (ev->type == libvlc_MediaPlayerESDeleted)
        {
            libvlc_track_type_t type = ev->u.media_player_es_changed.i_type;
            struct track *track =
                get_track_from_id(alltracks[type],
                                  ev->u.media_player_es_changed.psz_id);
            assert(track);
            track->removed = true;
        }
        event_ctx_release(&ctx);
    }

    libvlc_event_detach(em, libvlc_MediaPlayerESAdded, on_event, &ctx);
    libvlc_event_detach(em, libvlc_MediaPlayerESDeleted, on_event, &ctx);
    libvlc_event_detach(em, libvlc_MediaPlayerESSelected, on_event, &ctx);

    libvlc_media_player_release (mp);
    libvlc_release (vlc);
}

/* Regression test when having multiple libvlc instances */
static void test_media_player_multiple_instance(const char** argv, int argc)
{
    /* When multiple libvlc instance exist */
    libvlc_instance_t *instance1 = libvlc_new(argc, argv);
    libvlc_instance_t *instance2 = libvlc_new(argc, argv);

    /* ...with the media and the player being on different instances */
    libvlc_media_t *media1 = libvlc_media_new_path(instance2, "foo");
    libvlc_media_player_t *player1 = libvlc_media_player_new(instance1);
    libvlc_media_player_set_media(player1, media1);

    /* ...and both being released */
    libvlc_media_release(media1);
    libvlc_media_player_release(player1);

    /* There is no use-after-free when creating a player on the media instance,
     * meaning that the player1 did release the correct libvlc instance.*/
    libvlc_media_player_t *player2 = libvlc_media_player_new(instance2);

    /* And the libvlc nstances can be released without breaking the
     * instance inside the player. */
    libvlc_release(instance2);
    libvlc_release(instance1);

    libvlc_media_player_release(player2);
}

int main (void)
{
    test_init();

    test_media_player_set_media (test_defaults_args, test_defaults_nargs);
    test_media_player_play_stop (test_defaults_args, test_defaults_nargs);
    test_media_player_pause_stop (test_defaults_args, test_defaults_nargs);
    test_media_player_tracks (test_defaults_args, test_defaults_nargs);
    test_media_player_multiple_instance (test_defaults_args, test_defaults_nargs);

    return 0;
}
