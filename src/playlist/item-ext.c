/*****************************************************************************
 * item-ext.c : Playlist item management functions
 *****************************************************************************
 * Copyright (C) 1999-2004 VideoLAN
 * $Id: item-ext.c,v 1.13 2004/01/29 17:51:08 zorglub Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/
#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/sout.h>

#include "vlc_playlist.h"

/***************************************************************************
 * Item creation/addition functions
 ***************************************************************************/

/**
 * Add a MRL into the playlist, duration and options given
 *
 * \param p_playlist the playlist to add into
 * \param psz_uri the mrl to add to the playlist
 * \param psz_name a text giving a name or description of this item
 * \param i_mode the mode used when adding
 * \param i_pos the position in the playlist where to add. If this is
 *        PLAYLIST_END the item will be added at the end of the playlist
 *        regardless of it's size
 * \param i_duration length of the item in milliseconds.
 * \param ppsz_options an array of options
 * \param i_options the number of options
 * \return The id of the playlist item
*/
int playlist_AddExt( playlist_t *p_playlist, const char * psz_uri,
                     const char *psz_name, int i_mode, int i_pos,
                     mtime_t i_duration, const char **ppsz_options,
                     int i_options )
{
    playlist_item_t * p_item = playlist_ItemNew( p_playlist , psz_uri, psz_name );

    if( p_item == NULL )
    {
        msg_Err( p_playlist, "unable to add item to playlist" );
        return -1;
    }

    p_item->i_duration = i_duration;
    p_item->ppsz_options = (char **)ppsz_options;
    p_item->i_options  = i_options;

    return playlist_AddItem( p_playlist, p_item, i_mode, i_pos );
}

/**
 * Add a MRL into the playlist.
 *
 * \param p_playlist the playlist to add into
 * \param psz_uri the mrl to add to the playlist
 * \param psz_name a text giving a name or description of this item
 * \param i_mode the mode used when adding
 * \param i_pos the position in the playlist where to add. If this is
 *        PLAYLIST_END the item will be added at the end of the playlist
 *        regardless of it's size
 * \return The id of the playlist item
*/
int playlist_Add( playlist_t *p_playlist, const char * psz_uri,
                     const char *psz_name, int i_mode, int i_pos )
{
    return playlist_AddExt ( p_playlist, psz_uri, psz_name, i_mode, i_pos,
                            -1, NULL, 0 );
}

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
    if( p_item == NULL )
    {
        return NULL;
    }
    if( psz_uri == NULL)
    {
        return NULL;
    }
    memset( p_item, 0, sizeof( playlist_item_t ) );
    p_item->psz_uri    = strdup( psz_uri );
    if( psz_name != NULL )
    {
        p_item->psz_name   = strdup( psz_name );
    }
    else
    {
        p_item->psz_name = strdup ( psz_uri );
    }
    p_item->b_enabled = VLC_TRUE;
    p_item->i_group = PLAYLIST_TYPE_MANUAL;

    p_item->i_duration = -1;
    p_item->ppsz_options = NULL;
    p_item->i_options = 0;

    vlc_mutex_init( p_obj->p_vlc , &p_item->lock );

    playlist_ItemCreateCategory( p_item, _("General") );
    return p_item;
}

/**
 * Deletes a playlist item
 *
 * \param p_item the item to delete
 * \return nothing
 */
void playlist_ItemDelete( playlist_item_t *p_item)
{
    int i,j;

    vlc_mutex_lock( &p_item->lock );

    if( p_item->psz_name )
    {
        free( p_item->psz_name );
    }
    if( p_item->psz_uri )
    {
        free( p_item->psz_uri );
    }

    /* Free the info categories. Welcome to the segfault factory */
    if( p_item->i_categories > 0 )
    {
        for( i = 0; i < p_item->i_categories; i++ )
        {
            for( j= 0 ; j < p_item->pp_categories[i]->i_infos; j++)
            {
                if( p_item->pp_categories[i]->pp_infos[j]->psz_name)
                {
                    free( p_item->pp_categories[i]->
                                  pp_infos[j]->psz_name);
                }
                if( p_item->pp_categories[i]->pp_infos[j]->psz_value)
                {
                    free( p_item->pp_categories[i]->
                                  pp_infos[j]->psz_value);
                }
                free( p_item->pp_categories[i]->pp_infos[j] );
            }
            if( p_item->pp_categories[i]->i_infos )
                free( p_item->pp_categories[i]->pp_infos );
            if( p_item->pp_categories[i]->psz_name)
            {
                free( p_item->pp_categories[i]->psz_name );
            }
            free( p_item->pp_categories[i] );
       }
       free( p_item->pp_categories );
   }

    vlc_mutex_unlock( &p_item->lock );
    vlc_mutex_destroy( &p_item->lock );

    free( p_item );
}

/***************************************************************************
 * Item search functions
 ***************************************************************************/

/**
 * Search the position of an item by its id
 * This function must be entered with the playlist lock
 *
 * \param p_playlist the playlist
 * \param i_id the id to find
 * \return the position, or VLC_EGENERIC on failure
 */
int playlist_GetPositionById( playlist_t * p_playlist , int i_id )
{
    int i;
    for( i =  0 ; i < p_playlist->i_size ; i++ )
    {
        if( p_playlist->pp_items[i]->i_id == i_id )
        {
            return i;
        }
    }
    return VLC_EGENERIC;
}


/**
 * Search an item by its id
 *
 * \param p_playlist the playlist
 * \param i_id the id to find
 * \return the item, or NULL on failure
 */
playlist_item_t * playlist_ItemGetById( playlist_t * p_playlist , int i_id )
{
    int i;
    for( i =  0 ; i < p_playlist->i_size ; i++ )
    {
        if( p_playlist->pp_items[i]->i_id == i_id )
        {
            return p_playlist->pp_items[i];
        }
    }
    return NULL;
}

/**
 * Search an item by its position
 * This function must be entered with the playlist lock
 *
 * \param p_playlist the playlist
 * \param i_id the id to find
 * \return the item, or NULL on failure
 */
playlist_item_t * playlist_ItemGetByPos( playlist_t * p_playlist , int i_pos )
{
    if( i_pos >= 0 && i_pos < p_playlist->i_size)
    {
        return p_playlist->pp_items[i_pos];
    }
    else if( p_playlist->i_size > 0)
    {
        return p_playlist->pp_items[p_playlist->i_index];
    }
    else
    {
        return NULL;
    }
}

/**********************************************************************
 * playlist_item_t structure accessors
 * These functions give access to the fields of the playlist_item_t
 * structure
 **********************************************************************/

/**
 * Set the group of a playlist item
 *
 * \param p_item the item
 * \return VLC_SUCCESS on success
 */
int playlist_ItemSetGroup( playlist_item_t *p_item, int i_group)
{
    p_item->i_group = i_group;
    return VLC_SUCCESS;
}

/**
 * Set the group of a playlist item (by position)
 * This function must be entered with the playlist lock
 * Legacy function due to disappear (locks the whole playlist)
 *
 * \param p_playlist the playlist
 * \param i_pos the postition of the item of which we change the group
 * \param i_group the new group
 * \return VLC_SUCCESS on success, VLC_EGENERIC on failure
 */
int playlist_SetGroup( playlist_t *p_playlist, int i_pos, int i_group )
{
    vlc_value_t val;
    playlist_item_t *p_item;
    if( !p_playlist )
    {
        return VLC_ENOOBJ;
    }

    p_item = playlist_ItemGetByPos( p_playlist , i_pos );
    if( !p_item )
    {
        return VLC_ENOOBJ;
    }

    vlc_mutex_lock( &p_item->lock );
    playlist_ItemSetGroup( p_item , i_group );
    vlc_mutex_unlock( &p_item->lock );

    val.b_bool = (i_pos >= 0 && i_pos < p_playlist->i_size ) ? i_pos : -1;
    var_Set( p_playlist, "item-change", val );

    return VLC_SUCCESS;
}

/**
 * Set the name of a playlist item
 *
 * \param p_item the item
 * \param psz_name the new name
 * \return VLC_SUCCESS on success, VLC_EGENERIC on failure
 */
int playlist_ItemSetName( playlist_item_t *p_item, char *psz_name )
{
    if( psz_name && p_item )
    {
        p_item->psz_name = strdup( psz_name );
        return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

/**
 * Set the name of a playlist item (by position)
 * This function must be entered with the playlist lock
 * Legacy function due to disappear (locks the whole playlist)
 *
 * \param p_playlist the playlist
 * \param i_pos the position of the item of which we change the name
 * \param psz_name the new name
 * \return VLC_SUCCESS on success, VLC_EGENERIC on failure
 */
int playlist_SetName( playlist_t *p_playlist, int i_pos, char *psz_name )
{
    vlc_value_t val;
    playlist_item_t *p_item;
    if( !p_playlist )
    {
        return VLC_ENOOBJ;
    }

    p_item = playlist_ItemGetByPos( p_playlist , i_pos );
    if( !p_item )
    {
        return VLC_ENOOBJ;
    }

    vlc_mutex_lock( &p_item->lock );
    playlist_ItemSetName( p_item , psz_name );
    vlc_mutex_unlock( &p_item->lock );

    val.b_bool = (i_pos >= 0 && i_pos < p_playlist->i_size ) ? i_pos : -1;
    var_Set( p_playlist, "item-change", val );

    return VLC_SUCCESS;
}

/**
 * Set the duration of a playlist item
 * This function must be entered with the item lock
 *
 * \param p_item the item
 * \param psz_name the new name
 * \return VLC_SUCCESS on success, VLC_EGENERIC on failure
 */
int playlist_ItemSetDuration( playlist_item_t *p_item, mtime_t i_duration )
{
    char psz_buffer[MSTRTIME_MAX_SIZE];
    if( p_item )
    {
        p_item->i_duration = i_duration;
        if( i_duration != -1 )
        {
            secstotimestr( psz_buffer, i_duration/1000000 );
        }
        else
        {
            memcpy( psz_buffer, "--:--:--", sizeof("--:--:--") );
        }
        playlist_ItemAddInfo( p_item, _("General") , _("Duration"),
                      "%s", psz_buffer );

        return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

/**
 * Set the duration of a playlist item
 * This function must be entered with the playlist lock
 * Legacy function due to disappear (locks the whole playlist)
 *
 * \param p_playlist the playlist
 * \param i_pos the position of the item of which we change the duration
 * \param i_duration the duration to set
 * \return VLC_SUCCESS on success, VLC_EGENERIC on failure
 */
int playlist_SetDuration( playlist_t *p_playlist, int i_pos, mtime_t i_duration )
{
    vlc_value_t val;
    playlist_item_t *p_item;
    if( !p_playlist )
    {
        return VLC_ENOOBJ;
    }

    p_item = playlist_ItemGetByPos( p_playlist , i_pos );
    if( !p_item )
    {
        return VLC_ENOOBJ;
    }

    vlc_mutex_lock( &p_item->lock );
    playlist_ItemSetDuration( p_item , i_duration );
    vlc_mutex_unlock( &p_item->lock );

    val.b_bool = (i_pos >= 0 && i_pos < p_playlist->i_size ) ? i_pos : -1;
    var_Set( p_playlist, "item-change", val );

    return VLC_SUCCESS;
}

/**********************************************************************
 * Actions on existing playlist items
 **********************************************************************/


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
    int i,j;
    int i_delay=0;

    /* if i_pos is the current played item, playlist should stop playing it */
    if( ( p_playlist->i_status == PLAYLIST_RUNNING) &&
                    (p_playlist->i_index == i_pos) )
    {
        playlist_Command( p_playlist, PLAYLIST_STOP, 0 );
    }

    vlc_mutex_lock( &p_playlist->object_lock );
    if( i_pos >= 0 && i_pos < p_playlist->i_size )
    {
        playlist_item_t *p_item = p_playlist->pp_items[i_pos];

        msg_Dbg( p_playlist, "deleting playlist item « %s »",
                 p_item->psz_name );
#if 0
        vlc_mutex_lock( &p_item->lock );

        if( p_item->psz_name )
        {
            free( p_item->psz_name );
        }
        if( p_item->psz_uri )
        {
            free( p_item->psz_uri );
        }

        /* Free the info categories. Welcome to the segfault factory */
        if( p_item->i_categories > 0 )
        {
            for( i = 0; i < p_item->i_categories; i++ )
            {
                for( j= 0 ; j < p_item->pp_categories[i]->i_infos; j++)
                {
                    if( p_item->pp_categories[i]->pp_infos[j]->psz_name)
                    {
                        free( p_item->pp_categories[i]->
                                      pp_infos[j]->psz_name);
                    }
                    if( p_item->pp_categories[i]->pp_infos[j]->psz_value)
                    {
                        free( p_item->pp_categories[i]->
                                      pp_infos[j]->psz_value);
                    }
                    free( p_item->pp_categories[i]->pp_infos[j] );
                }
                if( p_item->pp_categories[i]->i_infos )
                    free( p_item->pp_categories[i]->pp_infos );

                if( p_item->pp_categories[i]->psz_name)
                {
                    free( p_item->pp_categories[i]->psz_name );
                }
                free( p_item->pp_categories[i] );
            }
            free( p_item->pp_categories );
        }

        /* XXX: what if the item is still in use? */
#endif
        playlist_ItemDelete( p_item );
#if 0
        free( p_item );
#endif
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
 * Clear all playlist items
 *
 * \param p_playlist the playlist to be cleared.
 * \return returns 0
 */
int playlist_Clear( playlist_t * p_playlist ) {

    while( p_playlist->i_groups > 0 )
    {
        playlist_DeleteGroup( p_playlist, p_playlist->pp_groups[0]->i_id );
    }

    while( p_playlist->i_size > 0 )
    {
        playlist_Delete( p_playlist, 0 );
    }

    p_playlist->i_index = -1;
    p_playlist->i_size = 0;
    p_playlist->pp_items = NULL;

    p_playlist->i_groups = 0;
    p_playlist->pp_groups = NULL;

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

    val.b_bool = i_pos;
    var_Set( p_playlist, "item-change", val );

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

    val.b_bool = i_pos;
    var_Set( p_playlist, "item-change", val );

    return 0;
}

/**
 * Disables a playlist group
 *
 * \param p_playlist the playlist to disable from.
 * \param i_group the id of the group to disable
 * \return returns 0
 */
int playlist_DisableGroup( playlist_t * p_playlist, int i_group)
{
    vlc_value_t     val;
    int i;
    vlc_mutex_lock( &p_playlist->object_lock );

    msg_Dbg(p_playlist,"disabling group %i",i_group);
    for( i = 0 ; i< p_playlist->i_size; i++ )
    {
        if( p_playlist->pp_items[i]->i_group == i_group )
        {
            msg_Dbg( p_playlist, "disabling playlist item « %s »",
                           p_playlist->pp_items[i]->psz_name );

            if( p_playlist->pp_items[i]->b_enabled == VLC_TRUE )
                p_playlist->i_enabled--;

            p_playlist->pp_items[i]->b_enabled = VLC_FALSE;
            val.b_bool = i;
            var_Set( p_playlist, "item-change", val );
        }
    }
    vlc_mutex_unlock( &p_playlist->object_lock );

    return 0;
}

/**
 * Enables a playlist group
 *
 * \param p_playlist the playlist to enable from.
 * \param i_group the id of the group to enable
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
            val.b_bool = i;
            var_Set( p_playlist, "item-change", val );
        }
    }
    vlc_mutex_unlock( &p_playlist->object_lock );

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
 * \return returns VLC_SUCCESS
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

    return VLC_SUCCESS;
}
