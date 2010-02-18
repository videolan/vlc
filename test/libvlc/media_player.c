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

static void test_media_player_set_media(const char** argv, int argc)
{
    const char * file = test_default_sample;

    log ("Testing set_media\n");

    libvlc_instance_t *vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    libvlc_media_t *md = libvlc_media_new (vlc, file);
    assert (md != NULL);

    libvlc_media_player_t *mp = libvlc_media_player_new (vlc);
    assert (mp != NULL);

    libvlc_media_player_set_media (mp, md);

    libvlc_media_release (md);

    libvlc_media_player_play (mp);

    /* Wait a correct state */
    libvlc_state_t state;
    do {
        state = libvlc_media_player_get_state (mp);
    } while(state != libvlc_Playing &&
            state != libvlc_Error &&
            state != libvlc_Ended );

    assert(state == libvlc_Playing || state == libvlc_Ended);

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

    md = libvlc_media_new (vlc, file);
    assert (md != NULL);

    mi = libvlc_media_player_new_from_media (md);
    assert (mi != NULL);

    libvlc_media_release (md);

    libvlc_media_player_play (mi);

    /* Wait a correct state */
    libvlc_state_t state;
    do {
        state = libvlc_media_player_get_state (mi);
    } while( state != libvlc_Playing &&
             state != libvlc_Error &&
             state != libvlc_Ended );

    assert( state == libvlc_Playing || state == libvlc_Ended );

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

    md = libvlc_media_new (vlc, file);
    assert (md != NULL);

    mi = libvlc_media_player_new_from_media (md);
    assert (mi != NULL);

    libvlc_media_release (md);

    libvlc_media_player_play (mi);

    log ("Waiting for playing\n");

    /* Wait a correct state */
    libvlc_state_t state;
    do {
        state = libvlc_media_player_get_state (mi);
    } while( state != libvlc_Playing &&
             state != libvlc_Error &&
             state != libvlc_Ended );

    assert( state == libvlc_Playing || state == libvlc_Ended );

#if 0
    /* This can't work because under some condition (short file, this is the case) this will be
     * equivalent to a play() */
    libvlc_media_player_pause (mi);

    log ("Waiting for pause\n");

    /* Wait a correct state */
    do {
        state = libvlc_media_player_get_state (mi);
    } while( state != libvlc_Paused &&
            state != libvlc_Error &&
            state != libvlc_Ended );

    assert( state == libvlc_Paused || state == libvlc_Ended );
#endif

    libvlc_media_player_stop (mi);
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
