/*****************************************************************************
 * md5.c: test md5
 *****************************************************************************
 * Copyright (C) 2011 VideoLAN
 * $Id$
 *
 * Authors: Jean-Bapstiste Kempf <jb@videolan.org>
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
#include <vlc_md5.h>

typedef struct
{
    const char *psz_string;
    const char *psz_md5;
} md5_sample_t;

static const md5_sample_t md5_samples[] =
{
    { "azertyuiop", "7682fe272099ea26efe39c890b33675b"    },
    { NULL,         NULL            }
};

static void test_config_StringEscape()
{
    for( int i = 0; md5_samples[i].psz_string; i++ )
    {
        struct md5_s md5;
        InitMD5( &md5 );
        AddMD5( &md5, md5_samples[i].psz_string, strlen( md5_samples[i].psz_string ) );
        EndMD5( &md5 );
        char * psz_hash = psz_md5_hash( &md5 );

        printf( "Output: %s, Expected: %s\n", psz_hash, md5_samples[i].psz_md5 );
        assert( !strcmp( psz_hash, md5_samples[i].psz_md5 ) );
        free( psz_hash );
    }
}

int main( void )
{
    log( "Testing md5 calculation\n" );
    test_config_StringEscape();

    return 0;
}
