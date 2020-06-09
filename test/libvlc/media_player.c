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

static void on_event(const struct libvlc_event_t *event, void *data)
{
    (void) event;
    vlc_sem_t *sem = data;
    vlc_sem_post(sem);
}

static void play_and_wait(libvlc_media_player_t *mp)
{
    libvlc_event_manager_t *em = libvlc_media_player_event_manager(mp);

    vlc_sem_t sem;
    vlc_sem_init(&sem, 0);

    int res;
    res = libvlc_event_attach(em, libvlc_MediaPlayerPlaying, on_event, &sem);
    assert(!res);

    libvlc_media_player_play(mp);

    test_log("Waiting for playing\n");
    vlc_sem_wait(&sem);

    libvlc_event_detach(em, libvlc_MediaPlayerPlaying, on_event, &sem);
}

static void pause_and_wait(libvlc_media_player_t *mp)
{
    libvlc_event_manager_t *em = libvlc_media_player_event_manager(mp);

    vlc_sem_t sem;
    vlc_sem_init(&sem, 0);

    int res;
    res = libvlc_event_attach(em, libvlc_MediaPlayerPaused, on_event, &sem);
    assert(!res);

    assert(libvlc_media_player_get_state(mp) == libvlc_Playing);

    libvlc_media_player_set_pause(mp, true);

    test_log("Waiting for pause\n");
    vlc_sem_wait(&sem);

    assert(libvlc_media_player_get_state(mp) == libvlc_Paused);

    libvlc_event_detach(em, libvlc_MediaPlayerPaused, on_event, &sem);
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
