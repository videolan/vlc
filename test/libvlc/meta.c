/*
 * meta.c - libvlc smoke test
 *
 */

/**********************************************************************
 *  Copyright (C) 2007 Rémi Denis-Courmont.                           *
 *  Copyright (C) 2008 Pierre d'Herbemont.                            *
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

#include <string.h>

#include "test.h"

static void test_meta (const char ** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_t *media;
    char * artist;

    test_log ("Testing meta\n");

    vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    media = libvlc_media_new_path (vlc, "samples/meta.sample");
    assert( media );

    libvlc_media_parse (media);

    artist = libvlc_media_get_meta (media, libvlc_meta_Artist);

    const char *expected_artist = "mike";

    assert (artist);
    test_log ("+ got '%s' as Artist, expecting %s\n", artist, expected_artist);

    int string_compare = strcmp (artist, expected_artist);
    assert (!string_compare);

    free (artist);
    libvlc_media_release (media);
    libvlc_release (vlc);
}


int main (void)
{
    test_init();

    test_meta (test_defaults_args, test_defaults_nargs);

    return 0;
}
