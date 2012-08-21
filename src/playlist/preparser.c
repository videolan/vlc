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

#include <vlc_common.h>
#include <vlc_playlist.h>

#include "art.h"
#include "fetcher.h"
#include "preparser.h"
#include "../input/input_interface.h"


/*****************************************************************************
 * Structures/definitions
 *****************************************************************************/
struct playlist_preparser_t
{
    vlc_object_t        *object;
    playlist_fetcher_t  *p_fetcher;

    vlc_mutex_t     lock;
    vlc_cond_t      wait;
    bool            b_live;
    input_item_t  **pp_waiting;
    int             i_waiting;

    int             i_art_policy;
};

static void *Thread( void * );

/*****************************************************************************
 * Public functions
 *****************************************************************************/
playlist_preparser_t *playlist_preparser_New( vlc_object_t *parent,
                                              playlist_fetcher_t *p_fetcher )
{
    playlist_preparser_t *p_preparser = malloc( sizeof(*p_preparser) );
    if( !p_preparser )
        return NULL;

    p_preparser->object = parent;
    p_preparser->p_fetcher = p_fetcher;
    vlc_mutex_init( &p_preparser->lock );
    vlc_cond_init( &p_preparser->wait );
    p_preparser->b_live = false;
    p_preparser->i_art_policy = var_InheritInteger( parent, "album-art" );
    p_preparser->i_waiting = 0;
    p_preparser->pp_waiting = NULL;

    return p_preparser;
}

void playlist_preparser_Push( playlist_preparser_t *p_preparser, input_item_t *p_item )
{
    vlc_gc_incref( p_item );

    vlc_mutex_lock( &p_preparser->lock );
    INSERT_ELEM( p_preparser->pp_waiting, p_preparser->i_waiting,
                 p_preparser->i_waiting, p_item );
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

void playlist_preparser_Delete( playlist_preparser_t *p_preparser )
{
    vlc_mutex_lock( &p_preparser->lock );
    /* Remove pending item to speed up preparser thread exit */
    while( p_preparser->i_waiting > 0 )
    {
        vlc_gc_decref( p_preparser->pp_waiting[0] );
        REMOVE_ELEM( p_preparser->pp_waiting, p_preparser->i_waiting, 0 );
    }

    while( p_preparser->b_live )
        vlc_cond_wait( &p_preparser->wait, &p_preparser->lock );
    vlc_mutex_unlock( &p_preparser->lock );

    /* Destroy the item preparser */
    vlc_cond_destroy( &p_preparser->wait );
    vlc_mutex_destroy( &p_preparser->lock );
    free( p_preparser );
}

/*****************************************************************************
 * Privates functions
 *****************************************************************************/
/**
 * This function preparses an item when needed.
 */
static void Preparse( vlc_object_t *obj, input_item_t *p_item )
{
    vlc_mutex_lock( &p_item->lock );
    int i_type = p_item->i_type;
    vlc_mutex_unlock( &p_item->lock );

    if( i_type != ITEM_TYPE_FILE )
    {
        input_item_SetPreparsed( p_item, true );
        return;
    }

    /* Do not preparse if it is already done (like by playing it) */
    if( !input_item_IsPreparsed( p_item ) )
    {
        input_Preparse( obj, p_item );
        input_item_SetPreparsed( p_item, true );

        var_SetAddress( obj, "item-change", p_item );
    }
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

        if( p_preparser->i_art_policy == ALBUM_ART_ALL &&
                ( !psz_arturl ||
                  ( strncmp( psz_arturl, "file://", 7 ) &&
                    strncmp( psz_arturl, "attachment://", 13 ) ) ) )
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
        playlist_fetcher_Push( p_fetcher, p_item );
}

/**
 * This function does the preparsing and issues the art fetching requests
 */
static void *Thread( void *data )
{
    playlist_preparser_t *p_preparser = data;
    vlc_object_t *obj = p_preparser->object;

    for( ;; )
    {
        input_item_t *p_current;

        /* */
        vlc_mutex_lock( &p_preparser->lock );
        if( p_preparser->i_waiting > 0 )
        {
            p_current = p_preparser->pp_waiting[0];
            REMOVE_ELEM( p_preparser->pp_waiting, p_preparser->i_waiting, 0 );
        }
        else
        {
            p_current = NULL;
            p_preparser->b_live = false;
            vlc_cond_signal( &p_preparser->wait );
        }
        vlc_mutex_unlock( &p_preparser->lock );

        if( !p_current )
            break;

        Preparse( obj, p_current );

        Art( p_preparser, p_current );
        vlc_gc_decref(p_current);
    }
    return NULL;
}

