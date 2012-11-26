/*****************************************************************************
 * ml_watch.c: SQL-based media library: Medias watching system
 *****************************************************************************
 * Copyright (C) 2008-2010 the VideoLAN team and AUTHORS
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
#include <vlc_events.h>

static void watch_ItemChange( const vlc_event_t *, void * );
static int watch_PlaylistItemCurrent( vlc_object_t *p_this, char const *psz_var,
                                  vlc_value_t oldval, vlc_value_t newval,
                                  void *data );
static int watch_PlaylistItemAppend( vlc_object_t *p_this, char const *psz_var,
                                  vlc_value_t oldval, vlc_value_t newval,
                                  void *data );
static int watch_PlaylistItemDeleted( vlc_object_t *p_this, char const *psz_var,
                                  vlc_value_t oldval, vlc_value_t newval,
                                  void *data );
static void watch_loop( media_library_t *p_ml, bool b_force );
static void watch_Thread_Cleanup( void* p_object );
static int watch_update_Item( media_library_t *p_ml, int i_media_id,
                       input_item_t *p_item, bool b_raise_count, bool locked );
static void watch_ProcessAppendQueue( media_library_t* p_ml );

/**
 * @brief Watching thread
 */
static void* watch_Thread( void *obj )
{
    watch_thread_t *p_watch = ( watch_thread_t* )obj;
    media_library_t *p_ml = p_watch->p_ml;
    int i_ret = 0;

    vlc_mutex_lock( &p_watch->lock );
    vlc_cleanup_push( watch_Thread_Cleanup, p_ml );
    for( ;; )
    {
        watch_loop( p_ml, !i_ret );
        i_ret = vlc_cond_timedwait( &p_watch->cond, &p_watch->lock,
                                mdate() + 1000000 * THREAD_SLEEP_DELAY );
    }
    vlc_cleanup_run();
    return NULL;
}

/**
 * @brief Callback for thread exit
 */
static void watch_Thread_Cleanup( void* p_object )
{
    media_library_t* p_ml = ( media_library_t* )p_object;
    watch_loop( p_ml, true );
    vlc_mutex_unlock( &p_ml->p_sys->p_watch->lock );
}
/**
 * @brief Init watching system
 * @return Error if the object or the thread could not be created
 */
int watch_Init( media_library_t *p_ml )
{
    /* init and launch watching thread */
    p_ml->p_sys->p_watch = calloc( 1, sizeof(*p_ml->p_sys->p_watch) );
    if( !p_ml->p_sys->p_watch )
        return VLC_ENOMEM;

    watch_thread_t* p_wt = p_ml->p_sys->p_watch;
    vlc_mutex_init( &p_wt->list_mutex );
    p_wt->p_ml = p_ml;

    vlc_cond_init( &p_wt->cond );
    vlc_mutex_init( &p_wt->lock );

    /* Initialise item append queue */
    vlc_mutex_init( &p_wt->item_append_queue_lock );
    p_wt->item_append_queue = NULL;
    p_wt->item_append_queue_count = 0;

    if( vlc_clone( &p_wt->thread, watch_Thread, p_wt, VLC_THREAD_PRIORITY_LOW ) )
    {
        msg_Dbg( p_ml, "unable to launch the auto-updating thread" );
        free( p_wt );
        return VLC_EGENERIC;
    }

    /* Wait on playlist events
     * playlist-item-append -> entry to playlist
     * activity -> to ensure that we catch played item only!
     * playlist-item-deleted -> exit from playlist
     * item-change -> Currently not required, as we monitor input_item events
     */
    playlist_t *p_pl = pl_Get( p_ml );
    var_AddCallback( p_pl, "activity", watch_PlaylistItemCurrent, p_ml );
    var_AddCallback( p_pl, "playlist-item-append", watch_PlaylistItemAppend, p_ml );
    var_AddCallback( p_pl, "playlist-item-deleted", watch_PlaylistItemDeleted, p_ml );

    return VLC_SUCCESS;
}

/**
 * @brief Add the input to the watch system
 * @param p_ml The Media Library Object
 * @param p_item Item to be watched
 * @param p_media Corresponding media item to sync with
 * @param locked Status of item list lock
 * @return VLC_SUCCESS or error code
 */
int __watch_add_Item( media_library_t *p_ml, input_item_t *p_item,
                        ml_media_t* p_media, bool locked )
{
    vlc_gc_incref( p_item );
    ml_gc_incref( p_media );
    int i_ret = __item_list_add( p_ml->p_sys->p_watch, p_media, p_item, locked );
    if( i_ret != VLC_SUCCESS )
        return i_ret;
    vlc_event_manager_t *p_em = &p_item->event_manager;
    vlc_event_attach( p_em, vlc_InputItemMetaChanged, watch_ItemChange, p_ml );
    vlc_event_attach( p_em, vlc_InputItemNameChanged, watch_ItemChange, p_ml );
    vlc_event_attach( p_em, vlc_InputItemInfoChanged, watch_ItemChange, p_ml );
    /*
    Note: vlc_InputItemDurationChanged is disabled because
          it is triggered too often, even without consequent changes
    */
    return VLC_SUCCESS;
}


/**
 * @brief Detach event manager
 * @param p_ml The Media Library Object
 */
static void detachItemEvents( media_library_t *p_ml, input_item_t *p_item )
{
    vlc_event_manager_t *p_em = &p_item->event_manager;
    vlc_event_detach( p_em, vlc_InputItemMetaChanged, watch_ItemChange, p_ml );
    vlc_event_detach( p_em, vlc_InputItemNameChanged, watch_ItemChange, p_ml );
    vlc_event_detach( p_em, vlc_InputItemInfoChanged, watch_ItemChange, p_ml );
}


/**
 * @brief Close the watching system
 * @param p_ml The Media Library Object
 */
void watch_Close( media_library_t *p_ml )
{
    playlist_t *p_pl = pl_Get( p_ml );
    var_DelCallback( p_pl, "playlist-item-deleted", watch_PlaylistItemDeleted, p_ml );
    var_DelCallback( p_pl, "playlist-item-append", watch_PlaylistItemAppend, p_ml );
    var_DelCallback( p_pl, "activity", watch_PlaylistItemCurrent, p_ml );

    /* Flush item list */
    il_foreachhashlist( p_ml->p_sys->p_watch->p_hlist, p_elt, ixx )
    {
        detachItemEvents( p_ml, p_elt->p_item );
        ml_gc_decref( p_elt->p_media );
        vlc_gc_decref( p_elt->p_item );
    }
    item_list_destroy( p_ml->p_sys->p_watch );

    /* Stop the watch thread and join in */
    vlc_cancel( p_ml->p_sys->p_watch->thread );
    vlc_join( p_ml->p_sys->p_watch->thread, NULL );

    /* Clear up other stuff */
    vlc_mutex_destroy( &p_ml->p_sys->p_watch->lock );
    vlc_cond_destroy( &p_ml->p_sys->p_watch->cond );
    vlc_mutex_destroy( &p_ml->p_sys->p_watch->list_mutex );
    free( p_ml->p_sys->p_watch );

    free( p_ml->p_sys->p_watch->item_append_queue );
    vlc_mutex_destroy( &p_ml->p_sys->p_watch->item_append_queue_lock );
    p_ml->p_sys->p_watch = NULL;
}

/**
 * @brief Del item that is currently being watched
 * @param p_ml The Media Library Object
 * @param p_item Item to stop watching
 * @param locked Lock state of item list
 */
int __watch_del_Item( media_library_t* p_ml, input_item_t* p_item, bool locked )
{
    assert( p_item );
    item_list_t* p_tmp = item_list_delItem( p_ml->p_sys->p_watch, p_item, locked );
    if( p_tmp == NULL )
        return VLC_EGENERIC;
    detachItemEvents( p_ml, p_tmp->p_item );
    vlc_gc_decref( p_tmp->p_item );
    ml_gc_decref( p_tmp->p_media );
    free( p_tmp );
    return VLC_SUCCESS;
}

/**
 * @brief Del media from watching by ID
 * @param p_ml The Media Library Object
 * @param i_media_id Media ID
 */
int watch_del_MediaById( media_library_t* p_ml, int i_media_id )
{
    assert( i_media_id > 0 );
    item_list_t* p_elt = item_list_delMedia( p_ml->p_sys->p_watch, i_media_id );
    if( p_elt == NULL )
        return VLC_EGENERIC;
    detachItemEvents( p_ml, p_elt->p_item );
    vlc_gc_decref( p_elt->p_item );
    ml_gc_decref( p_elt->p_media );
    free( p_elt );
    return VLC_SUCCESS;
}

/**
 * @brief Get item using media id, if exists in item list
 * @param p_ml The Media Library Object
 * @param i_media_id Media ID
 */
input_item_t* watch_get_itemOfMediaId( media_library_t *p_ml, int i_media_id )
{
    input_item_t* p_tmp = item_list_itemOfMediaId( p_ml->p_sys->p_watch, i_media_id );
    if( p_tmp == NULL )
        return NULL;
    vlc_gc_incref( p_tmp );
    return p_tmp;
}

/**
 * @brief Get media using media id, if exists in item list
 * @param p_ml The Media Library Object
 * @param i_media_id Media ID
 */
ml_media_t* watch_get_mediaOfMediaId( media_library_t* p_ml, int i_media_id )
{
    ml_media_t* p_tmp = item_list_mediaOfMediaId( p_ml->p_sys->p_watch, i_media_id );
    if( p_tmp == NULL )
        return NULL;
    ml_gc_incref( p_tmp );
    return p_tmp;
}

/**
 * @brief Get mediaid of existing item
 * @param p_ml The Media Library Object
 * @param p_item Pointer to input item
 */
int watch_get_mediaIdOfItem( media_library_t *p_ml, input_item_t *p_item )
{
    return item_list_mediaIdOfItem( p_ml->p_sys->p_watch, p_item );
}

/**
 * @brief Updates a media each time it is changed (name, info or meta)
 */
static void watch_ItemChange( const vlc_event_t *event, void *data )
{
    input_item_t *p_item = ( input_item_t* ) event->p_obj;
    media_library_t *p_ml = ( media_library_t* ) data;
    /* Note: we don't add items to the item_list, but normally there should
       not be any item at this point that is not in the list. */
    if( item_list_updateInput( p_ml->p_sys->p_watch, p_item, false ) <= 0 )
    {
#ifndef NDEBUG
        msg_Dbg( p_ml, "Couldn't update in watch_ItemChange(): (%s:%d)",
                 __FILE__, __LINE__ );
#endif
    }

    /*
    if( event->type == vlc_InputItemMetaChanged )
    {
        int id = item_list_mediaIdOfItem( p_ml->p_sys->p_watch, p_item );
        if( !id ) return;

        * Tell the world what happened *
        var_SetInteger( p_ml, "media-meta-change", id );
    }
    */
}

/**
 * @brief Callback when item is added to playlist
 */
static int watch_PlaylistItemAppend( vlc_object_t *p_this, char const *psz_var,
                                  vlc_value_t oldval, vlc_value_t newval,
                                  void *data )
{
    VLC_UNUSED( oldval );
    VLC_UNUSED( p_this );
    VLC_UNUSED( psz_var );
    media_library_t* p_ml = ( media_library_t* ) data;
    playlist_t* p_playlist = pl_Get( p_ml );
    playlist_add_t* p_add;
    p_add = ( playlist_add_t* ) newval.p_address;
    playlist_item_t* p_pitem = playlist_ItemGetById( p_playlist, p_add->i_item );
    input_item_t* p_item = p_pitem->p_input;
    watch_thread_t* p_wt = p_ml->p_sys->p_watch;

    vlc_mutex_lock( &p_wt->list_mutex );
    /* Check if we are already watching this item */
    il_foreachlist( p_wt->p_hlist[ item_hash( p_item ) ], p_elt )
    {
        if( p_elt->p_item->i_id == p_item->i_id )
        {
            p_elt->i_refs++;
            vlc_mutex_unlock( &p_wt->list_mutex );
            goto quit_playlistitemappend;
        }
    }
    vlc_mutex_unlock( &p_wt->list_mutex );

    /* Add the the append queue */
    vlc_mutex_lock( &p_wt->item_append_queue_lock );
    p_wt->item_append_queue_count++;
    p_wt->item_append_queue = realloc( p_wt->item_append_queue,
            sizeof( input_item_t* ) * p_wt->item_append_queue_count );
    vlc_gc_incref( p_item );
    p_wt->item_append_queue[ p_wt->item_append_queue_count - 1 ] = p_item;
    vlc_mutex_unlock( &p_wt->item_append_queue_lock );
quit_playlistitemappend:
    return VLC_SUCCESS;
}

/**
 * @brief Callback when item is deleted from playlist
 */
static int watch_PlaylistItemDeleted( vlc_object_t *p_this, char const *psz_var,
                                  vlc_value_t oldval, vlc_value_t newval,
                                  void *data )
{
    VLC_UNUSED( oldval );
    VLC_UNUSED( p_this );
    VLC_UNUSED( psz_var );
    media_library_t* p_ml = ( media_library_t* ) data;
    playlist_t* p_playlist = pl_Get( p_ml );

    /* Luckily this works, because the item isn't deleted from PL, yet */
    playlist_item_t* p_pitem = playlist_ItemGetById( p_playlist, newval.i_int );
    input_item_t* p_item = p_pitem->p_input;

    /* Find the new item and decrement its ref */
    il_foreachlist( p_ml->p_sys->p_watch->p_hlist[ item_hash( p_item ) ], p_elt )
    {
        if( p_elt->p_item->i_id == p_item->i_id )
        {
            p_elt->i_refs--;
            break;
        }
    }

    return VLC_SUCCESS;
}
/**
 * @brief Callback when watched input item starts playing
 * @note This will update playcount mainly
 * TODO: Increment playcount on playing 50%(configurable)
 */
static int watch_PlaylistItemCurrent( vlc_object_t *p_this, char const *psz_var,
                                  vlc_value_t oldval, vlc_value_t newval,
                                  void *data )
{
    (void)p_this;
    (void)oldval;
    (void)newval;
    media_library_t *p_ml = ( media_library_t* ) data;
    input_item_t *p_item = NULL;

    /* Get current input */
    input_thread_t *p_input = pl_CurrentInput( p_ml );
    p_item = p_input ? input_GetItem( p_input ) : NULL;

    if( p_input )
        vlc_object_release( p_input );

    if( !p_item )
        return VLC_EGENERIC;

    if( item_list_updateInput( p_ml->p_sys->p_watch, p_item, true ) == 0 )
    {
#ifndef NDEBUG
        msg_Dbg( p_ml, "couldn't in watch_PlaylistItemCurrent(): (%s:%d)",
                 __FILE__, __LINE__ );
#endif
    }

    return VLC_SUCCESS;
}

/**
 * @brief Update information in the DB for an input item
 *
 * @param p_ml this media library instance
 * @param i_media_id may be 0 (but not recommended)
 * @param p_item input item that was updated
 * @param b_raise_count increment the played count
 * @return result of UpdateMedia()
 */
static int watch_update_Item( media_library_t *p_ml,
                       int i_media_id, input_item_t *p_item,
                       bool b_raise_count, bool locked )
{
#ifndef NDEBUG
    msg_Dbg( p_ml, "automatically updating media %d", i_media_id );
#endif
    ml_media_t* p_media = item_list_mediaOfItem( p_ml->p_sys->p_watch, p_item, locked );
    CopyInputItemToMedia( p_media, p_item );
    ml_LockMedia( p_media );
    p_media->i_played_count += b_raise_count ? 1 : 0;
    ml_UnlockMedia( p_media );
    int i_ret = UpdateMedia( p_ml, p_media );

    /* Add the poster to the album */
    ml_LockMedia( p_media );
    if( p_media->i_album_id && p_media->psz_cover )
    {
        SetArtCover( p_ml, p_media->i_album_id, p_media->psz_cover );
    }
    ml_UnlockMedia( p_media );

    return i_ret;
}

/**
 * @brief Signals the watch system to update all medias
 */
void watch_Force_Update( media_library_t* p_ml )
{
    vlc_mutex_lock( &p_ml->p_sys->p_watch->lock );
    vlc_cond_signal( &p_ml->p_sys->p_watch->cond );
    vlc_mutex_unlock( &p_ml->p_sys->p_watch->lock );
}

/**
 * @brief Loop on the item_list: old objects collector and automatic updater
 *
 * This function is *not* a garbage collector. It actually decrefs items
 * when they are too old. ITEM_GC_MAX_AGE is the maximum 'time' an item
 * can stay in the list. After that, it is gc_decref'ed but not removed
 * from this list. If you try to get it after that, either the input item
 * is still alive, then you get it, or you'll have
 *
 * The update of an item is done when its age is >= ITEM_LOOP_UPDATE
 * (0 could lead to a too early update)
 *
 * A thread should call this function every N seconds
 *
 * @param p_ml the media library instance
 */
static void watch_loop( media_library_t *p_ml, bool b_force )
{
    /* Do the garbage collection */
    pool_GC( p_ml );

    /* Process the append queue */
    watch_ProcessAppendQueue( p_ml );

    /* Do the item update if necessary */
    vlc_mutex_lock( &p_ml->p_sys->p_watch->list_mutex );
    item_list_t *p_prev = NULL;
    il_foreachhashlist( p_ml->p_sys->p_watch->p_hlist, p_elt, ixx )
    {
        if( ( p_elt->i_update && p_elt->i_age >= ITEM_LOOP_UPDATE )
                || b_force )
        {
            /* This is the automatic delayed update */
            watch_update_Item( p_ml, p_elt->i_media_id, p_elt->p_item,
                               ( p_elt->i_update & 2 ) ? true : false, true );
            /* The item gets older */
            p_prev = p_elt;
            p_elt->i_age++;
            p_elt->i_update = false;
        }
        else if( p_elt->i_refs == 0 )
        {
            if( p_elt->i_update )
            watch_update_Item( p_ml, p_elt->i_media_id, p_elt->p_item,
                               ( p_elt->i_update & 2 ) ? true : false, true );
            __watch_del_Item( p_ml, p_elt->p_item, true );
            /* TODO: Do something about below crazy hack */
            if( p_prev != NULL )
                p_elt = p_prev;
            else
            {
                ixx--;
                break;
            }
        }
        else
        {
            p_prev = p_elt;
            p_elt->i_age++;
        }
    }
    vlc_mutex_unlock( &p_ml->p_sys->p_watch->list_mutex );
}

/**
 * This function goes through a queue of input_items and checks
 * if they are present in ML. All the items we wish to add in the
 * watch Queue
 */
static void watch_ProcessAppendQueue( media_library_t* p_ml )
{
    watch_thread_t* p_wt = p_ml->p_sys->p_watch;
    vlc_mutex_lock( &p_wt->item_append_queue_lock );
    bool b_add = var_CreateGetBool( p_ml, "ml-auto-add" );
    for( int i = 0; i < p_wt->item_append_queue_count; i++ )
    {
        input_item_t* p_item = p_wt->item_append_queue[i];
        ml_media_t* p_media = NULL;
        /* Is this item in ML? */
        int i_media_id = GetMediaIdOfURI( p_ml, p_item->psz_uri );
        int i_ret = 0;
        if( i_media_id <= 0 )
        {
            if( b_add )
            {
                i_ret = AddInputItem( p_ml, p_item );
                /* FIXME: Need to skip? */
                if( i_ret != VLC_SUCCESS )
                    continue;
                i_media_id = GetMediaIdOfURI( p_ml, p_item->psz_uri );
            }
            else
                continue;
        }
        vlc_mutex_lock( &p_wt->list_mutex );
        p_media = media_New( p_ml, i_media_id, ML_MEDIA, true );
        if( p_media == NULL )
        {
            vlc_mutex_unlock( &p_wt->list_mutex );
            continue;
        }
        /* If duplicate, then it just continues */
        i_ret = __watch_add_Item( p_ml, p_item, p_media, true );
        if( i_ret != VLC_SUCCESS )
        {
            ml_gc_decref( p_media );
            vlc_mutex_unlock( &p_wt->list_mutex );
            continue;
        }

        /* Find the new item and increment its ref */
        il_foreachlist( p_wt->p_hlist[ item_hash( p_item ) ], p_elt )
        {
            if( p_elt->p_item->i_id == p_item->i_id )
            {
                p_elt->i_refs++;
                break;
            }
        }
        vlc_mutex_unlock( &p_wt->list_mutex );
        ml_gc_decref( p_media );
    }
    p_wt->item_append_queue_count = 0;
    FREENULL( p_wt->item_append_queue );
    vlc_mutex_unlock( &p_wt->item_append_queue_lock );
}
