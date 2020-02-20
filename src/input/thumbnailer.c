/*****************************************************************************
 * thumbnailer.c: Thumbnailing API
 *****************************************************************************
 * Copyright (C) 2018 VLC authors and VideoLAN
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
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

#include <vlc_thumbnailer.h>
#include "input_internal.h"
#include "misc/background_worker.h"

struct vlc_thumbnailer_t
{
    vlc_object_t* parent;
    struct background_worker* worker;
};

typedef struct vlc_thumbnailer_params_t
{
    union
    {
        vlc_tick_t time;
        float pos;
    };
    enum
    {
        VLC_THUMBNAILER_SEEK_TIME,
        VLC_THUMBNAILER_SEEK_POS,
    } type;
    bool fast_seek;
    input_item_t* input_item;
    /**
     * A positive value will be used as the timeout duration
     * VLC_TICK_INVALID means no timeout
     */
    vlc_tick_t timeout;
    vlc_thumbnailer_cb cb;
    void* user_data;
} vlc_thumbnailer_params_t;

struct vlc_thumbnailer_request_t
{
    vlc_thumbnailer_t *thumbnailer;
    input_thread_t *input_thread;

    vlc_thumbnailer_params_t params;

    vlc_mutex_t lock;
    bool done;
};

static void
on_thumbnailer_input_event( input_thread_t *input,
                            const struct vlc_input_event *event, void *userdata )
{
    VLC_UNUSED(input);
    if ( event->type != INPUT_EVENT_THUMBNAIL_READY &&
         ( event->type != INPUT_EVENT_STATE || ( event->state.value != ERROR_S &&
                                                 event->state.value != END_S ) ) )
         return;

    vlc_thumbnailer_request_t* request = userdata;
    picture_t *pic = NULL;

    if ( event->type == INPUT_EVENT_THUMBNAIL_READY )
    {
        /*
         * Stop the input thread ASAP, delegate its release to
         * thumbnailer_request_Release
         */
        input_Stop( request->input_thread );
        pic = event->thumbnail;
    }
    vlc_mutex_lock( &request->lock );
    request->done = true;
    /*
     * If the request has not been cancelled, we can invoke the completion
     * callback.
     */
    if ( request->params.cb )
    {
        request->params.cb( request->params.user_data, pic );
        request->params.cb = NULL;
    }
    vlc_mutex_unlock( &request->lock );
    background_worker_RequestProbe( request->thumbnailer->worker );
}

static void thumbnailer_request_Hold( void* data )
{
    VLC_UNUSED(data);
}

static void thumbnailer_request_Release( void* data )
{
    vlc_thumbnailer_request_t* request = data;
    if ( request->input_thread )
        input_Close( request->input_thread );

    input_item_Release( request->params.input_item );
    free( request );
}

static int thumbnailer_request_Start( void* owner, void* entity, void** out )
{
    vlc_thumbnailer_t* thumbnailer = owner;
    vlc_thumbnailer_request_t* request = entity;
    input_thread_t* input = request->input_thread =
            input_CreateThumbnailer( thumbnailer->parent,
                                     on_thumbnailer_input_event, request,
                                     request->params.input_item );
    if ( unlikely( input == NULL ) )
    {
        request->params.cb( request->params.user_data, NULL );
        return VLC_EGENERIC;
    }
    if ( request->params.type == VLC_THUMBNAILER_SEEK_TIME )
    {
        input_SetTime( input, request->params.time,
                       request->params.fast_seek );
    }
    else
    {
        assert( request->params.type == VLC_THUMBNAILER_SEEK_POS );
        input_SetPosition( input, request->params.pos,
                       request->params.fast_seek );
    }
    if ( input_Start( input ) != VLC_SUCCESS )
    {
        request->params.cb( request->params.user_data, NULL );
        return VLC_EGENERIC;
    }
    *out = request;
    return VLC_SUCCESS;
}

static void thumbnailer_request_Stop( void* owner, void* handle )
{
    VLC_UNUSED(owner);

    vlc_thumbnailer_request_t *request = handle;
    vlc_mutex_lock( &request->lock );
    /*
     * If the callback hasn't been invoked yet, we assume a timeout and
     * signal it back to the user
     */
    if ( request->params.cb != NULL )
    {
        request->params.cb( request->params.user_data, NULL );
        request->params.cb = NULL;
    }
    vlc_mutex_unlock( &request->lock );
    assert( request->input_thread != NULL );
    input_Stop( request->input_thread );
}

static int thumbnailer_request_Probe( void* owner, void* handle )
{
    VLC_UNUSED(owner);
    vlc_thumbnailer_request_t *request = handle;
    vlc_mutex_lock( &request->lock );
    int res = request->done;
    vlc_mutex_unlock( &request->lock );
    return res;
}

static vlc_thumbnailer_request_t*
thumbnailer_RequestCommon( vlc_thumbnailer_t* thumbnailer,
                           const vlc_thumbnailer_params_t* params )
{
    vlc_thumbnailer_request_t *request = malloc( sizeof( *request ) );
    if ( unlikely( request == NULL ) )
        return NULL;
    request->thumbnailer = thumbnailer;
    request->input_thread = NULL;
    request->params = *(vlc_thumbnailer_params_t*)params;
    request->done = false;
    input_item_Hold( request->params.input_item );
    vlc_mutex_init( &request->lock );

    int timeout = params->timeout == VLC_TICK_INVALID ?
                0 : MS_FROM_VLC_TICK( params->timeout );
    if ( background_worker_Push( thumbnailer->worker, request, request,
                                  timeout ) != VLC_SUCCESS )
    {
        thumbnailer_request_Release( request );
        return NULL;
    }
    return request;
}

vlc_thumbnailer_request_t*
vlc_thumbnailer_RequestByTime( vlc_thumbnailer_t *thumbnailer,
                               vlc_tick_t time,
                               enum vlc_thumbnailer_seek_speed speed,
                               input_item_t *input_item, vlc_tick_t timeout,
                               vlc_thumbnailer_cb cb, void* user_data )
{
    return thumbnailer_RequestCommon( thumbnailer,
            &(const vlc_thumbnailer_params_t){
                .time = time,
                .type = VLC_THUMBNAILER_SEEK_TIME,
                .fast_seek = speed == VLC_THUMBNAILER_SEEK_FAST,
                .input_item = input_item,
                .timeout = timeout,
                .cb = cb,
                .user_data = user_data,
        });
}

vlc_thumbnailer_request_t*
vlc_thumbnailer_RequestByPos( vlc_thumbnailer_t *thumbnailer,
                              float pos, enum vlc_thumbnailer_seek_speed speed,
                              input_item_t *input_item, vlc_tick_t timeout,
                              vlc_thumbnailer_cb cb, void* user_data )
{
    return thumbnailer_RequestCommon( thumbnailer,
            &(const vlc_thumbnailer_params_t){
                .pos = pos,
                .type = VLC_THUMBNAILER_SEEK_POS,
                .fast_seek = speed == VLC_THUMBNAILER_SEEK_FAST,
                .input_item = input_item,
                .timeout = timeout,
                .cb = cb,
                .user_data = user_data,
        });
}

void vlc_thumbnailer_Cancel( vlc_thumbnailer_t* thumbnailer,
                             vlc_thumbnailer_request_t* req )
{
    vlc_mutex_lock( &req->lock );
    /* Ensure we won't invoke the callback if the input was running. */
    req->params.cb = NULL;
    vlc_mutex_unlock( &req->lock );
    background_worker_Cancel( thumbnailer->worker, req );
}

vlc_thumbnailer_t *vlc_thumbnailer_Create( vlc_object_t* parent)
{
    vlc_thumbnailer_t *thumbnailer = malloc( sizeof( *thumbnailer ) );
    if ( unlikely( thumbnailer == NULL ) )
        return NULL;
    thumbnailer->parent = parent;
    struct background_worker_config cfg = {
        .default_timeout = -1,
        .max_threads = 1,
        .pf_release = thumbnailer_request_Release,
        .pf_hold = thumbnailer_request_Hold,
        .pf_start = thumbnailer_request_Start,
        .pf_probe = thumbnailer_request_Probe,
        .pf_stop = thumbnailer_request_Stop,
    };
    thumbnailer->worker = background_worker_New( thumbnailer, &cfg );
    if ( unlikely( thumbnailer->worker == NULL ) )
    {
        free( thumbnailer );
        return NULL;
    }
    return thumbnailer;
}

void vlc_thumbnailer_Release( vlc_thumbnailer_t *thumbnailer )
{
    background_worker_Delete( thumbnailer->worker );
    free( thumbnailer );
}
