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
 *        libvlc_event_manager_t event_manager;
 *        ...
 * }
 *
 * libvlc_my_cool_object_new()
 * {
 *        ...
 *        libvlc_event_manager_init(&p_self->event_manager, p_self)
 *        ...
 * }
 *
 * libvlc_my_cool_object_release()
 * {
 *         ...
 *         libvlc_event_manager_release(&p_self->event_manager);
 *         ...
 * }
 *
 * libvlc_my_cool_object_do_something()
 * {
 *        ...
 *        libvlc_event_t event;
 *        event.type = libvlc_MyCoolObjectDidSomething;
 *        event.u.my_cool_object_did_something.what_it_did = kSomething;
 *        libvlc_event_send(&p_self->event_manager, &event);
 * }
 * */

typedef struct libvlc_event_listener_t
{
    libvlc_event_type_t event_type;
    void *              p_user_data;
    libvlc_callback_t   pf_callback;
} libvlc_event_listener_t;

/*
 * Internal libvlc functions
 */

void libvlc_event_manager_init(libvlc_event_manager_t *em, void *obj)
{
    em->p_obj = obj;
    vlc_array_init(&em->listeners);
    vlc_mutex_init_recursive(&em->lock);
}

void libvlc_event_manager_destroy(libvlc_event_manager_t *em)
{
    vlc_mutex_destroy(&em->lock);

    for (size_t i = 0; i < vlc_array_count(&em->listeners); i++)
        free(vlc_array_item_at_index(&em->listeners, i));

    vlc_array_clear(&em->listeners);
}

/**************************************************************************
 *       libvlc_event_send (internal) :
 *
 * Send a callback.
 **************************************************************************/
void libvlc_event_send( libvlc_event_manager_t * p_em,
                        libvlc_event_t * p_event )
{
    /* Fill event with the sending object now */
    p_event->p_obj = p_em->p_obj;

    vlc_mutex_lock(&p_em->lock);
    for (size_t i = 0; i < vlc_array_count(&p_em->listeners); i++)
    {
        libvlc_event_listener_t *listener;

        listener = vlc_array_item_at_index(&p_em->listeners, i);
        if (listener->event_type == p_event->type)
            listener->pf_callback(p_event, listener->p_user_data);
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
    DEF(MediaPlayerChapterChanged)

    DEF(MediaListItemAdded)
    DEF(MediaListWillAddItem)
    DEF(MediaListItemDeleted)
    DEF(MediaListWillDeleteItem)
    DEF(MediaListEndReached)

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
 *       libvlc_event_attach (public) :
 *
 * Add a callback for an event.
 **************************************************************************/
int libvlc_event_attach(libvlc_event_manager_t *em, libvlc_event_type_t type,
                        libvlc_callback_t callback, void *opaque)
{
    libvlc_event_listener_t *listener = malloc(sizeof (*listener));
    if (unlikely(listener == NULL))
        return ENOMEM;

    listener->event_type = type;
    listener->p_user_data = opaque;
    listener->pf_callback = callback;

    int i_ret;
    vlc_mutex_lock(&em->lock);
    if(vlc_array_append(&em->listeners, listener) != 0)
    {
        i_ret = VLC_EGENERIC;
        free(listener);
    }
    else
        i_ret = VLC_SUCCESS;
    vlc_mutex_unlock(&em->lock);
    return i_ret;
}

/**************************************************************************
 *       libvlc_event_detach (public) :
 *
 * Remove a callback for an event.
 **************************************************************************/
void libvlc_event_detach(libvlc_event_manager_t *em, libvlc_event_type_t type,
                         libvlc_callback_t callback, void *opaque)
{
    vlc_mutex_lock(&em->lock);
    for (size_t i = 0; i < vlc_array_count(&em->listeners); i++)
    {
         libvlc_event_listener_t *listener;

         listener = vlc_array_item_at_index(&em->listeners, i);

         if (listener->event_type == type
          && listener->pf_callback == callback
          && listener->p_user_data == opaque)
         {   /* that's our listener */
             vlc_array_remove(&em->listeners, i);
             vlc_mutex_unlock(&em->lock);
             free(listener);
             return;
         }
    }
    abort();
}
