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

static void test_media_player_play_stop(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_t *md;
    libvlc_media_player_t *mi;
    const char * file = test_default_sample;

    log ("Testing play and pause of %s\n", file);

    libvlc_exception_init (&ex);
    vlc = libvlc_new (argc, argv, &ex);
    catch ();

    md = libvlc_media_new (vlc, file, &ex);
    catch ();

    mi = libvlc_media_player_new_from_media (md, &ex);
    catch ();

    libvlc_media_release (md);

    libvlc_media_player_play (mi, &ex);
    catch ();

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

    libvlc_exception_init (&ex);
    vlc = libvlc_new (argc, argv, &ex);
    catch ();

    md = libvlc_media_new (vlc, file, &ex);
    catch ();

    mi = libvlc_media_player_new_from_media (md, &ex);
    catch ();

    libvlc_media_release (md);

    libvlc_media_player_play (mi, &ex);
    catch ();

    log ("Waiting for playing\n");

    /* Wait a correct state */
    libvlc_state_t state;
    do {
        state = libvlc_media_player_get_state (mi);
        catch ();
    } while( state != libvlc_Playing &&
             state != libvlc_Error &&
             state != libvlc_Ended );

    assert( state == libvlc_Playing || state == libvlc_Ended );

#if 0
    /* This can't work because under some condition (short file, this is the case) this will be
     * equivalent to a play() */
    libvlc_media_player_pause (mi, &ex);
    catch();

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

    test_media_player_play_stop (test_defaults_args, test_defaults_nargs);
    test_media_player_pause_stop (test_defaults_args, test_defaults_nargs);

    return 0;
}
