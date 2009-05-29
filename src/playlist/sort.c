/*****************************************************************************
 * sort.c : Playlist sorting functions
 *****************************************************************************
 * Copyright (C) 1999-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Ilkka Ollakka <ileoo@videolan.org>
 *          Rémi Duraffort <ivoire@videolan.org>
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include "vlc_playlist.h"
#include "playlist_internal.h"


static void playlist_ItemArraySort( int i_items, playlist_item_t **pp_items,
                                    int i_mode, int i_type );
static int playlist_cmp( const void *, const void * );

/* Comparison functions */
static int playlist_cmp_album( const playlist_item_t *, const playlist_item_t *);
static int playlist_cmp_artist( const playlist_item_t *, const playlist_item_t *);
static int playlist_cmp_desc( const playlist_item_t *, const playlist_item_t *);
static int playlist_cmp_duration( const playlist_item_t *, const playlist_item_t *);
static int playlist_cmp_genre( const playlist_item_t *, const playlist_item_t *);
static int playlist_cmp_id( const playlist_item_t *, const playlist_item_t *);
static int playlist_cmp_rating( const playlist_item_t *, const playlist_item_t *);
static int playlist_cmp_title( const playlist_item_t *, const playlist_item_t *);
static int playlist_cmp_title_nodes_first( const playlist_item_t *,
                                          const playlist_item_t *);
static int playlist_cmp_title_num( const playlist_item_t *, const playlist_item_t *);
static int playlist_cmp_track_num( const playlist_item_t *, const playlist_item_t *);
static int playlist_cmp_uri( const playlist_item_t *, const playlist_item_t *);

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


/**
 * Sort a node recursively.
 * This function must be entered with the playlist lock !
 * @param p_playlist the playlist
 * @param p_node the node to sort
 * @param i_mode: SORT_ID, SORT_TITLE, SORT_ARTIST, SORT_ALBUM, SORT_RANDOM
 * @param i_type: ORDER_NORMAL or ORDER_REVERSE (reversed order)
 * @return VLC_SUCCESS on success
 */
static int recursiveNodeSort( playlist_t *p_playlist, playlist_item_t *p_node,
                              int i_mode, int i_type )
{
    int i;
    playlist_ItemArraySort( p_node->i_children, p_node->pp_children,
                            i_mode, i_type );
    for( i = 0 ; i< p_node->i_children; i++ )
    {
        if( p_node->pp_children[i]->i_children != -1 )
        {
            recursiveNodeSort( p_playlist, p_node->pp_children[i],
                               i_mode, i_type );
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
 * \param i_mode: SORT_ID, SORT_TITLE, SORT_ARTIST, SORT_ALBUM, SORT_RANDOM
 * \param i_type: ORDER_NORMAL or ORDER_REVERSE (reversed order)
 * \return VLC_SUCCESS on success
 */
int playlist_RecursiveNodeSort( playlist_t *p_playlist, playlist_item_t *p_node,
                                int i_mode, int i_type )
{
    /* Ask the playlist to reset as we are changing the order */
    pl_priv(p_playlist)->b_reset_currently_playing = true;

    /* Do the real job recursively */
    return recursiveNodeSort( p_playlist, p_node, i_mode, i_type );
}


static int (*sort_function)(const playlist_item_t *, const playlist_item_t *);
static int sort_order = 1;


/**
 * Sort an array of items recursively
 * @param i_items: number of items
 * @param pp_items: the array of items
 * @param i_mode: the criterias for the comparisons
 * @param i_type: ORDER_NORMAL or ORDER_REVERSE
 * @return nothing
 */
static void playlist_ItemArraySort( int i_items, playlist_item_t **pp_items,
                                   int i_mode, int i_type )
{
    int i_position;
    playlist_item_t *p_temp;

    /* Random sort */
    if( i_mode == SORT_RANDOM )
    {
        for( i_position = 0; i_position < i_items ; i_position++ )
        {
            int i_new;

            if( i_items > 1 )
                i_new = rand() % (i_items - 1);
            else
                i_new = 0;
            p_temp = pp_items[i_position];
            pp_items[i_position] = pp_items[i_new];
            pp_items[i_new] = p_temp;
        }
    }
    else
    {
        /* Choose the funtion to compare two items */
        switch( i_mode )
        {
        case SORT_ALBUM:
            sort_function = playlist_cmp_album;
            break;
        case SORT_ARTIST:
            sort_function = playlist_cmp_artist;
            break;
        case SORT_DESCRIPTION:
            sort_function = playlist_cmp_desc;
            break;
        case SORT_DURATION:
            sort_function = playlist_cmp_duration;
            break;
        case SORT_GENRE:
            sort_function = playlist_cmp_genre;
            break;
        case SORT_ID:
            sort_function = playlist_cmp_id;
            break;
        case SORT_TITLE:
            sort_function = playlist_cmp_title;
            break;
        case SORT_TITLE_NODES_FIRST:
            sort_function = playlist_cmp_title_nodes_first;
            break;
        case SORT_TITLE_NUMERIC:
            sort_function = playlist_cmp_title_num;
            break;
        case SORT_TRACK_NUMBER:
            sort_function = playlist_cmp_track_num;
            break;
        case SORT_RATING:
            sort_function = playlist_cmp_rating;
            break;
        case SORT_URI:
            sort_function = playlist_cmp_uri;
            break;
        default:
            assert(0);
        }
        sort_order = i_type == ORDER_REVERSE ? -1 : 1;
        qsort( pp_items, i_items, sizeof( pp_items[0] ), playlist_cmp );
    }
}


/**
 * Wrapper around playlist_cmp_* function
 * @param first: the first item
 * @param second: the second item
 * @return -1, 0 or 1 like strcmp
 */
static int playlist_cmp( const void *first, const void *second )
{
    if( sort_order == -1 )
        return -1 * sort_function( *(playlist_item_t **)first,
                                   *(playlist_item_t **)second );
    else
        return sort_function( *(playlist_item_t **)first,
                              *(playlist_item_t **)second );
}


/**
 * Compare two items according to the title
 * @param first: the first item
 * @param second: the second item
 * @return -1, 0 or 1 like strcmp
 */
static int playlist_cmp_album( const playlist_item_t *first,
                               const playlist_item_t *second )
{
    int i_ret = meta_sort( first, second, vlc_meta_Album, false );
    /* Items came from the same album: compare the track numbers */
    if( i_ret == 0 )
        i_ret = meta_sort( first, second, vlc_meta_TrackNumber, true );

    return i_ret;
}


/**
 * Compare two items according to the artist
 * @param first: the first item
 * @param second: the second item
 * @return -1, 0 or 1 like strcmp
 */
static int playlist_cmp_artist( const playlist_item_t *first,
                                const playlist_item_t *second )
{
    int i_ret = meta_sort( first, second, vlc_meta_Artist, false );
    /* Items came from the same artist: compare the albums */
    if( i_ret == 0 )
        i_ret = playlist_cmp_album( first, second );

    return i_ret;
}


/**
 * Compare two items according to the description
 * @param first: the first item
 * @param second: the second item
 * @return -1, 0 or 1 like strcmp
 */
static int playlist_cmp_desc( const playlist_item_t *first,
                              const playlist_item_t *second )
{
    return meta_sort( first, second, vlc_meta_Description, false );
}


/**
 * Compare two items according to the duration
 * @param first: the first item
 * @param second: the second item
 * @return -1, 0 or 1 like strcmp
 */
static int playlist_cmp_duration( const playlist_item_t *first,
                                  const playlist_item_t *second )
{
    return input_item_GetDuration( first->p_input ) -
           input_item_GetDuration( second->p_input );
}


/**
 * Compare two items according to the genre
 * @param first: the first item
 * @param second: the second item
 * @return -1, 0 or 1 like strcmp
 */
static int playlist_cmp_genre( const playlist_item_t *first,
                               const playlist_item_t *second )
{
    return meta_sort( first, second, vlc_meta_Genre, false );
}


/**
 * Compare two items according to the ID
 * @param first: the first item
 * @param second: the second item
 * @return -1, 0 or 1 like strcmp
 */
static int playlist_cmp_id( const playlist_item_t *first,
                            const playlist_item_t *second )
{
    return first->i_id - second->i_id;
}


/**
 * Compare two items according to the rating
 * @param first: the first item
 * @param second: the second item
 * @return -1, 0 or 1 like strcmp
 */
static int playlist_cmp_rating( const playlist_item_t *first,
                                const playlist_item_t *second )
{
    return meta_sort( first, second, vlc_meta_Rating, true );
}


/**
 * Compare two items according to the title
 * @param first: the first item
 * @param second: the second item
 * @return -1, 0 or 1 like strcmp
 */
static int playlist_cmp_title( const playlist_item_t *first,
                               const playlist_item_t *second )
{
    return meta_strcasecmp_title( first, second );
}


/**
 * Compare two items according to the title, with the nodes first in the list
 * @param first: the first item
 * @param second: the second item
 * @return -1, 0 or 1 like strcmp
 */
static int playlist_cmp_title_nodes_first( const playlist_item_t *first,
                                           const playlist_item_t *second )
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


/**
 * Compare two item according to the title as a numeric value
 * @param first: the first item
 * @param second: the second item
 * @return -1, 0 or 1 like strcmp
 */
static int playlist_cmp_title_num( const playlist_item_t *first,
                                   const playlist_item_t *second )
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


/**
 * Compare two item according to the track number
 * @param first: the first item
 * @param second: the second item
 * @return -1, 0 or 1 like strcmp
 */
static int playlist_cmp_track_num( const playlist_item_t *first,
                                   const playlist_item_t *second )
{
    return meta_sort( first, second, vlc_meta_TrackNumber, true );
}


/**
 * Compare two item according to the URI
 * @param first: the first item
 * @param second: the second item
 * @return -1, 0 or 1 like strcmp
 */
static int playlist_cmp_uri( const playlist_item_t *first,
                             const playlist_item_t *second )
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


