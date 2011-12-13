/*****************************************************************************
 * event_async.c: New libvlc event control API
 *****************************************************************************
 * Copyright (C) 2007 VLC authors and VideoLAN
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

#include <vlc/libvlc.h>

#include "libvlc_internal.h"
#include "event_internal.h"

struct queue_elmt {
    libvlc_event_listener_t listener;
    libvlc_event_t event;
    struct queue_elmt * next;
};

struct libvlc_event_async_queue {
    struct queue_elmt *first_elmt, *last_elmt;
    vlc_mutex_t lock;
    vlc_cond_t signal;
    vlc_thread_t thread;
    bool is_idle;
    vlc_cond_t signal_idle;
    vlc_threadvar_t is_asynch_dispatch_thread_var;
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

static inline bool current_thread_is_asynch_thread(libvlc_event_manager_t * p_em)
{
    return vlc_threadvar_get(queue(p_em)->is_asynch_dispatch_thread_var)
            != NULL;
}

/* Lock must be held */
static void push(libvlc_event_manager_t * p_em,
                 libvlc_event_listener_t * listener, libvlc_event_t * event)
{
    struct queue_elmt * elmt = malloc(sizeof(struct queue_elmt));
    elmt->listener = *listener;
    elmt->event = *event;
    elmt->next = NULL;

    /* Append to the end of the queue */
    if(!queue(p_em)->first_elmt)
        queue(p_em)->first_elmt = elmt;
    else
        queue(p_em)->last_elmt->next = elmt;
    queue(p_em)->last_elmt = elmt;
}

static inline void queue_lock(libvlc_event_manager_t * p_em)
{
    vlc_mutex_lock(&queue(p_em)->lock);
}

static inline void queue_unlock(libvlc_event_manager_t * p_em)
{
    vlc_mutex_unlock(&queue(p_em)->lock);
}

/* Lock must be held */
static bool pop(libvlc_event_manager_t * p_em,
                libvlc_event_listener_t * listener, libvlc_event_t * event)
{
    if(!queue(p_em)->first_elmt)
        return false; /* No first_elmt */

    struct queue_elmt * elmt = queue(p_em)->first_elmt;
    *listener = elmt->listener;
    *event = elmt->event;

    queue(p_em)->first_elmt = elmt->next;
    if( !elmt->next ) queue(p_em)->last_elmt=NULL;

    free(elmt);
    return true;
}

/* Lock must be held */
static void pop_listener(libvlc_event_manager_t * p_em, libvlc_event_listener_t * listener)
{
    struct queue_elmt * iter = queue(p_em)->first_elmt;
    struct queue_elmt * prev = NULL;
    while (iter) {
        if(listeners_are_equal(&iter->listener, listener))
        {
            struct queue_elmt * to_delete = iter;
            if(!prev)
                queue(p_em)->first_elmt = to_delete->next;
            else
                prev->next = to_delete->next;
            iter = to_delete->next;
            free(to_delete);
        }
        else {
            prev = iter;
            iter = iter->next;
        }
    }
    queue(p_em)->last_elmt=prev;
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

    if(current_thread_is_asynch_thread(p_em))
    {
        fprintf(stderr, "*** Error: releasing the last reference of the observed object from its callback thread is not (yet!) supported\n");
        abort();
    }

    vlc_thread_t thread = queue(p_em)->thread;
    if(thread)
    {
        vlc_cancel(thread);
        vlc_join(thread, NULL);
    }

    vlc_mutex_destroy(&queue(p_em)->lock);
    vlc_cond_destroy(&queue(p_em)->signal);
    vlc_cond_destroy(&queue(p_em)->signal_idle);
    vlc_threadvar_delete(&queue(p_em)->is_asynch_dispatch_thread_var);

    struct queue_elmt * iter = queue(p_em)->first_elmt;
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

    int error = vlc_threadvar_create(&queue(p_em)->is_asynch_dispatch_thread_var, NULL);
    assert(!error);

    vlc_mutex_init(&queue(p_em)->lock);
    vlc_cond_init(&queue(p_em)->signal);
    vlc_cond_init(&queue(p_em)->signal_idle);

    error = vlc_clone (&queue(p_em)->thread, event_async_loop, p_em, VLC_THREAD_PRIORITY_LOW);
    if(error)
    {
        free(p_em->async_event_queue);
        p_em->async_event_queue = NULL;
        return;
    }

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

    queue_lock(p_em);
    pop_listener(p_em, listener);

    // Wait for the asynch_loop to have processed all events.
    if(!current_thread_is_asynch_thread(p_em))
    {
        while(!queue(p_em)->is_idle)
            vlc_cond_wait(&queue(p_em)->signal_idle, &queue(p_em)->lock);
    }
    queue_unlock(p_em);
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

    queue_lock(p_em);
    push(p_em, listener, event);
    vlc_cond_signal(&queue(p_em)->signal);
    queue_unlock(p_em);
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

    vlc_threadvar_set(queue(p_em)->is_asynch_dispatch_thread_var, p_em);

    queue_lock(p_em);
    while (true) {
        int has_listener = pop(p_em, &listener, &event);

        if (has_listener)
        {
            queue_unlock(p_em);
            listener.pf_callback(&event, listener.p_user_data); // This might edit the queue
            queue_lock(p_em);
        }
        else
        {
            queue(p_em)->is_idle = true;

            mutex_cleanup_push(&queue(p_em)->lock);
            vlc_cond_broadcast(&queue(p_em)->signal_idle); // We'll be idle
            vlc_cond_wait(&queue(p_em)->signal, &queue(p_em)->lock);
            vlc_cleanup_pop();

            queue(p_em)->is_idle = false;
        }
    }
    queue_unlock(p_em);
    return NULL;
}
