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
#include "../../../modules/packetizer/hxxx_ep3b.h"

#define test_assert(foo, bar) do {\
    ssize_t a = (foo); \
    ssize_t b = (bar); \
    if(a != b) { \
        printf("Failed (%s) line %d : %ld != %ld\n", psz_tag, __LINE__, a, b ); \
        return 1; \
    } } while( 0 )

#define TESTSET_COUNT 4
struct testset
{
    uint8_t count;
    const uint8_t data[12];
};

enum dataset
{
    TESTSET0 = 0,
    TESTSET1,
    TESTSET_EXPGOLOMB,
    TESTSET2,
} ;

#define bs_init(a,b,c) \
    bs_init( a, b, c); \
    if( callbacks ) { (a)->cb = *callbacks; \
                      if( cb_priv ) { (a)->p_priv = cb_priv; priv_init( cb_priv ); }; }

static int run_tests( const struct testset *p_testsets,
                      const char *psz_tag,
                      bs_byte_callbacks_t *callbacks,
                      void *cb_priv, void (*priv_init)(void *) )
{
    bs_t bs;

    bs_init( &bs, NULL, 0 );
    test_assert( bs_remain(&bs), 0 );
    test_assert( bs_pos(&bs), 0 );

    bs_init( &bs, p_testsets[TESTSET0].data, 0 );
    test_assert( bs_remain(&bs), 0 );
    test_assert( bs_pos(&bs), 0 );

    bs_init( &bs, p_testsets[TESTSET0].data,
                  p_testsets[TESTSET0].count );
    test_assert( bs_remain(&bs), 8 );
    test_assert( bs_pos(&bs), 0 );

    bs_skip( &bs, 3 );
    test_assert( bs_remain(&bs), 5 );
    test_assert( bs_pos(&bs), 3 );

    bs_init( &bs, p_testsets[TESTSET1].data,
                  p_testsets[TESTSET1].count );
    test_assert( bs_remain(&bs), 16 );

    bs_write( &bs, 1, 0 );
    test_assert( bs_remain(&bs), 16 );

    bs_read1( &bs );
    test_assert( bs_remain(&bs), 15 );
    test_assert( bs_pos(&bs), 1 );

    bs_read( &bs, 7 );
    test_assert( bs_remain(&bs), 8 );
    test_assert( bs_pos(&bs), 8 );

    bs_read1( &bs );
    test_assert( bs_remain(&bs), 7 );
    test_assert( bs_pos(&bs), 9 );

    bs_align( &bs );
    test_assert( bs_remain(&bs), 0 );
    test_assert( bs_pos(&bs), 16 );

    bs_init( &bs, p_testsets[TESTSET1].data,
                  p_testsets[TESTSET1].count );
    test_assert( bs_read(&bs, 4), 0x0A );
    test_assert( bs_read(&bs, 12), ((0x0A << 8) | 0x55) );

    bs_init( &bs, p_testsets[TESTSET_EXPGOLOMB].data,
                  p_testsets[TESTSET_EXPGOLOMB].count );
    test_assert( bs_read_ue(&bs), 0x09 );
    test_assert( bs_remain(&bs), 9 );
    test_assert( bs_read1(&bs), 1 );
    test_assert( bs_read_se(&bs), 2 );
    test_assert( bs_remain(&bs), 3 );
    test_assert( bs_read_se(&bs), -1 );
    test_assert( bs_eof(&bs), !0 );

    bs_init( &bs, p_testsets[TESTSET2].data,
                  p_testsets[TESTSET2].count );
    bs_skip( &bs, 24 );
    test_assert( bs_read( &bs, 8 ), 0xDD );
    test_assert( bs_read( &bs, 4 ), 0x0E );
    test_assert( bs_read( &bs, 8 ), 0xEF );
    test_assert( bs_remain( &bs ), 4 );

    bs_init( &bs, p_testsets[TESTSET2].data,
                  p_testsets[TESTSET2].count );
    bs_skip( &bs, 40 );
    test_assert( bs_read( &bs, 8 ), 0xFF );

    bs_init( &bs, p_testsets[TESTSET2].data,
                  p_testsets[TESTSET2].count );
    bs_skip( &bs, 20 );
    test_assert( bs_read( &bs, 8 ), 0xCD );
    test_assert( bs_read( &bs, 4 ), 0x0D );
    test_assert( bs_read( &bs, 8 ), 0xEE );
    test_assert( bs_remain( &bs ), 8 );

    /* */
    bs_init( &bs, p_testsets[TESTSET2].data,
                  p_testsets[TESTSET2].count );
    for( size_t i=0; i<6*8; i++ )
    {
        test_assert(bs_aligned( &bs ), !!(i%8 == 0));
        test_assert(bs_remain( &bs ), 6*8 - i);
        test_assert(bs_pos( &bs ), i);
        bs_read( &bs, 1 );
    }
    test_assert(bs_eof( &bs ), 1);

    /* test writes */
    uint8_t buf[5] = { 0 };
    uint8_t bufok[5] = { 0x7D, 0xF7, 0xDF, 0x7D, 0xF7 };
    bs_write_init( &bs, &buf, 5 );
    bs_write(&bs, 1, 1 );
    test_assert(buf[0], 0x80);
    bs_write(&bs, 2, 0 );
    test_assert(buf[0], 0x80);
    bs_write(&bs, 1, 1 );
    test_assert(buf[0], 0x90);

    bs_write_init( &bs, &buf, 5 );
    for( size_t i=0, j=0; i<5*8; j++ )
    {
        test_assert(bs_aligned( &bs ), !!(i%8 == 0));
        test_assert(bs_remain( &bs ), 5*8 - i);
        test_assert(bs_pos( &bs ), i);
        bs_write(&bs, j % 4, (i % 2) ? 0xFF >> (8 - (j % 4)) : 0 );
        i += j % 4;
    }
    test_assert(bs_eof( &bs ), 1);
    test_assert(!memcmp(buf, bufok, 5), true);

    bs_write_init( &bs, &buf, 5 );
    bs_write( &bs, 1, 0 );
    bs_write_align( &bs, 1 );
    test_assert(bs_aligned( &bs ), true);
    test_assert(bs_pos( &bs ), 8);
    test_assert(buf[0], 0x7F);
    bs_write( &bs, 1, 1 );
    bs_write_align( &bs, 0 );
    test_assert(bs_aligned( &bs ), true);
    test_assert(bs_pos( &bs ), 16);
    test_assert(buf[1], 0x80);

    /* overflows */
    bs_init( &bs, p_testsets[TESTSET1].data, p_testsets[TESTSET1].count );
    bs_read( &bs, 42 );
    test_assert(bs_remain( &bs ), 0);
    test_assert(bs_pos( &bs ), 16);

    bs_init( &bs, p_testsets[TESTSET1].data, p_testsets[TESTSET1].count );
    bs_skip( &bs, 42 );
    test_assert(bs_remain( &bs ), 0);
    test_assert(bs_pos( &bs ), 16);

    bs_init( &bs, p_testsets[TESTSET1].data, p_testsets[TESTSET1].count );
    bs_skip( &bs, 8 );
    test_assert(bs_remain( &bs ), 8);
    test_assert(bs_pos( &bs ), 8);
    test_assert(bs_read( &bs, 8 + 2 ), 0x55 << 2);
    test_assert(bs_remain( &bs ), 0);
    test_assert(bs_pos( &bs ), 16);

    return 0;
}
#undef bs_init

static size_t bs_skipeven_bytes_forward( bs_t *s, size_t i_count )
{
    if( s->p == NULL )
    {
        s->p = s->p_start;
        return 1 + bs_skipeven_bytes_forward( s, i_count - 1 );
    }

    if( s->p_end - s->p > (ssize_t) i_count * 2 )
    {
        s->p += i_count * 2;
        return i_count;
    }
    else
    {
        s->p = s->p_end;
        return 0;
    }
}

static size_t bs_skipeven_bytes_remain( const bs_t *s )
{
    if( s->p )
        return s->p < s->p_end ? (s->p_end - s->p + 1) / 2 - 1: 0;
    else
        return (s->p_end - s->p_start) / 2;
}

static size_t bs_skipeven_bytes_pos( const bs_t *s )
{
    if( s->p )
        return s->p + 1 < s->p_end ? (s->p - s->p_start) / 2 + 1 : (s->p - s->p_start) / 2;
    else
        return 0;
}

bs_byte_callbacks_t skipeven_cb = {
    bs_skipeven_bytes_forward,
    bs_skipeven_bytes_pos,
    bs_skipeven_bytes_remain,
};

static int test_annexb( const char *psz_tag )
{
    const uint8_t annexb[] = { 0xFF, 0x00, 0x00, 0x03, 0x01, 0xFF,
                               0x03, 0x00, 0x00, 0x03, 0x02, 0x00, 0x00, 0x03 };
    const uint8_t unesc[]  = { 0xFF, 0x00, 0x00,       0x01, 0xFF,
                               0x03, 0x00, 0x00,       0x02, 0x00, 0x00, 0x03 };

    bs_t bs;
    struct hxxx_bsfw_ep3b_ctx_s bsctx;
    bs_init( &bs, &annexb, ARRAY_SIZE(annexb) );
    hxxx_bsfw_ep3b_ctx_init( &bsctx );
    bs.cb = hxxx_bsfw_ep3b_callbacks;
    bs.p_priv = &bsctx;
    for( size_t i=0; i<ARRAY_SIZE(unesc)*8; i++ )
    {
        test_assert(bs_aligned( &bs ), !!(i%8 == 0));
        test_assert(bs_remain( &bs ), ARRAY_SIZE(unesc)*8 - i);
        test_assert(bs_pos( &bs ), i);
        bs_read( &bs, 1 );
    }
    test_assert(bs_eof( &bs ), 1);

    bs_init( &bs, &annexb, ARRAY_SIZE(annexb) );
    hxxx_bsfw_ep3b_ctx_init( &bsctx );
    bs.cb = hxxx_bsfw_ep3b_callbacks;
    bs.p_priv = &bsctx;
    for( size_t i=0; i<ARRAY_SIZE(unesc)*4; i++ )
    {
        test_assert(bs_remain( &bs ), ARRAY_SIZE(unesc)*8 - i*2);
        test_assert(bs_pos( &bs ), i*2);
        bs_read( &bs, 2 );
    }
    test_assert(bs_eof( &bs ), 1);

    /* overflows */
    bs_init( &bs, &annexb, ARRAY_SIZE(annexb) );
    bs_skip( &bs, (ARRAY_SIZE(annexb) + 1) * 8 );
    test_assert(bs_remain( &bs ), 0);
    test_assert(bs_pos( &bs ), ARRAY_SIZE(annexb) * 8);

    return 0;
}


int main( void )
{
    test_init();

    struct testset testsets[TESTSET_COUNT] =
    {
        [TESTSET0] =
            { 1, { 0x00 } },
        [TESTSET1] =
            { 2, { 0xAA, 0x55 } },
        [TESTSET_EXPGOLOMB] =
            { 2, { 0x15, 0x23 } },
        [TESTSET2] =
            { 6, { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF } },
    };

    if( run_tests( testsets, "normal", NULL, NULL, NULL ) )
        return 1;

    struct testset skipeventestsets[TESTSET_COUNT] =
    {
        [TESTSET0] =
            { 2, { 0x00, 0x00 } },
        [TESTSET1] =
            { 4, { 0xAA, 0xFF, 0x55, 0xFF } },
        [TESTSET_EXPGOLOMB] =
            { 4, { 0x15, 0xFF, 0x23, 0xFF } },
        [TESTSET2] =
            { 12, { 0xAA, 0xFF, 0xBB, 0xFF, 0xCC, 0xFF, 0xDD, 0xFF, 0xEE, 0xFF, 0xFF, 0xFF } },
    };

    if( run_tests( skipeventestsets, "even byte skip", &skipeven_cb, NULL, NULL ) )
        return 1;

    if( test_annexb( "annexb ") )
        return 1;

    return 0;
}
