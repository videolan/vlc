/*****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
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
#  include "config.h"
#endif

#include <assert.h>
#include <vlc_common.h>
#include <vlc_threads.h>
#include <vlc_arrays.h>

#include "libvlc.h"
#include "background_worker.h"

struct bg_queued_item {
    void* id; /**< id associated with entity */
    void* entity; /**< the entity to process */
    int timeout; /**< timeout duration in microseconds */
};

struct background_worker {
    void* owner;
    struct background_worker_config conf;

    vlc_mutex_t lock; /**< acquire to inspect members that follow */
    struct {
        bool probe_request; /**< true if a probe is requested */
        vlc_cond_t wait; /**< wait for update in terms of head */
        vlc_cond_t worker_wait; /**< wait for probe request or cancelation */
        mtime_t deadline; /**< deadline of the current task */
        void* id; /**< id of the current task */
        bool active; /**< true if there is an active thread */
    } head;

    struct {
        vlc_cond_t wait; /**< wait for update in terms of tail */
        vlc_array_t data; /**< queue of pending entities to process */
    } tail;
};

static void* Thread( void* data )
{
    struct background_worker* worker = data;

    for( ;; )
    {
        struct bg_queued_item* item = NULL;
        void* handle;

        vlc_mutex_lock( &worker->lock );
        for( ;; )
        {
            if( vlc_array_count( &worker->tail.data ) )
            {
                item = vlc_array_item_at_index( &worker->tail.data, 0 );
                handle = NULL;

                vlc_array_remove( &worker->tail.data, 0 );
            }

            if( worker->head.deadline == VLC_TS_0 && item == NULL )
                worker->head.active = false;
            worker->head.id = item ? item->id : NULL;
            vlc_cond_broadcast( &worker->head.wait );

            if( item )
            {
                if( item->timeout > 0 )
                    worker->head.deadline = mdate() + item->timeout * 1000;
                else
                    worker->head.deadline = INT64_MAX;
            }
            else if( worker->head.deadline != VLC_TS_0 )
            {
                /* Wait 1 seconds for new inputs before terminating */
                mtime_t deadline = mdate() + INT64_C(1000000);
                int ret = vlc_cond_timedwait( &worker->tail.wait,
                                              &worker->lock, deadline );
                if( ret != 0 )
                {
                    /* Timeout: if there is still no items, the thread will be
                     * terminated at next loop iteration (active = false). */
                    worker->head.deadline = VLC_TS_0;
                }
                continue;
            }
            break;
        }

        if( !worker->head.active )
        {
            vlc_mutex_unlock( &worker->lock );
            break;
        }
        vlc_mutex_unlock( &worker->lock );

        assert( item != NULL );

        if( worker->conf.pf_start( worker->owner, item->entity, &handle ) )
        {
            worker->conf.pf_release( item->entity );
            free( item );
            continue;
        }

        for( ;; )
        {
            vlc_mutex_lock( &worker->lock );

            bool const b_timeout = worker->head.deadline <= mdate();
            worker->head.probe_request = false;

            vlc_mutex_unlock( &worker->lock );

            if( b_timeout ||
                worker->conf.pf_probe( worker->owner, handle ) )
            {
                worker->conf.pf_stop( worker->owner, handle );
                worker->conf.pf_release( item->entity );
                free( item );
                break;
            }

            vlc_mutex_lock( &worker->lock );
            if( worker->head.probe_request == false &&
                worker->head.deadline > mdate() )
            {
                vlc_cond_timedwait( &worker->head.worker_wait, &worker->lock,
                                     worker->head.deadline );
            }
            vlc_mutex_unlock( &worker->lock );
        }
    }

    return NULL;
}

static void BackgroundWorkerCancel( struct background_worker* worker, void* id)
{
    vlc_mutex_lock( &worker->lock );
    for( size_t i = 0; i < vlc_array_count( &worker->tail.data ); )
    {
        struct bg_queued_item* item =
            vlc_array_item_at_index( &worker->tail.data, i );

        if( id == NULL || item->id == id )
        {
            vlc_array_remove( &worker->tail.data, i );
            worker->conf.pf_release( item->entity );
            free( item );
            continue;
        }

        ++i;
    }

    while( ( id == NULL && worker->head.active )
        || ( id != NULL && worker->head.id == id ) )
    {
        worker->head.deadline = VLC_TS_0;
        vlc_cond_signal( &worker->head.worker_wait );
        vlc_cond_signal( &worker->tail.wait );
        vlc_cond_wait( &worker->head.wait, &worker->lock );
    }
    vlc_mutex_unlock( &worker->lock );
}

struct background_worker* background_worker_New( void* owner,
    struct background_worker_config* conf )
{
    struct background_worker* worker = malloc( sizeof *worker );

    if( unlikely( !worker ) )
        return NULL;

    worker->conf = *conf;
    worker->owner = owner;
    worker->head.id = NULL;
    worker->head.active = false;
    worker->head.deadline = VLC_TS_INVALID;

    vlc_mutex_init( &worker->lock );
    vlc_cond_init( &worker->head.wait );
    vlc_cond_init( &worker->head.worker_wait );

    vlc_array_init( &worker->tail.data );
    vlc_cond_init( &worker->tail.wait );

    return worker;
}

int background_worker_Push( struct background_worker* worker, void* entity,
                        void* id, int timeout )
{
    struct bg_queued_item* item = malloc( sizeof( *item ) );

    if( unlikely( !item ) )
        return VLC_EGENERIC;

    item->id = id;
    item->entity = entity;
    item->timeout = timeout < 0 ? worker->conf.default_timeout : timeout;

    vlc_mutex_lock( &worker->lock );
    int i_ret = vlc_array_append( &worker->tail.data, item );
    vlc_cond_signal( &worker->tail.wait );
    if( i_ret != 0 )
    {
        free( item );
        return VLC_EGENERIC;
    }

    if( worker->head.active == false )
    {
        worker->head.probe_request = false;
        worker->head.active =
            !vlc_clone_detach( NULL, Thread, worker, VLC_THREAD_PRIORITY_LOW );
    }

    if( worker->head.active )
        worker->conf.pf_hold( item->entity );

    int ret = worker->head.active ? VLC_SUCCESS : VLC_EGENERIC;
    vlc_mutex_unlock( &worker->lock );

    return ret;
}

void background_worker_Cancel( struct background_worker* worker, void* id )
{
    BackgroundWorkerCancel( worker, id );
}

void background_worker_RequestProbe( struct background_worker* worker )
{
    vlc_mutex_lock( &worker->lock );
    worker->head.probe_request = true;
    vlc_cond_signal( &worker->head.worker_wait );
    vlc_mutex_unlock( &worker->lock );
}

void background_worker_Delete( struct background_worker* worker )
{
    BackgroundWorkerCancel( worker, NULL );
    vlc_array_clear( &worker->tail.data );
    vlc_mutex_destroy( &worker->lock );
    vlc_cond_destroy( &worker->head.wait );
    vlc_cond_destroy( &worker->head.worker_wait );
    vlc_cond_destroy( &worker->tail.wait );
    free( worker );
}
