/*****************************************************************************
 * helpers.c:
 *****************************************************************************
 * Copyright © 2018 VideoLabs and VideoLAN Authors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#undef NDEBUG
#include <assert.h>

#include <vlc_common.h>
#include <vlc_block.h>
#include <vlc_block_helper.h>

#include "../modules/packetizer/startcode_helper.h"

struct results_s
{
    size_t offset;
    size_t size;
};

static int check_set( const uint8_t *p_set, const uint8_t *p_end,
                      const struct results_s *p_results, size_t i_results,
                      ssize_t i_results_offset,
                      const uint8_t *(*pf_find)(const uint8_t *, const uint8_t *))
{
    const uint8_t *p = p_set;
    size_t i_entry = 0;
    while( p != NULL )
    {
        p = pf_find( p, p_end );
        if( p == NULL )
            break;
        printf("- entry %zu offset %ld\n", i_entry, p - p_set);
        if( i_entry == i_results )
            break;
        if( p_results[i_entry].offset + i_results_offset != (size_t) (p - p_set) )
            return 1;
        i_entry++;
        p++;
    }

    if( p != NULL || i_entry != i_results )
        return 1;

    return 0;
}

static int run_annexb_sets( const uint8_t *p_set, const uint8_t *p_end,
                            const struct results_s *p_results, size_t i_results,
                            ssize_t i_results_offset )
{
    int i_ret;

    printf("checking bits code:\n");
    i_ret = check_set( p_set, p_end, p_results, i_results, i_results_offset,
                       startcode_FindAnnexB_Bits );
    if( i_ret != 0 )
        return i_ret;

    /* Perform same tests on simd optimized code */
    if( startcode_FindAnnexB_Bits != startcode_FindAnnexB )
    {
        printf("checking asm:\n");
        i_ret = check_set( p_set, p_end, p_results, i_results, i_results_offset,
                           startcode_FindAnnexB );
        if( i_ret != 0 )
            return i_ret;
    }
    else printf("asm not built in, skipping test:\n");

    return 0;
}

int main( void )
{
    const uint8_t test1_annexbdata[] = { 0, 0, 0, 1, 0x55, 0x55, 0x55, 0x55, 0x55, // 9
                                         0, 0, 1, 0x22, 0x22, //14
                                         0, 0, 1, 0x0, 0x0, //19
                                         0, 0, 1, //22
                                         0, 0, 1, 0, //26
                                         0, 0, 1, 0x11, //30
                                         0, 0, 1,
                                       };
    const struct results_s test1_results[] = {
                                        { 1,  3 + 5 },
                                        { 9,  3 + 2 },
                                        { 14, 3 + 2 },
                                        { 19, 3 + 0 },
                                        { 22, 3 + 1 },
                                        { 26, 3 + 1 },
                                        { 30, 3 + 0 },
                                       };

    printf("* Running tests on set 1:\n");
    int i_ret = run_annexb_sets( test1_annexbdata,
                                 test1_annexbdata + sizeof(test1_annexbdata),
                                 test1_results, ARRAY_SIZE(test1_results), 0 );
    if( i_ret != 0 )
        return i_ret;

    uint8_t *p_data = malloc( 4096 );
    if( p_data )
    {
        const ssize_t i_dataoffset = 4096 - sizeof(test1_annexbdata) - 111;
        memset( p_data, 0x42, 4096 );
        memcpy( &p_data[i_dataoffset],
                test1_annexbdata, sizeof(test1_annexbdata) );
        printf("* Running tests on extended set 1:\n");
        i_ret = run_annexb_sets( p_data,
                                 p_data + 4096,
                                 test1_results, ARRAY_SIZE(test1_results),
                                 i_dataoffset );
        free( p_data );
        if( i_ret != 0 )
            return i_ret;
    }

    return 0;
}
