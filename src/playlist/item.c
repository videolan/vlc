/*****************************************************************************
 * item.c : Playlist item functions
 *****************************************************************************
 * Copyright (C) 1999-2004 VideoLAN
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
#include <vlc/input.h>

#include "vlc_playlist.h"





/**
 * Add a playlist item into a playlist
 *
 * \param p_playlist the playlist to insert into
 * \param p_item the playlist item to insert
 * \param i_mode the mode used when adding
 * \param i_pos the possition in the playlist where to add. If this is
 *        PLAYLIST_END the item will be added at the end of the playlist
 *        regardless of it's size
 * \return The id of the playlist item
*/
int playlist_AddItem( playlist_t *p_playlist, playlist_item_t * p_item,
                int i_mode, int i_pos)
{
    vlc_value_t     val;

    vlc_mutex_lock( &p_playlist->object_lock );

    /*
     * CHECK_INSERT : checks if the item is already enqued before
     * enqueing it
     */
    if ( i_mode & PLAYLIST_CHECK_INSERT )
    {
         int j;

         if ( p_playlist->pp_items )
         {
             for ( j = 0; j < p_playlist->i_size; j++ )
             {
                 if ( !strcmp( p_playlist->pp_items[j]->psz_uri, p_item->psz_uri ) )
                 {
                      if( p_item->psz_name )
                      {
                          free( p_item->psz_name );
                      }
                      if( p_item->psz_uri )
                      {
                          free( p_item->psz_uri );
                      }
                      free( p_item );
                      vlc_mutex_unlock( &p_playlist->object_lock );
                      return -1;
                 }
             }
         }
         i_mode &= ~PLAYLIST_CHECK_INSERT;
         i_mode |= PLAYLIST_APPEND;
    }


    msg_Dbg( p_playlist, "adding playlist item `%s' ( %s )",
             p_item->psz_name, p_item->psz_uri );


    p_item->i_id = ++p_playlist->i_last_id;

    /* Do a few boundary checks and allocate space for the item */
    if( i_pos == PLAYLIST_END )
    {
        if( i_mode & PLAYLIST_INSERT )
        {
            i_mode &= ~PLAYLIST_INSERT;
            i_mode |= PLAYLIST_APPEND;
        }

        i_pos = p_playlist->i_size - 1;
    }

    if( !(i_mode & PLAYLIST_REPLACE)
         || i_pos < 0 || i_pos >= p_playlist->i_size )
    {
        /* Additional boundary checks */
        if( i_mode & PLAYLIST_APPEND )
        {
            i_pos++;
        }

        if( i_pos < 0 )
        {
            i_pos = 0;
        }
        else if( i_pos > p_playlist->i_size )
        {
            i_pos = p_playlist->i_size;
        }

        INSERT_ELEM( p_playlist->pp_items,
                     p_playlist->i_size,
                     i_pos,
                     p_item );
        p_playlist->i_enabled ++;

        if( p_playlist->i_index >= i_pos )
        {
            p_playlist->i_index++;
        }
    }
    else
    {
        /* i_mode == PLAYLIST_REPLACE and 0 <= i_pos < p_playlist->i_size */
        if( p_playlist->pp_items[i_pos]->psz_name )
        {
            free( p_playlist->pp_items[i_pos]->psz_name );
        }
        if( p_playlist->pp_items[i_pos]->psz_uri )
        {
            free( p_playlist->pp_items[i_pos]->psz_uri );
        }
        /* XXX: what if the item is still in use? */
        free( p_playlist->pp_items[i_pos] );
        p_playlist->pp_items[i_pos] = p_item;
    }

    if( i_mode & PLAYLIST_GO )
    {
        p_playlist->i_index = i_pos;
        if( p_playlist->p_input )
        {
            input_StopThread( p_playlist->p_input );
        }
        p_playlist->i_status = PLAYLIST_RUNNING;
    }

    vlc_mutex_unlock( &p_playlist->object_lock );

    val.b_bool = VLC_TRUE;
    var_Set( p_playlist, "intf-change", val );

    return p_item->i_id;
}

/**
 *  Add a option to one item ( no need for p_playlist )
 *
 * \param p_item the item on which we want the info
 * \param psz_format the option
 * \return 0 on success
*/
int playlist_ItemAddOption( playlist_item_t *p_item,
                            const char *psz_option )
{
    if( !psz_option ) return VLC_EGENERIC;

    INSERT_ELEM( p_item->ppsz_options, p_item->i_options, p_item->i_options,
                 strdup( psz_option ) );

    return VLC_SUCCESS;
}
