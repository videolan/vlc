/*****************************************************************************
 * media_list.c: libvlc new API media list functions
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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

#include "libvlc_internal.h"
#include <vlc/libvlc.h>
#include <assert.h>
#include "vlc_arrays.h"

/*
 * Private functions
 */

/**************************************************************************
 *       notify_item_addition (private)
 *
 * Call parent playlist and send the appropriate event.
 **************************************************************************/
static void
notify_item_addition( libvlc_media_list_t * p_mlist,
                      libvlc_media_descriptor_t * p_md,
                      int index )
{
    libvlc_event_t event;

    event.type = libvlc_MediaListItemAdded;
    event.u.media_list_item_added.item = p_md;
    event.u.media_list_item_added.index = index;
    
    libvlc_event_send( p_mlist->p_event_manager, &event );
}

/**************************************************************************
 *       notify_item_deletion (private)
 *
 * Call parent playlist and send the appropriate event.
 **************************************************************************/
static void
notify_item_deletion( libvlc_media_list_t * p_mlist,
                      libvlc_media_descriptor_t * p_md,
                      int index )
{
    libvlc_event_t event;
    
    event.type = libvlc_MediaListItemDeleted;
    event.u.media_list_item_deleted.item = p_md;
    event.u.media_list_item_deleted.index = index;

    libvlc_event_send( p_mlist->p_event_manager, &event );
}

/**************************************************************************
 *       dynamic_list_propose_item (private) (Event Callback)
 *
 * This is called if the dynamic sublist's data provider adds a new item.
 **************************************************************************/
static void
dynamic_list_propose_item( const libvlc_event_t * p_event, void * p_user_data )
{
    libvlc_media_list_t * p_submlist = p_user_data;
    libvlc_media_descriptor_t * p_md = p_event->u.media_list_item_added.item;

    //libvlc_media_descriptor_lock( p_md );
    if( libvlc_tag_query_match( p_submlist->p_query, p_md, NULL ) )
    {
        libvlc_media_list_lock( p_submlist );
        libvlc_media_list_add_media_descriptor( p_submlist, p_md, NULL );
        libvlc_media_list_unlock( p_submlist );
    }
    //libvlc_media_descriptor_unlock( p_md );
}

/**************************************************************************
 *       dynamic_list_remove_item (private) (Event Callback)
 *
 * This is called if the dynamic sublist's data provider adds a new item.
 **************************************************************************/
static void
dynamic_list_remove_item( const libvlc_event_t * p_event, void * p_user_data )
{
    libvlc_media_list_t * p_submlist = p_user_data;
    libvlc_media_descriptor_t * p_md = p_event->u.media_list_item_deleted.item;

    //libvlc_media_descriptor_lock( p_md );
    if( libvlc_tag_query_match( p_submlist->p_query, p_md, NULL ) )
    {
        int i;
        libvlc_media_list_lock( p_submlist );        
        i = libvlc_media_list_index_of_item( p_submlist, p_md, NULL );
        if ( i < 0 )
        {
            /* We've missed one item addition, that could happen especially
             * if we add item in a threaded maner, so we just ignore */
            libvlc_media_list_unlock( p_submlist );
            //libvlc_media_descriptor_unlock( p_md );           
            return;
        }
        libvlc_media_list_remove_index( p_submlist, i, NULL );
        libvlc_media_list_unlock( p_submlist );
    }
    //libvlc_media_descriptor_unlock( p_md );
}

/**************************************************************************
 *       dynamic_list_change_item (private) (Event Callback)
 *
 * This is called if the dynamic sublist's data provider adds a new item.
 **************************************************************************/
static void
dynamic_list_change_item( const libvlc_event_t * p_event , void * p_user_data)
{
    libvlc_media_list_t * p_submlist = p_user_data;
    libvlc_media_descriptor_t * p_md = p_event->u.media_list_item_changed.item;
    int index;

    libvlc_media_list_lock( p_submlist );        
    
    index = libvlc_media_list_index_of_item( p_submlist, p_md, NULL );
    if( index < 0 )
    {
        libvlc_media_list_unlock( p_submlist );     
        return; /* Not found, no prob, just ignore */
    }

    //libvlc_media_descriptor_lock( p_md );
    if( !libvlc_tag_query_match( p_submlist->p_query, p_md, NULL ) )
        libvlc_media_list_remove_index( p_submlist, index, NULL );
    //libvlc_media_descriptor_unlock( p_md );

    libvlc_media_list_unlock( p_submlist );        
}

/*
 * Public libvlc functions
 */

/**************************************************************************
 *       libvlc_media_list_new (Public)
 *
 * Init an object.
 **************************************************************************/
libvlc_media_list_t *
libvlc_media_list_new( libvlc_instance_t * p_inst,
                       libvlc_exception_t * p_e )
{
    libvlc_media_list_t * p_mlist;

    p_mlist = malloc(sizeof(libvlc_media_list_t));

    if( !p_mlist )
        return NULL;
    
    p_mlist->p_libvlc_instance = p_inst;
    p_mlist->p_event_manager = libvlc_event_manager_new( p_mlist, p_inst, p_e );

    libvlc_event_manager_register_event_type( p_mlist->p_event_manager,
            libvlc_MediaListItemAdded, p_e );
    libvlc_event_manager_register_event_type( p_mlist->p_event_manager,
            libvlc_MediaListItemChanged, p_e );
    libvlc_event_manager_register_event_type( p_mlist->p_event_manager,
            libvlc_MediaListItemDeleted, p_e );

    if( libvlc_exception_raised( p_e ) )
    {
        libvlc_event_manager_release( p_mlist->p_event_manager );
        free( p_mlist );
        return NULL;
    }

    vlc_mutex_init( p_inst->p_libvlc_int, &p_mlist->object_lock );
    
    ARRAY_INIT(p_mlist->items);
    p_mlist->i_refcount = 1;
    p_mlist->p_media_provider = NULL;

    return p_mlist;
}

/**************************************************************************
 *       libvlc_media_list_release (Public)
 *
 * Release an object.
 **************************************************************************/
void libvlc_media_list_release( libvlc_media_list_t * p_mlist )
{
    libvlc_media_descriptor_t * p_md;

    vlc_mutex_lock( &p_mlist->object_lock );
    p_mlist->i_refcount--;
    if( p_mlist->i_refcount > 0 )
    {
        vlc_mutex_unlock( &p_mlist->object_lock );        
        return;
    }
    vlc_mutex_unlock( &p_mlist->object_lock );        

    /* Refcount null, time to free */
    if( p_mlist->p_media_provider )
        libvlc_media_list_release( p_mlist->p_media_provider );

    if( p_mlist->p_query )
        libvlc_tag_query_release( p_mlist->p_query );

    libvlc_event_manager_release( p_mlist->p_event_manager );

    FOREACH_ARRAY( p_md, p_mlist->items )
        libvlc_media_descriptor_release( p_md );
    FOREACH_END()
 
    free( p_mlist );
}
/**************************************************************************
 *       libvlc_media_list_retain (Public)
 *
 * Increase an object refcount.
 **************************************************************************/
void libvlc_media_list_retain( libvlc_media_list_t * p_mlist )
{
    vlc_mutex_lock( &p_mlist->object_lock );
    p_mlist->i_refcount++;
    vlc_mutex_unlock( &p_mlist->object_lock );
}

/**************************************************************************
 *       libvlc_media_list_count (Public)
 *
 * Lock should be hold when entering.
 **************************************************************************/
int libvlc_media_list_count( libvlc_media_list_t * p_mlist,
                             libvlc_exception_t * p_e )
{
    (void)p_e;
    return p_mlist->items.i_size;
}

/**************************************************************************
 *       libvlc_media_list_add_media_descriptor (Public)
 *
 * Lock should be hold when entering.
 **************************************************************************/
void libvlc_media_list_add_media_descriptor( 
                                   libvlc_media_list_t * p_mlist,
                                   libvlc_media_descriptor_t * p_md,
                                   libvlc_exception_t * p_e )
{
    (void)p_e;
    libvlc_media_descriptor_retain( p_md );
    
    ARRAY_APPEND( p_mlist->items, p_md );
    notify_item_addition( p_mlist, p_md, p_mlist->items.i_size-1 );
}

/**************************************************************************
 *       libvlc_media_list_insert_media_descriptor (Public)
 *
 * Lock should be hold when entering.
 **************************************************************************/
void libvlc_media_list_insert_media_descriptor( 
                                   libvlc_media_list_t * p_mlist,
                                   libvlc_media_descriptor_t * p_md,
                                   int index,
                                   libvlc_exception_t * p_e )
{
    (void)p_e;
    libvlc_media_descriptor_retain( p_md );

    ARRAY_INSERT( p_mlist->items, p_md, index);
    notify_item_addition( p_mlist, p_md, index );
}

/**************************************************************************
 *       libvlc_media_list_remove_index (Public)
 *
 * Lock should be hold when entering.
 **************************************************************************/
void libvlc_media_list_remove_index( libvlc_media_list_t * p_mlist,
                                     int index,
                                     libvlc_exception_t * p_e )
{
    libvlc_media_descriptor_t * p_md;

    p_md = ARRAY_VAL( p_mlist->items, index );

    ARRAY_REMOVE( p_mlist->items, index )
    notify_item_deletion( p_mlist, p_md, index );

    libvlc_media_descriptor_release( p_md );
}

/**************************************************************************
 *       libvlc_media_list_item_at_index (Public)
 *
 * Lock should be hold when entering.
 **************************************************************************/
libvlc_media_descriptor_t *
libvlc_media_list_item_at_index( libvlc_media_list_t * p_mlist,
                                 int index,
                                 libvlc_exception_t * p_e )
{
    return ARRAY_VAL( p_mlist->items, index );
}

/**************************************************************************
 *       libvlc_media_list_index_of_item (Public)
 *
 * Lock should be hold when entering.
 * Warning: this function would return the first matching item
 **************************************************************************/
int libvlc_media_list_index_of_item( libvlc_media_list_t * p_mlist,
                                     libvlc_media_descriptor_t * p_searched_md,
                                     libvlc_exception_t * p_e )
{
    libvlc_media_descriptor_t * p_md;
    FOREACH_ARRAY( p_md, p_mlist->items )
        if( p_searched_md == p_md )
            return fe_idx; /* Once more, we hate macro for that */
    FOREACH_END()
    return -1;
}

/**************************************************************************
 *       libvlc_media_list_lock (Public)
 *
 * The lock must be held in access operations. It is never used in the
 * Public method.
 **************************************************************************/
void libvlc_media_list_lock( libvlc_media_list_t * p_mlist )
{
    vlc_mutex_lock( &p_mlist->object_lock );
}


/**************************************************************************
 *       libvlc_media_list_unlock (Public)
 *
 * The lock must be held in access operations
 **************************************************************************/
void libvlc_media_list_unlock( libvlc_media_list_t * p_mlist )
{
    vlc_mutex_unlock( &p_mlist->object_lock );
}



/**************************************************************************
 *       libvlc_media_list_p_event_manager (Public)
 *
 * The p_event_manager is immutable, so you don't have to hold the lock
 **************************************************************************/
libvlc_event_manager_t *
libvlc_media_list_event_manager( libvlc_media_list_t * p_mlist,
                                    libvlc_exception_t * p_e )
{
    (void)p_e;
    return p_mlist->p_event_manager;
}

/**************************************************************************
 *       libvlc_media_list_dynamic_sublist (Public)
 *
 * Lock should be hold when entering.
 **************************************************************************/
libvlc_media_list_t *
libvlc_media_list_dynamic_sublist( libvlc_media_list_t * p_mlist,
                                   libvlc_tag_query_t * p_query,
                                   libvlc_exception_t * p_e )
{
    libvlc_media_list_t * p_submlist;
    libvlc_event_manager_t * p_em;
    int count, i;

    (void)p_e;

    p_submlist = libvlc_media_list_new( p_mlist->p_libvlc_instance, p_e );
    if( !p_submlist )
    {
        if( !libvlc_exception_raised( p_e ) )
            libvlc_exception_raise( p_e, "Can't get the new media_list" );
        return NULL;
    }

    /* We have a query */
    libvlc_tag_query_retain( p_query );
    p_submlist->p_query = p_query;

    /* We have a media provider */
    libvlc_media_list_retain( p_mlist );
    p_submlist->p_media_provider = p_mlist;


    libvlc_media_list_lock( p_submlist );        
    
    count = libvlc_media_list_count( p_mlist, p_e );

    /* This should be running in a thread, a good plan to achieve that
     * move all the dynamic code to libvlc_tag_query. */
    for( i = 0; i < count; i++ )
    {
        libvlc_media_descriptor_t * p_md;
        p_md = libvlc_media_list_item_at_index( p_mlist, i, p_e );
        if( libvlc_tag_query_match( p_query, p_md, NULL ) )
            libvlc_media_list_add_media_descriptor( p_submlist, p_md, p_e );
    }

    /* And we will listen to its event, so we can update p_submlist
     * accordingly */
    p_em = libvlc_media_list_event_manager( p_mlist, p_e );
    libvlc_event_attach( p_em, libvlc_MediaListItemAdded,
                         dynamic_list_propose_item, p_submlist, p_e );
    libvlc_event_attach( p_em, libvlc_MediaListItemDeleted,
                         dynamic_list_remove_item, p_submlist, p_e );
    libvlc_event_attach( p_em, libvlc_MediaListItemChanged,
                         dynamic_list_change_item, p_submlist, p_e );

    libvlc_media_list_unlock( p_submlist );        

    return p_submlist;
}

