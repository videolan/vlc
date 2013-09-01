/*****************************************************************************
 * event.c: New libvlc event control API
 *****************************************************************************
 * Copyright (C) 2007-2010 VLC authors and VideoLAN
 * $Id $
 *
 * Authors: Filippo Carone <filippo@carone.org>
 *          Pierre d'Herbemont <pdherbemont # videolan.org>
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

#define  LIBVLC_EVENT_TYPES_KEEP_DEFINE
#include <vlc/libvlc.h>

#include "libvlc_internal.h"
#include "event_internal.h"
#include <assert.h>
#include <errno.h>

typedef struct libvlc_event_listeners_group_t
{
    libvlc_event_type_t event_type;
    vlc_array_t listeners;
    bool b_sublistener_removed;
} libvlc_event_listeners_group_t;

/*
 * Private functions
 */

static bool
group_contains_listener( libvlc_event_listeners_group_t * group,
                         libvlc_event_listener_t * searched_listener )
{
    int i;
    for( i = 0; i < vlc_array_count(&group->listeners); i++ )
    {
        if( listeners_are_equal(searched_listener, vlc_array_item_at_index(&group->listeners, i)) )
            return true;
    }
    return false;
}

/*
 * Internal libvlc functions
 */

/**************************************************************************
 *       libvlc_event_manager_new (internal) :
 *
 * Init an object's event manager.
 **************************************************************************/
libvlc_event_manager_t *
libvlc_event_manager_new( void * p_obj, libvlc_instance_t * p_libvlc_inst )
{
    libvlc_event_manager_t * p_em;

    p_em = malloc(sizeof( libvlc_event_manager_t ));
    if( !p_em )
    {
        libvlc_printerr( "Not enough memory" );
        return NULL;
    }

    p_em->p_obj = p_obj;
    p_em->p_obj = p_obj;
    p_em->async_event_queue = NULL;
    p_em->p_libvlc_instance = p_libvlc_inst;

    libvlc_retain( p_libvlc_inst );
    vlc_array_init( &p_em->listeners_groups );
    vlc_mutex_init( &p_em->object_lock );
    vlc_mutex_init_recursive( &p_em->event_sending_lock );
    return p_em;
}

/**************************************************************************
 *       libvlc_event_manager_release (internal) :
 *
 * Release an object's event manager.
 **************************************************************************/
void libvlc_event_manager_release( libvlc_event_manager_t * p_em )
{
    libvlc_event_listeners_group_t * p_lg;
    int i,j ;

    libvlc_event_async_fini(p_em);

    vlc_mutex_destroy( &p_em->event_sending_lock );
    vlc_mutex_destroy( &p_em->object_lock );

    for( i = 0; i < vlc_array_count(&p_em->listeners_groups); i++)
    {
        p_lg = vlc_array_item_at_index( &p_em->listeners_groups, i );

        for( j = 0; j < vlc_array_count(&p_lg->listeners); j++)
            free( vlc_array_item_at_index( &p_lg->listeners, j ) );

        vlc_array_clear( &p_lg->listeners );
        free( p_lg );
    }
    vlc_array_clear( &p_em->listeners_groups );
    libvlc_release( p_em->p_libvlc_instance );
    free( p_em );
}

/**************************************************************************
 *       libvlc_event_manager_register_event_type (internal) :
 *
 * Init an object's event manager.
 **************************************************************************/
void libvlc_event_manager_register_event_type(
        libvlc_event_manager_t * p_em,
        libvlc_event_type_t event_type )
{
    libvlc_event_listeners_group_t * listeners_group;
    listeners_group = xmalloc(sizeof(libvlc_event_listeners_group_t));
    listeners_group->event_type = event_type;
    vlc_array_init( &listeners_group->listeners );

    vlc_mutex_lock( &p_em->object_lock );
    vlc_array_append( &p_em->listeners_groups, listeners_group );
    vlc_mutex_unlock( &p_em->object_lock );
}

/**************************************************************************
 *       libvlc_event_send (internal) :
 *
 * Send a callback.
 **************************************************************************/
void libvlc_event_send( libvlc_event_manager_t * p_em,
                        libvlc_event_t * p_event )
{
    libvlc_event_listeners_group_t * listeners_group = NULL;
    libvlc_event_listener_t * listener_cached;
    libvlc_event_listener_t * listener;
    libvlc_event_listener_t * array_listeners_cached = NULL;
    int i, i_cached_listeners = 0;

    /* Fill event with the sending object now */
    p_event->p_obj = p_em->p_obj;

    vlc_mutex_lock( &p_em->event_sending_lock );
    vlc_mutex_lock( &p_em->object_lock );
    for( i = 0; i < vlc_array_count(&p_em->listeners_groups); i++)
    {
        listeners_group = vlc_array_item_at_index(&p_em->listeners_groups, i);
        if( listeners_group->event_type == p_event->type )
        {
            if( vlc_array_count( &listeners_group->listeners ) <= 0 )
                break;

            /* Cache a copy of the listener to avoid locking issues,
             * and allow that edition of listeners during callbacks will garantee immediate effect. */
            i_cached_listeners = vlc_array_count(&listeners_group->listeners);
            array_listeners_cached = malloc(sizeof(libvlc_event_listener_t)*(i_cached_listeners));
            if( !array_listeners_cached )
            {
                vlc_mutex_unlock( &p_em->object_lock );
                vlc_mutex_unlock( &p_em->event_sending_lock );
                fprintf(stderr, "Can't alloc memory in libvlc_event_send" );
                return;
            }

            listener_cached = array_listeners_cached;
            for( i = 0; i < vlc_array_count(&listeners_group->listeners); i++)
            {
                listener = vlc_array_item_at_index(&listeners_group->listeners, i);
                memcpy( listener_cached, listener, sizeof(libvlc_event_listener_t) );
                listener_cached++;
            }
            break;
        }
    }

    if( !listeners_group )
    {
        free( array_listeners_cached );
        vlc_mutex_unlock( &p_em->object_lock );
        vlc_mutex_unlock( &p_em->event_sending_lock );
        return;
    }

    /* Track item removed from *this* thread, with a simple flag. Indeed
     * event_sending_lock is a recursive lock. This has the advantage of
     * allowing to remove an event listener from within a callback */
    listeners_group->b_sublistener_removed = false;

    vlc_mutex_unlock( &p_em->object_lock );

    listener_cached = array_listeners_cached;
    for( i = 0; i < i_cached_listeners; i++ )
    {
        if(listener_cached->is_asynchronous)
        {
            /* The listener wants not to block the emitter during event callback */
            libvlc_event_async_dispatch(p_em, listener_cached, p_event);
        }
        else
        {
            /* The listener wants to block the emitter during event callback */

            listener_cached->pf_callback( p_event, listener_cached->p_user_data );
            listener_cached++;

            if( listeners_group->b_sublistener_removed )
            {
                /* If a callback was removed, this gets called */
                bool valid_listener;
                vlc_mutex_lock( &p_em->object_lock );
                valid_listener = group_contains_listener( listeners_group, listener_cached );
                vlc_mutex_unlock( &p_em->object_lock );
                if( !valid_listener )
                {
                    listener_cached++;
                    continue;
                }
            }
        }
    }
    vlc_mutex_unlock( &p_em->event_sending_lock );

    free( array_listeners_cached );
}

/*
 * Public libvlc functions
 */

#define DEF( a ) { libvlc_##a, #a, },

typedef struct
{
    int type;
    const char name[40];
} event_name_t;

static const event_name_t event_list[] = {
    DEF(MediaMetaChanged)
    DEF(MediaSubItemAdded)
    DEF(MediaDurationChanged)
    DEF(MediaParsedChanged)
    DEF(MediaFreed)
    DEF(MediaStateChanged)
    DEF(MediaSubItemTreeAdded)

    DEF(MediaPlayerMediaChanged)
    DEF(MediaPlayerNothingSpecial)
    DEF(MediaPlayerOpening)
    DEF(MediaPlayerBuffering)
    DEF(MediaPlayerPlaying)
    DEF(MediaPlayerPaused)
    DEF(MediaPlayerStopped)
    DEF(MediaPlayerForward)
    DEF(MediaPlayerBackward)
    DEF(MediaPlayerEndReached)
    DEF(MediaPlayerEncounteredError)
    DEF(MediaPlayerTimeChanged)
    DEF(MediaPlayerPositionChanged)
    DEF(MediaPlayerSeekableChanged)
    DEF(MediaPlayerPausableChanged)
    DEF(MediaPlayerTitleChanged)
    DEF(MediaPlayerSnapshotTaken)
    DEF(MediaPlayerLengthChanged)
    DEF(MediaPlayerVout)

    DEF(MediaListItemAdded)
    DEF(MediaListWillAddItem)
    DEF(MediaListItemDeleted)
    DEF(MediaListWillDeleteItem)

    DEF(MediaListViewItemAdded)
    DEF(MediaListViewWillAddItem)
    DEF(MediaListViewItemDeleted)
    DEF(MediaListViewWillDeleteItem)

    DEF(MediaListPlayerPlayed)
    DEF(MediaListPlayerNextItemSet)
    DEF(MediaListPlayerStopped)

    DEF(MediaDiscovererStarted)
    DEF(MediaDiscovererEnded)

    DEF(VlmMediaAdded)
    DEF(VlmMediaRemoved)
    DEF(VlmMediaChanged)
    DEF(VlmMediaInstanceStarted)
    DEF(VlmMediaInstanceStopped)
    DEF(VlmMediaInstanceStatusInit)
    DEF(VlmMediaInstanceStatusOpening)
    DEF(VlmMediaInstanceStatusPlaying)
    DEF(VlmMediaInstanceStatusPause)
    DEF(VlmMediaInstanceStatusEnd)
    DEF(VlmMediaInstanceStatusError)
};
#undef DEF

static const char unknown_event_name[] = "Unknown Event";

static int evcmp( const void *a, const void *b )
{
    return (*(const int *)a) - ((event_name_t *)b)->type;
}

const char * libvlc_event_type_name( int event_type )
{
    const event_name_t *p;

    p = bsearch( &event_type, event_list,
                 sizeof(event_list)/sizeof(event_list[0]), sizeof(*p),
                 evcmp );
    return p ? p->name : unknown_event_name;
}

/**************************************************************************
 *       event_attach (internal) :
 *
 * Add a callback for an event.
 **************************************************************************/
static
int event_attach( libvlc_event_manager_t * p_event_manager,
                  libvlc_event_type_t event_type,
                  libvlc_callback_t pf_callback, void *p_user_data,
                  bool is_asynchronous )
{
    libvlc_event_listeners_group_t * listeners_group;
    libvlc_event_listener_t * listener;
    int i;

    listener = malloc(sizeof(libvlc_event_listener_t));
    if( unlikely(listener == NULL) )
        return ENOMEM;

    listener->event_type = event_type;
    listener->p_user_data = p_user_data;
    listener->pf_callback = pf_callback;
    listener->is_asynchronous = is_asynchronous;

    vlc_mutex_lock( &p_event_manager->object_lock );
    for( i = 0; i < vlc_array_count(&p_event_manager->listeners_groups); i++ )
    {
        listeners_group = vlc_array_item_at_index(&p_event_manager->listeners_groups, i);
        if( listeners_group->event_type == listener->event_type )
        {
            vlc_array_append( &listeners_group->listeners, listener );
            vlc_mutex_unlock( &p_event_manager->object_lock );
            return 0;
        }
    }
    vlc_mutex_unlock( &p_event_manager->object_lock );

    free(listener);
    fprintf( stderr, "This object event manager doesn't know about '%s' events",
             libvlc_event_type_name(event_type) );
    assert(0);
    return -1;
}

/**************************************************************************
 *       libvlc_event_attach (public) :
 *
 * Add a callback for an event.
 **************************************************************************/
int libvlc_event_attach( libvlc_event_manager_t * p_event_manager,
                         libvlc_event_type_t event_type,
                         libvlc_callback_t pf_callback,
                         void *p_user_data )
{
    return event_attach(p_event_manager, event_type, pf_callback, p_user_data,
                        false /* synchronous */);
}

/**************************************************************************
 *       libvlc_event_attach (public) :
 *
 * Add a callback for an event.
 **************************************************************************/
void libvlc_event_attach_async( libvlc_event_manager_t * p_event_manager,
                         libvlc_event_type_t event_type,
                         libvlc_callback_t pf_callback,
                         void *p_user_data )
{
    event_attach(p_event_manager, event_type, pf_callback, p_user_data,
                 true /* asynchronous */);
}

/**************************************************************************
 *       libvlc_event_detach (public) :
 *
 * Remove a callback for an event.
 **************************************************************************/
void libvlc_event_detach( libvlc_event_manager_t *p_event_manager,
                                     libvlc_event_type_t event_type,
                                     libvlc_callback_t pf_callback,
                                     void *p_user_data )
{
    libvlc_event_listeners_group_t * listeners_group;
    libvlc_event_listener_t * listener;
    int i, j;
    bool found = false;

    vlc_mutex_lock( &p_event_manager->event_sending_lock );
    vlc_mutex_lock( &p_event_manager->object_lock );
    for( i = 0; i < vlc_array_count(&p_event_manager->listeners_groups); i++)
    {
        listeners_group = vlc_array_item_at_index(&p_event_manager->listeners_groups, i);
        if( listeners_group->event_type == event_type )
        {
            for( j = 0; j < vlc_array_count(&listeners_group->listeners); j++)
            {
                listener = vlc_array_item_at_index(&listeners_group->listeners, j);
                if( listener->event_type == event_type &&
                    listener->pf_callback == pf_callback &&
                    listener->p_user_data == p_user_data )
                {
                    /* that's our listener */

                    /* Mark this group as edited so that libvlc_event_send
                     * will recheck what listener to call */
                    listeners_group->b_sublistener_removed = true;

                    free( listener );
                    vlc_array_remove( &listeners_group->listeners, j );
                    found = true;
                    break;
                }
            }
        }
    }
    vlc_mutex_unlock( &p_event_manager->object_lock );
    vlc_mutex_unlock( &p_event_manager->event_sending_lock );

    /* Now make sure any pending async event won't get fired after that point */
    libvlc_event_listener_t listener_to_remove;
    listener_to_remove.event_type  = event_type;
    listener_to_remove.pf_callback = pf_callback;
    listener_to_remove.p_user_data = p_user_data;
    listener_to_remove.is_asynchronous = true;

    libvlc_event_async_ensure_listener_removal(p_event_manager, &listener_to_remove);

    assert(found);
}
