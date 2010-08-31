/*****************************************************************************
 * chain.c: test configuration chains
 *****************************************************************************
 * Copyright (C) 2010 VideoLAN and authors
 * $Id$
 *
 * Authors: Rémi Duraffort <ivoire@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <stdlib.h>
#include <string.h>

#include "../../libvlc/test.h"

#include <vlc_common.h>
#include <vlc_configuration.h>

typedef struct
{
    const char *psz_string;
    const char *psz_escaped;
}sample_t;

static const sample_t samples[] =
{
    { "a",          "a" },
    { "azertyuiop", "azertyuiop"    },
    { "  test    ", "  test    "    },
    { "it's",       "it\\'s"        },
    { "''''",       "\\'\\'\\'\\'"  },
    { "' a '",      "\\' a \\'"     },
    { "\"quote\"",  "\\\"quote\\\"" },
    { " az\" ",     " az\\\" "      },
    { "\\test",     "\\\\test"      },
    { NULL,   NULL }
};

static void test_config_StringEscape()
{
    for( int i = 0; samples[i].psz_string; i++ )
    {
        char *psz_tmp = config_StringEscape( samples[i].psz_string );
        assert( !strcmp( psz_tmp, samples[i].psz_escaped ) );
        free( psz_tmp );
    }
}

static void test_config_StringUnEscape()
{
    for( int i =0; samples[i].psz_string; i++ )
    {
        char *psz_tmp = strdup( samples[i].psz_escaped );
        config_StringUnescape( psz_tmp );
        assert( !strcmp( psz_tmp, samples[i].psz_string ) );
        free( psz_tmp );
    }
}

int main( void )
{
    log( "Testing config chain escaping\n" );
    test_config_StringEscape();
    log( "Testing config chain un-escaping\n" );
    test_config_StringUnEscape();

    return 0;
}
