/*****************************************************************************
 * dict.c: Dictionnary handling
 *****************************************************************************
 * Copyright (C) 2002-2004 the VideoLAN team
 * $Id: variables.c 13991 2006-01-22 17:12:24Z zorglub $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Clément Stenac <zorglub@videolan.org>
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

#include <vlc/vlc.h>
#include <assert.h>

/*****************************************************************************
 * Associative array
 *****************************************************************************
 * This is quite a weak implementation of an associative array.
 * It hashes the key and stores the entry in a sorted array that gets 
 * reallocated to insert new entries (so, this is SLOW)
 * Lookup is done by using binary search on the array
 */

static uint64_t DoHash    ( const char *, int );

#define DARRAY p_dict->p_entries
#define DSIZE p_dict->i_entries

dict_t *vlc_DictNew()
{
    DECMALLOC_NULL( p_dict, dict_t );
    p_dict->i_entries = 0;
    p_dict->p_entries = NULL;
    return p_dict;
}

void vlc_DictClear( dict_t *p_dict )
{
    int i = 0;
    for( i = 0 ; i< DSIZE; i++ )
    {
        FREE( DARRAY[i].psz_string );
    }
    free( DARRAY );
    free( p_dict );
}

void vlc_DictInsert( dict_t *p_dict, int i_int, const char *psz_string, 
                     void *p_data )
{
    uint64_t i_hash = DoHash( psz_string, i_int );
    int i_new;

    /* Find a free slot */
    if( DSIZE == 0 || i_hash <= DARRAY[0].i_hash )
        i_new = 0;
    else if( i_hash >= DARRAY[DSIZE-1].i_hash )
        i_new = DSIZE;
    else 
    {
        int i_low = 0, i_high = DSIZE;
        while( i_low != i_high )
        {
            int i_mid = (i_low + i_high)/2;
            if( DARRAY[i_mid].i_hash < i_hash )
                i_low = i_mid + 1;
            if( DARRAY[i_mid].i_hash > i_hash )
                i_high = i_low + 1;
        }
        i_new = i_low;
    }
    DARRAY = realloc( DARRAY, (DSIZE + 1) * sizeof( dict_entry_t ) );
    DSIZE++;

    if( i_new != DSIZE -1 )
        memmove( DARRAY + i_new + 1 , DARRAY + i_new, ( DSIZE - i_new - 1 ) *
                    sizeof( dict_entry_t ) );
    DARRAY[i_new].i_hash = i_hash;
    DARRAY[i_new].i_int = i_int;
    if( psz_string )
        DARRAY[i_new].psz_string = strdup( psz_string );
    else
        DARRAY[i_new].psz_string = NULL;
    DARRAY[i_new].p_data = p_data;
}

void * vlc_DictGet( dict_t *p_dict, int i_int, const char *psz_string )
{
    int i_new = vlc_DictLookup( p_dict, i_int, psz_string );
    if( i_new >=  0 )
        return DARRAY[i_new].p_data;
    return NULL;
}

int vlc_DictLookup( dict_t *p_dict, int i_int, const char *psz_string )
{
    uint64_t i_hash;
    int i, i_pos;
    if( DSIZE == 0 ) return -1;

    i_hash = DoHash( psz_string, i_int );
    BSEARCH( p_dict->p_entries, p_dict->i_entries, .i_hash, uint64_t,
             i_hash, i_pos );
    if( i_pos == -1 ) return -1;

    /* Hash found, let's check it looks like the entry */
    if( !strcmp( psz_string, DARRAY[i_pos].psz_string ) )
        return i_pos;

    /* Hash collision! This should be very rare, but we cannot guarantee
     * it will never happen. Just do an exhaustive search amongst all
     * entries with the same hash. */
    for( i = i_pos - 1 ; i > 0 && i_hash == DARRAY[i].i_hash ; i-- )
    {
        if( !strcmp( psz_string, DARRAY[i].psz_string ) &&
                   DARRAY[i].i_int == i_int ) 
            return i;
    }
    for( i = i_pos + 1 ; i < DSIZE && i_hash == DARRAY[i].i_hash ; i++ )
    {
         if( !strcmp( psz_string, DARRAY[i].psz_string ) &&
                   DARRAY[i].i_int == i_int ) 
            return i;
    }
    /* Hash found, but entry not found (quite strange !) */
    assert( 0 );
    return -1;
}

/*****************************************************************************
 * DoHash: our cool hash function
 *****************************************************************************
 * This function is not intended to be crypto-secure, we only want it to be
 * fast and not suck too much. This one is pretty fast and did 0 collisions
 * in wenglish's dictionary.
 *****************************************************************************/
static uint64_t DoHash( const char *psz_string, int i_int )
{
    uint64_t i_hash = 0;
    if( psz_string )
    {
        while( *psz_string )
        {
            i_hash += *psz_string++;
            i_hash += i_hash << 10;
            i_hash ^= i_hash >> 8;
        }
    }
    return i_hash + ( (uint64_t)i_int << 32 );
}
