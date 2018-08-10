/*****************************************************************************
 * preparser.c
 *****************************************************************************
 * Copyright © 2017-2017 VLC authors and VideoLAN
 * $Id$
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

#include <vlc_common.h>
#include <vlc_atomic.h>

#include "misc/background_worker.h"
#include "input/input_interface.h"
#include "input/input_internal.h"
#include "preparser.h"
#include "fetcher.h"

struct input_preparser_t
{
    vlc_object_t* owner;
    input_fetcher_t* fetcher;
    struct background_worker* worker;
    atomic_bool deactivated;
};

typedef struct input_preparser_req_t
{
    input_item_t *item;
    const input_preparser_callbacks_t *cbs;
    void *userdata;
    vlc_atomic_rc_t rc;
} input_preparser_req_t;

typedef struct input_preparser_task_t
{
    input_preparser_req_t *req;
    input_preparser_t* preparser;
    input_thread_t* input;
    atomic_int state;
    atomic_bool done;
} input_preparser_task_t;

static input_preparser_req_t *ReqCreate(input_item_t *item,
                                        const input_preparser_callbacks_t *cbs,
                                        void *userdata)
{
    input_preparser_req_t *req = malloc(sizeof(*req));
    if (unlikely(!req))
        return NULL;

    req->item = item;
    req->cbs = cbs;
    req->userdata = userdata;
    vlc_atomic_rc_init(&req->rc);

    input_item_Hold(item);

    return req;
}

static void ReqHold(input_preparser_req_t *req)
{
    vlc_atomic_rc_inc(&req->rc);
}

static void ReqRelease(input_preparser_req_t *req)
{
    if (vlc_atomic_rc_dec(&req->rc))
    {
        input_item_Release(req->item);
        free(req);
    }
}

static void InputEvent( input_thread_t *input, void *task_,
                        const struct vlc_input_event *event )
{
    VLC_UNUSED( input );
    input_preparser_task_t* task = task_;

    switch( event->type )
    {
        case INPUT_EVENT_STATE:
            atomic_store( &task->state, event->state );
            break;

        case INPUT_EVENT_DEAD:
            atomic_store( &task->done, true );
            background_worker_RequestProbe( task->preparser->worker );
            break;
        case INPUT_EVENT_SUBITEMS:
        {
            input_preparser_req_t *req = task->req;
            if (req->cbs && req->cbs->on_subtree_added)
                req->cbs->on_subtree_added(req->item, event->subitems, req->userdata);
            break;
        }
        default: ;
    }
}

static int PreparserOpenInput( void* preparser_, void* req_, void** out )
{
    input_preparser_t* preparser = preparser_;
    input_preparser_req_t *req = req_;
    input_preparser_task_t* task = malloc( sizeof *task );

    if( unlikely( !task ) )
        goto error;

    atomic_init( &task->state, INIT_S );
    atomic_init( &task->done, false );

    task->preparser = preparser_;
    task->input = input_CreatePreparser( preparser->owner, InputEvent,
                                         task, req->item );
    if( !task->input )
        goto error;

    task->req = req;

    if( input_Start( task->input ) )
    {
        input_Close( task->input );
        goto error;
    }

    *out = task;

    return VLC_SUCCESS;

error:
    free( task );
    /* TODO remove legacy input_item_SignalPreparseEnded() */
    input_item_SignalPreparseEnded(req->item, ITEM_PREPARSE_FAILED);
    if (req->cbs && req->cbs->on_preparse_ended)
        req->cbs->on_preparse_ended(req->item, ITEM_PREPARSE_FAILED, req->userdata);
    return VLC_EGENERIC;
}

static int PreparserProbeInput( void* preparser_, void* task_ )
{
    input_preparser_task_t* task = task_;
    return atomic_load( &task->done );
    VLC_UNUSED( preparser_ );
}

static void PreparserCloseInput( void* preparser_, void* task_ )
{
    input_preparser_task_t* task = task_;
    input_preparser_req_t *req = task->req;

    input_preparser_t* preparser = preparser_;
    input_thread_t* input = task->input;
    input_item_t* item = input_priv(task->input)->p_item;

    int status;
    switch( atomic_load( &task->state ) )
    {
        case END_S:
            status = ITEM_PREPARSE_DONE;
            break;
        case ERROR_S:
            status = ITEM_PREPARSE_FAILED;
            break;
        default:
            status = ITEM_PREPARSE_TIMEOUT;
    }

    input_Stop( input );
    input_Close( input );

    free( task );

    if( preparser->fetcher )
    {
        if( !input_fetcher_Push( preparser->fetcher, item, 0, status ) )
            return;
    }

    input_item_SetPreparsed( item, true );
    /* TODO remove legacy input_item_SignalPreparseEnded() */
    input_item_SignalPreparseEnded( item, status );
    if (req->cbs && req->cbs->on_preparse_ended)
        req->cbs->on_preparse_ended(req->item, status, req->userdata);
}

static void ReqHoldVoid(void *item) { ReqHold(item); }
static void ReqReleaseVoid(void *item) { ReqRelease(item); }

input_preparser_t* input_preparser_New( vlc_object_t *parent )
{
    input_preparser_t* preparser = malloc( sizeof *preparser );

    struct background_worker_config conf = {
        .default_timeout = var_InheritInteger( parent, "preparse-timeout" ),
        .max_threads = var_InheritInteger( parent, "preparse-threads" ),
        .pf_start = PreparserOpenInput,
        .pf_probe = PreparserProbeInput,
        .pf_stop = PreparserCloseInput,
        .pf_release = ReqReleaseVoid,
        .pf_hold = ReqHoldVoid
    };


    if( likely( preparser ) )
        preparser->worker = background_worker_New( preparser, &conf );

    if( unlikely( !preparser || !preparser->worker ) )
    {
        free( preparser );
        return NULL;
    }

    preparser->owner = parent;
    preparser->fetcher = input_fetcher_New( parent );
    atomic_init( &preparser->deactivated, false );

    if( unlikely( !preparser->fetcher ) )
        msg_Warn( parent, "unable to create art fetcher" );

    return preparser;
}

void input_preparser_Push( input_preparser_t *preparser,
    input_item_t *item, input_item_meta_request_option_t i_options,
    const input_preparser_callbacks_t *cbs, void *cbs_userdata,
    int timeout, void *id )
{
    if( atomic_load( &preparser->deactivated ) )
        return;

    vlc_mutex_lock( &item->lock );
    enum input_item_type_e i_type = item->i_type;
    int b_net = item->b_net;
    vlc_mutex_unlock( &item->lock );

    switch( i_type )
    {
        case ITEM_TYPE_NODE:
        case ITEM_TYPE_FILE:
        case ITEM_TYPE_DIRECTORY:
        case ITEM_TYPE_PLAYLIST:
            if( !b_net || i_options & META_REQUEST_OPTION_SCOPE_NETWORK )
                break;
            /* fallthrough */
        default:
            /* TODO remove legacy input_item_SignalPreparseEnded() */
            input_item_SignalPreparseEnded( item, ITEM_PREPARSE_SKIPPED );
            if (cbs && cbs->on_preparse_ended)
                cbs->on_preparse_ended(item, ITEM_PREPARSE_SKIPPED, cbs_userdata);
            return;
    }

    struct input_preparser_req_t *req = ReqCreate(item, cbs, cbs_userdata);

    if (background_worker_Push(preparser->worker, req, id, timeout))
    {
        /* TODO remove legacy input_item_SignalPreparseEnded() */
        input_item_SignalPreparseEnded( item, ITEM_PREPARSE_FAILED );
        if (req->cbs && cbs->on_preparse_ended)
            cbs->on_preparse_ended(item, ITEM_PREPARSE_FAILED, cbs_userdata);
    }

    ReqRelease(req);
}

void input_preparser_fetcher_Push( input_preparser_t *preparser,
    input_item_t *item, input_item_meta_request_option_t options )
{
    if( preparser->fetcher )
        input_fetcher_Push( preparser->fetcher, item, options, -1 );
}

void input_preparser_Cancel( input_preparser_t *preparser, void *id )
{
    background_worker_Cancel( preparser->worker, id );
}

void input_preparser_Deactivate( input_preparser_t* preparser )
{
    atomic_store( &preparser->deactivated, true );
    background_worker_Cancel( preparser->worker, NULL );
}

void input_preparser_Delete( input_preparser_t *preparser )
{
    background_worker_Delete( preparser->worker );

    if( preparser->fetcher )
        input_fetcher_Delete( preparser->fetcher );

    free( preparser );
}
