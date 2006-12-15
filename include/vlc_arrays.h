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


#define TAB_APPEND( count, tab, p )             \
    if( (count) > 0 )                           \
    {                                           \
        (tab) = realloc( tab, sizeof( void ** ) * ( (count) + 1 ) ); \
    }                                           \
    else                                        \
    {                                           \
        (tab) = malloc( sizeof( void ** ) );    \
    }                                           \
    (tab)[count] = (p);        \
    (count)++

#define TAB_FIND( count, tab, p, index )        \
    {                                           \
        int _i_;                                \
        (index) = -1;                           \
        for( _i_ = 0; _i_ < (count); _i_++ )    \
        {                                       \
            if( (tab)[_i_] == (p) )  \
            {                                   \
                (index) = _i_;                  \
                break;                          \
            }                                   \
        }                                       \
    }

#define TAB_REMOVE( count, tab, p )             \
    {                                           \
        int _i_index_;                          \
        TAB_FIND( count, tab, p, _i_index_ );   \
        if( _i_index_ >= 0 )                    \
        {                                       \
            if( (count) > 1 )                     \
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
    }

/**
 * Binary search in a sorted array. The key must be comparable by < and >
 * \param entries array of entries
 * \param count number of entries
 * \param elem key to check within an entry (like .id, or ->i_id)
 * \param zetype type of the key
 * \param key value of the key
 * \param answer index of answer within the array. -1 if not found
 */
#define BSEARCH( entries, count, elem, zetype, key, answer ) {  \
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
}

/************************************************************************
 * Dictionaries
 ************************************************************************/

/* This function is not intended to be crypto-secure, we only want it to be
 * fast and not suck too much. This one is pretty fast and did 0 collisions
 * in wenglish's dictionary.
 */
static inline uint64_t DictHash( const char *psz_string, int i_int )
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

#define DICT_TYPE(name,type)                                                  \
    typedef struct dict_entry_##name##_t {                                    \
        int i_int;                                                            \
        char *psz_string;                                                     \
        uint64_t i_hash;                                                      \
        type data;                                                            \
    } dict_entry_##name##_t;                                                  \
    typedef struct dict_##name##_t {                                          \
        dict_entry_##name##_t *p_entries;                                     \
        int i_entries;                                                        \
    } dict_##name##_t;

#define DICT_NEW( p_dict ) {                                                  \
    p_dict = malloc( sizeof(int)+sizeof(void*) );                             \
    p_dict->i_entries = 0;                                                    \
    p_dict->p_entries = NULL;                                                 \
}

#define DICT_CLEAR( zdict ) {                                                 \
    int _i_dict = 0;                                                          \
    for ( _i_dict = 0; _i_dict < zdict->i_entries; _i_dict++ )                \
    {                                                                         \
        FREE( zdict->p_entries[_i_dict].psz_string );                         \
    }                                                                         \
    FREE( zdict->p_entries );                                                 \
    free( zdict );                                                            \
}

#define DICT_INSERT( zdict, zint, zstring, zdata ) {                          \
    uint64_t i_hash = DictHash( (zstring), (zint) );                          \
    int i_new;                                                                \
    /* Find a free slot */                                                    \
    if( zdict->i_entries == 0 || i_hash <= zdict->p_entries[0].i_hash )       \
        i_new = 0;                                                            \
    else if( i_hash >= zdict->p_entries[zdict->i_entries-1].i_hash )          \
        i_new = zdict->i_entries;\
    else                                                                      \
    {                                                                         \
        int i_low = 0, i_high = zdict->i_entries - 1;                         \
        while( i_high - i_low > 1 )                                           \
        {                                                                     \
            int i_mid = (i_low + i_high)/2;                                   \
            fprintf(stderr, "Low %i, high %i\n", i_low, i_high);              \
            if( zdict->p_entries[i_mid].i_hash < i_hash ) {                   \
                i_low = i_mid;                                                \
            } else if( zdict->p_entries[i_mid].i_hash > i_hash ) {            \
                i_high = i_mid;                                               \
            }                                                                 \
        }                                                                     \
        if( zdict->p_entries[i_low].i_hash < i_hash )                         \
            i_new = i_high;                                                   \
        else                                                                  \
            i_new = i_low;                                                    \
    }                                                                         \
    zdict->p_entries = realloc( zdict->p_entries, (zdict->i_entries + 1) *    \
        ( sizeof(zdata) + sizeof(int) + sizeof(void*) + sizeof(uint64_t) ) ); \
    zdict->i_entries++;                                                       \
    if( i_new != zdict->i_entries -1 )                                        \
        memmove( &zdict->p_entries[i_new+1], &zdict->p_entries[i_new],        \
         ( zdict->i_entries - i_new - 1 ) *                                   \
         ( sizeof(zdata) + sizeof(int) + sizeof(void*) + sizeof(uint64_t) ) );\
                                                                              \
    zdict->p_entries[i_new].i_hash = i_hash;                                  \
    zdict->p_entries[i_new].i_int = (zint);                                   \
    if( (zstring) ) {                                                         \
        zdict->p_entries[i_new].psz_string = strdup( (zstring) );             \
    } else {                                                                  \
        zdict->p_entries[i_new].psz_string = NULL;                            \
    }                                                                         \
    zdict->p_entries[i_new].data = zdata;                                     \
}

#define DICT_LOOKUP( zdict, zint, zstring, answer ) do {                      \
    uint64_t i_hash;                                                          \
    int i, i_pos;                                                             \
    vlc_bool_t b_found = VLC_FALSE;                                           \
    if( zdict->i_entries == 0 ) {                                             \
        answer = -1;                                                          \
        break;                                                                \
    }                                                                         \
                                                                              \
    i_hash = DictHash( (zstring), (zint) );                                   \
    BSEARCH( zdict->p_entries, zdict->i_entries, .i_hash, uint64_t,           \
             i_hash, i_pos );                                                 \
    if( i_pos == -1 ) {                                                       \
        answer = -1;                                                          \
        break;                                                                \
    }                                                                         \
                                                                              \
    /* Hash found, let's check it looks like the entry */                     \
    if( !strcmp( (zstring), zdict->p_entries[i_pos].psz_string ) ) {          \
        answer = i_pos;                                                       \
        break;                                                                \
    }                                                                         \
                                                                              \
    /* Hash collision! This should be very rare, but we cannot guarantee      \
     * it will never happen. Just do an exhaustive search amongst all         \
     * entries with the same hash. */                                         \
    for( i = i_pos - 1 ; i > 0 && i_hash == zdict->p_entries[i].i_hash ; i-- )\
    {                                                                         \
        if( !strcmp( (zstring), zdict->p_entries[i].psz_string ) &&           \
                   zdict->p_entries[i].i_int == (zint) ) {                    \
            b_found = VLC_TRUE;                                               \
            answer = i;                                                       \
            break;                                                            \
        }                                                                     \
    }                                                                         \
    if( b_found == VLC_TRUE )                                                 \
        break;                                                                \
    for( i = i_pos + 1 ; i < zdict->i_entries &&                              \
                         i_hash == zdict->p_entries[i].i_hash ; i++ )         \
    {                                                                         \
         if( !strcmp( (zstring), zdict->p_entries[i].psz_string ) &&          \
                      zdict->p_entries[i].i_int == (zint) ) {                 \
            b_found = VLC_TRUE;                                               \
            answer = i;                                                       \
            break;                                                            \
        }                                                                     \
    }                                                                         \
    /* Hash found, but entry not found (quite strange !) */                   \
    assert( 0 );                                                              \
} while(0)

#define DICT_GET( zdict, i_int, psz_string, answer ) {                        \
    int int_answer;                                                           \
    DICT_LOOKUP( zdict, i_int, psz_string, int_answer );                      \
    if( int_answer >=  0 )                                                    \
        answer = zdict->p_entries[int_answer].data;                           \
}

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
