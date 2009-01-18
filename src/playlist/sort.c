/*****************************************************************************
 * sort.c : Playlist sorting functions
 *****************************************************************************
 * Copyright (C) 1999-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Ilkka Ollakka <ileoo@videolan.org>
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


static int playlist_ItemArraySort( playlist_t *p_playlist, int i_items,
                                   playlist_item_t **pp_items, int i_mode,
                                   int i_type );
static int playlist_cmp(const void *, const void *);

/**
 * Sort a node.
 * This function must be entered with the playlist lock !
 *
 * \param p_playlist the playlist
 * \param p_node the node to sort
 * \param i_mode: SORT_ID, SORT_TITLE, SORT_ARTIST, SORT_ALBUM, SORT_RANDOM
 * \param i_type: ORDER_NORMAL or ORDER_REVERSE (reversed order)
 * \return VLC_SUCCESS on success
 */
static int playlist_NodeSort( playlist_t * p_playlist , playlist_item_t *p_node,
                              int i_mode, int i_type )
{
    playlist_ItemArraySort( p_playlist,p_node->i_children,
                            p_node->pp_children, i_mode, i_type );
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
    int i;
    playlist_NodeSort( p_playlist, p_node, i_mode, i_type );
    for( i = 0 ; i< p_node->i_children; i++ )
    {
        if( p_node->pp_children[i]->i_children != -1 )
        {
            playlist_RecursiveNodeSort( p_playlist, p_node->pp_children[i],
                                        i_mode,i_type );
        }
    }
    return VLC_SUCCESS;
}

static int sort_mode = 0;
static int sort_type = 0;

static int playlist_ItemArraySort( playlist_t *p_playlist, int i_items,
                                   playlist_item_t **pp_items, int i_mode,
                                   int i_type )
{
    int i_position;
    playlist_item_t *p_temp;
    vlc_value_t val;
    val.b_bool = true;
    sort_mode = i_mode;
    sort_type = i_type;

    (void)p_playlist; // a bit surprising we don't need p_playlist!

    if( i_mode == SORT_RANDOM )
    {
        for( i_position = 0; i_position < i_items ; i_position ++ )
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

        return VLC_SUCCESS;
    }
    qsort(pp_items,i_items,sizeof(pp_items[0]),playlist_cmp);
    return VLC_SUCCESS;
}

static int playlist_cmp(const void *first, const void *second)
{

#define META_STRCASECMP_NAME( ) { \
    char *psz_i = input_item_GetName( (*(playlist_item_t **)first)->p_input ); \
    char *psz_ismall = input_item_GetName( (*(playlist_item_t **)second)->p_input ); \
    if( psz_i != NULL && psz_ismall != NULL ) i_test = strcasecmp( psz_i, psz_ismall ); \
    else if ( psz_i == NULL && psz_ismall != NULL ) i_test = 1; \
    else if ( psz_ismall == NULL && psz_i != NULL ) i_test = -1; \
    else i_test = 0; \
    free( psz_i ); \
    free( psz_ismall ); \
}


#define DO_META_SORT_ADV( node, integer ) { \
    char *psz_a = input_item_GetMeta( (*(playlist_item_t **)first)->p_input, vlc_meta_##node ); \
    char *psz_b = input_item_GetMeta( (*(playlist_item_t **)second)->p_input, vlc_meta_##node ); \
    /* Nodes go first */ \
    if( (*(playlist_item_t **)first)->i_children == -1 && (*(playlist_item_t **)second)->i_children >= 0 ) \
        i_test = 1;\
    else if( (*(playlist_item_t **)first)->i_children >= 0 &&\
             (*(playlist_item_t **)second)->i_children == -1 ) \
       i_test = -1; \
    /* Both are nodes, sort by name */ \
    else if( (*(playlist_item_t **)first)->i_children >= 0 && \
               (*(playlist_item_t **)second)->i_children >= 0 ) \
    { \
        META_STRCASECMP_NAME( ) \
    } \
    /* Both are items */ \
    else if( psz_a == NULL && psz_b != NULL ) \
        i_test = 1; \
    else if( psz_a != NULL && psz_b == NULL ) \
        i_test = -1;\
    /* No meta, sort by name */ \
    else if( psz_a == NULL && psz_b == NULL ) \
    { \
        META_STRCASECMP_NAME( ); \
    } \
    else \
    { \
        if( !integer ) i_test = strcasecmp( psz_a, psz_b ); \
        else           i_test = atoi( psz_a ) - atoi( psz_b ); \
    } \
    free( psz_a ); \
    free( psz_b ); \
}
#define DO_META_SORT( node ) DO_META_SORT_ADV( node, false )

    int i_test = 0;

    if( sort_mode == SORT_TITLE )
        {
            META_STRCASECMP_NAME( );
        }
    else if( sort_mode == SORT_TITLE_NUMERIC )
    {
        char *psz_i = input_item_GetName( (*(playlist_item_t **)first)->p_input );
        char *psz_ismall =
                input_item_GetName( (*(playlist_item_t **)second)->p_input );
        i_test = atoi( psz_i ) - atoi( psz_ismall );
        free( psz_i );
        free( psz_ismall );
    }
    else if( sort_mode == SORT_DURATION )
    {
        i_test = input_item_GetDuration( (*(playlist_item_t **)first)->p_input ) -
                 input_item_GetDuration( (*(playlist_item_t **)second)->p_input );
    }
    else if( sort_mode == SORT_ARTIST )
    {
        DO_META_SORT( Artist );
        /* sort by artist, album, tracknumber */
        if( i_test == 0 )
            DO_META_SORT( Album );
        if( i_test == 0 )
            DO_META_SORT_ADV( TrackNumber, true );
    }
    else if( sort_mode == SORT_GENRE )
    {
        DO_META_SORT( Genre );
    }
    else if( sort_mode == SORT_ALBUM )
    {
        DO_META_SORT( Album );
        /* Sort by tracknumber if albums are the same */
        if( i_test == 0 )
            DO_META_SORT_ADV( TrackNumber, true );
    }
    else if( sort_mode == SORT_TRACK_NUMBER )
    {
        DO_META_SORT_ADV( TrackNumber, true );
    }
    else if( sort_mode == SORT_DESCRIPTION )
    {
        DO_META_SORT( Description );
    }
    else if( sort_mode == SORT_ID )
    {
        i_test = (*(playlist_item_t **)first)->i_id - (*(playlist_item_t **)second)->i_id;
    }
    else if( sort_mode == SORT_TITLE_NODES_FIRST )
    {
        /* Alphabetic sort, all nodes first */

        if( (*(playlist_item_t **)first)->i_children == -1 &&
            (*(playlist_item_t **)second)->i_children >= 0 )
        {
            i_test = 1;
        }
        else if( (*(playlist_item_t **)first)->i_children >= 0 &&
                 (*(playlist_item_t **)second)->i_children == -1 )
        {
            i_test = -1;
        }
        else
        {
            if ( (*(playlist_item_t **)first)->p_input->psz_name != NULL &&
                 (*(playlist_item_t **)second)->p_input->psz_name != NULL )
            {
                i_test = strcasecmp( (*(playlist_item_t **)first)->p_input->psz_name,
                                 (*(playlist_item_t **)second)->p_input->psz_name );
            }
            else if ( (*(playlist_item_t **)first)->p_input->psz_name != NULL &&
                 (*(playlist_item_t **)second)->p_input->psz_name == NULL )
            {
                i_test = 1;
            }
            else if ( (*(playlist_item_t **)first)->p_input->psz_name == NULL &&
                 (*(playlist_item_t **)second)->p_input->psz_name != NULL )
            {
                i_test = -1;
            }
            else i_test = 0;
        }
    }
    else if( sort_mode == SORT_URI )
    {
        char *psz_i = input_item_GetURI( (*(playlist_item_t **)first)->p_input );
        char *psz_ismall =
                input_item_GetURI( (*(playlist_item_t **)second)->p_input );
        i_test = strcasecmp( psz_i, psz_ismall );
        free( psz_i );
        free( psz_ismall );
    }

    if ( sort_type == ORDER_REVERSE )
        i_test = i_test * -1;
#undef DO_META_SORT
#undef DO_META_SORT_ADV

    return i_test;
}
