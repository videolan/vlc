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

static const char * test_defaults_args[] = {
    "-v",
    "--ignore-config",
    "-I",
    "dummy",
    "--no-media-library",
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

static inline void test_init (void)
{
    (void)test_default_sample; /* This one may not be used */
    alarm (10); /* Make sure "make check" does not get stuck */
    setenv( "VLC_PLUGIN_PATH", "../modules", 1 );
}

#endif /* TEST_H */
