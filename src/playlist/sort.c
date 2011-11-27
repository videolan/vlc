/*****************************************************************************
 * sort.c : Playlist sorting functions
 *****************************************************************************
 * Copyright (C) 1999-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Ilkka Ollakka <ileoo@videolan.org>
 *          Rémi Duraffort <ivoire@videolan.org>
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

#include <vlc_common.h>
#include <vlc_rand.h>
#define  VLC_INTERNAL_PLAYLIST_SORT_FUNCTIONS
#include "vlc_playlist.h"
#include "playlist_internal.h"


/* General comparison functions */
/**
 * Compare two items using their title or name
 * @param first: the first item
 * @param second: the second item
 * @return -1, 0 or 1 like strcmp
 */
static inline int meta_strcasecmp_title( const playlist_item_t *first,
                              const playlist_item_t *second )
{
    int i_ret;
    char *psz_first = input_item_GetTitleFbName( first->p_input );
    char *psz_second = input_item_GetTitleFbName( second->p_input );

    if( psz_first && psz_second )
        i_ret = strcasecmp( psz_first, psz_second );
    else if( !psz_first && psz_second )
        i_ret = 1;
    else if( psz_first && !psz_second )
        i_ret = -1;
    else
        i_ret = 0;
    free( psz_first );
    free( psz_second );

    return i_ret;
}

/**
 * Compare two intems accoring to the given meta type
 * @param first: the first item
 * @param second: the second item
 * @param meta: the meta type to use to sort the items
 * @param b_integer: true if the meta are integers
 * @return -1, 0 or 1 like strcmp
 */
static inline int meta_sort( const playlist_item_t *first,
                             const playlist_item_t *second,
                             vlc_meta_type_t meta, bool b_integer )
{
    int i_ret;
    char *psz_first = input_item_GetMeta( first->p_input, meta );
    char *psz_second = input_item_GetMeta( second->p_input, meta );

    /* Nodes go first */
    if( first->i_children == -1 && second->i_children >= 0 )
        i_ret = 1;
    else if( first->i_children >= 0 && second->i_children == -1 )
       i_ret = -1;
    /* Both are nodes, sort by name */
    else if( first->i_children >= 0 && second->i_children >= 0 )
        i_ret = meta_strcasecmp_title( first, second );
    /* Both are items */
    else if( !psz_first && psz_second )
        i_ret = 1;
    else if( psz_first && !psz_second )
        i_ret = -1;
    /* No meta, sort by name */
    else if( !psz_first && !psz_second )
        i_ret = meta_strcasecmp_title( first, second );
    else
    {
        if( b_integer )
            i_ret = atoi( psz_first ) - atoi( psz_second );
        else
            i_ret = strcasecmp( psz_first, psz_second );
    }

    free( psz_first );
    free( psz_second );
    return i_ret;
}

/* Comparison functions */

/**
 * Return the comparison function appropriate for the SORT_* and ORDER_*
 * arguments given, or NULL for SORT_RANDOM.
 * @param i_mode: a SORT_* enum indicating the field to sort on
 * @param i_type: ORDER_NORMAL or ORDER_REVERSE
 * @return function pointer, or NULL for SORT_RANDOM or invalid input
 */
typedef int (*sortfn_t)(const void *,const void *);
static const sortfn_t sorting_fns[NUM_SORT_FNS][2];
static inline sortfn_t find_sorting_fn( unsigned i_mode, unsigned i_type )
{
    if( i_mode>=NUM_SORT_FNS || i_type>1 )
        return 0;
    return sorting_fns[i_mode][i_type];
}

/**
 * Sort an array of items recursively
 * @param i_items: number of items
 * @param pp_items: the array of items
 * @param p_sortfn: the sorting function
 * @return nothing
 */
static inline
void playlist_ItemArraySort( unsigned i_items, playlist_item_t **pp_items,
                             sortfn_t p_sortfn )
{
    if( p_sortfn )
    {
        qsort( pp_items, i_items, sizeof( pp_items[0] ), p_sortfn );
    }
    else /* Randomise */
    {
        unsigned i_position;
        unsigned i_new;
        playlist_item_t *p_temp;

        for( i_position = i_items - 1; i_position > 0; i_position-- )
        {
            i_new = ((unsigned)vlc_mrand48()) % (i_position+1);
            p_temp = pp_items[i_position];
            pp_items[i_position] = pp_items[i_new];
            pp_items[i_new] = p_temp;
        }
    }
}


/**
 * Sort a node recursively.
 * This function must be entered with the playlist lock !
 * @param p_playlist the playlist
 * @param p_node the node to sort
 * @param p_sortfn the sorting function
 * @return VLC_SUCCESS on success
 */
static int recursiveNodeSort( playlist_t *p_playlist, playlist_item_t *p_node,
                              sortfn_t p_sortfn )
{
    int i;
    playlist_ItemArraySort(p_node->i_children,p_node->pp_children,p_sortfn);
    for( i = 0 ; i< p_node->i_children; i++ )
    {
        if( p_node->pp_children[i]->i_children != -1 )
        {
            recursiveNodeSort( p_playlist, p_node->pp_children[i], p_sortfn );
        }
    }
    return VLC_SUCCESS;
}

/**
 * Sort a node recursively.
 *
 * This function must be entered with the playlist lock !
 *
 * \param p_playlist the playlist
 * \param p_node the node to sort
 * \param i_mode: a SORT_* constant indicating the field to sort on
 * \param i_type: ORDER_NORMAL or ORDER_REVERSE (reversed order)
 * \return VLC_SUCCESS on success
 */
int playlist_RecursiveNodeSort( playlist_t *p_playlist, playlist_item_t *p_node,
                                int i_mode, int i_type )
{
    /* Ask the playlist to reset as we are changing the order */
    pl_priv(p_playlist)->b_reset_currently_playing = true;

    /* Do the real job recursively */
    return recursiveNodeSort(p_playlist,p_node,find_sorting_fn(i_mode,i_type));
}


/* This is the stuff the sorting functions are made of. The proto_##
 * functions are wrapped in cmp_a_## and cmp_d_## functions that do
 * void * to const playlist_item_t * casting and dereferencing and
 * cmp_d_## inverts the result, too. proto_## are static inline,
 * cmp_[ad]_## are merely static as they're the target of pointers.
 *
 * In any case, each SORT_## constant (except SORT_RANDOM) must have
 * a matching SORTFN( )-declared function here.
 */

#define SORTFN( SORT, first, second ) static inline int proto_##SORT \
	( const playlist_item_t *first, const playlist_item_t *second )

SORTFN( SORT_ALBUM, first, second )
{
    int i_ret = meta_sort( first, second, vlc_meta_Album, false );
    /* Items came from the same album: compare the track numbers */
    if( i_ret == 0 )
        i_ret = meta_sort( first, second, vlc_meta_TrackNumber, true );

    return i_ret;
}

SORTFN( SORT_ARTIST, first, second )
{
    int i_ret = meta_sort( first, second, vlc_meta_Artist, false );
    /* Items came from the same artist: compare the albums */
    if( i_ret == 0 )
        i_ret = proto_SORT_ALBUM( first, second );

    return i_ret;
}

SORTFN( SORT_DESCRIPTION, first, second )
{
    return meta_sort( first, second, vlc_meta_Description, false );
}

SORTFN( SORT_DURATION, first, second )
{
    mtime_t time1 = input_item_GetDuration( first->p_input );
    mtime_t time2 = input_item_GetDuration( second->p_input );
    int i_ret = time1 > time2 ? 1 :
                    ( time1 == time2 ? 0 : -1 );
    return i_ret;
}

SORTFN( SORT_GENRE, first, second )
{
    return meta_sort( first, second, vlc_meta_Genre, false );
}

SORTFN( SORT_ID, first, second )
{
    return first->i_id - second->i_id;
}

SORTFN( SORT_RATING, first, second )
{
    return meta_sort( first, second, vlc_meta_Rating, true );
}

SORTFN( SORT_TITLE, first, second )
{
    return meta_strcasecmp_title( first, second );
}

SORTFN( SORT_TITLE_NODES_FIRST, first, second )
{
    /* If first is a node but not second */
    if( first->i_children == -1 && second->i_children >= 0 )
        return -1;
    /* If second is a node but not first */
    else if( first->i_children >= 0 && second->i_children == -1 )
        return 1;
    /* Both are nodes or both are not nodes */
    else
        return meta_strcasecmp_title( first, second );
}

SORTFN( SORT_TITLE_NUMERIC, first, second )
{
    int i_ret;
    char *psz_first = input_item_GetTitleFbName( first->p_input );
    char *psz_second = input_item_GetTitleFbName( second->p_input );

    if( psz_first && psz_second )
        i_ret = atoi( psz_first ) - atoi( psz_second );
    else if( !psz_first && psz_second )
        i_ret = 1;
    else if( psz_first && !psz_second )
        i_ret = -1;
    else
        i_ret = 0;

    free( psz_first );
    free( psz_second );
    return i_ret;
}

SORTFN( SORT_TRACK_NUMBER, first, second )
{
    return meta_sort( first, second, vlc_meta_TrackNumber, true );
}

SORTFN( SORT_URI, first, second )
{
    int i_ret;
    char *psz_first = input_item_GetURI( first->p_input );
    char *psz_second = input_item_GetURI( second->p_input );

    if( psz_first && psz_second )
        i_ret = strcasecmp( psz_first, psz_second );
    else if( !psz_first && psz_second )
        i_ret = 1;
    else if( psz_first && !psz_second )
        i_ret = -1;
    else
        i_ret = 0;

    free( psz_first );
    free( psz_second );
    return i_ret;
}

#undef  SORTFN

/* Generate stubs around the proto_## sorting functions, ascending and
 * descending both. Preprocessor magic up ahead. Brace yourself.
 */

#ifndef VLC_DEFINE_SORT_FUNCTIONS
#error  Where is VLC_DEFINE_SORT_FUNCTIONS?
#endif

#define DEF( s ) \
	static int cmp_a_##s(const void *l,const void *r) \
	{ return proto_##s(*(const playlist_item_t *const *)l, \
                           *(const playlist_item_t *const *)r); } \
	static int cmp_d_##s(const void *l,const void *r) \
	{ return -1*proto_##s(*(const playlist_item_t * const *)l, \
                              *(const playlist_item_t * const *)r); }

	VLC_DEFINE_SORT_FUNCTIONS

#undef  DEF

/* And populate an array with the addresses */

static const sortfn_t sorting_fns[NUM_SORT_FNS][2] =
#define DEF( a ) { cmp_a_##a, cmp_d_##a },
{ VLC_DEFINE_SORT_FUNCTIONS };
#undef  DEF

