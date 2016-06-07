/*****************************************************************************
 * preparser.c: Preparser thread.
 *****************************************************************************
 * Copyright © 1999-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Clément Stenac <zorglub@videolan.org>
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

#include <vlc_common.h>

#include "fetcher.h"
#include "preparser.h"
#include "input/input_interface.h"

/*****************************************************************************
 * Structures/definitions
 *****************************************************************************/
typedef struct preparser_entry_t preparser_entry_t;

struct preparser_entry_t
{
    input_item_t    *p_item;
    input_item_meta_request_option_t i_options;
    void            *id;
    mtime_t          timeout;
};

struct playlist_preparser_t
{
    vlc_object_t        *object;
    playlist_fetcher_t  *p_fetcher;
    mtime_t              default_timeout;

    input_thread_t      *input;
    void                *input_id;
    bool                 input_done;

    vlc_mutex_t     lock;
    vlc_cond_t      wait;
    vlc_cond_t      thread_wait;
    bool            b_live;
    preparser_entry_t  **pp_waiting;
    size_t          i_waiting;
};

static void *Thread( void * );

/*****************************************************************************
 * Public functions
 *****************************************************************************/
playlist_preparser_t *playlist_preparser_New( vlc_object_t *parent )
{
    playlist_preparser_t *p_preparser = malloc( sizeof(*p_preparser) );
    if( !p_preparser )
        return NULL;

    p_preparser->input = NULL;
    p_preparser->input_id = NULL;
    p_preparser->input_done = false;
    p_preparser->object = parent;
    p_preparser->default_timeout = var_InheritInteger( parent, "preparse-timeout" );
    p_preparser->p_fetcher = playlist_fetcher_New( parent );
    if( unlikely(p_preparser->p_fetcher == NULL) )
        msg_Err( parent, "cannot create fetcher" );

    vlc_mutex_init( &p_preparser->lock );
    vlc_cond_init( &p_preparser->wait );
    vlc_cond_init( &p_preparser->thread_wait );
    p_preparser->b_live = false;
    p_preparser->i_waiting = 0;
    p_preparser->pp_waiting = NULL;

    return p_preparser;
}

void playlist_preparser_Push( playlist_preparser_t *p_preparser, input_item_t *p_item,
                              input_item_meta_request_option_t i_options,
                              int timeout, void *id )
{
    preparser_entry_t *p_entry = malloc( sizeof(preparser_entry_t) );

    if ( !p_entry )
        return;
    p_entry->p_item = p_item;
    p_entry->i_options = i_options;
    p_entry->id = id;
    p_entry->timeout = (timeout < 0 ? p_preparser->default_timeout : timeout) * 1000;
    vlc_gc_incref( p_entry->p_item );

    vlc_mutex_lock( &p_preparser->lock );
    INSERT_ELEM( p_preparser->pp_waiting, p_preparser->i_waiting,
                 p_preparser->i_waiting, p_entry );
    if( !p_preparser->b_live )
    {
        if( vlc_clone_detach( NULL, Thread, p_preparser,
                              VLC_THREAD_PRIORITY_LOW ) )
            msg_Warn( p_preparser->object, "cannot spawn pre-parser thread" );
        else
            p_preparser->b_live = true;
    }
    vlc_mutex_unlock( &p_preparser->lock );
}

void playlist_preparser_fetcher_Push( playlist_preparser_t *p_preparser,
             input_item_t *p_item, input_item_meta_request_option_t i_options )
{
    if( p_preparser->p_fetcher != NULL )
        playlist_fetcher_Push( p_preparser->p_fetcher, p_item, i_options );
}

void playlist_preparser_Cancel( playlist_preparser_t *p_preparser, void *id )
{
    assert( id != NULL );
    vlc_mutex_lock( &p_preparser->lock );

    /* Remove entries that match with the id */
    for( int i = p_preparser->i_waiting - 1; i >= 0; --i )
    {
        preparser_entry_t *p_entry = p_preparser->pp_waiting[i];
        if( p_entry->id == id )
        {
            vlc_gc_decref( p_entry->p_item );
            free( p_entry );
            REMOVE_ELEM( p_preparser->pp_waiting, p_preparser->i_waiting, i );
        }
    }

    /* Stop the input_thread reading the item (if any) */
    if( p_preparser->input_id == id )
    {
        assert( p_preparser->input != NULL );
        input_Stop( p_preparser->input );
    }
    vlc_mutex_unlock( &p_preparser->lock );
}

void playlist_preparser_Delete( playlist_preparser_t *p_preparser )
{
    vlc_mutex_lock( &p_preparser->lock );
    /* Remove pending item to speed up preparser thread exit */
    while( p_preparser->i_waiting > 0 )
    {
        preparser_entry_t *p_entry = p_preparser->pp_waiting[0];
        vlc_gc_decref( p_entry->p_item );
        free( p_entry );
        REMOVE_ELEM( p_preparser->pp_waiting, p_preparser->i_waiting, 0 );
    }

    if( p_preparser->input != NULL )
        input_Stop( p_preparser->input );

    while( p_preparser->b_live )
        vlc_cond_wait( &p_preparser->wait, &p_preparser->lock );
    vlc_mutex_unlock( &p_preparser->lock );

    /* Destroy the item preparser */
    vlc_cond_destroy( &p_preparser->thread_wait );
    vlc_cond_destroy( &p_preparser->wait );
    vlc_mutex_destroy( &p_preparser->lock );

    if( p_preparser->p_fetcher != NULL )
        playlist_fetcher_Delete( p_preparser->p_fetcher );
    free( p_preparser );
}

/*****************************************************************************
 * Privates functions
 *****************************************************************************/

static int InputEvent( vlc_object_t *obj, const char *varname,
                       vlc_value_t old, vlc_value_t cur, void *data )
{
    playlist_preparser_t *preparser = data;
    int event = cur.i_int;

    if( event == INPUT_EVENT_DEAD )
    {
        preparser->input_done = true;
        vlc_cond_signal( &preparser->thread_wait );
    }

    (void) obj; (void) varname; (void) old;
    return VLC_SUCCESS;
}

/**
 * This function preparses an item when needed.
 */
static void Preparse( playlist_preparser_t *preparser,
                      preparser_entry_t *p_entry )
{
    input_item_t *p_item = p_entry->p_item;

    vlc_mutex_lock( &p_item->lock );
    int i_type = p_item->i_type;
    bool b_net = p_item->b_net;
    vlc_mutex_unlock( &p_item->lock );

    bool b_preparse = false;
    switch (i_type) {
    case ITEM_TYPE_FILE:
    case ITEM_TYPE_DIRECTORY:
    case ITEM_TYPE_PLAYLIST:
    case ITEM_TYPE_NODE:
        if( !b_net || p_entry->i_options & META_REQUEST_OPTION_SCOPE_NETWORK )
            b_preparse = true;
        break;
    }

    /* Do not preparse if it is already done (like by playing it) */
    if( b_preparse && !input_item_IsPreparsed( p_item ) )
    {
        int status;
        preparser->input = input_CreatePreparser( preparser->object, p_item );
        if( preparser->input == NULL )
        {
            input_item_SignalPreparseEnded( p_item, ITEM_PREPARSE_FAILED );
            return;
        }
        preparser->input_done = false;
        preparser->input_id = p_entry->id;

        var_AddCallback( preparser->input, "intf-event", InputEvent,
                         preparser );
        if( input_Start( preparser->input ) == VLC_SUCCESS )
        {
            if( p_entry->timeout > 0 )
            {
                mtime_t deadline = mdate() + p_entry->timeout;
                int ret = 0;
                while( !preparser->input_done && ret == 0 )
                    ret = vlc_cond_timedwait( &preparser->thread_wait,
                                              &preparser->lock, deadline );
                status = ret == 0 ? ITEM_PREPARSE_DONE : ITEM_PREPARSE_TIMEOUT;
            }
            else
            {
                while( !preparser->input_done )
                    vlc_cond_wait( &preparser->thread_wait, &preparser->lock );
                status = ITEM_PREPARSE_DONE;
            }
        }
        else
            status = ITEM_PREPARSE_FAILED;

        var_DelCallback( preparser->input, "intf-event", InputEvent,
                         preparser );
        input_Close( preparser->input );
        preparser->input = NULL;
        preparser->input_id = NULL;

        var_SetAddress( preparser->object, "item-change", p_item );
        input_item_SetPreparsed( p_item, true );
        input_item_SignalPreparseEnded( p_item, status );
    }
    else if (!b_preparse)
        input_item_SignalPreparseEnded( p_item, ITEM_PREPARSE_SKIPPED );

}

/**
 * This function ask the fetcher object to fetch the art when needed
 */
static void Art( playlist_preparser_t *p_preparser, input_item_t *p_item )
{
    vlc_object_t *obj = p_preparser->object;
    playlist_fetcher_t *p_fetcher = p_preparser->p_fetcher;

    bool b_fetch = false;
    /* If we haven't retrieved enough meta, add to secondary queue
     * which will run the "meta fetchers".
     * This only checks for meta, not for art
     * \todo don't do this for things we won't get meta for, like vids
     */

    vlc_mutex_lock( &p_item->lock );
    if( p_item->p_meta )
    {
        const char *psz_arturl = vlc_meta_Get( p_item->p_meta, vlc_meta_ArtworkURL );
        const char *psz_name = vlc_meta_Get( p_item->p_meta, vlc_meta_Title );

        if( !psz_arturl || ( strncmp( psz_arturl, "file://", 7 ) &&
                             strncmp( psz_arturl, "attachment://", 13 ) ) )
        {
            msg_Dbg( obj, "meta ok for %s, need to fetch art",
                     psz_name ? psz_name : "(null)" );
            b_fetch = true;
        }
        else
        {
            msg_Dbg( obj, "no fetch required for %s (art currently %s)",
                     psz_name ? psz_name : "(null)",
                     psz_arturl ? psz_arturl : "(null)" );
        }
    }
    vlc_mutex_unlock( &p_item->lock );

    if( b_fetch && p_fetcher )
        playlist_fetcher_Push( p_fetcher, p_item, 0 );
}

/**
 * This function does the preparsing and issues the art fetching requests
 */
static void *Thread( void *data )
{
    playlist_preparser_t *p_preparser = data;

    vlc_mutex_lock( &p_preparser->lock );
    for( ;; )
    {
        preparser_entry_t *p_entry;

        /* */
        if( p_preparser->i_waiting > 0 )
        {
            p_entry = p_preparser->pp_waiting[0];
            REMOVE_ELEM( p_preparser->pp_waiting, p_preparser->i_waiting, 0 );
        }
        else
        {
            p_preparser->b_live = false;
            vlc_cond_signal( &p_preparser->wait );
            break;
        }
        assert( p_entry );

        Preparse( p_preparser, p_entry );

        Art( p_preparser, p_entry->p_item );
        vlc_gc_decref( p_entry->p_item );
        free( p_entry );
    }
    vlc_mutex_unlock( &p_preparser->lock );
    return NULL;
}

