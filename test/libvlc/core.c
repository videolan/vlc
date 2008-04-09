/*
 * core.c - libvlc smoke test
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

static void test_core (const char ** argv, int argc)
{
    libvlc_instance_t *vlc;
    int id;

    log ("Testing core\n");

    libvlc_exception_init (&ex);
    vlc = libvlc_new (argc, argv, &ex);
    catch ();

    libvlc_playlist_clear (vlc, &ex);
    catch ();

    id = libvlc_playlist_add_extended (vlc, "/dev/null", "Test", 0, NULL,
                                       &ex);
    catch ();

    libvlc_playlist_clear (vlc, &ex);
    catch ();

    libvlc_retain (vlc);
    libvlc_release (vlc);
    libvlc_release (vlc);
}


int main (void)
{
    test_init();

    test_core (test_defaults_args, test_defaults_nargs);

    return 0;
}
