/*****************************************************************************
 * sort.c : Playlist sorting functions
 *****************************************************************************
 * Copyright (C) 1999-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/vout.h>
#include <vlc/sout.h>

#include "vlc_playlist.h"


int playlist_ItemArraySort( playlist_t *p_playlist, int i_items,
                playlist_item_t **pp_items, int i_mode,
                int i_type );

/**
 * Sort a node.
 *
 * This function must be entered with the playlist lock !
 *
 * \param p_playlist the playlist
 * \param p_node the node to sort
 * \param i_mode: SORT_ID, SORT_TITLE, SORT_ARTIST, SORT_ALBUM, SORT_RANDOM
 * \param i_type: ORDER_NORMAL or ORDER_REVERSE (reversed order)
 * \return VLC_SUCCESS on success
 */
int playlist_NodeSort( playlist_t * p_playlist , playlist_item_t *p_node,
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


int playlist_ItemArraySort( playlist_t *p_playlist, int i_items,
                            playlist_item_t **pp_items, int i_mode,
                            int i_type )
{
    int i , i_small , i_position;
    playlist_item_t *p_temp;
    vlc_value_t val;
    val.b_bool = VLC_TRUE;

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

#define DO_META_SORT( node ) { \
    char *psz_a = pp_items[i]->p_input->p_meta ?  \
                       pp_items[i]->p_input->p_meta->psz_##node : NULL ; \
    char *psz_b = pp_items[i_small]->p_input->p_meta ?  \
                       pp_items[i_small]->p_input->p_meta->psz_##node : NULL; \
    /* Nodes go first */ \
    if( pp_items[i]->i_children == -1 && pp_items[i_small]->i_children >= 0 ) \
        i_test = 1;\
    else if( pp_items[i]->i_children >= 0 &&\
             pp_items[i_small]->i_children == -1 ) \
       i_test = -1; \
    /* Both are nodes, sort by name */ \
    else if( pp_items[i]->i_children >= 0 && \
               pp_items[i_small]->i_children >= 0 ) \
    { \
         i_test = strcasecmp( pp_items[i]->p_input->psz_name, \
                              pp_items[i_small]->p_input->psz_name ); \
    } \
    /* Both are items */ \
    else if( psz_a == NULL && psz_b != NULL ) \
        i_test = 1; \
    else if( psz_a != NULL && psz_b == NULL ) \
        i_test = -1;\
    /* No meta, sort by name */ \
    else if( psz_a == NULL && psz_b == NULL ) \
    { \
        i_test = strcasecmp( pp_items[i]->p_input->psz_name, \
                             pp_items[i_small]->p_input->psz_name ); \
    } \
    else \
    { \
        i_test = strcmp( psz_b, psz_a ); \
    } \
}

    for( i_position = 0; i_position < i_items -1 ; i_position ++ )
    {
        i_small  = i_position;
        for( i = i_position + 1 ; i< i_items ; i++)
        {
            int i_test = 0;

            if( i_mode == SORT_TITLE )
            {
                i_test = strcasecmp( pp_items[i]->p_input->psz_name,
                                     pp_items[i_small]->p_input->psz_name );
            }
            else if( i_mode == SORT_TITLE_NUMERIC )
            {
                i_test = atoi( pp_items[i]->p_input->psz_name ) -
                         atoi( pp_items[i_small]->p_input->psz_name );
            }
            else if( i_mode == SORT_DURATION )
            {
                i_test = pp_items[i]->p_input->i_duration -
                             pp_items[i_small]->p_input->i_duration;
            }
            else if( i_mode == SORT_ARTIST )
            {
                DO_META_SORT( artist );
            }
            else if( i_mode == SORT_ALBUM )
            {
                DO_META_SORT( album );
            }
            else if( i_mode == SORT_TITLE_NODES_FIRST )
            {
                /* Alphabetic sort, all nodes first */

                if( pp_items[i]->i_children == -1 &&
                    pp_items[i_small]->i_children >= 0 )
                {
                    i_test = 1;
                }
                else if( pp_items[i]->i_children >= 0 &&
                         pp_items[i_small]->i_children == -1 )
                {
                    i_test = -1;
                }
                else
                {
                    i_test = strcasecmp( pp_items[i]->p_input->psz_name,
                                         pp_items[i_small]->p_input->psz_name );
                }
            }

            if( ( i_type == ORDER_NORMAL  && i_test < 0 ) ||
                ( i_type == ORDER_REVERSE && i_test > 0 ) )
            {
                i_small = i;
            }
        }
        p_temp = pp_items[i_position];
        pp_items[i_position] = pp_items[i_small];
        pp_items[i_small] = p_temp;
    }
    return VLC_SUCCESS;
}
