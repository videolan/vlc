/*
 * testapi.c - libvlc smoke test
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <vlc/libvlc.h>

#undef NDEBUG
#include <assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static libvlc_exception_t ex;

static void catch (void)
{
    if (libvlc_exception_raised (&ex))
    {
         fprintf (stderr, "Exception: %s\n",
                  libvlc_exception_get_message (&ex));
         abort ();
    }

    assert (libvlc_exception_get_message (&ex) == NULL);
    libvlc_exception_clear (&ex);
}

static void test_core (const char ** argv, int argc);
static void test_media_list (const char ** argv, int argc);

static void test_core (const char ** argv, int argc)
{
    libvlc_instance_t *vlc;
    int id;

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
    libvlc_release (vlc, &ex);
    catch ();
    libvlc_release (vlc, &ex);
    catch ();
}

static void test_media_list (const char ** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_descriptor_t *md;
    libvlc_media_list_t *ml;

    libvlc_exception_init (&ex);
    vlc = libvlc_new (argc, argv, &ex);
    catch ();

    ml = libvlc_media_list_new (vlc, &ex);
    catch ();

    md = libvlc_media_descriptor_new (vlc, "/dev/null", &ex);
    catch ();

    libvlc_media_list_add_media_descriptor (ml, md, &ex);
    catch ();
    libvlc_media_list_add_media_descriptor (ml, md, &ex);
    catch ();

    assert( libvlc_media_list_count (ml, &ex) == 2 );
    catch ();

    libvlc_media_descriptor_release (md);

    libvlc_media_list_release (ml);

    libvlc_release (vlc, &ex);
    catch ();
}

int main (int argc, char *argv[])
{
    const char *args[argc + 3];
    int nlibvlc_args = sizeof (args) / sizeof (args[0]);

    alarm (30); /* Make sure "make check" does not get stuck */

    args[0] = "-vvv";
    args[1] = "-I";
    args[2] = "-dummy";
    args[3] = "--plugin-path=../modules";
    for (int i = 1; i < argc; i++)
        args[i + 3] = argv[i];

    test_core (args, nlibvlc_args);

    test_media_list (args, nlibvlc_args);
    return 0;
}
