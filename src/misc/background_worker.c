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

    struct {
        bool probe_request; /**< true if a probe is requested */
        vlc_mutex_t lock; /**< acquire to inspect members that follow */
        vlc_cond_t wait; /**< wait for update in terms of head */
        mtime_t deadline; /**< deadline of the current task */
        void* id; /**< id of the current task */
        bool active; /**< true if there is an active thread */
    } head;

    struct {
        vlc_mutex_t lock; /**< acquire to inspect members that follow */
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

        vlc_mutex_lock( &worker->tail.lock );
        {
            if( vlc_array_count( &worker->tail.data ) )
            {
                item = vlc_array_item_at_index( &worker->tail.data, 0 );
                handle = NULL;

                vlc_array_remove( &worker->tail.data, 0 );
            }

            vlc_mutex_lock( &worker->head.lock );
            {
                worker->head.deadline = INT64_MAX;
                worker->head.active = item != NULL;
                worker->head.id = item ? item->id : NULL;

                if( item && item->timeout > 0 )
                    worker->head.deadline = mdate() + item->timeout * 1000;
            }
            vlc_cond_broadcast( &worker->head.wait );
            vlc_mutex_unlock( &worker->head.lock );
        }
        vlc_mutex_unlock( &worker->tail.lock );

        if( item == NULL )
            break;

        if( worker->conf.pf_start( worker->owner, item->entity, &handle ) )
        {
            worker->conf.pf_release( item->entity );
            free( item );
            continue;
        }

        for( ;; )
        {
            vlc_mutex_lock( &worker->head.lock );

            bool const b_timeout = worker->head.deadline <= mdate();
            worker->head.probe_request = false;

            vlc_mutex_unlock( &worker->head.lock );

            if( b_timeout ||
                worker->conf.pf_probe( worker->owner, handle ) )
            {
                worker->conf.pf_stop( worker->owner, handle );
                worker->conf.pf_release( item->entity );
                free( item );
                break;
            }

            vlc_mutex_lock( &worker->head.lock );
            if( worker->head.probe_request == false &&
                worker->head.deadline > mdate() )
            {
                vlc_cond_timedwait( &worker->head.wait, &worker->head.lock,
                                     worker->head.deadline );
            }
            vlc_mutex_unlock( &worker->head.lock );
        }
    }

    return NULL;
}

static void BackgroundWorkerCancel( struct background_worker* worker, void* id)
{
    vlc_mutex_lock( &worker->tail.lock );
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
    vlc_mutex_unlock( &worker->tail.lock );

    vlc_mutex_lock( &worker->head.lock );
    while( ( id == NULL && worker->head.active )
        || ( id != NULL && worker->head.id == id ) )
    {
        worker->head.deadline = VLC_TS_0;
        vlc_cond_broadcast( &worker->head.wait );
        vlc_cond_wait( &worker->head.wait, &worker->head.lock );
    }
    vlc_mutex_unlock( &worker->head.lock );
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

    vlc_mutex_init( &worker->head.lock );
    vlc_cond_init( &worker->head.wait );

    vlc_array_init( &worker->tail.data );
    vlc_mutex_init( &worker->tail.lock );

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

    vlc_mutex_lock( &worker->tail.lock );
    vlc_array_append( &worker->tail.data, item );
    vlc_mutex_unlock( &worker->tail.lock );

    vlc_mutex_lock( &worker->head.lock );
    if( worker->head.active == false )
    {
        worker->head.probe_request = false;
        worker->head.active =
            !vlc_clone_detach( NULL, Thread, worker, VLC_THREAD_PRIORITY_LOW );
    }

    if( worker->head.active )
        worker->conf.pf_hold( item->entity );

    int ret = worker->head.active ? VLC_SUCCESS : VLC_EGENERIC;
    vlc_mutex_unlock( &worker->head.lock );

    return ret;
}

void background_worker_Cancel( struct background_worker* worker, void* id )
{
    BackgroundWorkerCancel( worker, id );
}

void background_worker_RequestProbe( struct background_worker* worker )
{
    vlc_mutex_lock( &worker->head.lock );
    worker->head.probe_request = true;
    vlc_cond_broadcast( &worker->head.wait );
    vlc_mutex_unlock( &worker->head.lock );
}

void background_worker_Delete( struct background_worker* worker )
{
    BackgroundWorkerCancel( worker, NULL );
    vlc_array_clear( &worker->tail.data );
    free( worker );
}
