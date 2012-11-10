/*****************************************************************************
 * media_pool.c : Media pool for watching system
 *****************************************************************************
 * Copyright (C) 2009-2010 the VideoLAN team and AUTHORS
 * $Id$
 *
 * Authors: Srikanth Raju <srikiraju at gmail dot com>
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

#define mp_foreachlist( a, b ) for( ml_poolobject_t* b = a; b; b = b->p_next )

static inline int mediapool_hash( int media_id )
{
    return media_id % ML_MEDIAPOOL_HASH_LENGTH;
}

/**
 * @brief Get a media from the pool
 * @param p_ml ML object
 * @param media_id The media id of the object to get
 * @return the found media or NULL if not found
 */
ml_media_t* pool_GetMedia( media_library_t* p_ml, int media_id )
{
    vlc_mutex_lock( &p_ml->p_sys->pool_mutex );
    ml_media_t* p_media = NULL;
    mp_foreachlist( p_ml->p_sys->p_mediapool[ mediapool_hash( media_id ) ], p_item )
    {
        if( p_item->p_media->i_id == media_id )
        {
            p_media = p_item->p_media;
            break;
        }
    }
    if( p_media )
        ml_gc_incref( p_media );
    vlc_mutex_unlock( &p_ml->p_sys->pool_mutex );
    return p_media;
}

/**
 * @brief Insert a media into the media pool
 * @param p_ml ML object
 * @param p_media Media object to insert
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
int pool_InsertMedia( media_library_t* p_ml, ml_media_t* p_media, bool locked )
{
    if( !locked )
        ml_LockMedia( p_media );
    assert( p_media );
    assert( p_media->i_id > 0 );
    if( p_media->ml_gc_data.pool )
    {
        msg_Dbg( p_ml, "Already in pool! %s %d", p_media->psz_uri, p_media->i_id );
        ml_UnlockMedia( p_media );
        return VLC_EGENERIC;
    }
    p_media->ml_gc_data.pool = true;
    int i_ret = VLC_SUCCESS;
    vlc_mutex_lock( &p_ml->p_sys->pool_mutex );
    mp_foreachlist( p_ml->p_sys->p_mediapool[ (mediapool_hash(p_media->i_id)) ], p_item )
    {
        if( p_media == p_item->p_media )
        {
            i_ret = VLC_EGENERIC;
            break;
        }
        else if( p_media->i_id == p_item->p_media->i_id )
        {
            i_ret = VLC_EGENERIC;
            msg_Warn( p_ml, "A media of the same id was found, but in different objects!" );
            break;
        }
    }
    if( i_ret == VLC_SUCCESS )
    {
        ml_poolobject_t* p_new = ( ml_poolobject_t * ) calloc( 1, sizeof( ml_poolobject_t* ) );
        if( !p_new )
            i_ret = VLC_EGENERIC;
        else
        {
            ml_gc_incref( p_media );
            p_new->p_media = p_media;
            p_new->p_next = p_ml->p_sys->p_mediapool[ ( mediapool_hash( p_media->i_id ) ) ];
            p_ml->p_sys->p_mediapool[ ( mediapool_hash( p_media->i_id ) ) ] = p_new;
        }
    }
    vlc_mutex_unlock( &p_ml->p_sys->pool_mutex );
    if( !locked )
        ml_UnlockMedia( p_media );
    return i_ret;
}

/**
 * @brief Perform a single garbage collection scan on the media pool
 * @param p_ml The ML object
 * @note Scans all media and removes any medias not held by any other objects.
 */
void pool_GC( media_library_t* p_ml )
{
    vlc_mutex_lock( &p_ml->p_sys->pool_mutex );
    ml_poolobject_t* p_prev = NULL;
    ml_media_t* p_media = NULL;
    for( int i_idx = 0; i_idx < ML_MEDIAPOOL_HASH_LENGTH; i_idx++ )
    {
        p_prev = NULL;
        for( ml_poolobject_t* p_item = p_ml->p_sys->p_mediapool[ i_idx ];
                p_item != NULL; p_item = p_item->p_next )
        {
            p_media = p_item->p_media;
            int refs;
            refs = p_media->ml_gc_data.refs;
            if( refs == 1 )
            {
                if( p_prev == NULL )
                    p_ml->p_sys->p_mediapool[i_idx] = p_item->p_next;
                else
                    p_prev->p_next = p_item->p_next;
                p_media->ml_gc_data.pool = false;
                ml_gc_decref( p_item->p_media );//This should destroy the object
                free( p_item );
            }
            p_prev = p_item;
        }
    }
    vlc_mutex_unlock( &p_ml->p_sys->pool_mutex );
}
