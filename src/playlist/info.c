/*****************************************************************************
 * info.c : Playlist info management
 *****************************************************************************
 * Copyright (C) 1999-2004 VideoLAN
 * $Id: info.c,v 1.5 2004/01/17 16:24:14 gbazin Exp $
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
 * Get one special info
 *
 * \param p_playlist the playlist to get the info from
 * \param i_item the item on which we want the info ( -1 for current )
 * \param psz_cat the category in which the info is stored
 * \param psz_name the name of the info
 * \return the info value if any, NULL else
*/
char * playlist_GetInfo( playlist_t *p_playlist, int i_item,
                      const char * psz_cat, const char *psz_name )
{
    /* Check the existence of the playlist */
    if( p_playlist == NULL)
    {
        return strdup("");
    }
    /* Get a correct item */
    if( i_item >= 0 && i_item < p_playlist->i_size )
    {
    }
    else if( p_playlist->i_size > 0 )
    {
        i_item = p_playlist->i_index;
    }
    else
    {
        return strdup("");
    }
    return playlist_GetItemInfo( p_playlist->pp_items[i_item] , psz_cat,
                                 psz_name );
}

/**
 *  Get one special info, from an item (no need for p_playlist)
 *
 * \param p_item the item on which we want the info
 * \param psz_cat the category in which the info is stored
 * \param psz_name the name of the info
 * \return the info value if any, NULL else
*/
char * playlist_GetItemInfo( playlist_item_t *p_item,
                      const char * psz_cat, const char *psz_name )
{
     int i,j ;
     for( i = 0 ; i< p_item->i_categories ; i++ )
     {
         if( !strcmp( p_item->pp_categories[i]->psz_name , psz_cat ) )
         {
             for( j = 0 ; j< p_item->pp_categories[i]->i_infos ; j++ )
             {
                 if( !strcmp( p_item->pp_categories[i]->pp_infos[j]->psz_name,
                                         psz_name ) )
                 {
                     return
                     strdup(p_item->pp_categories[i]->pp_infos[j]->psz_value );
                 }
             }
         }
     }
     return strdup("");
}

/**
 * Get one info category. Creates it if it does not exist
 *
 * \param p_playlist the playlist to get the info from
 * \param i_item the item on which we want the info ( -1 for current )
 * \param psz_cat the category we want
 * \return the info category.
 */
item_info_category_t *
playlist_GetCategory( playlist_t *p_playlist, int i_item,
                      const char * psz_cat )
{
    /* Check the existence of the playlist */
    if( p_playlist == NULL)
    {
        return NULL;
    }

    /* Get a correct item */
    if( i_item >= 0 && i_item < p_playlist->i_size )
    {
    }
    else if( p_playlist->i_size > 0 )
    {
        i_item = p_playlist->i_index;
    }
    else
    {
        return NULL;
    }

    return playlist_GetItemCategory( p_playlist->pp_items[i_item] , psz_cat );
}

/**
 * Get one info category (no p_playlist). Creates it if it does not exist
 *
 * \param p_item the playlist to search categories in
 * \param psz_cat the category we want
 * \return the info category.
 */
item_info_category_t *playlist_GetItemCategory( playlist_item_t *p_item,
                                                const char *psz_cat )
{
    int i;
    /* Search the category */
    for( i = 0 ; i< p_item->i_categories ; i++ )
    {
        if( !strncmp( p_item->pp_categories[i]->psz_name , psz_cat,
                                strlen(psz_cat) ) )
        {
            return p_item->pp_categories[i];
        }
    }

    /* We did not find the category, create it */
    return playlist_CreateItemCategory( p_item, psz_cat );
}


/**
 * Create one info category.
 *
 * \param p_playlist the playlist to get the info from
 * \param i_item the item on which we want the info ( -1 for current )
 * \param psz_cat the category we want to create
 * \return the info category.
 */
item_info_category_t *
playlist_CreateCategory( playlist_t *p_playlist, int i_item,
                      const char * psz_cat )
{
    playlist_item_t *p_item = NULL;

    /* Check the existence of the playlist */
    if( p_playlist == NULL)
    {
        return NULL;
    }

    /* Get a correct item */
    if( i_item >= 0 && i_item < p_playlist->i_size )
    {
        p_item = p_playlist->pp_items[i_item];
    }
    else if( p_playlist->i_size > 0 )
    {
        p_item = p_playlist->pp_items[p_playlist->i_index];
    }
    else
    {
        return NULL;
    }

    return playlist_CreateItemCategory( p_item, psz_cat );
}

/**
 * Create one info category for an item ( no p_playlist required )
 *
 * \param p_playlist the playlist to get the info from
 * \param i_item the item on which we want the info ( -1 for current )
 * \param psz_cat the category we want to create
 * \return the info category.
 */
item_info_category_t *
playlist_CreateItemCategory( playlist_item_t *p_item, const char *psz_cat )
{
    item_info_category_t *p_cat;
    int i;
    for( i = 0 ; i< p_item->i_categories ; i++)
    {
        if( !strcmp( p_item->pp_categories[i]->psz_name,psz_cat ) )
        {
            return p_item->pp_categories[i];
        }
    }
    if( ( p_cat = malloc( sizeof( item_info_category_t) ) ) == NULL )
    {
        return NULL;
    }

    p_cat->psz_name = strdup( psz_cat);
    p_cat->i_infos = 0;
    p_cat->pp_infos = NULL;

    INSERT_ELEM( p_item->pp_categories ,
                 p_item->i_categories ,
                 p_item->i_categories ,
                 p_cat );

    return p_cat;
}

/**
 * Add an info item
 *
 * \param p_playlist the playlist to get the info from
 * \param i_item the item on which we want the info ( -1 for current )
 * \param psz_cat the category we want to put the info into
 *     (gets created if needed)
 * \return the info category.
 */
int playlist_AddInfo( playlist_t *p_playlist, int i_item,
                      const char * psz_cat, const char *psz_name,
                      const char * psz_format, ...)
{
    va_list args;
    int i_ret;
    playlist_item_t *p_item;
    char *psz_value;

    /* Check the existence of the playlist */
    if( p_playlist == NULL)
    {
        return -1;
    }

    /* Get a correct item */
    if( i_item >= 0 && i_item < p_playlist->i_size )
    {
        p_item = p_playlist->pp_items[i_item];
    }
    else if( p_playlist->i_size > 0 )
    {
        p_item = p_playlist->pp_items[p_playlist->i_index];
    }
    else
    {
        return -1;
    }

    va_start( args, psz_format );
    vasprintf( &psz_value, psz_format, args );
    va_end( args );

    i_ret = playlist_AddItemInfo( p_item , psz_cat , psz_name , psz_value );

    free( psz_value );
    return i_ret;
}


/**
 *  Add info to one item ( no need for p_playlist )
 *
 * \param p_item the item on which we want the info
 * \param psz_cat the category in which the info is stored (must exist !)
 * \param psz_name the name of the info
 * \return the info value if any, NULL else
*/
int playlist_AddItemInfo( playlist_item_t *p_item,
                      const char *psz_cat, const char *psz_name,
                      const char *psz_format, ... )
{
    va_list args;
    int i;
    int i_new = VLC_TRUE;
    item_info_t *p_info = NULL;
    item_info_category_t *p_cat;

    /* Find or create the category */
    p_cat = playlist_GetItemCategory( p_item, psz_cat );
    if( p_cat == NULL)
    {
        return -1;
    }

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
        if( ( p_info = malloc( sizeof( item_info_t) ) ) == NULL )
        {
            return -1;
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
        INSERT_ELEM( p_cat->pp_infos,
                     p_cat->i_infos,
                     p_cat->i_infos,
                     p_info );
    }

    return 0;
}

/**
 * Add a special info : option
 *
 * \param p_playlist the playlist to get the info from
 * \param i_item the item on which we want the info ( -1 for current )
 * \param psz_value the option to add
 * \return the info category.
 */
int playlist_AddOption( playlist_t *p_playlist, int i_item,
                        const char * psz_format, ...)
{
    va_list args;
    item_info_t *p_info = NULL;
    item_info_category_t *p_cat;

    /* Check the existence of the playlist */
    if( p_playlist == NULL)
    {
        return -1;
    }

    /* Get a correct item */
    if( i_item >= 0 && i_item < p_playlist->i_size )
    {
    }
    else if( p_playlist->i_size > 0 )
    {
        i_item = p_playlist->i_index;
    }
    else
    {
        return -1;
    }

    p_cat = playlist_GetCategory( p_playlist, i_item , "Options" );

    if( p_cat == NULL)
    {
        return -1;
    }

    if( ( p_info = malloc( sizeof( item_info_t) ) ) == NULL )
    {
        msg_Err( p_playlist, "out of memory" );
        return -1;
    }

    p_info->psz_name = strdup( "option" );

    va_start( args, psz_format );
    vasprintf( &p_info->psz_value, psz_format, args );
    va_end( args );

    INSERT_ELEM( p_cat->pp_infos, p_cat->i_infos, p_cat->i_infos, p_info );

    return 0;
}

/**
 *  Add a option to one item ( no need for p_playlist )
 *
 * \param p_item the item on which we want the info
 * \param psz_format the option
 * \return 0 on success
*/
int playlist_AddItemOption( playlist_item_t *p_item,
                            const char *psz_format, ... )
{
    va_list args;
    item_info_t *p_info = NULL;
    item_info_category_t *p_cat;

    p_cat = playlist_GetItemCategory( p_item, "Options" );
    if( p_cat == NULL)
    {
        return -1;
    }

    if( ( p_info = malloc( sizeof( item_info_t) ) ) == NULL )
    {
        return -1;
    }

    p_info->psz_name = strdup( "option" );

    va_start( args, psz_format );
    vasprintf( &p_info->psz_value, psz_format, args );
    va_end( args );

    INSERT_ELEM( p_cat->pp_infos, p_cat->i_infos, p_cat->i_infos, p_info );

    return 0;
}
