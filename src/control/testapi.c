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

#include <vlc/libvlc.h>

#undef NDEBUG
#include <assert.h>

#include <stdio.h>
#include <stdlib.h>

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

int main (int argc, char *argv[])
{
    libvlc_instance_t *vlc;
    const char *args[argc + 3];

    args[0] = "-I";
    args[1] = "dummy";
    args[2] = "-vvv";
    args[3] = "--plugin-path=..";
    for (int i = 1; i < argc; i++)
        args[i + 3] = argv[i];

    libvlc_exception_init (&ex);
    vlc = libvlc_new (sizeof (args) / sizeof (args[0]), args, &ex);
    catch ();

    libvlc_destroy (vlc, &ex);
    catch ();
    return 0;
}
