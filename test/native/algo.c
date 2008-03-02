/*****************************************************************************
 * algo.c : Algorithms test
 *****************************************************************************
 * Copyright (C) 2006 VideoLAN
 * $Id$
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

#include "../pyunit.h"
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>

/**********************************************************************
 * Arrays
 *********************************************************************/

TYPEDEF_ARRAY(long,long_array_t);

PyObject *arrays_test( PyObject *self, PyObject *args )
{
    mtime_t one, two;
    int number = 1000000;
    int number2 = 50000; /* For slow with memmove */
    printf("\n");
    {
        int i_items = 0;
        int *p_items = NULL;
        int i;
        one = mdate();
        for( i = 0 ; i<number;i++) {
            INSERT_ELEM(p_items,i_items, i_items, i+50);
        }
        two = mdate();
        printf( " Std array %i items appended in "I64Fi" µs\n", number,
                (two-one) );
        for( i = number-1 ; i>=0; i--) {
            REMOVE_ELEM( p_items, i_items, i );
        }
        one = mdate();
        printf( " Std array %i items removed in  "I64Fi" µs\n", number,
                (one-two) );

        for( i = 0 ; i<number2;i++) {
            int pos = i_items == 0  ? 0 : rand() % i_items;
            INSERT_ELEM(p_items, i_items, pos, pos + 50);
        }
        two = mdate();
        printf( " Std array %i items inserted in  "I64Fi" µs\n", number2,
                (two-one) );
    }
    {
        DECL_ARRAY(int) int_array;
        int i = 0;
        ARRAY_INIT(int_array);
        ASSERT(int_array.i_size == 0, "" );
        ASSERT(int_array.i_alloc == 0, "" );
        ASSERT(int_array.p_elems == 0, "" );

        ARRAY_APPEND(int_array, 42 );
        ASSERT(int_array.i_size == 1, "" );
        ASSERT(int_array.i_alloc > 1, "" );
        ASSERT(int_array.p_elems[0] == 42, "" );
        ARRAY_REMOVE(int_array,0);
        ASSERT(int_array.i_size == 0, "" );

        one = mdate();
        for( i = 0 ; i<number;i++) {
            ARRAY_APPEND(int_array, i+50);
        }
        two = mdate();
        printf( " New array %i items appended in "I64Fi" µs\n", number,
                (two-one) );
        ASSERT(int_array.p_elems[1242] == 1292 , "");
        for( i = number-1 ; i>=0; i--) {
            ARRAY_REMOVE(int_array,i);
        }
        one = mdate();
        printf( " New array %i items removed in  "I64Fi" µs\n", number,
                (one-two) );

        /* Now random inserts */
        for( i = 0 ; i<number2;i++) {
            int pos = int_array.i_size == 0  ? 0 : rand() % int_array.i_size;
            ARRAY_INSERT(int_array, pos+50, pos);
        }
        two = mdate();
        printf( " New array %i items inserted in  "I64Fi" µs\n", number2,
                (two-one) );
    }
    {
        long_array_t larray;
        ARRAY_INIT(larray);
    }
    Py_INCREF( Py_None);
    return Py_None;
}

/**********************************************************************
 * Binary search
 *********************************************************************/

PyObject *bsearch_direct_test( PyObject *self, PyObject *args )
{
#define DIRCHECK( size, initial, checked, expected ) { \
    int array[size] = initial; \
    int answer = -1;  \
    BSEARCH( array, size, , int, checked, answer ); \
    ASSERT( answer == expected , "" ); }

#define ORDERED10 {0,1,2,3,4,5,6,7,8,9}
    DIRCHECK( 10, ORDERED10, 0, 0 );
    DIRCHECK( 10, ORDERED10, 1, 1 );
    DIRCHECK( 10, ORDERED10, 2, 2 );
    DIRCHECK( 10, ORDERED10, 3, 3 );
    DIRCHECK( 10, ORDERED10, 4, 4 );
    DIRCHECK( 10, ORDERED10, 5, 5 );
    DIRCHECK( 10, ORDERED10, 6, 6 );
    DIRCHECK( 10, ORDERED10, 7, 7 );
    DIRCHECK( 10, ORDERED10, 8, 8 );
    DIRCHECK( 10, ORDERED10, 9,9 );

    DIRCHECK( 10, ORDERED10, 10, -1 );
    DIRCHECK( 10, ORDERED10, -1, -1 );

    /* TODO: tests on unordered arrays, odd number of elements, 1 element, 2 */

    Py_INCREF( Py_None);
    return Py_None;
}

struct bsearch_tester
{
    int key; int value;
};

/* Lighter test, we just check correct member access, all the real testing
 * has been made already */
PyObject *bsearch_member_test( PyObject *self, PyObject *args )
{
    struct bsearch_tester array[] =
    {
        { 0, 12 }, { 1, 22 } , { 2, 33 } , { 3, 68 } , { 4, 56 }
    };
#define MEMBCHECK( checked, expected ) { \
    int answer = -1;  \
    BSEARCH( array, 5, .key , int, checked, answer ); \
    ASSERT( answer == expected , "" ); }

    MEMBCHECK( 0, 0 ) ;
    MEMBCHECK( 1, 1 );
    MEMBCHECK( 2, 2 );
    MEMBCHECK( 3, 3 );
    MEMBCHECK( 4, 4 );
    MEMBCHECK( 5, -1 );

    Py_INCREF( Py_None);
    return Py_None;
}

/**********************************************************************
 * Dictionnary
 *********************************************************************/
DICT_TYPE( test, int );

static void DumpDict( dict_test_t *p_dict )
{
    int i = 0;
    fprintf( stderr, "**** Begin Dump ****\n" );
    for( i = 0 ; i < p_dict->i_entries; i++ )
    {
        fprintf( stderr, "Entry %i - hash %lli int %i string %s data %i\n",
                        i, p_dict->p_entries[i].i_hash,
                        p_dict->p_entries[i].i_int,
                        p_dict->p_entries[i].psz_string,
                        p_dict->p_entries[i].data );
    }
    fprintf( stderr, "**** End Dump ****\n" );
}

PyObject *dict_test( PyObject *self, PyObject *args )
{
    int i42 = 42,i40 = 40,i12 = 12, i0 = 0, i00 = 0;
    int answer;

    printf("\n");

    dict_test_t *p_dict;
    DICT_NEW( p_dict );
    ASSERT( p_dict->i_entries == 0, "" );
    ASSERT( p_dict->p_entries == NULL, "" );

    DICT_INSERT( p_dict, 0, NULL, i42 );
    ASSERT( p_dict->i_entries == 1, "" );
    ASSERT( p_dict->p_entries[0].data == i42, "" );

    DICT_INSERT( p_dict, 1, "42", i42 );
    ASSERT( p_dict->i_entries == 2, "" );

    DICT_LOOKUP( p_dict, 1, "42", answer );
    DICT_GET( p_dict, 1, "42", answer );
    ASSERT( answer == i42, "" );
    DICT_LOOKUP( p_dict, 0, "42", answer ); ASSERT( answer == -1, "" );
    DICT_LOOKUP( p_dict, 1, " 42", answer ); ASSERT( answer == -1, "" );

    DICT_INSERT( p_dict, 1, "12", i12 );
    DICT_GET( p_dict, 1, "12", answer ) ; ASSERT( answer == i12, "" );

    DICT_INSERT( p_dict, 3, "40", i40 );
    DICT_GET( p_dict, 1, "42", answer ); ASSERT( answer == i42, "" );
    DICT_GET( p_dict, 3, "40", answer ); ASSERT( answer == i40, "" );
    DICT_GET( p_dict, 1, "12", answer ); ASSERT( answer == i12, "" );

    DICT_INSERT( p_dict, 12, "zero-1", i0 );
    DICT_INSERT( p_dict, 5, "zero-0", i00 );
    DICT_GET( p_dict, 12, "zero-1", answer ); ASSERT( answer == i0, "" );
    DICT_GET( p_dict, 5, "zero-0", answer ); ASSERT( answer == i00, "" );
    answer = -1;
    DICT_GET( p_dict, 12, "zero-0", answer ); ASSERT( answer == -1, "" );
    DICT_GET( p_dict, 1, "12", answer ); ASSERT( answer == i12, "" );

    DICT_INSERT( p_dict, 0, "zero", 17 );
    DICT_GET( p_dict, 1, "12", answer ); ASSERT( answer == i12, "" );
    DICT_GET( p_dict, 12, "zero-1", answer ); ASSERT( answer == i0, "" );
    DICT_GET( p_dict, 0, "zero", answer ); ASSERT( answer == 17, "" );

    DICT_INSERT( p_dict, 0, "12", i12 );
    DICT_INSERT( p_dict, 0, "thisisaverylongstringwith12", i12 );
    answer = -1;
    DICT_GET( p_dict, 0, "thisisaverylongstringwith12", answer );
    ASSERT( answer == i12, "" );
    answer = -1;
    DICT_GET( p_dict, 0, "thisisaverylongstringwith13", answer );
    ASSERT( answer == -1, "" );

    DICT_CLEAR( p_dict );

    Py_INCREF( Py_None);
    return Py_None;
}
