/*****************************************************************************
 * md5.c: test md5
 *****************************************************************************
 * Copyright (C) 2011 VideoLAN
 * $Id$
 *
 * Authors: Jean-Bapstiste Kempf <jb@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include <vlc_common.h>
#include <vlc_md5.h>

typedef struct
{
    const char *psz_string;
    const char *psz_md5;
} md5_sample_t;

static const md5_sample_t md5_samples[] =
{
    { "", "d41d8cd98f00b204e9800998ecf8427e" },
    { "a", "0cc175b9c0f1b6a831c399e269772661" },
    { "abc", "900150983cd24fb0d6963f7d28e17f72" },
    { "message digest", "f96b697d7cb7938d525a2f31aaf161d0" },
    { "abcdefghijklmnopqrstuvwxyz", "c3fcd3d76192e4007dfb496cca67e13b" },
    { "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
      "d174ab98d277d9f5a5611c2c9f419d9f" },
    { "12345678901234567890123456789012345678901234567890123456789012345678901"
      "234567890", "57edf4a22be3c955ac49da2e2107b67a" },
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

        if( strcmp( psz_hash, md5_samples[i].psz_md5 ) )
        {
            printf( "Output: %s\nExpected: %s\n", psz_hash,
                    md5_samples[i].psz_md5 );
            abort();
        }
        free( psz_hash );
    }
}

int main( void )
{
    test_config_StringEscape();

    return 0;
}
