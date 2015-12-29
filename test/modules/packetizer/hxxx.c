/*****************************************************************************
 * hxxx.c tests NAL conversions
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
#ifdef NDEBUG
 #undef NDEBUG
#endif
#include <assert.h>
#include <vlc_common.h>
#include <vlc_block.h>
#include "../modules/packetizer/hxxx_nal.h"
#include "../modules/packetizer/hxxx_nal.c"

static void testannexbin( const uint8_t *p_data, size_t i_data,
                          const uint8_t **pp_res, size_t *pi_res )
{
    printf("INPUT SET    : ");
    for(size_t j=0; j<i_data; j++)
        printf("0x%.2x, ", p_data[j] );
    printf("\n");

    for( unsigned int i=0; i<3; i++)
    {
        block_t *p_block = block_Alloc( i_data );
        memcpy( p_block->p_buffer, p_data, i_data );

        p_block = hxxx_AnnexB_to_xVC( p_block, 1 << i );
        printf("DUMP prefix %d: ", 1 << i);
        if( p_block )
        {
            for(size_t j=0; j<p_block->i_buffer; j++)
                printf("0x%.2x, ", p_block->p_buffer[j] );
            printf("\n");

            printf("COMPARE SET    : ");
            for(size_t j=0; j<pi_res[i]; j++)
                printf("0x%.2x, ", pp_res[i][j] );
            printf("\n");

            assert( p_block->i_buffer == pi_res[i] );
            assert( memcmp( p_block->p_buffer, pp_res[i], pi_res[i] ) == 0 );
            block_Release( p_block );
        }
        else
        {
            printf("** No output **\n");
            assert(0);
        }
    }
}
#define runtest(number, name) \
    printf("\nTEST %d %s\n", number, name);\
    p_res[0] = test##number##_avcdata1;  rgi_res[0] = sizeof(test##number##_avcdata1);\
    p_res[1] = test##number##_avcdata2;  rgi_res[1] = sizeof(test##number##_avcdata2);\
    p_res[2] = test##number##_avcdata4;  rgi_res[2] = sizeof(test##number##_avcdata4);\
    testannexbin( test##number##_annexbdata, sizeof(test##number##_annexbdata), \
                  p_res, rgi_res )

static void test_annexb()
{
    const uint8_t *p_res[3];
    size_t rgi_res[3];

    /* Full mixed set */
    const uint8_t test1_annexbdata[] = { 0, 0, 0, 1, 0x55, 0x55, 0x55, 0x55, 0x55,
                                         0, 0, 1, 0x22, 0x22,
                                         0, 0, 0, 1, 0x11,
                                         0, 0, 1, 0x11,
                                         0, 0, 0, 1, 0x33, 0x33, 0x33, };
    const uint8_t test1_avcdata1[] = { 5, 0x55, 0x55, 0x55, 0x55, 0x55,
                                       2, 0x22, 0x22,
                                       1, 0x11,
                                       1, 0x11,
                                       3, 0x33, 0x33, 0x33, };
    const uint8_t test1_avcdata2[] = { 0, 5, 0x55, 0x55, 0x55, 0x55, 0x55,
                                       0, 2, 0x22, 0x22,
                                       0, 1, 0x11,
                                       0, 1, 0x11,
                                       0, 3, 0x33, 0x33, 0x33, };
    const uint8_t test1_avcdata4[] = { 0, 0, 0, 5, 0x55, 0x55, 0x55, 0x55, 0x55,
                                       0, 0, 0, 2, 0x22, 0x22,
                                       0, 0, 0, 1, 0x11,
                                       0, 0, 0, 1, 0x11,
                                       0, 0, 0, 3, 0x33, 0x33, 0x33, };

    /* single nal test */
    const uint8_t test2_annexbdata[] = { 0, 0, 0, 1, 0x55, 0x55, 0x55, 0x55, 0x55 };
    const uint8_t test2_avcdata1[]   = { 5, 0x55, 0x55, 0x55, 0x55, 0x55 };
    const uint8_t test2_avcdata2[]   = { 0, 5, 0x55, 0x55, 0x55, 0x55, 0x55 };
    const uint8_t test2_avcdata4[]   = { 0, 0, 0, 5, 0x55, 0x55, 0x55, 0x55, 0x55 };

    /* single nal test, startcode 3 */
    const uint8_t test3_annexbdata[] = { 0, 0, 1, 0x11 };
    const uint8_t test3_avcdata1[] =   { 1, 0x11 };
    const uint8_t test3_avcdata2[] =   { 0, 1, 0x11 };
    const uint8_t test3_avcdata4[] =   { 0, 0, 0, 1, 0x11 };

    /* empty nal test */
    const uint8_t test4_annexbdata[] = { 0, 0, 1 };
    const uint8_t test4_avcdata1[]   = { 0 };
    const uint8_t test4_avcdata2[]   = { 0, 0 };
    const uint8_t test4_avcdata4[]   = { 0, 0, 0, 0 };

    /* 4 bytes prefixed nal only (4 prefix optz) */
    const uint8_t test5_annexbdata[] = { 0, 0, 0, 1, 0x11, 0, 0, 0, 1, 0x22, 0x22 };
    const uint8_t test5_avcdata1[]   = { 1, 0x11, 2, 0x22, 0x22 };
    const uint8_t test5_avcdata2[]   = { 0, 1, 0x11, 0, 2, 0x22, 0x22 };
    const uint8_t test5_avcdata4[]   = { 0, 0, 0, 1, 0x11, 0, 0, 0, 2, 0x22, 0x22 };

    /* startcode repeat / empty nal */
    const uint8_t test6_annexbdata[] = { 0, 0, 1, 0x11, 0, 0, 1, 0, 0, 1 };
    const uint8_t test6_avcdata1[] =   { 1, 0x11, 0, 0 };
    const uint8_t test6_avcdata2[] =   { 0, 1, 0x11, 0, 0, 0, 0 };
    const uint8_t test6_avcdata4[] =   { 0, 0, 0, 1, 0x11, 0, 0, 0, 0, 0, 0, 0, 0 };

    runtest(4, "empty nal test");
    runtest(2, "single nal test");
    runtest(3, "single nal test, startcode 3");
    runtest(5, "4 bytes prefixed nal only (4 prefix optz)");
    runtest(1, "mixed nal set");
    runtest(6, "startcode repeat / empty nal");
}

int main( void )
{
    test_init();

    test_annexb();

    return 0;
}
