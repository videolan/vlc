/*****************************************************************************
 * fetcher.c
 *****************************************************************************
 * Copyright © 2017-2017 VLC authors and VideoLAN
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
#include <vlc_stream.h>
#include <vlc_modules.h>
#include <vlc_interrupt.h>
#include <vlc_arrays.h>
#include <vlc_threads.h>
#include <vlc_memstream.h>
#include <vlc_meta_fetcher.h>

#include "art.h"
#include "libvlc.h"
#include "fetcher.h"
#include "input/input_interface.h"
#include "misc/background_worker.h"
#include "misc/interrupt.h"

struct input_fetcher_t {
    struct background_worker* local;
    struct background_worker* network;
    struct background_worker* downloader;

    vlc_dictionary_t album_cache;
    vlc_object_t* owner;
    vlc_mutex_t lock;
};

struct fetcher_request {
    input_item_t* item;
    vlc_atomic_rc_t rc;
    int options;
    const input_fetcher_callbacks_t *cbs;
    void *userdata;
};

struct fetcher_thread {
    void (*pf_worker)( input_fetcher_t*, struct fetcher_request* );

    struct background_worker* worker;
    struct fetcher_request* req;
    input_fetcher_t* fetcher;

    vlc_interrupt_t interrupt;
    vlc_thread_t thread;
    atomic_bool active;
};

static char* CreateCacheKey( input_item_t* item )
{
    vlc_mutex_lock( &item->lock );

    if( !item->p_meta )
    {
        vlc_mutex_unlock( &item->lock );
        return NULL;
    }

    char const* artist = vlc_meta_Get( item->p_meta, vlc_meta_Artist );
    char const* album = vlc_meta_Get( item->p_meta, vlc_meta_Album );
    char const *date = vlc_meta_Get( item->p_meta, vlc_meta_Date );
    char* key;

    /**
     * Simple concatenation of artist and album can lead to the same key
     * for entities that should not have such. Imagine { dogs, tick } and
     * { dog, stick } */
    if( !artist || !album || asprintf( &key, "%s:%zu:%s:%zu:%s",
          artist, strlen( artist ), album, strlen( album ),
          date ? date : "0000" ) < 0 )
    {
        key = NULL;
    }
    vlc_mutex_unlock( &item->lock );

    return key;
}

static void FreeCacheEntry( void* data, void* obj )
{
    free( data );
    VLC_UNUSED( obj );
}

static int ReadAlbumCache( input_fetcher_t* fetcher, input_item_t* item )
{
    char* key = CreateCacheKey( item );

    if( key == NULL )
        return VLC_EGENERIC;

    vlc_mutex_lock( &fetcher->lock );
    char const* art = vlc_dictionary_value_for_key( &fetcher->album_cache,
                                                    key );
    if( art )
        input_item_SetArtURL( item, art );
    vlc_mutex_unlock( &fetcher->lock );

    free( key );
    return art ? VLC_SUCCESS : VLC_EGENERIC;
}

static void AddAlbumCache( input_fetcher_t* fetcher, input_item_t* item,
                           bool overwrite )
{
    char* art = input_item_GetArtURL( item );
    char* key = CreateCacheKey( item );

    if( key && art && strncasecmp( art, "attachment://", 13 ) )
    {
        vlc_mutex_lock( &fetcher->lock );
        if( overwrite || !vlc_dictionary_has_key( &fetcher->album_cache, key ) )
        {
            vlc_dictionary_insert( &fetcher->album_cache, key, art );
            art = NULL;
        }
        vlc_mutex_unlock( &fetcher->lock );
    }

    free( art );
    free( key );
}

static int InvokeModule( input_fetcher_t* fetcher, input_item_t* item,
                         int scope, char const* type )
{
    meta_fetcher_t* mf = vlc_custom_create( fetcher->owner,
                                            sizeof( *mf ), type );
    if( unlikely( !mf ) )
        return VLC_ENOMEM;

    mf->e_scope = scope;
    mf->p_item = item;

    module_t* mf_module = module_need( mf, type, NULL, false );

    if( mf_module )
        module_unneed( mf, mf_module );

    vlc_object_delete(mf);

    return VLC_SUCCESS;
}

static int CheckMeta( input_item_t* item )
{
    vlc_mutex_lock( &item->lock );
    bool error = !item->p_meta ||
                 !vlc_meta_Get( item->p_meta, vlc_meta_Title ) ||
                 !vlc_meta_Get( item->p_meta, vlc_meta_Artist ) ||
                 !vlc_meta_Get( item->p_meta, vlc_meta_Album );
    vlc_mutex_unlock( &item->lock );
    return error;
}

static int CheckArt( input_item_t* item )
{
    vlc_mutex_lock( &item->lock );
    bool error = !item->p_meta ||
                 !vlc_meta_Get( item->p_meta, vlc_meta_ArtworkURL );
    vlc_mutex_unlock( &item->lock );
    return error;
}

static int SearchArt( input_fetcher_t* fetcher, input_item_t* item, int scope)
{
    InvokeModule( fetcher, item, scope, "art finder" );
    return CheckArt( item );
}

static int SearchByScope( input_fetcher_t* fetcher,
    struct fetcher_request* req, int scope )
{
    input_item_t* item = req->item;

    if( CheckMeta( item ) &&
        InvokeModule( fetcher, req->item, scope, "meta fetcher" ) )
    {
        return VLC_EGENERIC;
    }

    if( ! CheckArt( item )                         ||
        ! ReadAlbumCache( fetcher, item )          ||
        ! input_FindArtInCacheUsingItemUID( item ) ||
        ! input_FindArtInCache( item )             ||
        ! SearchArt( fetcher, item, scope ) )
    {
        AddAlbumCache( fetcher, req->item, false );
        if( !background_worker_Push( fetcher->downloader, req, NULL, 0 ) )
            return VLC_SUCCESS;
    }

    return VLC_EGENERIC;
}

static void NotifyArtFetchEnded( struct fetcher_request* req, bool fetched )
{
    if (req->cbs && req->cbs->on_art_fetch_ended)
        req->cbs->on_art_fetch_ended(req->item, fetched, req->userdata);
}

static void Downloader( input_fetcher_t* fetcher,
    struct fetcher_request* req )
{
    ReadAlbumCache( fetcher, req->item );

    char *psz_arturl = input_item_GetArtURL( req->item );
    if( !psz_arturl )
        goto error;

    if( !strncasecmp( psz_arturl, "file://", 7 ) ||
        !strncasecmp( psz_arturl, "attachment://", 13 ) )
        goto out; /* no fetch required */

    stream_t* source = vlc_stream_NewURL( fetcher->owner, psz_arturl );

    if( !source )
        goto error;

    struct vlc_memstream output_stream;
    vlc_memstream_open( &output_stream );

    for( ;; )
    {
        char buffer[2048];

        int read = vlc_stream_Read( source, buffer, sizeof( buffer ) );
        if( read <= 0 )
            break;

        if( (int)vlc_memstream_write( &output_stream, buffer, read ) < read )
            break;
    }

    vlc_stream_Delete( source );

    if( vlc_memstream_close( &output_stream ) )
        goto error;

    if( vlc_killed() )
    {
        free( output_stream.ptr );
        goto error;
    }

    input_SaveArt( fetcher->owner, req->item, output_stream.ptr,
                   output_stream.length, NULL );

    free( output_stream.ptr );
    AddAlbumCache( fetcher, req->item, true );

out:
    if( psz_arturl )
    {
        var_SetAddress( fetcher->owner, "item-change", req->item );
        input_item_SetArtFetched( req->item, true );
    }

    free( psz_arturl );
    NotifyArtFetchEnded(req, psz_arturl != NULL);
    return;

error:
    FREENULL( psz_arturl );
    goto out;
}

static void SearchLocal( input_fetcher_t* fetcher, struct fetcher_request* req )
{
    if( SearchByScope( fetcher, req, FETCHER_SCOPE_LOCAL ) == VLC_SUCCESS )
        return; /* done */

    if( var_InheritBool( fetcher->owner, "metadata-network-access" ) ||
        req->options & META_REQUEST_OPTION_FETCH_NETWORK )
    {
        if( background_worker_Push( fetcher->network, req, NULL, 0 ) )
            NotifyArtFetchEnded(req, false);
    }
    else
    {
        input_item_SetArtNotFound( req->item, true );
        NotifyArtFetchEnded(req, false);
    }
}

static void SearchNetwork( input_fetcher_t* fetcher, struct fetcher_request* req )
{
    if( SearchByScope( fetcher, req, FETCHER_SCOPE_NETWORK ) )
    {
        input_item_SetArtNotFound( req->item, true );
        NotifyArtFetchEnded(req, false);
    }
}

static void RequestRelease( void* req_ )
{
    struct fetcher_request* req = req_;

    if( !vlc_atomic_rc_dec( &req->rc ) )
        return;

    input_item_Release( req->item );
    free( req );
}

static void RequestHold( void* req_ )
{
    struct fetcher_request* req = req_;
    vlc_atomic_rc_inc( &req->rc );
}

static void* FetcherThread( void* handle )
{
    struct fetcher_thread* th = handle;
    vlc_interrupt_set( &th->interrupt );

    th->pf_worker( th->fetcher, th->req );

    atomic_store( &th->active, false );
    background_worker_RequestProbe( th->worker );
    return NULL;
}

static int StartWorker( input_fetcher_t* fetcher,
    void( *pf_worker )( input_fetcher_t*, struct fetcher_request* ),
    struct background_worker* bg, struct fetcher_request* req, void** handle )
{
    struct fetcher_thread* th = malloc( sizeof *th );

    if( unlikely( !th ) )
        return VLC_ENOMEM;

    th->req = req;
    th->worker = bg;
    th->fetcher = fetcher;
    th->pf_worker = pf_worker;

    vlc_interrupt_init( &th->interrupt );
    atomic_init( &th->active, true );

    if( !vlc_clone( &th->thread, FetcherThread, th, VLC_THREAD_PRIORITY_LOW ) )
    {
        *handle = th;
        return VLC_SUCCESS;
    }

    vlc_interrupt_deinit( &th->interrupt );
    free( th );
    return VLC_EGENERIC;
}

static int ProbeWorker( void* fetcher_, void* th_ )
{
    return !atomic_load( &((struct fetcher_thread*)th_)->active );
    VLC_UNUSED( fetcher_ );
}

static void CloseWorker( void* fetcher_, void* th_ )
{
    struct fetcher_thread* th = th_;
    VLC_UNUSED( fetcher_ );

    vlc_interrupt_kill( &th->interrupt );
    vlc_join( th->thread, NULL );
    vlc_interrupt_deinit( &th->interrupt );
    free( th );
}

#define DEF_STARTER(name, worker) \
static int Start ## name( void* fetcher_, void* req_, void** out ) { \
    input_fetcher_t* fetcher = fetcher_; \
    return StartWorker( fetcher, name, worker, req_, out ); }

DEF_STARTER(  SearchLocal, fetcher->local )
DEF_STARTER(SearchNetwork, fetcher->network )
DEF_STARTER(   Downloader, fetcher->downloader )

static void WorkerInit( input_fetcher_t* fetcher,
    struct background_worker** worker, int( *starter )( void*, void*, void** ) )
{
    struct background_worker_config conf = {
        .default_timeout = 0,
        .max_threads = var_InheritInteger( fetcher->owner, "fetch-art-threads" ),
        .pf_start = starter,
        .pf_probe = ProbeWorker,
        .pf_stop = CloseWorker,
        .pf_release = RequestRelease,
        .pf_hold = RequestHold };

    *worker = background_worker_New( fetcher, &conf );
}

input_fetcher_t* input_fetcher_New( vlc_object_t* owner )
{
    input_fetcher_t* fetcher = malloc( sizeof( *fetcher ) );

    if( unlikely( !fetcher ) )
        return NULL;

    fetcher->owner = owner;

    WorkerInit( fetcher, &fetcher->local, StartSearchLocal );
    WorkerInit( fetcher, &fetcher->network, StartSearchNetwork );
    WorkerInit( fetcher, &fetcher->downloader, StartDownloader );

    if( unlikely( !fetcher->local || !fetcher->network || !fetcher->downloader ) )
    {
        if( fetcher->local )
            background_worker_Delete( fetcher->local );

        if( fetcher->network )
            background_worker_Delete( fetcher->network );

        if( fetcher->downloader )
            background_worker_Delete( fetcher->downloader );

        free( fetcher );
        return NULL;
    }

    vlc_mutex_init( &fetcher->lock );
    vlc_dictionary_init( &fetcher->album_cache, 0 );

    return fetcher;
}

int input_fetcher_Push( input_fetcher_t* fetcher, input_item_t* item,
    input_item_meta_request_option_t options,
    const input_fetcher_callbacks_t *cbs, void *cbs_userdata )
{
    assert(options & META_REQUEST_OPTION_FETCH_ANY);
    struct fetcher_request* req = malloc( sizeof *req );

    if( unlikely( !req ) )
        return VLC_ENOMEM;

    req->item = item;
    req->options = options;
    req->cbs = cbs;
    req->userdata = cbs_userdata;

    vlc_atomic_rc_init( &req->rc );
    input_item_Hold( item );

    struct background_worker* worker =
        options & META_REQUEST_OPTION_FETCH_LOCAL ? fetcher->local : fetcher->network;
    if( background_worker_Push( worker, req, NULL, 0 ) )
        NotifyArtFetchEnded(req, false);

    RequestRelease( req );
    return VLC_SUCCESS;
}

void input_fetcher_Delete( input_fetcher_t* fetcher )
{
    background_worker_Delete( fetcher->local );
    background_worker_Delete( fetcher->network );
    background_worker_Delete( fetcher->downloader );

    vlc_dictionary_clear( &fetcher->album_cache, FreeCacheEntry, NULL );
    free( fetcher );
}
