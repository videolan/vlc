/*****************************************************************************
 * item.c : Playlist item functions
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: item.c,v 1.7 2003/12/05 01:52:11 rocky Exp $
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
 * Add an MRL to the playlist. This is a simplified version of
 * playlist_AddExt inculded for convenince. It equals calling playlist_AddExt
 * with psz_name == psz_target and i_duration == -1
 */

int playlist_Add( playlist_t *p_playlist, const char *psz_target,
                  const char **ppsz_options, int i_options,
                  int i_mode, int i_pos )
{
    return playlist_AddExt( p_playlist, psz_target, psz_target, -1,
                            ppsz_options, i_options, i_mode, i_pos );
}

/**
 * Add a MRL into the playlist.
 *
 * \param p_playlist the playlist to add into
 * \param psz_uri the mrl to add to the playlist
 * \param psz_name a text giving a name or description of this item
 * \param i_duration a hint about the duration of this item, in milliseconds, 
 *        or -1 if unknown.
 * \param ppsz_options array of options
 * \param i_options number of items in ppsz_options
 * \param i_mode the mode used when adding
 * \param i_pos the position in the playlist where to add. If this is
 *        PLAYLIST_END the item will be added at the end of the playlist
 *        regardless of it's size
 * \return always returns 0
*/
int playlist_AddExt( playlist_t *p_playlist, const char * psz_uri,
                     const char * psz_name, mtime_t i_duration,
                     const char **ppsz_options, int i_options, int i_mode,
                     int i_pos )
{
    playlist_item_t * p_item;

    p_item = malloc( sizeof( playlist_item_t ) );
    if( p_item == NULL )
    {
        msg_Err( p_playlist, "out of memory" );
    }

    p_item->psz_name   = strdup( psz_name );
    p_item->psz_uri    = strdup( psz_uri );
    p_item->psz_author = strdup( "" );
    p_item->i_duration = i_duration;
    p_item->i_type = 0;
    p_item->i_status = 0;
    p_item->b_autodeletion = VLC_FALSE;
    p_item->b_enabled = VLC_TRUE;
    p_item->i_group = PLAYLIST_TYPE_MANUAL;

    p_item->ppsz_options = NULL;
    p_item->i_options = i_options;

    if( i_options > 0 )
    {
        int i;

        p_item->ppsz_options = malloc( i_options * sizeof(char *) );
        for( i = 0; i < i_options; i++ )
        {
            p_item->ppsz_options[i] = strdup( ppsz_options[i] );
        }

    }

    return playlist_AddItem( p_playlist, p_item, i_mode, i_pos );
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
 * \return always returns 0
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
                      if( p_item->i_options )
                      {
                          int i_opt;
                          for( i_opt = 0; i_opt < p_item->i_options; i_opt++ )
                          {
                              free( p_item->ppsz_options[i_opt] );
                          }
                          free( p_item->ppsz_options );
                      }
                      if( p_item->psz_author )
                      {
                          free( p_item->psz_author );
                      }
                      free( p_item );
                      vlc_mutex_unlock( &p_playlist->object_lock );
                      return 0;
                 }
             }
         }
         i_mode &= ~PLAYLIST_CHECK_INSERT;
         i_mode |= PLAYLIST_APPEND;
    }


    msg_Dbg( p_playlist, "adding playlist item « %s » ( %s )", p_item->psz_name, p_item->psz_uri);

    /* Create the new playlist item */


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
        if( p_playlist->pp_items[i_pos]->psz_author )
        {
            free( p_playlist->pp_items[i_pos]->psz_author );
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

    return 0;
}

/**
 * delete an item from a playlist.
 *
 * \param p_playlist the playlist to remove from.
 * \param i_pos the position of the item to remove
 * \return returns 0
 */
int playlist_Delete( playlist_t * p_playlist, int i_pos )
{
    vlc_value_t     val;

    /* if i_pos is the current played item, playlist should stop playing it */
    if( ( p_playlist->i_status == PLAYLIST_RUNNING) && (p_playlist->i_index == i_pos) )
    {
        playlist_Command( p_playlist, PLAYLIST_STOP, 0 );
    }

    vlc_mutex_lock( &p_playlist->object_lock );
    if( i_pos >= 0 && i_pos < p_playlist->i_size )
    {
        playlist_item_t *p_item = p_playlist->pp_items[i_pos];

        msg_Dbg( p_playlist, "deleting playlist item « %s »",
                 p_item->psz_name );

        if( p_item->psz_name )
        {
            free( p_item->psz_name );
        }
        if( p_item->psz_uri )
        {
            free( p_item->psz_uri );
        }
        if( p_item->psz_author )
        {
            free( p_item->psz_author );
        }
        if( p_item->i_options > 0 )
        {
            int i;

            for( i = 0; i < p_item->i_options; i++ )
            {
                free( p_item->ppsz_options[i] );
            }

            free( p_item->ppsz_options );
        }

        /* XXX: what if the item is still in use? */
        free( p_item );

        if( i_pos <= p_playlist->i_index )
        {
            p_playlist->i_index--;
        }

        /* Renumber the playlist */
        REMOVE_ELEM( p_playlist->pp_items,
                     p_playlist->i_size,
                     i_pos );
        if( p_playlist->i_enabled > 0 )
            p_playlist->i_enabled--;
    }

    vlc_mutex_unlock( &p_playlist->object_lock );

    val.b_bool = VLC_TRUE;
    var_Set( p_playlist, "intf-change", val );

    return 0;
}

/**
 * Disables a playlist item
 *
 * \param p_playlist the playlist to disable from.
 * \param i_pos the position of the item to disable
 * \return returns 0
 */
int playlist_Disable( playlist_t * p_playlist, int i_pos )
{
    vlc_value_t     val;
    vlc_mutex_lock( &p_playlist->object_lock );


    if( i_pos >= 0 && i_pos < p_playlist->i_size )
    {
        msg_Dbg( p_playlist, "disabling playlist item « %s »",
                             p_playlist->pp_items[i_pos]->psz_name );

        if( p_playlist->pp_items[i_pos]->b_enabled == VLC_TRUE )
            p_playlist->i_enabled--;
        p_playlist->pp_items[i_pos]->b_enabled = VLC_FALSE;
    }

    vlc_mutex_unlock( &p_playlist->object_lock );

    val.b_bool = VLC_TRUE;
    var_Set( p_playlist, "intf-change", val );

    return 0;
}

/**
 * Enables a playlist item
 *
 * \param p_playlist the playlist to enable from.
 * \param i_pos the position of the item to enable
 * \return returns 0
 */
int playlist_Enable( playlist_t * p_playlist, int i_pos )
{
    vlc_value_t     val;
    vlc_mutex_lock( &p_playlist->object_lock );

    if( i_pos >= 0 && i_pos < p_playlist->i_size )
    {
        msg_Dbg( p_playlist, "enabling playlist item « %s »",
                             p_playlist->pp_items[i_pos]->psz_name );

        if( p_playlist->pp_items[i_pos]->b_enabled == VLC_FALSE )
            p_playlist->i_enabled++;

        p_playlist->pp_items[i_pos]->b_enabled = VLC_TRUE;
    }

    vlc_mutex_unlock( &p_playlist->object_lock );

    val.b_bool = VLC_TRUE;
    var_Set( p_playlist, "intf-change", val );

    return 0;
}

/**
 * Disables a playlist group
 *
 * \param p_playlist the playlist to disable from.
 * \param i_pos the id of the group to disable
 * \return returns 0
 */
int playlist_DisableGroup( playlist_t * p_playlist, int i_group)
{
    vlc_value_t     val;
    int i;
    vlc_mutex_lock( &p_playlist->object_lock );

    msg_Dbg(p_playlist,"Disabling group %i",i_group);
    for( i = 0 ; i< p_playlist->i_size; i++ )
    {
        if( p_playlist->pp_items[i]->i_group == i_group )
        {
            msg_Dbg( p_playlist, "disabling playlist item « %s »",
                           p_playlist->pp_items[i]->psz_name );

            if( p_playlist->pp_items[i]->b_enabled == VLC_TRUE )
                p_playlist->i_enabled--;

            p_playlist->pp_items[i]->b_enabled = VLC_FALSE;
        }
    }
    vlc_mutex_unlock( &p_playlist->object_lock );

    val.b_bool = VLC_TRUE;
    var_Set( p_playlist, "intf-change", val );

    return 0;
}

/**
 * Enables a playlist group
 *
 * \param p_playlist the playlist to enable from.
 * \param i_pos the id of the group to enable
 * \return returns 0
 */
int playlist_EnableGroup( playlist_t * p_playlist, int i_group)
{
    vlc_value_t     val;
    int i;
    vlc_mutex_lock( &p_playlist->object_lock );

    for( i = 0 ; i< p_playlist->i_size; i++ )
    {
        if( p_playlist->pp_items[i]->i_group == i_group )
        {
            msg_Dbg( p_playlist, "enabling playlist item « %s »",
                           p_playlist->pp_items[i]->psz_name );

            if( p_playlist->pp_items[i]->b_enabled == VLC_FALSE )
                p_playlist->i_enabled++;

            p_playlist->pp_items[i]->b_enabled = VLC_TRUE;
        }
    }
    vlc_mutex_unlock( &p_playlist->object_lock );

    val.b_bool = VLC_TRUE;
    var_Set( p_playlist, "intf-change", val );

    return 0;
}

/**
 * Move an item in a playlist
 *
 * Move the item in the playlist with position i_pos before the current item
 * at position i_newpos.
 * \param p_playlist the playlist to move items in
 * \param i_pos the position of the item to move
 * \param i_newpos the position of the item that will be behind the moved item
 *        after the move
 * \return returns 0
 */
int playlist_Move( playlist_t * p_playlist, int i_pos, int i_newpos)
{
    vlc_value_t     val;
    vlc_mutex_lock( &p_playlist->object_lock );

    /* take into account that our own row disappears. */
    if ( i_pos < i_newpos ) i_newpos--;

    if( i_pos >= 0 && i_newpos >=0 && i_pos <= p_playlist->i_size
                     && i_newpos <= p_playlist->i_size )
    {
        playlist_item_t * temp;

        msg_Dbg( p_playlist, "moving playlist item « %s » (%i -> %i)",
                             p_playlist->pp_items[i_pos]->psz_name, i_pos,
                             i_newpos );

        if( i_pos == p_playlist->i_index )
        {
            p_playlist->i_index = i_newpos;
        }
        else if( i_pos > p_playlist->i_index && i_newpos <= p_playlist->i_index )
        {
            p_playlist->i_index++;
        }
        else if( i_pos < p_playlist->i_index && i_newpos >= p_playlist->i_index )
        {
            p_playlist->i_index--;
        }

        if ( i_pos < i_newpos )
        {
            temp = p_playlist->pp_items[i_pos];
            while ( i_pos < i_newpos )
            {
                p_playlist->pp_items[i_pos] = p_playlist->pp_items[i_pos+1];
                i_pos++;
            }
            p_playlist->pp_items[i_newpos] = temp;
        }
        else if ( i_pos > i_newpos )
        {
            temp = p_playlist->pp_items[i_pos];
            while ( i_pos > i_newpos )
            {
                p_playlist->pp_items[i_pos] = p_playlist->pp_items[i_pos-1];
                i_pos--;
            }
            p_playlist->pp_items[i_newpos] = temp;
        }
    }

    vlc_mutex_unlock( &p_playlist->object_lock );

    val.b_bool = VLC_TRUE;
    var_Set( p_playlist, "intf-change", val );

    return 0;
}
