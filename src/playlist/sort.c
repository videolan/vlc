/*****************************************************************************
 * sort.c : Playlist sorting functions
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: sort.c,v 1.1 2003/10/29 18:00:46 zorglub Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/
#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/sout.h>

#include "vlc_playlist.h"

/**
 * Sort the playlist by title
 * \param p_playlist the playlist
 * \param i_type: SORT_NORMAL or SORT_REVERSE (reversed order)
 * \return 0 on success
 */
int playlist_SortTitle( playlist_t * p_playlist , int i_type )
{
    int i , i_small , i_position;
    playlist_item_t *p_temp;

    vlc_mutex_lock( &p_playlist->object_lock );

    for( i_position = 0; i_position < p_playlist->i_size -1 ; i_position ++ )
    {
        i_small  = i_position;
        for( i = i_position + 1 ; i<  p_playlist->i_size ; i++)
        {
            int i_test;

            i_test = strcasecmp( p_playlist->pp_items[i]->psz_name,
                                 p_playlist->pp_items[i_small]->psz_name );

            if( ( i_type == SORT_NORMAL  && i_test < 0 ) ||
                ( i_type == SORT_REVERSE && i_test > 0 ) )
            {
                i_small = i;
            }
        }
        /* Keep the correct current index */
        if( i_small == p_playlist->i_index )
            p_playlist->i_index = i_position;
        else if( i_position == p_playlist->i_index )
            p_playlist->i_index = i_small;

        p_temp = p_playlist->pp_items[i_position];
        p_playlist->pp_items[i_position] = p_playlist->pp_items[i_small];
        p_playlist->pp_items[i_small] = p_temp;
    }
    vlc_mutex_unlock( &p_playlist->object_lock );

    return 0;
}

/**
 * Sort the playlist by author
 * \param p_playlist the playlist
 * \param i_type: SORT_NORMAL or SORT_REVERSE (reversed order)
 * \return 0 on success
 */
int playlist_SortAuthor( playlist_t * p_playlist , int i_type )
{
    int i , i_small , i_position;
    playlist_item_t *p_temp;

    vlc_mutex_lock( &p_playlist->object_lock );

    for( i_position = 0; i_position < p_playlist->i_size -1 ; i_position ++ )
    {
        i_small  = i_position;
        for( i = i_position + 1 ; i<  p_playlist->i_size ; i++)
        {
            int i_test;

            i_test = strcasecmp( p_playlist->pp_items[i]->psz_author,
                                 p_playlist->pp_items[i_small]->psz_author );

            if( ( i_type == SORT_NORMAL  && i_test < 0 ) ||
                ( i_type == SORT_REVERSE && i_test > 0 ) )
            {
                i_small = i;
            }
        }
        /* Keep the correct current index */
        if( i_small == p_playlist->i_index )
            p_playlist->i_index = i_position;
        else if( i_position == p_playlist->i_index )
            p_playlist->i_index = i_small;

        p_temp = p_playlist->pp_items[i_position];
        p_playlist->pp_items[i_position] = p_playlist->pp_items[i_small];
        p_playlist->pp_items[i_small] = p_temp;
    }
    vlc_mutex_unlock( &p_playlist->object_lock );

    return 0;
}

int playlist_SortGroup( playlist_t * p_playlist , int i_type )
{
    int i , i_small , i_position;
    playlist_item_t *p_temp;

    vlc_mutex_lock( &p_playlist->object_lock );

    for( i_position = 0; i_position < p_playlist->i_size -1 ; i_position ++ )
    {
        i_small  = i_position;
        for( i = i_position + 1 ; i<  p_playlist->i_size ; i++)
        {
            int i_test;

            i_test = p_playlist->pp_items[i]->i_group -
                                 p_playlist->pp_items[i_small]->i_group;

            if( ( i_type == SORT_NORMAL  && i_test < 0 ) ||
                ( i_type == SORT_REVERSE && i_test > 0 ) )
            {
                i_small = i;
            }
        }
        /* Keep the correct current index */
        if( i_small == p_playlist->i_index )
            p_playlist->i_index = i_position;
        else if( i_position == p_playlist->i_index )
            p_playlist->i_index = i_small;

        p_temp = p_playlist->pp_items[i_position];
        p_playlist->pp_items[i_position] = p_playlist->pp_items[i_small];
        p_playlist->pp_items[i_small] = p_temp;
    }
    vlc_mutex_unlock( &p_playlist->object_lock );

    return 0;
}
