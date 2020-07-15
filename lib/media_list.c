/*****************************************************************************
 * media_list.c: libvlc new API media list functions
 *****************************************************************************
 * Copyright (C) 2007 VLC authors and VideoLAN
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc/libvlc.h>
#include <vlc/libvlc_picture.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_media_list.h>
#include <vlc/libvlc_events.h>

#include <vlc_common.h>

#include "libvlc_internal.h"
#include "media_internal.h" // libvlc_media_new_from_input_item()
#include "media_list_internal.h"

typedef enum EventPlaceInTime {
    EventWillHappen,
    EventDidHappen
} EventPlaceInTime;

/*
 * Private functions
 */



/**************************************************************************
 *       notify_item_addition (private)
 *
 * Do the appropriate action when an item is deleted.
 **************************************************************************/
static void
notify_item_addition( libvlc_media_list_t * p_mlist,
                      libvlc_media_t * p_md,
                      int index,
                      EventPlaceInTime event_status )
{
    libvlc_event_t event;

    /* Construct the event */
    if( event_status == EventDidHappen )
    {
        event.type = libvlc_MediaListItemAdded;
        event.u.media_list_item_added.item = p_md;
        event.u.media_list_item_added.index = index;
    }
    else /* if( event_status == EventWillHappen ) */
    {
        event.type = libvlc_MediaListWillAddItem;
        event.u.media_list_will_add_item.item = p_md;
        event.u.media_list_will_add_item.index = index;
    }

    /* Send the event */
    libvlc_event_send( &p_mlist->event_manager, &event );
}

/**************************************************************************
 *       notify_item_deletion (private)
 *
 * Do the appropriate action when an item is added.
 **************************************************************************/
static void
notify_item_deletion( libvlc_media_list_t * p_mlist,
                      libvlc_media_t * p_md,
                      int index,
                      EventPlaceInTime event_status )
{
    libvlc_event_t event;

    /* Construct the event */
    if( event_status == EventDidHappen )
    {
        event.type = libvlc_MediaListItemDeleted;
        event.u.media_list_item_deleted.item = p_md;
        event.u.media_list_item_deleted.index = index;
    }
    else /* if( event_status == EventWillHappen ) */
    {
        event.type = libvlc_MediaListWillDeleteItem;
        event.u.media_list_will_delete_item.item = p_md;
        event.u.media_list_will_delete_item.index = index;
    }

    /* Send the event */
    libvlc_event_send( &p_mlist->event_manager, &event );
}

/* LibVLC internal */
void libvlc_media_list_internal_end_reached( libvlc_media_list_t * p_mlist )
{
    libvlc_event_t event;

    event.type = libvlc_MediaListEndReached;

    /* Send the event */
    libvlc_event_send( &p_mlist->event_manager, &event );
}

/**************************************************************************
 *       static mlist_is_writable (private)
 **************************************************************************/
static inline
bool mlist_is_writable( libvlc_media_list_t *p_mlist )
{
    if( !p_mlist||p_mlist->b_read_only )
    {
        /* We are read-only from user side */
        libvlc_printerr( "Attempt to write a read-only media list" );
        return false;
    }
    return true;
}

/*
 * Public libvlc functions
 */

/**************************************************************************
 *       libvlc_media_list_new (Public)
 *
 * Init an object.
 **************************************************************************/
libvlc_media_list_t *libvlc_media_list_new(void)
{
    libvlc_media_list_t * p_mlist;

    p_mlist = malloc(sizeof(libvlc_media_list_t));
    if( unlikely(p_mlist == NULL) )
    {
        libvlc_printerr( "Not enough memory" );
        return NULL;
    }

    libvlc_event_manager_init( &p_mlist->event_manager, p_mlist );
    p_mlist->b_read_only = false;

    vlc_mutex_init( &p_mlist->object_lock );
    vlc_mutex_init( &p_mlist->refcount_lock ); // FIXME: spinlock?

    vlc_array_init( &p_mlist->items );
    assert( p_mlist->items.i_count == 0 );
    p_mlist->i_refcount = 1;
    p_mlist->p_md = NULL;
    p_mlist->p_internal_md = NULL;

    return p_mlist;
}

/**************************************************************************
 *       libvlc_media_list_release (Public)
 *
 * Release an object.
 **************************************************************************/
void libvlc_media_list_release( libvlc_media_list_t * p_mlist )
{
    vlc_mutex_lock( &p_mlist->refcount_lock );
    p_mlist->i_refcount--;
    if( p_mlist->i_refcount > 0 )
    {
        vlc_mutex_unlock( &p_mlist->refcount_lock );
        return;
    }
    vlc_mutex_unlock( &p_mlist->refcount_lock );

    /* Refcount null, time to free */

    libvlc_event_manager_destroy( &p_mlist->event_manager );
    libvlc_media_release( p_mlist->p_md );

    for( size_t i = 0; i < vlc_array_count( &p_mlist->items ); i++ )
    {
        libvlc_media_t* p_md = vlc_array_item_at_index( &p_mlist->items, i );
        libvlc_media_release( p_md );
    }

    vlc_array_clear( &p_mlist->items );

    free( p_mlist );
}

/**************************************************************************
 *       libvlc_media_list_retain (Public)
 *
 * Increase an object refcount.
 **************************************************************************/
void libvlc_media_list_retain( libvlc_media_list_t * p_mlist )
{
    vlc_mutex_lock( &p_mlist->refcount_lock );
    p_mlist->i_refcount++;
    vlc_mutex_unlock( &p_mlist->refcount_lock );
}

/**************************************************************************
 *       set_media (Public)
 **************************************************************************/
void libvlc_media_list_set_media( libvlc_media_list_t * p_mlist,
                                  libvlc_media_t * p_md )

{
    vlc_mutex_lock( &p_mlist->object_lock );
    if( p_mlist->p_internal_md || !mlist_is_writable(p_mlist) )
    {
        vlc_mutex_unlock( &p_mlist->object_lock );
        return;
    }
    libvlc_media_release( p_mlist->p_md );
    libvlc_media_retain( p_md );
    p_mlist->p_md = p_md;
    vlc_mutex_unlock( &p_mlist->object_lock );
}

/**************************************************************************
 *       media (Public)
 *
 * If this media_list comes is a media's subitems,
 * This holds the corresponding media.
 * This md is also seen as the information holder for the media_list.
 * Indeed a media_list can have meta information through this
 * media.
 **************************************************************************/
libvlc_media_t *
libvlc_media_list_media( libvlc_media_list_t * p_mlist )
{
    libvlc_media_t *p_md;

    vlc_mutex_lock( &p_mlist->object_lock );
    p_md = p_mlist->p_internal_md ? p_mlist->p_internal_md : p_mlist->p_md;
    if( p_md )
        libvlc_media_retain( p_md );
    vlc_mutex_unlock( &p_mlist->object_lock );

    return p_md;
}

/**************************************************************************
 *       libvlc_media_list_count (Public)
 *
 * Lock should be held when entering.
 **************************************************************************/
int libvlc_media_list_count( libvlc_media_list_t * p_mlist )
{
    return vlc_array_count( &p_mlist->items );
}

/**************************************************************************
 *       libvlc_media_list_add_media (Public)
 *
 * Lock should be held when entering.
 **************************************************************************/
int libvlc_media_list_add_media( libvlc_media_list_t * p_mlist,
                                 libvlc_media_t * p_md )
{
    if( !mlist_is_writable(p_mlist) )
        return -1;
    libvlc_media_list_internal_add_media( p_mlist, p_md );
    return 0;
}

/* LibVLC internal version */
void libvlc_media_list_internal_add_media( libvlc_media_list_t * p_mlist,
                                           libvlc_media_t * p_md )
{
    libvlc_media_retain( p_md );

    notify_item_addition( p_mlist, p_md, vlc_array_count( &p_mlist->items ),
                          EventWillHappen );
    vlc_array_append_or_abort( &p_mlist->items, p_md );
    notify_item_addition( p_mlist, p_md, vlc_array_count( &p_mlist->items )-1,
                          EventDidHappen );
}

/**************************************************************************
 *       libvlc_media_list_insert_media (Public)
 *
 * Lock should be hold when entering.
 **************************************************************************/
int libvlc_media_list_insert_media( libvlc_media_list_t * p_mlist,
                                    libvlc_media_t * p_md,
                                    int index )
{
    if( !mlist_is_writable(p_mlist) )
        return -1;
    libvlc_media_list_internal_insert_media( p_mlist, p_md, index );
    return 0;
}

/* LibVLC internal version */
void libvlc_media_list_internal_insert_media( libvlc_media_list_t * p_mlist,
                                              libvlc_media_t * p_md,
                                              int index )
{
    libvlc_media_retain( p_md );

    notify_item_addition( p_mlist, p_md, index, EventWillHappen );
    vlc_array_insert_or_abort( &p_mlist->items, p_md, index );
    notify_item_addition( p_mlist, p_md, index, EventDidHappen );
}

/**************************************************************************
 *       libvlc_media_list_remove_index (Public)
 *
 * Lock should be held when entering.
 **************************************************************************/
int libvlc_media_list_remove_index( libvlc_media_list_t * p_mlist,
                                     int index )
{
    if( !mlist_is_writable(p_mlist) )
        return -1;
    return libvlc_media_list_internal_remove_index( p_mlist, index );
}

/* LibVLC internal version */
int libvlc_media_list_internal_remove_index( libvlc_media_list_t * p_mlist,
                                             int index )
{
    libvlc_media_t * p_md;

    if( (size_t) index >= vlc_array_count( &p_mlist->items ))
    {
        libvlc_printerr( "Index out of bounds" );
        return -1;
    }

    p_md = vlc_array_item_at_index( &p_mlist->items, index );

    notify_item_deletion( p_mlist, p_md, index, EventWillHappen );
    vlc_array_remove( &p_mlist->items, index );
    notify_item_deletion( p_mlist, p_md, index, EventDidHappen );

    libvlc_media_release( p_md );
    return 0;
}

/**************************************************************************
 *       libvlc_media_list_item_at_index (Public)
 *
 * Lock should be held when entering.
 **************************************************************************/
libvlc_media_t *
libvlc_media_list_item_at_index( libvlc_media_list_t * p_mlist,
                                 int index )
{
    libvlc_media_t * p_md;

    if( (size_t) index >= vlc_array_count( &p_mlist->items ))
    {
        libvlc_printerr( "Index out of bounds" );
        return NULL;
    }

    p_md = vlc_array_item_at_index( &p_mlist->items, index );
    libvlc_media_retain( p_md );
    return p_md;
}

/**************************************************************************
 *       libvlc_media_list_index_of_item (Public)
 *
 * Lock should be held when entering.
 * Warning: this function returns the first matching item.
 **************************************************************************/
int libvlc_media_list_index_of_item( libvlc_media_list_t * p_mlist,
                                     libvlc_media_t * p_searched_md )
{
    int idx = vlc_array_index_of_item( &p_mlist->items, p_searched_md );
    if( idx == -1 )
        libvlc_printerr( "Media not found" );

    return idx;
}

/**************************************************************************
 *       libvlc_media_list_is_readonly (Public)
 *
 * This indicates if this media list is read-only from a user point of view
 **************************************************************************/
bool libvlc_media_list_is_readonly( libvlc_media_list_t * p_mlist )
{
    return p_mlist->b_read_only;
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
 *       libvlc_media_list_event_manager (Public)
 *
 * The p_event_manager is immutable, so you don't have to hold the lock
 **************************************************************************/
libvlc_event_manager_t *
libvlc_media_list_event_manager( libvlc_media_list_t * p_mlist )
{
    return &p_mlist->event_manager;
}
