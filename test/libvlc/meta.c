/*
 * meta.c - libvlc smoke test
 *
 * $Id$
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

#include "test.h"

static void test_meta (const char ** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_t *media;
    char * artist;

    log ("Testing meta\n");

    libvlc_exception_init (&ex);
    vlc = libvlc_new (argc, argv, &ex);
    catch ();

    media = libvlc_media_new (vlc, "samples/meta.sample", &ex);
    catch ();

    /* Tell that we are interested in this precise meta data
     * This is needed to trigger meta data reading
     * (the first calls return NULL) */
    artist = libvlc_media_get_meta (media, libvlc_meta_Artist, &ex);
    catch ();

    free (artist);

    /* Wait for the meta */
    while (!libvlc_media_is_preparsed (media, &ex))
    {
        catch ();
        msleep (10000);
    }

    artist = libvlc_media_get_meta (media, libvlc_meta_Artist, &ex);
    catch ();

    const char *expected_artist = "mike";

    assert (artist);
    log ("+ got '%s' as Artist, expecting %s\n", artist, expected_artist);

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
