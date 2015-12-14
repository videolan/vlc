/*****************************************************************************
 * bits.c test bitstream reader
 *****************************************************************************
 * Copyright (C) 2015 VLC authors and VideoLAN
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

#include "../../libvlc/test.h"

#include <vlc_bits.h>
#include <assert.h>

int main( void )
{
    test_init();

    uint8_t z[2];
    bs_t bs;

    bs_init( &bs, NULL, 0 );
    assert( bs_remain(&bs) == 0 );
    assert( bs_pos(&bs) == 0 );

    bs_init( &bs, &z, 0 );
    assert( bs_remain(&bs) == 0 );
    assert( bs_pos(&bs) == 0 );

    bs_init( &bs, &z, 1 );
    assert( bs_remain(&bs) == 8 );
    assert( bs_pos(&bs) == 0 );

    bs_skip( &bs, 3 );
    assert( bs_remain(&bs) == 5 );
    assert( bs_pos(&bs) == 3 );

    bs_init( &bs, &z, 2 );
    assert( bs_remain(&bs) == 16 );

    bs_write( &bs, 1, 0 );
    assert( bs_remain(&bs) == 16 );

    bs_read1( &bs );
    assert( bs_remain(&bs) == 15 );
    assert( bs_pos(&bs) == 1 );

    bs_read( &bs, 7 );
    assert( bs_remain(&bs) == 8 );
    assert( bs_pos(&bs) == 8 );

    bs_read1( &bs );
    assert( bs_remain(&bs) == 7 );
    assert( bs_pos(&bs) == 9 );

    bs_align( &bs );
    assert( bs_remain(&bs) == 0 );
    assert( bs_pos(&bs) == 16 );

    z[0] = 0xAA;
    z[1] = 0x55;
    bs_init( &bs, &z, 2 );
    assert( bs_read(&bs, 4) == 0x0A );
    assert( bs_read(&bs, 12) == ((0x0A << 8) | 0x55) );

    z[0] = 0x15;
    z[1] = 0x23;
    bs_init( &bs, &z, 2 );
    assert( bs_read_ue(&bs) == 0x09 );
    assert( bs_remain(&bs) == 9 );
    assert( bs_read1(&bs)  == 1 );
    assert( bs_read_se(&bs) == 2 );
    assert( bs_remain(&bs) == 3 );
    assert( bs_read_se(&bs) == -1 );
    assert( bs_eof(&bs) );

    return 0;
}
