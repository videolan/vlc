
/*****************************************************************************
 * dictionary.c: Tests for vlc_dictionary_t
 *****************************************************************************
 * Copyright (C) 2007 Pierre d'Herbemont
 * $Id$
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
# include "config.h"
#endif

#undef NDEBUG
#include <assert.h>

#include <vlc_common.h>
#include "vlc_arrays.h"

#include <stdio.h>
#include <stdlib.h>

static void test_dictionary_validity (vlc_dictionary_t * p_dict, const char ** our_keys, int size )
{
    /* Test values and keys now */
    char ** keys = vlc_dictionary_all_keys( p_dict );
    intptr_t i, j;

    assert( keys );

    for( j = 0; keys[j]; j++ )
    {
        bool found = false;
        for( i = 0; i < size; i++ )
        {
            if(!strcmp( keys[j], our_keys[i] ))
            {
                found = true;
                break;
            }
        }
        free( keys[j] );
        assert( found );
    }
    free( keys );

    for( i = 0; i < size; i++ )
        assert( vlc_dictionary_value_for_key( p_dict, our_keys[i] ) == (void *)i );
}

int main (void)
{
    static const char * our_keys[] = {
        "Hello", "Hella", "flowmeter", "Frostnipped", "frostnipped", "remiform", "quadrifoliolate", "singularity", "unafflicted"
    };
    const int size = sizeof(our_keys)/sizeof(our_keys[0]);
    char ** keys;
    intptr_t i = 0;

    vlc_dictionary_t dict;
    vlc_dictionary_init( &dict, 0 );

    assert( vlc_dictionary_keys_count( &dict ) == 0 );

    keys = vlc_dictionary_all_keys( &dict );
    assert( keys && !keys[0] );
    free(keys);


    /* Insert some values */
    for( i = 0; i < size; i++ )
        vlc_dictionary_insert( &dict, our_keys[i], (void *)i );

    test_dictionary_validity( &dict, our_keys, size );

    vlc_dictionary_remove_value_for_key( &dict, our_keys[size-1], NULL, NULL );

    test_dictionary_validity( &dict, our_keys, size-1 );

    vlc_dictionary_clear( &dict, NULL, NULL );

    assert( vlc_dictionary_keys_count( &dict ) == 0 );
    return 0;
}
