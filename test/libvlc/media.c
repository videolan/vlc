/*
 * media_player.c - libvlc smoke test
 *
 * $Id$
 */

/**********************************************************************
 *  Copyright (C) 2010 Pierre d'Herbemont.                            *
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
#include <vlc_threads.h>

struct test_media_preparsed_sys
{
    vlc_mutex_t lock;
    vlc_cond_t wait;
    bool preparsed;
};

static void preparsed_changed(const libvlc_event_t *event, void *user_data)
{
    (void)event;

    struct test_media_preparsed_sys *sys = user_data;

    vlc_mutex_lock (&sys->lock);
    sys->preparsed = true;
    vlc_cond_signal (&sys->wait);
    vlc_mutex_unlock (&sys->lock);
}

static void test_media_preparsed(const char** argv, int argc)
{
    // We use this image file because "empty.voc" has no track.
    const char * file = SRCDIR"/samples/image.jpg";

    log ("Testing set_media\n");

    libvlc_instance_t *vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    libvlc_media_t *media = libvlc_media_new_path (vlc, file);
    assert (media != NULL);

    struct test_media_preparsed_sys preparsed_sys, *sys = &preparsed_sys;
    sys->preparsed = false;
    vlc_mutex_init (&sys->lock);
    vlc_cond_init (&sys->wait);

    // Check to see if we are properly receiving the event.
    libvlc_event_manager_t *em = libvlc_media_event_manager (media);
    libvlc_event_attach (em, libvlc_MediaParsedChanged, preparsed_changed, sys);

    // Parse the media. This is synchronous.
    libvlc_media_parse_async(media);

    // Wait for preparsed event
    vlc_mutex_lock (&sys->lock);
    while (!sys->preparsed)
        vlc_cond_wait (&sys->wait, &sys->lock);
    vlc_mutex_unlock (&sys->lock);

    // We are good, now check Elementary Stream info.
    libvlc_media_track_t **tracks;
    unsigned nb_tracks = libvlc_media_tracks_get (media, &tracks);
    assert (nb_tracks == 1);
    assert (tracks[0]->i_type == libvlc_track_video);
    libvlc_media_tracks_release (tracks, nb_tracks);

    vlc_mutex_destroy (&sys->lock);
    vlc_cond_destroy (&sys->wait);

    libvlc_media_release (media);
    libvlc_release (vlc);
}

int main (void)
{
    test_init();

    test_media_preparsed (test_defaults_args, test_defaults_nargs);

    return 0;
}
