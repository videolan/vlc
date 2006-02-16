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
 * Sort the playlist.
 * \param p_playlist the playlist
 * \param i_mode: SORT_ID, SORT_TITLE, SORT_AUTHOR, SORT_ALBUM, SORT_RANDOM
 * \param i_type: ORDER_NORMAL or ORDER_REVERSE (reversed order)
 * \return VLC_SUCCESS on success
 */
int playlist_Sort( playlist_t * p_playlist , int i_mode, int i_type )
{
    int  i_id = -1;
    vlc_value_t val;
    val.b_bool = VLC_TRUE;

    vlc_mutex_lock( &p_playlist->object_lock );

    p_playlist->i_sort = i_mode;
    p_playlist->i_order = i_type;

    if( p_playlist->i_index >= 0 )
    {
        i_id = p_playlist->pp_items[p_playlist->i_index]->input.i_id;
    }

    playlist_ItemArraySort( p_playlist, p_playlist->i_size,
                    p_playlist->pp_items, i_mode, i_type );

    if( i_id != -1 )
    {
        p_playlist->i_index = playlist_GetPositionById( p_playlist, i_id );
    }

    /* ensure we are in no-view mode */
    p_playlist->status.i_view = -1;

    vlc_mutex_unlock( &p_playlist->object_lock );

    /* Notify the interfaces */
    var_Set( p_playlist, "intf-change", val );

    return VLC_SUCCESS;
}

/**
 * Sort a node.
 *
 * This function must be entered with the playlist lock !
 *
 * \param p_playlist the playlist
 * \param p_node the node to sort
 * \param i_mode: SORT_ID, SORT_TITLE, SORT_AUTHOR, SORT_ALBUM, SORT_RANDOM
 * \param i_type: ORDER_NORMAL or ORDER_REVERSE (reversed order)
 * \return VLC_SUCCESS on success
 */
int playlist_NodeSort( playlist_t * p_playlist , playlist_item_t *p_node,
                       int i_mode, int i_type )
{

    playlist_ItemArraySort( p_playlist,p_node->i_children,
                            p_node->pp_children, i_mode, i_type );

    p_node->i_serial++;

    return VLC_SUCCESS;
}

/**
 *
 * Sort a node recursively.
 *
 * This function must be entered with the playlist lock !
 *
 * \param p_playlist the playlist
 * \param p_node the node to sort
 * \param i_mode: SORT_ID, SORT_TITLE, SORT_AUTHOR, SORT_ALBUM, SORT_RANDOM
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

    for( i_position = 0; i_position < i_items -1 ; i_position ++ )
    {
        i_small  = i_position;
        for( i = i_position + 1 ; i< i_items ; i++)
        {
            int i_test = 0;

            if( i_mode == SORT_TITLE )
            {
                i_test = strcasecmp( pp_items[i]->input.psz_name,
                                         pp_items[i_small]->input.psz_name );
            }
            else if( i_mode == SORT_TITLE_NUMERIC )
            {
                i_test = atoi( pp_items[i]->input.psz_name ) -
                         atoi( pp_items[i_small]->input.psz_name );
            }
            else if( i_mode == SORT_DURATION )
            {
                i_test = pp_items[i]->input.i_duration -
                             pp_items[i_small]->input.i_duration;
            }
            else if( i_mode == SORT_AUTHOR )
            {
                char *psz_a = vlc_input_item_GetInfo(
                                 &pp_items[i]->input,
                                 _(VLC_META_INFO_CAT), _(VLC_META_ARTIST) );
                char *psz_b = vlc_input_item_GetInfo(
                                 &pp_items[i_small]->input,
                                 _(VLC_META_INFO_CAT), _(VLC_META_ARTIST) );
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
                // both are nodes
                else if( pp_items[i]->i_children >= 0 &&
                         pp_items[i_small]->i_children >= 0 )
                {
                    i_test = strcasecmp( pp_items[i]->input.psz_name,
                                         pp_items[i_small]->input.psz_name );
                }
                else if( psz_a == NULL && psz_b != NULL )
                {
                    i_test = 1;
                }
                else if( psz_a != NULL && psz_b == NULL )
                {
                    i_test = -1;
                }
                else if( psz_a == NULL && psz_b == NULL )
                {
                    i_test = strcasecmp( pp_items[i]->input.psz_name,
                                         pp_items[i_small]->input.psz_name );
                }
                else
                {
                    i_test = strcmp( psz_b, psz_a );
                }
            }
            else if( i_mode == SORT_ALBUM )
	    {
                char *psz_a = vlc_input_item_GetInfo(
                                 &pp_items[i]->input,
                                 _(VLC_META_INFO_CAT), _(VLC_META_COLLECTION) );
                char *psz_b = vlc_input_item_GetInfo(
                                 &pp_items[i_small]->input,
                                 _(VLC_META_INFO_CAT), _(VLC_META_COLLECTION) );
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
                // both are nodes
                else if( pp_items[i]->i_children >= 0 &&
                         pp_items[i_small]->i_children >= 0 )
                {
                    i_test = strcasecmp( pp_items[i]->input.psz_name,
                                         pp_items[i_small]->input.psz_name );
                }
                else if( psz_a == NULL && psz_b != NULL )
                {
                    i_test = 1;
                }
                else if( psz_a != NULL && psz_b == NULL )
                {
                    i_test = -1;
                }
                else if( psz_a == NULL && psz_b == NULL )
                {
                    i_test = strcasecmp( pp_items[i]->input.psz_name,
                                         pp_items[i_small]->input.psz_name );
                }
                else
                {
                    i_test = strcmp( psz_b, psz_a );
                }
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
                    i_test = strcasecmp( pp_items[i]->input.psz_name,
                                         pp_items[i_small]->input.psz_name );
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


int playlist_NodeGroup( playlist_t * p_playlist , int i_view,
                        playlist_item_t *p_root,
                        playlist_item_t **pp_items,int i_item,
                        int i_mode, int i_type )
{
    char *psz_search = NULL;
    int i_nodes = 0;
    playlist_item_t **pp_nodes = NULL;
    playlist_item_t *p_node;
    vlc_bool_t b_found;
    int i,j;
    for( i = 0; i< i_item ; i++ )
    {
        if( psz_search ) free( psz_search );
        if( i_mode == SORT_TITLE )
        {
            psz_search = strdup( pp_items[i]->input.psz_name );
        }
        else if ( i_mode == SORT_AUTHOR )
        {
            psz_search = vlc_input_item_GetInfo( &pp_items[i]->input,
                            _(VLC_META_INFO_CAT), _(VLC_META_ARTIST) );
        }
        else if ( i_mode == SORT_ALBUM )
        {
            psz_search = vlc_input_item_GetInfo( &pp_items[i]->input,
                            _(VLC_META_INFO_CAT), _(VLC_META_COLLECTION) );
        }
        else if ( i_mode == SORT_GENRE )
        {
            psz_search = vlc_input_item_GetInfo( &pp_items[i]->input,
                            _(VLC_META_INFO_CAT), _(VLC_META_GENRE) );
        }

        if( psz_search && !strcmp( psz_search, "" ) )
        {
            free( psz_search );
            psz_search = strdup( _("Undefined") );
        }

        b_found = VLC_FALSE;
        for( j = 0 ; j< i_nodes; j++ )
        {
           if( !strcasecmp( psz_search, pp_nodes[j]->input.psz_name ) )
           {
                playlist_NodeAppend( p_playlist, i_view,
                                     pp_items[i], pp_nodes[j] );
                b_found = VLC_TRUE;
                break;
           }
        }
        if( !b_found )
        {
            p_node = playlist_NodeCreate( p_playlist, i_view,psz_search,
                                          NULL );
            INSERT_ELEM( pp_nodes, i_nodes, i_nodes, p_node );
            playlist_NodeAppend( p_playlist, i_view,
                                 pp_items[i],p_node );
        }
    }

    /* Now, sort the nodes by name */
    playlist_ItemArraySort( p_playlist, i_nodes, pp_nodes, SORT_TITLE,
                            i_type );

    /* Now, sort each node and append it to the root node*/
    for( i = 0 ; i< i_nodes ; i++ )
    {
        playlist_ItemArraySort( p_playlist, pp_nodes[i]->i_children,
                                pp_nodes[i]->pp_children, SORT_TITLE, i_type );

        playlist_NodeAppend( p_playlist, i_view,
                             pp_nodes[i], p_root );
    }
    return VLC_SUCCESS;
}
