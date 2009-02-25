/*
 * test.h - libvlc smoke test common definitions
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

#ifndef TEST_H
#define TEST_H

/*********************************************************************
 * Some useful common headers
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <vlc/vlc.h>

#undef NDEBUG
#include <assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>


/*********************************************************************
 * Some useful global var
 */
static libvlc_exception_t ex;

static const char * test_defaults_args[] = {
    "-vvv",
    "--ignore-config",
    "-I",
    "dummy",
    "--no-media-library",
    "--plugin-path=../modules",
    "--vout=dummy",
    "--aout=dummy"
};

static const int test_defaults_nargs =
    sizeof (test_defaults_args) / sizeof (test_defaults_args[0]);

/*static const char test_default_sample[] = "samples/test.sample";*/
static const char test_default_sample[] = SRCDIR"/samples/empty.voc";


/*********************************************************************
 * Some useful common functions
 */

#define log( ... ) printf( "testapi: " __VA_ARGS__ );

/* test if we have exception */
static inline bool have_exception (void)
{
    if (libvlc_exception_raised (&ex))
    {
        libvlc_exception_clear (&ex);
        return true;
    }
    else
        return false;
}

static inline void catch (void)
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

static inline void test_init (void)
{
    (void)test_default_sample; /* This one may not be used */
    alarm (50); /* Make sure "make check" does not get stuck */
}

#endif /* TEST_H */
