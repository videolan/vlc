/*****************************************************************************
 * event.c: New libvlc event control API
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id $
 *
 * Authors: Filippo Carone <filippo@carone.org>
 *          Pierre d'Herbemont <pdherbemont # videolan.org>
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

#include <vlc/libvlc.h>

#include "libvlc_internal.h"
#include "event_internal.h"

struct queue_elmt {
    libvlc_event_listener_t listener;
    libvlc_event_t event;
    struct queue_elmt * next;
};

struct libvlc_event_async_queue {
    struct queue_elmt * elements;
    vlc_mutex_t lock;
    vlc_cond_t signal;
    vlc_thread_t thread;
};

/*
 * Utilities
 */

static void*  event_async_loop(void * arg);

static inline struct libvlc_event_async_queue * queue(libvlc_event_manager_t * p_em)
{
    return p_em->async_event_queue;
}

static inline bool is_queue_initialized(libvlc_event_manager_t * p_em)
{
    return queue(p_em) != NULL;
}

/* Lock must be held */
static void push(libvlc_event_manager_t * p_em, libvlc_event_listener_t * listener, libvlc_event_t * event)
{
#ifndef NDEBUG
    static const long MaxQueuedItem = 300000;
    long count = 0;
#endif
    
    struct queue_elmt * elmt = malloc(sizeof(struct queue_elmt));
    elmt->listener = *listener;
    elmt->event = *event;
    elmt->next = NULL;
    
    /* Append to the end of the queue */
    struct queue_elmt * iter = queue(p_em)->elements;
    if(!iter)
    {
        queue(p_em)->elements = elmt;
        return;
    }

    while (iter->next) {
        iter = iter->next;
#ifndef NDEBUG
        if(count++ > MaxQueuedItem)
        {
            fprintf(stderr, "Warning: libvlc event overflow.\n");
            abort();
        }
#endif
    }
    iter->next = elmt;
}

/* Lock must be held */
static bool pop(libvlc_event_manager_t * p_em, libvlc_event_listener_t * listener, libvlc_event_t * event)
{
    if(!queue(p_em)->elements)
        return false; /* No elements */

    *listener = queue(p_em)->elements->listener;
    *event = queue(p_em)->elements->event;
    
    struct queue_elmt * elmt = queue(p_em)->elements;
    queue(p_em)->elements = elmt->next;
    free(elmt);
    return true;
}

/* Lock must be held */
static void pop_listener(libvlc_event_manager_t * p_em, libvlc_event_listener_t * listener)
{
    struct queue_elmt * iter = queue(p_em)->elements;
    struct queue_elmt * prev = NULL;
    while (iter) {
        if(listeners_are_equal(&iter->listener, listener))
        {
            if(!prev)
                queue(p_em)->elements = iter->next;
            else
                prev->next = iter->next;
            free(iter);
        }
        prev = iter;
        iter = iter->next;
    }
}

/**************************************************************************
 *       libvlc_event_async_fini (internal) :
 *
 * Destroy what might have been created by.
 **************************************************************************/
void
libvlc_event_async_fini(libvlc_event_manager_t * p_em)
{    
    if(!is_queue_initialized(p_em)) return;
    
    vlc_thread_t thread = queue(p_em)->thread;
    if(thread)
    {
        vlc_cancel(thread);
        vlc_join(thread, NULL);
    }

    vlc_mutex_destroy(&queue(p_em)->lock);
    vlc_cond_destroy(&queue(p_em)->signal);

    struct queue_elmt * iter = queue(p_em)->elements;
    while (iter) {
        struct queue_elmt * elemt_to_delete = iter;
        iter = iter->next;
        free(elemt_to_delete);
    }
    
    free(queue(p_em));
}

/**************************************************************************
 *       libvlc_event_async_init (private) :
 *
 * Destroy what might have been created by.
 **************************************************************************/
static void
libvlc_event_async_init(libvlc_event_manager_t * p_em)
{
    p_em->async_event_queue = calloc(1, sizeof(struct libvlc_event_async_queue));

    int error = vlc_clone (&queue(p_em)->thread, event_async_loop, p_em, VLC_THREAD_PRIORITY_LOW);
    if(error)
    {
        free(p_em->async_event_queue);
        p_em->async_event_queue = NULL;
        return;
    }

    vlc_mutex_init_recursive(&queue(p_em)->lock); // Beware, this is re-entrant
    vlc_cond_init(&queue(p_em)->signal);
}

/**************************************************************************
 *       libvlc_event_async_ensure_listener_removal (internal) :
 *
 * Make sure no more message will be issued to the listener.
 **************************************************************************/
void
libvlc_event_async_ensure_listener_removal(libvlc_event_manager_t * p_em, libvlc_event_listener_t * listener)
{
    if(!is_queue_initialized(p_em)) return;

    vlc_mutex_lock(&queue(p_em)->lock);
    pop_listener(p_em, listener);
    vlc_mutex_unlock(&queue(p_em)->lock);
}

/**************************************************************************
 *       libvlc_event_async_dispatch (internal) :
 *
 * Send an event in an asynchronous way.
 **************************************************************************/
void
libvlc_event_async_dispatch(libvlc_event_manager_t * p_em, libvlc_event_listener_t * listener, libvlc_event_t * event)
{
    // We do a lazy init here, to prevent constructing the thread when not needed.
    vlc_mutex_lock(&p_em->object_lock);
    if(!queue(p_em))
        libvlc_event_async_init(p_em);
    vlc_mutex_unlock(&p_em->object_lock);

    vlc_mutex_lock(&queue(p_em)->lock);
    push(p_em, listener, event);
    vlc_cond_signal(&queue(p_em)->signal);
    vlc_mutex_unlock(&queue(p_em)->lock);
}

/**************************************************************************
 *       event_async_loop (private) :
 *
 * Send queued events.
 **************************************************************************/
static void * event_async_loop(void * arg)
{
    libvlc_event_manager_t * p_em = arg;
    libvlc_event_listener_t listener;
    libvlc_event_t event;

    vlc_mutex_lock(&queue(p_em)->lock);
    vlc_cleanup_push(vlc_cleanup_lock, &queue(p_em)->lock);
    while (true) {
        int has_listener = pop(p_em, &listener, &event);
        if (has_listener)
            listener.pf_callback( &event, listener.p_user_data ); // This might edit the queue, ->lock is recursive
        else
            vlc_cond_wait(&queue(p_em)->signal, &queue(p_em)->lock);
    }
    vlc_cleanup_pop();
    vlc_mutex_unlock(&queue(p_em)->lock);
    return NULL;
}
