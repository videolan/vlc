/*****************************************************************************
 * hashtables.c: Hash tables handling
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>

static uint64_t HashString    ( const char *, int );
static int      FindSlot      ( hashtable_entry_t *, int, uint64_t );
static int      LookupInner   ( hashtable_entry_t *, int, uint64_t );



void vlc_HashInsert( hashtable_entry_t **pp_array, int *pi_count, int i_id,
                     const char *psz_name, void *p_data )
{
    hashtable_entry_t *p_new;
    int64_t i_hash = HashString( psz_name, i_id );
    int i_new = FindSlot( *pp_array, *pi_count, i_hash );

    *pp_array = realloc( *pp_array, ( *pi_count + 2) * sizeof( hashtable_entry_t ) );

    memmove( *pp_array + i_new + 1 , *pp_array + i_new, ( *pi_count - i_new ) *
                sizeof( hashtable_entry_t ) );
    (*pi_count)++;

    p_new = &((*pp_array)[i_new]);
    p_new->i_hash = i_hash;
    p_new->i_id = i_id;
    p_new->psz_name = strdup( psz_name );
    p_new->p_data = p_data;

}

void * vlc_HashRetrieve( hashtable_entry_t *p_array, int i_count, int i_id,
                         const char *psz_name )
{
    int i_new = vlc_HashLookup( p_array, i_count, i_id, psz_name );

    if( i_new >=  0 && i_new < i_count )
    {
        return p_array[i_new].p_data;
    }
    return NULL;
}

/*****************************************************************************
 * FindSlot: find an empty slot to insert a new variable
 *****************************************************************************
 * We use a recursive inner function indexed on the hash. This function does
 * nothing in the rare cases where a collision may occur, see Lookup()
 * to see how we handle them.
 * XXX: does this really need to be written recursively?
 *****************************************************************************/
static int FindSlot( hashtable_entry_t *p_array, int i_count, uint64_t i_hash )
{
    int i_middle;


    if( i_hash <= p_array[0].i_hash )
    {
        return 0;
    }

    if( i_hash >= p_array[i_count - 1].i_hash )
    {
        return i_count;
    }

    i_middle = i_count / 2;

    /* We know that 0 < i_middle */
    if( i_hash < p_array[i_middle].i_hash )
    {
        return FindSlot( p_array, i_middle, i_hash );
    }

    /* We know that i_middle + 1 < i_count */
    if( i_hash > p_array[i_middle + 1].i_hash )
    {
        return i_middle + 1 + FindSlot( p_array + i_middle + 1,
                                        i_count - i_middle - 1,
                                        i_hash );
    }

    return i_middle + 1;
}

/*****************************************************************************
 * Lookup: find an existing variable given its name and id
 *****************************************************************************
 * We use a recursive inner function indexed on the hash. Care is taken of
 * possible hash collisions.
 * XXX: does this really need to be written recursively?
 *****************************************************************************/
int vlc_HashLookup( hashtable_entry_t *p_array, int i_count,
                    int i_id, const char *psz_name )
{
    uint64_t i_hash;
    int i, i_pos;

    if( i_count == 0 )
    {
        return -1;
    }

    i_hash = HashString( psz_name, i_id );

    i_pos = LookupInner( p_array, i_count, i_hash );

    /* Hash not found */
    if( i_hash != p_array[i_pos].i_hash )
    {
        return -1;
    }

    /* Hash found, let's check it looks like the entry
     * We don't check the whole entry, this could lead to bad surprises :( */
    if( psz_name[0] == p_array[i].psz_name[0] )
    {
        return i_pos;
    }

    /* Hash collision! This should be very rare, but we cannot guarantee
     * it will never happen. Just do an exhaustive search amongst all
     * entries with the same hash. */
    for( i = i_pos - 1 ; i > 0 && i_hash == p_array[i].i_hash ; i-- )
    {
        if( !strcmp( psz_name, p_array[i].psz_name ) && p_array[i].i_id == i_id )
        {
            return i;
        }
    }

    for( i = i_pos + 1 ; i < i_count && i_hash == p_array[i].i_hash ; i++ )
    {
        if( !strcmp( psz_name, p_array[i].psz_name ) && p_array[i].i_id == i_id )
        {
            return i;
        }
    }

    /* Hash found, but entry not found */
    return -1;
}

static int LookupInner( hashtable_entry_t *p_array, int i_count, uint64_t i_hash )
{
    int i_middle;

    if( i_hash <= p_array[0].i_hash )
    {
        return 0;
    }

    if( i_hash >= p_array[i_count-1].i_hash )
    {
        return i_count - 1;
    }

    i_middle = i_count / 2;

    /* We know that 0 < i_middle */
    if( i_hash < p_array[i_middle].i_hash )
    {
        return LookupInner( p_array, i_middle, i_hash );
    }

    /* We know that i_middle + 1 < i_count */
    if( i_hash > p_array[i_middle].i_hash )
    {
        return i_middle + LookupInner( p_array + i_middle,
                                       i_count - i_middle,
                                       i_hash );
    }

    return i_middle;
}


/*****************************************************************************
 * HashString: our cool hash function
 *****************************************************************************
 * This function is not intended to be crypto-secure, we only want it to be
 * fast and not suck too much. This one is pretty fast and did 0 collisions
 * in wenglish's dictionary.
 *****************************************************************************/
static uint64_t HashString( const char *psz_string, int i_id )
{
    uint64_t i_hash = 0;

    while( *psz_string )
    {
        i_hash += *psz_string++;
        i_hash += i_hash << 10;
        i_hash ^= i_hash >> 8;
    }

    i_hash += ( (uint64_t)i_id << 32 );

    return i_hash;
}
