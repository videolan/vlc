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

static void preparsed_changed(const libvlc_event_t *event, void *user_data)
{
    (void)event;

    int *received = user_data;
    *received = true;
}

static void test_media_preparsed(const char** argv, int argc)
{
    const char * file = test_default_sample;

    log ("Testing set_media\n");

    libvlc_instance_t *vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    libvlc_media_t *media = libvlc_media_new_path (vlc, file);
    assert (media != NULL);

    volatile int received = false;

    // Check to see if we are properly receiving the event.
    libvlc_event_manager_t *em = libvlc_media_event_manager (media);
    libvlc_event_attach (em, libvlc_MediaPreparsedChanged, preparsed_changed, &received);

    // Parse the media. This is synchronous.
    libvlc_media_parse(media);

    // Wait to see if we properly receive preparsed.
    while (!received);

    // We are good, now check Elementary Stream info.
    libvlc_media_es_t *es;
    int num = libvlc_media_get_es(media, &es);
    assert(num > 0);
    free(es);

    libvlc_media_release (media);
    libvlc_release (vlc);
}

int main (void)
{
    test_init();

    test_media_preparsed (test_defaults_args, test_defaults_nargs);

    return 0;
}
