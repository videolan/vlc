/*****************************************************************************
 * md5.c: test md5
 *****************************************************************************
 * Copyright (C) 2011 VideoLAN
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
#include <vlc_strings.h>
#include <vlc_hash.h>

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

static void test_vlc_hash_md5()
{
    for( int i = 0; md5_samples[i].psz_string; i++ )
    {
        char psz_hash[VLC_HASH_MD5_DIGEST_HEX_SIZE];
        vlc_hash_md5_t md5;
        vlc_hash_md5_Init( &md5 );
        vlc_hash_md5_Update( &md5, md5_samples[i].psz_string, strlen( md5_samples[i].psz_string ) );
        vlc_hash_FinishHex( &md5, psz_hash );

        if( strcmp( psz_hash, md5_samples[i].psz_md5 ) )
        {
            printf( "Output: %s\nExpected: %s\n", psz_hash,
                    md5_samples[i].psz_md5 );
            abort();
        }
    }
}

int main( void )
{
    test_vlc_hash_md5();

    return 0;
}
