/*****************************************************************************
 * info.c : Playlist info management
 *****************************************************************************
 * Copyright (C) 1999-2004 VideoLAN
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/
#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "vlc_playlist.h"

/**
 * Get one special info
 * Must be entered with playlist lock
 *
 * \param p_playlist the playlist to get the info from
 * \param i_pos position of the item on
 *              which we want the info ( -1 for current )
 * \param psz_cat the category in which the info is stored
 * \param psz_name the name of the info
 * \return the info value if any, an empty string else
 */
char * playlist_GetInfo( playlist_t *p_playlist, int i_pos,
                         const char *psz_cat, const char *psz_name )
{
    playlist_item_t *p_item;
    char *psz_buffer;

    /* Sanity check */
    if( p_playlist == NULL) return strdup("");

    p_item = playlist_ItemGetByPos( p_playlist, i_pos );
    if( !p_item ) return strdup("");

    vlc_mutex_lock( &p_item->input.lock );
    psz_buffer = playlist_ItemGetInfo( p_item , psz_cat, psz_name );
    vlc_mutex_unlock( &p_item->input.lock );

    return psz_buffer;
}

/**
 *  Get one special info, from an item (no need for p_playlist)
 *
 * \param p_item the item on which we want the info
 * \param psz_cat the category in which the info is stored
 * \param psz_name the name of the info
 * \return the info value if any, an empty string else
*/
char * playlist_ItemGetInfo( playlist_item_t *p_item,
                             const char * psz_cat, const char *psz_name )
{
     int i, j;

     for( i = 0 ; i< p_item->input.i_categories ; i++ )
     {
         info_category_t *p_category = p_item->input.pp_categories[i];

         if( !psz_cat || strcmp( p_category->psz_name , psz_cat ) ) continue;

         for( j = 0 ; j< p_category->i_infos ; j++ )
         {
             if( !strcmp( p_category->pp_infos[j]->psz_name, psz_name) )
             {
                 return strdup( p_category->pp_infos[j]->psz_value );
             }
         }
     }
     return strdup("");
}

/**
 * Get one info category (no p_playlist). Create it if it does not exist
 *
 * \param p_item the playlist item to get the category from
 * \param psz_cat the category we want
 * \return the info category.
 */
info_category_t * playlist_ItemGetCategory( playlist_item_t *p_item,
                                            const char *psz_cat )
{
    int i;
    /* Search the category */
    for( i = 0 ; i< p_item->input.i_categories ; i++ )
    {
        if( !strncmp( p_item->input.pp_categories[i]->psz_name, psz_cat,
                      strlen(psz_cat) ) )
        {
            return p_item->input.pp_categories[i];
        }
    }

    /* We did not find the category, create it */
    return playlist_ItemCreateCategory( p_item, psz_cat );
}

/**
 * Create one info category for an item ( no p_playlist required )
 *
 * \param p_item the item to create category for
 * \param psz_cat the category we want to create
 * \return the info category.
 */
info_category_t * playlist_ItemCreateCategory( playlist_item_t *p_item,
                                               const char *psz_cat )
{
    info_category_t *p_cat;
    int i;

    for( i = 0 ; i< p_item->input.i_categories ; i++)
    {
        if( !strcmp( p_item->input.pp_categories[i]->psz_name,psz_cat ) )
        {
            return p_item->input.pp_categories[i];
        }
    }

    if( ( p_cat = malloc( sizeof( info_category_t) ) ) == NULL )
    {
        return NULL;
    }

    p_cat->psz_name = strdup( psz_cat);
    p_cat->i_infos = 0;
    p_cat->pp_infos = NULL;

    INSERT_ELEM( p_item->input.pp_categories, p_item->input.i_categories,
                 p_item->input.i_categories, p_cat );

    return p_cat;
}

/**
 * Add an info item
 *
 * \param p_playlist the playlist
 * \param i_item the position of the item on which we want
 *               the info ( -1 for current )
 * \param psz_cat the category we want to put the info into
 *                (gets created if needed)
 * \param psz_name the name of the info
 * \param psz_format printf-style info
 * \return VLC_SUCCESS
 */
int playlist_AddInfo( playlist_t *p_playlist, int i_item,
                      const char * psz_cat, const char *psz_name,
                      const char * psz_format, ...)
{
    va_list args;
    int i_ret;
    playlist_item_t *p_item;
    char *psz_value;

    /* Sanity check */
    if( p_playlist == NULL) return VLC_EGENERIC;

    p_item = playlist_ItemGetByPos( p_playlist, i_item );
    if( !p_item ) return VLC_ENOOBJ;

    va_start( args, psz_format );
    vasprintf( &psz_value, psz_format, args );
    va_end( args );

    vlc_mutex_lock( &p_item->input.lock );
    i_ret = playlist_ItemAddInfo( p_item, psz_cat, psz_name, psz_value );
    vlc_mutex_unlock( &p_item->input.lock );

    free( psz_value );
    return i_ret;
}

/**
 *  Add info to one item ( no need for p_playlist )
 *
 * \param p_item the item for which we add the info
 * \param psz_cat the category in which the info is stored
 * \param psz_name the name of the info
 * \param psz_format printf-style info
 * \return VLC_SUCCESS on success
*/
int playlist_ItemAddInfo( playlist_item_t *p_item,
                          const char *psz_cat, const char *psz_name,
                          const char *psz_format, ... )
{
    va_list args;
    int i;
    int i_new = VLC_TRUE;
    info_t *p_info = NULL;
    info_category_t *p_cat;

    /* Find or create the category */
    p_cat = playlist_ItemGetCategory( p_item, psz_cat );
    if( p_cat == NULL) return VLC_EGENERIC;

    for( i = 0 ; i< p_cat->i_infos ; i++)
    {
        if( !strcmp( p_cat->pp_infos[i]->psz_name, psz_name ) )
        {
            /* This info is not new */
            p_info = p_cat->pp_infos[i];
            i_new = VLC_FALSE;
            break;
        }
    }

    /* New info, create it */
    if( p_info == NULL )
    {
        if( ( p_info = malloc( sizeof( info_t) ) ) == NULL )
        {
            return VLC_EGENERIC;
        }
        p_info->psz_name = strdup( psz_name);
    }
    else
    {
        if( p_info->psz_value != NULL ) free( p_info->psz_value ) ;
    }

    va_start( args, psz_format );
    vasprintf( &p_info->psz_value, psz_format, args );
    va_end( args );

    /* If this is new, insert it */
    if( i_new == VLC_TRUE )
    {
        INSERT_ELEM( p_cat->pp_infos, p_cat->i_infos, p_cat->i_infos, p_info );
    }

    return VLC_SUCCESS;
}
