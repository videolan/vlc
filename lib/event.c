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

#include <assert.h>
#include <errno.h>

#include <vlc/libvlc.h>
#include "libvlc_internal.h"

#include <vlc_common.h>

/*
 * Event Handling
 */

/* Example usage
 *
 * struct libvlc_cool_object_t
 * {
 *        ...
 *        libvlc_event_manager_t * p_event_manager;
 *        ...
 * }
 *
 * libvlc_my_cool_object_new()
 * {
 *        ...
 *        p_self->p_event_manager = libvlc_event_manager_new( p_self )
 *        libvlc_event_manager_register_event_type(p_self->p_event_manager,
 *                libvlc_MyCoolObjectDidSomething, p_e)
 *        ...
 * }
 *
 * libvlc_my_cool_object_release()
 * {
 *         ...
 *         libvlc_event_manager_release( p_self->p_event_manager );
 *         ...
 * }
 *
 * libvlc_my_cool_object_do_something()
 * {
 *        ...
 *        libvlc_event_t event;
 *        event.type = libvlc_MyCoolObjectDidSomething;
 *        event.u.my_cool_object_did_something.what_it_did = kSomething;
 *        libvlc_event_send( p_self->p_event_manager, &event );
 * }
 * */

typedef struct libvlc_event_listener_t
{
    libvlc_event_type_t event_type;
    void *              p_user_data;
    libvlc_callback_t   pf_callback;
} libvlc_event_listener_t;

typedef struct libvlc_event_manager_t
{
    void * p_obj;
    vlc_array_t listeners_groups;
    vlc_mutex_t lock;
} libvlc_event_sender_t;

typedef struct libvlc_event_listeners_group_t
{
    libvlc_event_type_t event_type;
    vlc_array_t listeners;
} libvlc_event_listeners_group_t;

/*
 * Internal libvlc functions
 */

/**************************************************************************
 *       libvlc_event_manager_new (internal) :
 *
 * Init an object's event manager.
 **************************************************************************/
libvlc_event_manager_t *
libvlc_event_manager_new( void * p_obj )
{
    libvlc_event_manager_t * p_em;

    p_em = malloc(sizeof( libvlc_event_manager_t ));
    if( !p_em )
    {
        libvlc_printerr( "Not enough memory" );
        return NULL;
    }

    p_em->p_obj = p_obj;

    vlc_array_init( &p_em->listeners_groups );
    vlc_mutex_init_recursive(&p_em->lock);
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

    vlc_mutex_destroy(&p_em->lock);

    for( i = 0; i < vlc_array_count(&p_em->listeners_groups); i++)
    {
        p_lg = vlc_array_item_at_index( &p_em->listeners_groups, i );

        for( j = 0; j < vlc_array_count(&p_lg->listeners); j++)
            free( vlc_array_item_at_index( &p_lg->listeners, j ) );

        vlc_array_clear( &p_lg->listeners );
        free( p_lg );
    }
    vlc_array_clear( &p_em->listeners_groups );
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

    vlc_mutex_lock(&p_em->lock);
    vlc_array_append( &p_em->listeners_groups, listeners_group );
    vlc_mutex_unlock(&p_em->lock);
}

/**************************************************************************
 *       libvlc_event_send (internal) :
 *
 * Send a callback.
 **************************************************************************/
void libvlc_event_send( libvlc_event_manager_t * p_em,
                        libvlc_event_t * p_event )
{
    int i;

    /* Fill event with the sending object now */
    p_event->p_obj = p_em->p_obj;

    vlc_mutex_lock(&p_em->lock);
    for( i = 0; i < vlc_array_count(&p_em->listeners_groups); i++)
    {
        libvlc_event_listeners_group_t *listeners_group;

        listeners_group = vlc_array_item_at_index(&p_em->listeners_groups, i);
        if (listeners_group->event_type != p_event->type)
            continue;

        for (int j = 0; j < vlc_array_count(&listeners_group->listeners); j++)
        {
            libvlc_event_listener_t *listener;

            listener = vlc_array_item_at_index(&listeners_group->listeners, j);
            listener->pf_callback(p_event, listener->p_user_data);
        }
        break;
    }
    vlc_mutex_unlock(&p_em->lock);
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
    DEF(MediaPlayerScrambledChanged)
    DEF(MediaPlayerESAdded)
    DEF(MediaPlayerESDeleted)
    DEF(MediaPlayerESSelected)
    DEF(MediaPlayerCorked)
    DEF(MediaPlayerUncorked)
    DEF(MediaPlayerMuted)
    DEF(MediaPlayerUnmuted)
    DEF(MediaPlayerAudioVolume)
    DEF(MediaPlayerAudioDevice)

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
                  libvlc_callback_t pf_callback, void *p_user_data )
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

    vlc_mutex_lock(&p_event_manager->lock);
    for( i = 0; i < vlc_array_count(&p_event_manager->listeners_groups); i++ )
    {
        listeners_group = vlc_array_item_at_index(&p_event_manager->listeners_groups, i);
        if( listeners_group->event_type == listener->event_type )
        {
            vlc_array_append( &listeners_group->listeners, listener );
            vlc_mutex_unlock(&p_event_manager->lock);
            return 0;
        }
    }
    vlc_mutex_unlock(&p_event_manager->lock);

    free(listener);
    fprintf( stderr, "This object event manager doesn't know about '%s' events",
             libvlc_event_type_name(event_type) );
    vlc_assert_unreachable();
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
    return event_attach(p_event_manager, event_type, pf_callback, p_user_data);
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

    vlc_mutex_lock(&p_event_manager->lock);
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
                    free( listener );
                    vlc_array_remove( &listeners_group->listeners, j );
                    found = true;
                    break;
                }
            }
        }
    }
    vlc_mutex_unlock(&p_event_manager->lock);

    assert(found);
}
