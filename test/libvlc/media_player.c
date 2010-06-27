/*
 * media_player.c - libvlc smoke test
 *
 * $Id$
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

static void wait_playing(libvlc_media_player_t *mp)
{
    libvlc_state_t state;
    do {
        state = libvlc_media_player_get_state (mp);
    } while(state != libvlc_Playing &&
            state != libvlc_Error &&
            state != libvlc_Ended );

    state = libvlc_media_player_get_state (mp);
    assert(state == libvlc_Playing || state == libvlc_Ended);
}

static void wait_paused(libvlc_media_player_t *mp)
{
    libvlc_state_t state;
    do {
        state = libvlc_media_player_get_state (mp);
    } while(state != libvlc_Paused &&
            state != libvlc_Ended );

    state = libvlc_media_player_get_state (mp);
    assert(state == libvlc_Paused || state == libvlc_Ended);
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
}

static void test_media_player_set_media(const char** argv, int argc)
{
    const char * file = test_default_sample;

    log ("Testing set_media\n");

    libvlc_instance_t *vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    libvlc_media_t *md = libvlc_media_new_path (vlc, file);
    assert (md != NULL);

    libvlc_media_player_t *mp = libvlc_media_player_new (vlc);
    assert (mp != NULL);

    libvlc_media_player_set_media (mp, md);

    libvlc_media_release (md);

    libvlc_media_player_play (mp);

    wait_playing (mp);

    libvlc_media_player_stop (mp);
    libvlc_media_player_release (mp);
    libvlc_release (vlc);
}

static void test_media_player_play_stop(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_t *md;
    libvlc_media_player_t *mi;
    const char * file = test_default_sample;

    log ("Testing play and pause of %s\n", file);

    vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    md = libvlc_media_new_path (vlc, file);
    assert (md != NULL);

    mi = libvlc_media_player_new_from_media (md);
    assert (mi != NULL);

    libvlc_media_release (md);

    libvlc_media_player_play (mi);

    wait_playing (mi);

    libvlc_media_player_stop (mi);
    libvlc_media_player_release (mi);
    libvlc_release (vlc);
}

static void test_media_player_pause_stop(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_t *md;
    libvlc_media_player_t *mi;
    const char * file = test_default_sample;

    log ("Testing pause and stop of %s\n", file);

    vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    md = libvlc_media_new_path (vlc, file);
    assert (md != NULL);

    mi = libvlc_media_player_new_from_media (md);
    assert (mi != NULL);

    libvlc_media_release (md);

    test_audio_video(mi);

    libvlc_media_player_play (mi);
    log ("Waiting for playing\n");
    wait_playing (mi);
    test_audio_video(mi);

    libvlc_media_player_set_pause (mi, true);
    log ("Waiting for pause\n");
    wait_paused (mi);
    test_audio_video(mi);

    libvlc_media_player_stop (mi);
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

    return 0;
}
