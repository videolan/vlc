/*****************************************************************************
 * vlc_arrays.h : Arrays and data structures handling
 *****************************************************************************
 * Copyright (C) 1999-2004 the VideoLAN team
 * $Id: vlc_playlist.h 17108 2006-10-15 15:28:34Z zorglub $
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA. *****************************************************************************/

#if !defined( __LIBVLC__ )
  #error You are not libvlc or one of its plugins. You cannot include this file
#endif

#ifndef _VLC_ARRAYS_H_
#define _VLC_ARRAYS_H_

#include <assert.h>

/**
 * Simple dynamic array handling. Array is realloced at each insert/removal
 */
#if defined( _MSC_VER ) && _MSC_VER < 1300 && !defined( UNDER_CE )
#   define VLCCVP (void**) /* Work-around for broken compiler */
#else
#   define VLCCVP
#endif
#define INSERT_ELEM( p_ar, i_oldsize, i_pos, elem )                           \
    do                                                                        \
    {                                                                         \
        if( !i_oldsize ) (p_ar) = NULL;                                       \
        (p_ar) = VLCCVP realloc( p_ar, ((i_oldsize) + 1) * sizeof(*(p_ar)) ); \
        if( (i_oldsize) - (i_pos) )                                           \
        {                                                                     \
            memmove( (p_ar) + (i_pos) + 1, (p_ar) + (i_pos),                  \
                     ((i_oldsize) - (i_pos)) * sizeof( *(p_ar) ) );           \
        }                                                                     \
        (p_ar)[i_pos] = elem;                                                 \
        (i_oldsize)++;                                                        \
    }                                                                         \
    while( 0 )

#define REMOVE_ELEM( p_ar, i_oldsize, i_pos )                                 \
    do                                                                        \
    {                                                                         \
        if( (i_oldsize) - (i_pos) - 1 )                                       \
        {                                                                     \
            memmove( (p_ar) + (i_pos),                                        \
                     (p_ar) + (i_pos) + 1,                                    \
                     ((i_oldsize) - (i_pos) - 1) * sizeof( *(p_ar) ) );       \
        }                                                                     \
        if( i_oldsize > 1 )                                                   \
        {                                                                     \
            (p_ar) = realloc( p_ar, ((i_oldsize) - 1) * sizeof( *(p_ar) ) );  \
        }                                                                     \
        else                                                                  \
        {                                                                     \
            free( p_ar );                                                     \
            (p_ar) = NULL;                                                    \
        }                                                                     \
        (i_oldsize)--;                                                        \
    }                                                                         \
    while( 0 )

#define TAB_INIT( count, tab )                  \
  do {                                          \
    (count) = 0;                                \
    (tab) = NULL;                               \
  } while(0)

#define TAB_CLEAN( count, tab )                 \
  do {                                          \
    if( tab ) free( tab );                      \
    (count)= 0;                                 \
    (tab)= NULL;                                \
  } while(0)

#define TAB_APPEND_CAST( cast, count, tab, p )             \
  do {                                          \
    if( (count) > 0 )                           \
        (tab) = cast realloc( tab, sizeof( void ** ) * ( (count) + 1 ) ); \
    else                                        \
        (tab) = cast malloc( sizeof( void ** ) );    \
    (tab)[count] = (p);                         \
    (count)++;                                  \
  } while(0)

#define TAB_APPEND( count, tab, p )             \
    TAB_APPEND_CAST( , count, tab, p )
#define TAB_APPEND_CPP( type, count, tab, p )   \
    TAB_APPEND_CAST( (type**), count, tab, p )

#define TAB_FIND( count, tab, p, index )        \
  do {                                          \
        int _i_;                                \
        (index) = -1;                           \
        for( _i_ = 0; _i_ < (count); _i_++ )    \
        {                                       \
            if( (tab)[_i_] == (p) )             \
            {                                   \
                (index) = _i_;                  \
                break;                          \
            }                                   \
        }                                       \
  } while(0)

#define TAB_REMOVE( count, tab, p )             \
  do {                                          \
        int _i_index_;                          \
        TAB_FIND( count, tab, p, _i_index_ );   \
        if( _i_index_ >= 0 )                    \
        {                                       \
            if( (count) > 1 )                   \
            {                                   \
                memmove( ((void**)(tab) + _i_index_),    \
                         ((void**)(tab) + _i_index_+1),  \
                         ( (count) - _i_index_ - 1 ) * sizeof( void* ) );\
            }                                   \
            (count)--;                          \
            if( (count) == 0 )                  \
            {                                   \
                free( tab );                    \
                (tab) = NULL;                   \
            }                                   \
        }                                       \
  } while(0)

#define TAB_INSERT_CAST( cast, count, tab, p, index ) do { \
    if( (count) > 0 )                           \
        (tab) = cast realloc( tab, sizeof( void ** ) * ( (count) + 1 ) ); \
    else                                        \
        (tab) = cast malloc( sizeof( void ** ) );       \
    if( (count) - (index) > 0 )                 \
        memmove( (void**)(tab) + (index) + 1,   \
                 (void**)(tab) + (index),       \
                 ((count) - (index)) * sizeof(*(tab)) );\
    (tab)[(index)] = (p);                       \
    (count)++;                                  \
} while(0)

#define TAB_INSERT( count, tab, p, index )      \
    TAB_INSERT_CAST( , count, tab, p, index )

/**
 * Binary search in a sorted array. The key must be comparable by < and >
 * \param entries array of entries
 * \param count number of entries
 * \param elem key to check within an entry (like .id, or ->i_id)
 * \param zetype type of the key
 * \param key value of the key
 * \param answer index of answer within the array. -1 if not found
 */
#define BSEARCH( entries, count, elem, zetype, key, answer ) \
   do {  \
    int low = 0, high = count - 1;   \
    answer = -1; \
    while( low <= high ) {\
        int mid = (low + high ) / 2; /* Just don't care about 2^30 tables */ \
        zetype mid_val = entries[mid] elem;\
        if( mid_val < key ) \
            low = mid + 1; \
        else if ( mid_val > key ) \
            high = mid -1;  \
        else    \
        {   \
            answer = mid;  break;   \
        }\
    } \
 } while(0)

/************************************************************************
 * Dictionaries
 ************************************************************************/

#ifdef __cplus_plus__
extern "C"
{
#endif
/* This function is not intended to be crypto-secure, we only want it to be
 * fast and not suck too much. This one is pretty fast and did 0 collisions
 * in wenglish's dictionary.
 */
static inline uint64_t DictHash( const char *psz_string )
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
    return i_hash;
}

typedef struct vlc_dictionary_t
{
    struct vlc_dictionary_entries_t
    {
        char *   psz_key;
        uint64_t i_hash;
        void *   p_value;
    } * p_entries;
    int i_entries;
} vlc_dictionary_t;

static void * const kVLCDictionaryNotFound = (void *)-1;

static inline void vlc_dictionary_init( vlc_dictionary_t * p_dict )
{
    p_dict->i_entries = 0;
    p_dict->p_entries = NULL;
}

static inline void vlc_dictionary_clear( vlc_dictionary_t * p_dict )
{
    int i;
    for ( i = 0; i < p_dict->i_entries; i++ )
    {
        free( p_dict->p_entries[i].psz_key );
    }
    free( p_dict->p_entries );
    p_dict->i_entries = 0;
    p_dict->p_entries = NULL;
}


static inline void *
vlc_dictionary_value_for_key( vlc_dictionary_t * p_dict, const char * psz_key )
{
    uint64_t i_hash;
    int i, i_pos;
    
    if( p_dict->i_entries == 0 )
        return kVLCDictionaryNotFound;

    i_hash = DictHash( psz_key );
    BSEARCH( p_dict->p_entries, p_dict->i_entries, .i_hash, uint64_t,
             i_hash, i_pos );
    if( i_pos == -1 )
        return kVLCDictionaryNotFound;

    /* Hash found, let's check it looks like the entry */
    if( !strcmp( psz_key, p_dict->p_entries[i_pos].psz_key ) )
        return p_dict->p_entries[i_pos].p_value;

    /* Hash collision! This should be very rare, but we cannot guarantee
     * it will never happen. Just do an exhaustive search amongst all
     * entries with the same hash. */
    for( i = i_pos - 1 ; i > 0 && i_hash == p_dict->p_entries[i].i_hash ; i-- )
    {
        if( !strcmp( psz_key, p_dict->p_entries[i].psz_key ) )
            return p_dict->p_entries[i_pos].p_value;
    }
    for( i = i_pos + 1 ; i < p_dict->i_entries &&
                         i_hash == p_dict->p_entries[i].i_hash ; i++ )
    {
         if( !strcmp( psz_key, p_dict->p_entries[i].psz_key ))
            return p_dict->p_entries[i_pos].p_value;
    }
    /* Hash found, but entry not found (shouldn't happen!) */
    return kVLCDictionaryNotFound;
}

static inline void
vlc_dictionary_insert( vlc_dictionary_t * p_dict, const char * psz_key, void * p_value )
{
    uint64_t i_hash = DictHash( psz_key );
    int i_new;

    /* First, caller should take care not to insert twice the same key */
    /* This could be removed for optimization */
    assert( vlc_dictionary_value_for_key( p_dict, psz_key ) == kVLCDictionaryNotFound );

    /* Find a free slot */
    if( p_dict->i_entries == 0 || i_hash <= p_dict->p_entries[0].i_hash )
        i_new = 0;
    else if( i_hash >= p_dict->p_entries[p_dict->i_entries-1].i_hash )
        i_new = p_dict->i_entries;
    else
    {
        int i_low = 0, i_high = p_dict->i_entries - 1;
        while( i_high - i_low > 1 )
        {
            int i_mid = (i_low + i_high)/2;
            if( p_dict->p_entries[i_mid].i_hash < i_hash ) {
                i_low = i_mid;
            } else if( p_dict->p_entries[i_mid].i_hash > i_hash ) {
                i_high = i_mid;
            }
        }
        if( p_dict->p_entries[i_low].i_hash < i_hash )
            i_new = i_high;
        else
            i_new = i_low;
    }
    p_dict->p_entries = realloc( p_dict->p_entries, (p_dict->i_entries + 1) *
        sizeof(struct vlc_dictionary_entries_t) );
    p_dict->i_entries++;
    if( i_new != p_dict->i_entries -1 )
    {
        memmove( &p_dict->p_entries[i_new+1], &p_dict->p_entries[i_new],
         ( p_dict->i_entries - i_new - 1 ) *
         sizeof(struct vlc_dictionary_entries_t) );
    }

    p_dict->p_entries[i_new].i_hash  = i_hash;
    p_dict->p_entries[i_new].psz_key = strdup( psz_key );
    p_dict->p_entries[i_new].p_value = p_value;
}
#ifdef __cplus_plus__
} /* extern "C" */
#endif
/************************************************************************
 * Dynamic arrays with progressive allocation
 ************************************************************************/

/* Internal functions */
#define _ARRAY_ALLOC(array, newsize) {                                      \
    array.i_alloc = newsize;                                                \
    array.p_elems = VLCCVP realloc( array.p_elems, array.i_alloc *          \
                                    sizeof(*array.p_elems) );               \
    assert(array.p_elems);                                                  \
}

#define _ARRAY_GROW1(array) {                                               \
    if( array.i_alloc < 10 )                                                \
        _ARRAY_ALLOC(array, 10 )                                            \
    else if( array.i_alloc == array.i_size )                                \
        _ARRAY_ALLOC(array, (int)(array.i_alloc * 1.5) )                    \
}

#define _ARRAY_GROW(array,additional) {                                     \
     int i_first = array.i_alloc;                                           \
     while( array.i_alloc - i_first < additional )                          \
     {                                                                      \
         if( array.i_alloc < 10 )                                           \
            _ARRAY_ALLOC(array, 10 )                                        \
        else if( array.i_alloc == array.i_size )                            \
            _ARRAY_ALLOC(array, (int)(array.i_alloc * 1.5) )                \
        else break;                                                         \
     }                                                                      \
}

#define _ARRAY_SHRINK(array) {                                              \
    if( array.i_size > 10 && array.i_size < (int)(array.i_alloc / 1.5) ) {  \
        _ARRAY_ALLOC(array, array.i_size + 5);                              \
    }                                                                       \
}


/* API */
#define DECL_ARRAY(type) struct {                                           \
    int i_alloc;                                                            \
    int i_size;                                                             \
    type *p_elems;                                                          \
}

#define TYPEDEF_ARRAY(type, name) typedef DECL_ARRAY(type) name;

#define ARRAY_INIT(array)                                                   \
    array.i_alloc = 0;                                                      \
    array.i_size = 0;                                                       \
    array.p_elems = NULL;

#define ARRAY_RESET(array)                                                  \
    array.i_alloc = 0;                                                      \
    array.i_size = 0;                                                       \
    free( array.p_elems ); array.p_elems = NULL;

#define ARRAY_APPEND(array, elem) {                                         \
    _ARRAY_GROW1(array);                                                    \
    array.p_elems[array.i_size] = elem;                                     \
    array.i_size++;                                                         \
}

#define ARRAY_INSERT(array,elem,pos) {                                      \
    _ARRAY_GROW1(array);                                                    \
    if( array.i_size - pos ) {                                              \
        memmove( array.p_elems + pos + 1, array.p_elems + pos,              \
                 (array.i_size-pos) * sizeof(*array.p_elems) );             \
    }                                                                       \
    array.p_elems[pos] = elem;                                              \
    array.i_size++;                                                         \
}

#define ARRAY_REMOVE(array,pos) {                                           \
    if( array.i_size - (pos) - 1 )                                          \
    {                                                                       \
        memmove( array.p_elems + pos, array.p_elems + pos + 1,              \
                 ( array.i_size - pos - 1 ) *sizeof(*array.p_elems) );      \
    }                                                                       \
    array.i_size--;                                                         \
    _ARRAY_SHRINK(array);                                                   \
}

#define ARRAY_VAL(array, pos) array.p_elems[pos]

#define ARRAY_BSEARCH(array, elem, zetype, key, answer) \
    BSEARCH( array.p_elems, array.i_size, elem, zetype, key, answer)

#define FOREACH_ARRAY( item, array ) { \
    int fe_idx; \
    for( fe_idx = 0 ; fe_idx < array.i_size ; fe_idx++ ) \
    { \
        item = array.p_elems[fe_idx];

#define FOREACH_END() } }

#endif
