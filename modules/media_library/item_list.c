/*****************************************************************************
 * item_list.c: An input_item_t+media_id couple list for the media library
 *****************************************************************************
 * Copyright (C) 2008-2010 the VideoLAN Team and AUTHORS
 * $Id$
 *
 * Authors: Antoine Lejeune <phytos@videolan.org>
 *          Jean-Philippe André <jpeg@videolan.org>
 *          Rémi Duraffort <ivoire@videolan.org>
 *          Adrien Maglo <magsoft@videolan.org>
 *          Srikanth Raju <srikiraju at gmail dot com>
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

#include "sql_media_library.h"
#include "item_list.h"

/**
 * @short item hash list for the media library monitoring system
 */

/**
 * @brief Add an item to the head of the list
 * @param p_ml
 * @param p_media media object. ID must be non zero and valid
 * @param p_item input item to add, MUST NOT be NULL
 * @param locked flag set if the list is locked. do not use
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
int __item_list_add( watch_thread_t *p_wt, ml_media_t* p_media, input_item_t *p_item,
                     bool locked )
{
    if( !locked )
        vlc_mutex_lock( &p_wt->list_mutex );
    ml_LockMedia( p_media );
    assert( p_media->i_id );
    /* Ensure duplication does not occur */
    il_foreachlist( p_wt->p_hlist[ item_hash( p_item ) ], p_elt )
    {
        if( p_elt->p_item->i_id == p_item->i_id )
        {
            ml_UnlockMedia( p_media );
            if( !locked )
                vlc_mutex_unlock( &p_wt->list_mutex );
            return VLC_EGENERIC;
        }
    }

    item_list_t *p_new = ( item_list_t* ) calloc( 1, sizeof( item_list_t ) );
    if( !p_new )
    {
        ml_UnlockMedia( p_media );
        if( !locked )
            vlc_mutex_unlock( &p_wt->list_mutex );
        return VLC_ENOMEM;
    }
    p_new->p_next = p_wt->p_hlist[ item_hash( p_item ) ];
    p_new->i_refs = 1;
    p_new->i_media_id = p_media->i_id;
    p_new->p_media = p_media;
    p_new->p_item = p_item;
    p_wt->p_hlist[ item_hash( p_item ) ] = p_new;
    ml_UnlockMedia( p_media );
    if( !locked )
        vlc_mutex_unlock( &p_wt->list_mutex );
    return VLC_SUCCESS;
}

/**
 * @brief Delete an item from the list
 * @param p_ml this media library
 * @param i_media_id media library's media ID
 */
item_list_t* item_list_delMedia( watch_thread_t *p_wt, int i_media_id )
{
    vlc_mutex_lock( &p_wt->list_mutex );
    item_list_t *p_prev = NULL;
    il_foreachhashlist( p_wt->p_hlist, p_elt, ixx )
    {
        if( p_elt->i_media_id == i_media_id )
        {
            if( p_prev )
                p_prev->p_next = p_elt->p_next;
            else
                p_wt->p_hlist[ixx] = p_elt->p_next;
            p_elt->p_next = NULL;
            vlc_mutex_unlock( &p_wt->list_mutex );
            return p_elt;
        }
        else
        {
            p_prev = p_elt;
        }
    }
    vlc_mutex_unlock( &p_wt->list_mutex );
    return NULL;
}

/**
 * @brief Delete an item from the list and return the single element
 * @param p_ml this media library
 * @param p_item item to delete
 * @return The element from the list containing p_item
 */
item_list_t* item_list_delItem( watch_thread_t *p_wt, input_item_t *p_item, bool locked )
{
    if( !locked )
        vlc_mutex_lock( &p_wt->list_mutex );
    item_list_t *p_prev = NULL;
    il_foreachlist( p_wt->p_hlist[ item_hash( p_item ) ], p_elt )
    {
        if( p_elt->p_item == p_item )
        {
            if( p_prev )
                p_prev->p_next = p_elt->p_next;
            else
                p_wt->p_hlist[ item_hash( p_item ) ] = p_elt->p_next;
            p_elt->p_next = NULL;
            if( !locked )
                vlc_mutex_unlock( &p_wt->list_mutex );
            return p_elt;
        }
        else
        {
            p_prev = p_elt;
        }
    }
    if( !locked )
        vlc_mutex_unlock( &p_wt->list_mutex );
    return NULL;
}

/**
 * @brief Find an input item
 * @param p_ml this media library
 * @param i_media_id item to find and gc_incref
 * @return input item if found, NULL if not found
 */
input_item_t* item_list_itemOfMediaId( watch_thread_t *p_wt, int i_media_id )
{
    item_list_t* p_tmp = item_list_listitemOfMediaId( p_wt, i_media_id );
    if( p_tmp )
        return p_tmp->p_item;
    else
        return NULL;
}

/**
 * @brief Find an item list item
 * @param p_ml this media library
 * @param i_media_id item to find and gc_incref
 * @return input item if found, NULL if not found
 */
item_list_t* item_list_listitemOfMediaId( watch_thread_t *p_wt, int i_media_id )
{
    vlc_mutex_lock( &p_wt->list_mutex );
    il_foreachhashlist( p_wt->p_hlist, p_elt, ixx )
    {
        if( p_elt->i_media_id == i_media_id )
        {
            p_elt->i_age = 0;
            vlc_mutex_unlock( &p_wt->list_mutex );
            return p_elt;
        }
    }
    vlc_mutex_unlock( &p_wt->list_mutex );
    return NULL;
}

/**
 * @brief Find a media
 * @param p_ml this media library
 * @param i_media_id item to find and gc_incref
 * @return media if found. NULL otherwise
 */
ml_media_t* item_list_mediaOfMediaId( watch_thread_t *p_wt, int i_media_id )
{
    item_list_t* p_tmp = item_list_listitemOfMediaId( p_wt, i_media_id );
    if( p_tmp )
        return p_tmp->p_media;
    else
        return NULL;
}

/**
 * @brief Find a media ID by its input_item
 * @param p_ml this media library
 * @param p_item item to find
 * @return media_id found, or VLC_EGENERIC
 */
int item_list_mediaIdOfItem( watch_thread_t *p_wt, input_item_t *p_item )
{
    vlc_mutex_lock( &p_wt->list_mutex );
    il_foreachlist( p_wt->p_hlist[ item_hash( p_item ) ], p_elt )
    {
        if( p_elt->p_item == p_item )
        {
            if( p_elt->i_media_id <= 0 )
                /* TODO! */
            p_elt->i_age = 0;
            vlc_mutex_unlock( &p_wt->list_mutex );
            return p_elt->i_media_id;
        }
    }
    vlc_mutex_unlock( &p_wt->list_mutex );
    return VLC_EGENERIC;
}

/**
 * @brief Find a media by its input_item
 * @param p_ml this media library
 * @param p_item item to find
 * @return media found, or VLC_EGENERIC
 */
ml_media_t* item_list_mediaOfItem( watch_thread_t *p_wt, input_item_t* p_item,
        bool locked )
{
    if( !locked )
        vlc_mutex_lock( &p_wt->list_mutex );
    il_foreachlist( p_wt->p_hlist[ item_hash( p_item ) ], p_elt )
    {
        if( p_elt->p_item == p_item )
        {
            p_wt->p_hlist[ item_hash( p_item ) ] = p_elt->p_next;
            p_elt->p_next = NULL;
            if( !locked )
                vlc_mutex_unlock( &p_wt->list_mutex );
            return p_elt->p_media;
        }
    }
    if( !locked )
        vlc_mutex_unlock( &p_wt->list_mutex );
    return NULL;
}
/**
 * @brief Flag an item as updated
 * @param p_ml this media library
 * @param p_item item to find and flag
 * @param b_played raise play count or not, update last play
 * @return media_id found, or VLC_EGENERIC
 */
int item_list_updateInput( watch_thread_t *p_wt, input_item_t *p_item,
                           bool b_played )
{
    vlc_mutex_lock( &p_wt->list_mutex );
    il_foreachlist( p_wt->p_hlist[ item_hash( p_item ) ], p_elt )
    {
        if( p_elt->p_item == p_item )
        {
            /* Item found, flag and return */
            p_elt->i_age = 0;
            p_elt->i_update |= b_played ? 3 : 1;
            vlc_mutex_unlock( &p_wt->list_mutex );
            return p_elt->i_media_id;
        }
    }
    vlc_mutex_unlock( &p_wt->list_mutex );
    return VLC_EGENERIC;
}

/**
 * @brief Free every item in the item list.
 * @param p_wt Watch thread
 * @note All decref of objects must be handled by watch system
 */
void item_list_destroy( watch_thread_t* p_wt )
{
    vlc_mutex_lock( &p_wt->list_mutex );
    for( int i = 0; i < ML_ITEMLIST_HASH_LENGTH ; i++ )
    {
        for( item_list_t* p_elt = p_wt->p_hlist[i] ; p_elt; p_elt = p_wt->p_hlist[i] )
        {
            p_wt->p_hlist[i] = p_elt->p_next;
            free( p_elt );
        }
    }
    vlc_mutex_unlock( &p_wt->list_mutex );
}
