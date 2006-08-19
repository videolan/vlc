/*****************************************************************************
 * algo.c : Algorithms test
 *****************************************************************************
 * Copyright (C) 2006 VideoLAN
 * $Id: i18n.c 16157 2006-07-29 13:32:12Z zorglub $
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
#include <vlc/vlc.h>

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
static void DumpDict( dict_t *p_dict )
{
    int i = 0;
    fprintf( stderr, "**** Begin Dump ****\n" );
    for( i = 0 ; i < p_dict->i_entries; i++ )
    {
        fprintf( stderr, "Entry %i - hash %lli int %i string %s data %p\n", 
                        i, p_dict->p_entries[i].i_hash,
                        p_dict->p_entries[i].i_int,
                        p_dict->p_entries[i].psz_string,
                        p_dict->p_entries[i].p_data );
    }
    fprintf( stderr, "**** End Dump ****\n" );
}

PyObject *dict_test( PyObject *self, PyObject *args )
{
    int i42 = 42,i40 = 40,i12 = 12, i0 = 0, i00 = 0;

    dict_t *p_dict = vlc_DictNew();
    ASSERT( p_dict->i_entries == 0, "" );
    ASSERT( p_dict->p_entries == NULL, "" );

    vlc_DictInsert( p_dict, 0, NULL, (void*)(&i42) );
    ASSERT( p_dict->i_entries == 1, "" );
    ASSERT( p_dict->p_entries[0].p_data == (void*)(&i42), "" );

    vlc_DictInsert( p_dict, 1, "42", (void*)(&i42) );
    ASSERT( p_dict->i_entries == 2, "" );
    ASSERT( vlc_DictGet( p_dict, 1, "42" ) == (void*)(&i42), "" );
    ASSERT( vlc_DictGet( p_dict, 0, "42" ) == NULL , "");
    ASSERT( vlc_DictGet( p_dict, 1, " 42" ) == NULL , "");

    vlc_DictInsert( p_dict, 1, "12", (void*)(&i12) );
    ASSERT( vlc_DictGet( p_dict, 1, "12") == (void*)(&i12), "" );

    vlc_DictInsert( p_dict, 3, "40", (void*)(&i40) );
    ASSERT( vlc_DictGet( p_dict, 3, "40") == (void*)(&i40), "" );
    ASSERT( vlc_DictGet( p_dict, 1, "12") == (void*)(&i12), "" );
    ASSERT( vlc_DictGet( p_dict, 1, "42") == (void*)(&i42), "" );

    vlc_DictInsert( p_dict, 12, "zero-1", (void*)(&i0) );
    vlc_DictInsert( p_dict, 5, "zero-0", (void*)(&i00) );
    ASSERT( vlc_DictGet( p_dict, 12, "zero-1") == (void*)(&i0), "" );
    ASSERT( vlc_DictGet( p_dict, 5, "zero-0") == (void*)(&i00), "" );
    ASSERT( vlc_DictGet( p_dict, 12, "zero-0") == NULL, "" );

    vlc_DictInsert( p_dict, 0, "12", (void*)(&i12) );
    vlc_DictInsert( p_dict, 0, "thisisaverylongstringwith12", (void*)(&i12) );
    ASSERT( vlc_DictGet( p_dict, 0, "thisisaverylongstringwith12" ) ==
                    (void*)(&i12),"" );
    ASSERT( vlc_DictGet( p_dict, 0, "thisisaverylongstringwith13" ) == NULL,"");

    vlc_DictClear( p_dict );

    Py_INCREF( Py_None);
    return Py_None;
}
