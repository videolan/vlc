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
#include <vlc/input.h>

#include "vlc_playlist.h"

/**
 * Create a new item, without adding it to the playlist
 *
 * \param psz_uri the mrl of the item
 * \param psz_name a text giving a name or description of the item
 * \return the new item or NULL on failure
 */
playlist_item_t * __playlist_ItemNew( vlc_object_t *p_obj,
                                      const char *psz_uri,
                                      const char *psz_name )
{
    playlist_item_t * p_item;

    p_item = malloc( sizeof( playlist_item_t ) );
    if( p_item == NULL ) return NULL;
    if( psz_uri == NULL) return NULL;

    memset( p_item, 0, sizeof( playlist_item_t ) );

    p_item->input.psz_uri = strdup( psz_uri );

    if( psz_name != NULL ) p_item->input.psz_name = strdup( psz_name );
    else p_item->input.psz_name = strdup ( psz_uri );

    p_item->b_enabled = VLC_TRUE;
    p_item->i_group = PLAYLIST_TYPE_MANUAL;
    p_item->i_nb_played = 0;

    p_item->input.i_duration = -1;
    p_item->input.ppsz_options = NULL;
    p_item->input.i_options = 0;

    vlc_mutex_init( p_obj, &p_item->input.lock );

    playlist_ItemCreateCategory( p_item, _("General") );
    return p_item;
}

/**
 * Deletes a playlist item
 *
 * \param p_item the item to delete
 * \return nothing
 */
void playlist_ItemDelete( playlist_item_t *p_item )
{
    vlc_mutex_lock( &p_item->input.lock );

    if( p_item->input.psz_name ) free( p_item->input.psz_name );
    if( p_item->input.psz_uri ) free( p_item->input.psz_uri );

    /* Free the info categories */
    if( p_item->input.i_categories > 0 )
    {
        int i, j;

        for( i = 0; i < p_item->input.i_categories; i++ )
        {
            info_category_t *p_category = p_item->input.pp_categories[i];

            for( j = 0; j < p_category->i_infos; j++)
            {
                if( p_category->pp_infos[j]->psz_name )
                {
                    free( p_category->pp_infos[j]->psz_name);
                }
                if( p_category->pp_infos[j]->psz_value )
                {
                    free( p_category->pp_infos[j]->psz_value );
                }
                free( p_category->pp_infos[j] );
            }

            if( p_category->i_infos ) free( p_category->pp_infos );
            if( p_category->psz_name ) free( p_category->psz_name );
            free( p_category );
        }

        free( p_item->input.pp_categories );
    }

    for( ; p_item->input.i_options > 0; p_item->input.i_options-- )
    {
        free( p_item->input.ppsz_options[p_item->input.i_options - 1] );
        if( p_item->input.i_options == 1 ) free( p_item->input.ppsz_options );
    }

    vlc_mutex_unlock( &p_item->input.lock );
    vlc_mutex_destroy( &p_item->input.lock );

    free( p_item );
}

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
int playlist_AddItem( playlist_t *p_playlist, playlist_item_t *p_item,
                      int i_mode, int i_pos)
{
    vlc_value_t val;

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
                 if ( !strcmp( p_playlist->pp_items[j]->input.psz_uri,
                               p_item->input.psz_uri ) )
                 {
                      if ( p_item->input.psz_name )
                      {
                          free( p_item->input.psz_name );
                      }
                      if ( p_item->input.psz_uri )
                      {
                          free ( p_item->input.psz_uri );
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
             p_item->input.psz_name, p_item->input.psz_uri );

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

        INSERT_ELEM( p_playlist->pp_items, p_playlist->i_size, i_pos, p_item );
        p_playlist->i_enabled ++;

        if( p_playlist->i_index >= i_pos )
        {
            p_playlist->i_index++;
        }
    }
    else
    {
        /* i_mode == PLAYLIST_REPLACE and 0 <= i_pos < p_playlist->i_size */
        if( p_playlist->pp_items[i_pos]->input.psz_name )
        {
            free( p_playlist->pp_items[i_pos]->input.psz_name );
        }
        if( p_playlist->pp_items[i_pos]->input.psz_uri )
        {
            free( p_playlist->pp_items[i_pos]->input.psz_uri );
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
int playlist_ItemAddOption( playlist_item_t *p_item, const char *psz_option )
{
    if( !psz_option ) return VLC_EGENERIC;

    vlc_mutex_lock( &p_item->input.lock );
    INSERT_ELEM( p_item->input.ppsz_options, p_item->input.i_options,
                 p_item->input.i_options, strdup( psz_option ) );
    vlc_mutex_unlock( &p_item->input.lock );

    return VLC_SUCCESS;
}
