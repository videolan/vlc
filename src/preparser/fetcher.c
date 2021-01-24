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
#include <vlc_executor.h>

#include "art.h"
#include "libvlc.h"
#include "fetcher.h"
#include "input/input_interface.h"
#include "misc/interrupt.h"

struct input_fetcher_t {
    vlc_executor_t *executor_local;
    vlc_executor_t *executor_network;
    vlc_executor_t *executor_downloader;

    vlc_dictionary_t album_cache;
    vlc_object_t* owner;

    vlc_mutex_t lock;
    struct vlc_list submitted_tasks; /**< list of struct task */
};

struct task {
    input_fetcher_t *fetcher;
    vlc_executor_t *executor;
    input_item_t* item;
    int options;
    const input_fetcher_callbacks_t *cbs;
    void *userdata;

    vlc_interrupt_t interrupt;

    struct vlc_runnable runnable; /**< to be passed to the executor */
    struct vlc_list node; /**< node of input_fetcher_t.submitted_tasks */
};

static void RunDownloader(void *);
static void RunSearchLocal(void *);
static void RunSearchNetwork(void *);

static struct task *
TaskNew(input_fetcher_t *fetcher, vlc_executor_t *executor, input_item_t *item,
        input_item_meta_request_option_t options,
        const input_fetcher_callbacks_t *cbs, void *userdata)
{
    struct task *task = malloc(sizeof(*task));
    if (!task)
        return NULL;

    task->fetcher = fetcher;
    task->executor = executor;
    task->item = item;
    task->options = options;
    task->cbs = cbs;
    task->userdata = userdata;

    vlc_interrupt_init(&task->interrupt);

    input_item_Hold(item);

    if (executor == fetcher->executor_local)
        task->runnable.run = RunSearchLocal;
    else if (executor == fetcher->executor_network)
        task->runnable.run = RunSearchNetwork;
    else
    {
        assert(executor == fetcher->executor_downloader);
        task->runnable.run = RunDownloader;
    }

    task->runnable.userdata = task;

    return task;
}

static void
TaskDelete(struct task *task)
{
    input_item_Release(task->item);
    vlc_interrupt_deinit(&task->interrupt);
    free(task);
}

static void
FetcherAddTask(input_fetcher_t *fetcher, struct task *task)
{
    vlc_mutex_lock(&fetcher->lock);
    vlc_list_append(&task->node, &fetcher->submitted_tasks);
    vlc_mutex_unlock(&fetcher->lock);
}

static void
FetcherRemoveTask(input_fetcher_t *fetcher, struct task *task)
{
    vlc_mutex_lock(&fetcher->lock);
    vlc_list_remove(&task->node);
    vlc_mutex_unlock(&fetcher->lock);
}

static int
Submit(input_fetcher_t *fetcher, vlc_executor_t *executor, input_item_t *item,
       input_item_meta_request_option_t options,
       const input_fetcher_callbacks_t *cbs, void *userdata)
{
    struct task *task =
        TaskNew(fetcher, executor, item, options, cbs, userdata);
    if (!task)
        return VLC_ENOMEM;

    FetcherAddTask(fetcher, task);
    vlc_executor_Submit(task->executor, &task->runnable);

    return VLC_SUCCESS;
}

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

static int SearchByScope(struct task *task, int scope)
{
    input_fetcher_t *fetcher = task->fetcher;
    input_item_t* item = task->item;

    if( CheckMeta( item ) &&
        InvokeModule( fetcher, item, scope, "meta fetcher" ) )
    {
        return VLC_EGENERIC;
    }

    if( ! CheckArt( item )                         ||
        ! ReadAlbumCache( fetcher, item )          ||
        ! input_FindArtInCacheUsingItemUID( item ) ||
        ! input_FindArtInCache( item )             ||
        ! SearchArt( fetcher, item, scope ) )
    {
        AddAlbumCache( fetcher, task->item, false );
        int ret = Submit(fetcher, fetcher->executor_downloader, item,
                         task->options, task->cbs, task->userdata);
        if (ret == VLC_SUCCESS)
            return VLC_SUCCESS;
    }

    return VLC_EGENERIC;
}

static void NotifyArtFetchEnded(struct task *task, bool fetched)
{
    if (task->cbs && task->cbs->on_art_fetch_ended)
        task->cbs->on_art_fetch_ended(task->item, fetched, task->userdata);
}

static void RunDownloader(void *userdata)
{
    struct task *task = userdata;
    input_fetcher_t *fetcher = task->fetcher;

    vlc_interrupt_set(&task->interrupt);

    ReadAlbumCache( fetcher, task->item );

    char *psz_arturl = input_item_GetArtURL( task->item );
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

    input_SaveArt( fetcher->owner, task->item, output_stream.ptr,
                   output_stream.length, NULL );

    free( output_stream.ptr );
    AddAlbumCache( fetcher, task->item, true );

out:
    vlc_interrupt_set(NULL);

    if( psz_arturl )
    {
        var_SetAddress( fetcher->owner, "item-change", task->item );
        input_item_SetArtFetched( task->item, true );
    }

    free( psz_arturl );
    NotifyArtFetchEnded(task, psz_arturl != NULL);
    FetcherRemoveTask(fetcher, task);
    TaskDelete(task);
    return;

error:
    vlc_interrupt_set(NULL);

    FREENULL( psz_arturl );
    NotifyArtFetchEnded(task, false);
    FetcherRemoveTask(fetcher, task);
    TaskDelete(task);
}

static void RunSearchLocal(void *userdata)
{
    struct task *task = userdata;
    input_fetcher_t *fetcher = task->fetcher;

    vlc_interrupt_set(&task->interrupt);

    if( SearchByScope( task, FETCHER_SCOPE_LOCAL ) == VLC_SUCCESS )
        goto end; /* done */

    if( var_InheritBool( fetcher->owner, "metadata-network-access" ) ||
        task->options & META_REQUEST_OPTION_FETCH_NETWORK )
    {
        int ret = Submit(fetcher, fetcher->executor_network, task->item,
                         task->options, task->cbs, task->userdata);
        if (ret != VLC_SUCCESS)
            NotifyArtFetchEnded(task, false);
    }
    else
    {
        input_item_SetArtNotFound( task->item, true );
        NotifyArtFetchEnded(task, false);
    }

end:
    vlc_interrupt_set(NULL);

    FetcherRemoveTask(fetcher, task);
    TaskDelete(task);
}

static void RunSearchNetwork(void *userdata)
{
    struct task *task = userdata;

    vlc_interrupt_set(&task->interrupt);

    if( SearchByScope( task, FETCHER_SCOPE_NETWORK ) != VLC_SUCCESS )
    {
        input_item_SetArtNotFound( task->item, true );
        NotifyArtFetchEnded(task, false);
    }

    vlc_interrupt_set(NULL);

    input_fetcher_t *fetcher = task->fetcher;
    FetcherRemoveTask(fetcher, task);
    TaskDelete(task);
}

input_fetcher_t* input_fetcher_New( vlc_object_t* owner )
{
    input_fetcher_t* fetcher = malloc( sizeof( *fetcher ) );

    if( unlikely( !fetcher ) )
        return NULL;

    int max_threads = var_InheritInteger(owner, "fetch-art-threads");
    if (max_threads < 1)
        max_threads = 1;

    fetcher->executor_local = vlc_executor_New(max_threads);
    if (!fetcher->executor_local)
    {
        free(fetcher);
        return NULL;
    }

    fetcher->executor_network = vlc_executor_New(max_threads);
    if (!fetcher->executor_network)
    {
        vlc_executor_Delete(fetcher->executor_local);
        free(fetcher);
        return NULL;
    }

    fetcher->executor_downloader = vlc_executor_New(max_threads);
    if (!fetcher->executor_downloader)
    {
        vlc_executor_Delete(fetcher->executor_network);
        vlc_executor_Delete(fetcher->executor_local);
        free(fetcher);
        return NULL;
    }

    fetcher->owner = owner;

    vlc_mutex_init(&fetcher->lock);
    vlc_list_init(&fetcher->submitted_tasks);

    vlc_dictionary_init( &fetcher->album_cache, 0 );

    return fetcher;
}

int input_fetcher_Push(input_fetcher_t* fetcher, input_item_t* item,
    input_item_meta_request_option_t options,
    const input_fetcher_callbacks_t *cbs, void *cbs_userdata)
{
    assert(options & META_REQUEST_OPTION_FETCH_ANY);

    vlc_executor_t *executor = options & META_REQUEST_OPTION_FETCH_LOCAL
                             ? fetcher->executor_local
                             : fetcher->executor_network;

    return Submit(fetcher, executor, item, options, cbs, cbs_userdata);
}

static void
CancelAllTasks(input_fetcher_t *fetcher)
{
    vlc_mutex_lock(&fetcher->lock);

    struct task *task;
    vlc_list_foreach(task, &fetcher->submitted_tasks, node)
    {
        bool canceled = vlc_executor_Cancel(task->executor, &task->runnable);
        if (canceled)
        {
            NotifyArtFetchEnded(task, false);
            vlc_list_remove(&task->node);
            TaskDelete(task);
        }
        /* Otherwise, the task will be finished and destroyed after run() */
    }

    vlc_mutex_unlock(&fetcher->lock);
}

void input_fetcher_Delete( input_fetcher_t* fetcher )
{
    CancelAllTasks(fetcher);

    vlc_executor_Delete(fetcher->executor_local);
    vlc_executor_Delete(fetcher->executor_network);
    vlc_executor_Delete(fetcher->executor_downloader);

    vlc_dictionary_clear( &fetcher->album_cache, FreeCacheEntry, NULL );
    free( fetcher );
}
