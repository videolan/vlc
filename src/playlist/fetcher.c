/*****************************************************************************
 * fetcher.c: Art fetcher thread.
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

#include <limits.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_stream.h>
#include <vlc_meta_fetcher.h>
#include <vlc_memory.h>
#include <vlc_demux.h>
#include <vlc_modules.h>
#include <vlc_interrupt.h>

#include "libvlc.h"
#include "art.h"
#include "fetcher.h"
#include "input/input_interface.h"

/*****************************************************************************
 * Structures/definitions
 *****************************************************************************/
typedef enum
{
    PASS1_LOCAL = 0,
    PASS2_NETWORK
} fetcher_pass_t;
#define PASS_COUNT 2

typedef struct
{
    char *psz_artist;
    char *psz_album;
    char *psz_arturl;
    bool b_found;
    meta_fetcher_scope_t e_scope; /* max scope */

} playlist_album_t;

typedef struct fetcher_entry_t fetcher_entry_t;

struct fetcher_entry_t
{
    input_item_t    *p_item;
    input_item_meta_request_option_t i_options;
    fetcher_entry_t *p_next;
};

struct playlist_fetcher_t
{
    vlc_object_t   *object;
    vlc_mutex_t     lock;
    vlc_cond_t      wait;
    bool            b_live;
    vlc_interrupt_t *interrupt;

    fetcher_entry_t *p_waiting_head[PASS_COUNT];
    fetcher_entry_t *p_waiting_tail[PASS_COUNT];

    DECL_ARRAY(playlist_album_t) albums;
    meta_fetcher_scope_t e_scope;
};

static void *Thread( void * );


/*****************************************************************************
 * Public functions
 *****************************************************************************/
playlist_fetcher_t *playlist_fetcher_New( vlc_object_t *parent )
{
    playlist_fetcher_t *p_fetcher = malloc( sizeof(*p_fetcher) );
    if( !p_fetcher )
        return NULL;

    p_fetcher->interrupt = vlc_interrupt_create();
    if( unlikely(p_fetcher->interrupt == NULL) )
    {
        free( p_fetcher );
        return NULL;
    }
    p_fetcher->object = parent;
    vlc_mutex_init( &p_fetcher->lock );
    vlc_cond_init( &p_fetcher->wait );
    p_fetcher->b_live = false;

    bool b_access = var_InheritBool( parent, "metadata-network-access" );
    if ( !b_access )
        b_access = ( var_InheritInteger( parent, "album-art" ) == ALBUM_ART_ALL );

    p_fetcher->e_scope = ( b_access ) ? FETCHER_SCOPE_ANY : FETCHER_SCOPE_LOCAL;

    memset( p_fetcher->p_waiting_head, 0, PASS_COUNT * sizeof(fetcher_entry_t *) );
    memset( p_fetcher->p_waiting_tail, 0, PASS_COUNT * sizeof(fetcher_entry_t *) );

    ARRAY_INIT( p_fetcher->albums );

    return p_fetcher;
}

void playlist_fetcher_Push( playlist_fetcher_t *p_fetcher, input_item_t *p_item,
                            input_item_meta_request_option_t i_options )
{
    fetcher_entry_t *p_entry = malloc( sizeof(fetcher_entry_t) );
    if ( !p_entry ) return;

    vlc_gc_incref( p_item );
    p_entry->p_item = p_item;
    p_entry->p_next = NULL;
    p_entry->i_options = i_options;
    vlc_mutex_lock( &p_fetcher->lock );
    /* Append last */
    if ( p_fetcher->p_waiting_head[PASS1_LOCAL] )
        p_fetcher->p_waiting_tail[PASS1_LOCAL]->p_next = p_entry;
    else
        p_fetcher->p_waiting_head[PASS1_LOCAL] = p_entry;
    p_fetcher->p_waiting_tail[PASS1_LOCAL] = p_entry;

    if( !p_fetcher->b_live )
    {
        assert( p_fetcher->p_waiting_head[PASS1_LOCAL] );
        if( vlc_clone_detach( NULL, Thread, p_fetcher,
                              VLC_THREAD_PRIORITY_LOW ) )
            msg_Err( p_fetcher->object,
                     "cannot spawn secondary preparse thread" );
        else
            p_fetcher->b_live = true;
    }
    vlc_mutex_unlock( &p_fetcher->lock );
}

void playlist_fetcher_Delete( playlist_fetcher_t *p_fetcher )
{
    fetcher_entry_t *p_next;

    vlc_interrupt_kill(p_fetcher->interrupt);

    vlc_mutex_lock( &p_fetcher->lock );
    /* Remove any left-over item, the fetcher will exit */
    for ( int i_queue=0; i_queue<PASS_COUNT; i_queue++ )
    {
        while( p_fetcher->p_waiting_head[i_queue] )
        {
            p_next = p_fetcher->p_waiting_head[i_queue]->p_next;
            vlc_gc_decref( p_fetcher->p_waiting_head[i_queue]->p_item );
            free( p_fetcher->p_waiting_head[i_queue] );
            p_fetcher->p_waiting_head[i_queue] = p_next;
        }
        p_fetcher->p_waiting_head[i_queue] = NULL;
    }

    while( p_fetcher->b_live )
        vlc_cond_wait( &p_fetcher->wait, &p_fetcher->lock );
    vlc_mutex_unlock( &p_fetcher->lock );

    vlc_cond_destroy( &p_fetcher->wait );
    vlc_mutex_destroy( &p_fetcher->lock );

    vlc_interrupt_destroy( p_fetcher->interrupt );

    playlist_album_t album;
    FOREACH_ARRAY( album, p_fetcher->albums )
        free( album.psz_album );
        free( album.psz_artist );
        free( album.psz_arturl );
    FOREACH_END()
    ARRAY_RESET( p_fetcher->albums );

    free( p_fetcher );
}


/*****************************************************************************
 * Privates functions
 *****************************************************************************/
/**
 * This function locates the art associated to an input item.
 * Return codes:
 *   0 : Art is in cache or is a local file
 *   1 : Art found, need to download
 *  -X : Error/not found
 */
static int FindArt( playlist_fetcher_t *p_fetcher, input_item_t *p_item )
{
    int i_ret;

    playlist_album_t *p_album = NULL;
    char *psz_artist = input_item_GetArtist( p_item );
    char *psz_album = input_item_GetAlbum( p_item );
    char *psz_title = input_item_GetTitle( p_item );
    if( !psz_title )
        psz_title = input_item_GetName( p_item );

    if( !psz_title && !psz_artist && !psz_album )
        return VLC_EGENERIC;

    free( psz_title );

    /* If we already checked this album in this session, skip */
    if( psz_artist && psz_album )
    {
        FOREACH_ARRAY( playlist_album_t album, p_fetcher->albums )
            if( !strcmp( album.psz_artist, psz_artist ) &&
                !strcmp( album.psz_album, psz_album ) )
            {
                msg_Dbg( p_fetcher->object,
                         " %s - %s has already been searched",
                         psz_artist, psz_album );
                /* TODO-fenrir if we cache art filename too, we can go faster */
                free( psz_artist );
                free( psz_album );
                if( album.b_found )
                {
                    if( !strncmp( album.psz_arturl, "file://", 7 ) )
                        input_item_SetArtURL( p_item, album.psz_arturl );
                    else /* Actually get URL from cache */
                        playlist_FindArtInCache( p_item );
                    return 0;
                }
                else if ( album.e_scope >= p_fetcher->e_scope )
                {
                    return VLC_EGENERIC;
                }
                msg_Dbg( p_fetcher->object,
                         " will search at higher scope, if possible" );
                p_album = &p_fetcher->albums.p_elems[fe_idx];

                psz_artist = psz_album = NULL;
                break;
            }
        FOREACH_END();
    }

    free( psz_artist );
    free( psz_album );

    if ( playlist_FindArtInCacheUsingItemUID( p_item ) != VLC_SUCCESS )
        playlist_FindArtInCache( p_item );
    else
        msg_Dbg( p_fetcher->object, "successfully retrieved arturl by uid" );

    char *psz_arturl = input_item_GetArtURL( p_item );
    if( psz_arturl )
    {
        /* We already have a URL */
        if( !strncmp( psz_arturl, "file://", strlen( "file://" ) ) )
        {
            free( psz_arturl );
            return 0; /* Art is in cache, no need to go further */
        }

        free( psz_arturl );

        /* Art need to be put in cache */
        return 1;
    }

    /* */
    psz_album = input_item_GetAlbum( p_item );
    psz_artist = input_item_GetArtist( p_item );
    if( psz_album && psz_artist )
    {
        msg_Dbg( p_fetcher->object, "searching art for %s - %s",
                 psz_artist, psz_album );
    }
    else
    {
        psz_title = input_item_GetTitle( p_item );
        if( !psz_title )
            psz_title = input_item_GetName( p_item );

        msg_Dbg( p_fetcher->object, "searching art for %s", psz_title );
        free( psz_title );
    }

    /* Fetch the art url */
    i_ret = VLC_EGENERIC;

    vlc_object_t *p_parent = p_fetcher->object;
    meta_fetcher_t *p_finder =
        vlc_custom_create( p_parent, sizeof( *p_finder ), "art finder" );
    if( p_finder != NULL)
    {
        module_t *p_module;

        p_finder->p_item = p_item;
        p_finder->e_scope = p_fetcher->e_scope;

        p_module = module_need( p_finder, "art finder", NULL, false );
        if( p_module )
        {
            module_unneed( p_finder, p_module );
            /* Try immediately if found in cache by download URL */
            if( !playlist_FindArtInCache( p_item ) )
                i_ret = 0;
            else
                i_ret = 1;
        }
        vlc_object_release( p_finder );
    }

    /* Record this album */
    if( psz_artist && psz_album )
    {
        if ( p_album )
        {
            p_album->e_scope = p_fetcher->e_scope;
            free( p_album->psz_arturl );
            p_album->psz_arturl = input_item_GetArtURL( p_item );
            p_album->b_found = (i_ret == VLC_EGENERIC ? false : true );
            free( psz_artist );
            free( psz_album );
        }
        else
        {
            playlist_album_t a;
            a.psz_artist = psz_artist;
            a.psz_album = psz_album;
            a.psz_arturl = input_item_GetArtURL( p_item );
            a.b_found = (i_ret == VLC_EGENERIC ? false : true );
            a.e_scope = p_fetcher->e_scope;
            ARRAY_APPEND( p_fetcher->albums, a );
        }
    }
    else
    {
        free( psz_artist );
        free( psz_album );
    }

    return i_ret;
}

/**
 * Download the art using the URL or an art downloaded
 * This function should be called only if data is not already in cache
 */
static int DownloadArt( playlist_fetcher_t *p_fetcher, input_item_t *p_item )
{
    char *psz_arturl = input_item_GetArtURL( p_item );
    assert( *psz_arturl );

    if( !strncmp( psz_arturl , "file://", 7 ) )
    {
        msg_Dbg( p_fetcher->object,
                 "Album art is local file, no need to cache" );
        free( psz_arturl );
        return VLC_SUCCESS;
    }

    if( !strncmp( psz_arturl , "APIC", 4 ) )
    {
        msg_Warn( p_fetcher->object, "APIC fetch not supported yet" );
        goto error;
    }

    stream_t *p_stream = vlc_stream_NewMRL( p_fetcher->object, psz_arturl );
    if( !p_stream )
        goto error;

    uint8_t *p_data = NULL;
    int i_data = 0;
    for( ;; )
    {
        int i_read = 65536;

        if( i_data >= INT_MAX - i_read )
            break;

        p_data = realloc_or_free( p_data, i_data + i_read );
        if( !p_data )
            break;

        i_read = vlc_stream_Read( p_stream, &p_data[i_data], i_read );
        if( i_read <= 0 )
            break;

        i_data += i_read;
    }
    vlc_stream_Delete( p_stream );

    if( p_data && i_data > 0 )
    {
        char *psz_type = strrchr( psz_arturl, '.' );
        if( psz_type && strlen( psz_type ) > 5 )
            psz_type = NULL; /* remove extension if it's > to 4 characters */

        playlist_SaveArt( p_fetcher->object, p_item,
                          p_data, i_data, psz_type );
    }

    free( p_data );

    free( psz_arturl );
    return VLC_SUCCESS;

error:
    free( psz_arturl );
    return VLC_EGENERIC;
}

/**
 * FetchMeta, run the "meta fetcher". They are going to do network
 * connections, and gather information upon the playing media.
 * (even artwork).
 */
static void FetchMeta( playlist_fetcher_t *p_fetcher, input_item_t *p_item )
{
    meta_fetcher_t *p_finder =
        vlc_custom_create( p_fetcher->object, sizeof( *p_finder ), "art finder" );
    if ( !p_finder )
        return;

    p_finder->e_scope = p_fetcher->e_scope;
    p_finder->p_item = p_item;

    module_t *p_module = module_need( p_finder, "meta fetcher", NULL, false );
    if( p_module )
        module_unneed( p_finder, p_module );

    vlc_object_release( p_finder );
}

static void *Thread( void *p_data )
{
    playlist_fetcher_t *p_fetcher = p_data;
    vlc_object_t *obj = p_fetcher->object;
    fetcher_pass_t e_pass = PASS1_LOCAL;

    vlc_interrupt_set(p_fetcher->interrupt);

    for( ;; )
    {
        fetcher_entry_t *p_entry = NULL;

        vlc_mutex_lock( &p_fetcher->lock );
        for ( int i=0; i<PASS_COUNT; i++ )
        {
            if ( p_fetcher->p_waiting_head[i] )
            {
                e_pass = i;
                break;
            }
        }

        if( p_fetcher->p_waiting_head[e_pass] )
        {
            p_entry = p_fetcher->p_waiting_head[e_pass];
            p_fetcher->p_waiting_head[e_pass] = p_entry->p_next;
            if ( p_entry->p_next == NULL )
                p_fetcher->p_waiting_tail[e_pass] = NULL;
            p_entry->p_next = NULL;
        }
        else
        {
            vlc_interrupt_set( NULL );
            p_fetcher->b_live = false;
            vlc_cond_signal( &p_fetcher->wait );
        }
        vlc_mutex_unlock( &p_fetcher->lock );

        if( !p_entry )
            break;

        meta_fetcher_scope_t e_prev_scope = p_fetcher->e_scope;

        /* scope override */
        switch ( p_entry->i_options ) {
        case META_REQUEST_OPTION_SCOPE_ANY:
            p_fetcher->e_scope = FETCHER_SCOPE_ANY;
            break;
        case META_REQUEST_OPTION_SCOPE_LOCAL:
            p_fetcher->e_scope = FETCHER_SCOPE_LOCAL;
            break;
        case META_REQUEST_OPTION_SCOPE_NETWORK:
            p_fetcher->e_scope = FETCHER_SCOPE_NETWORK;
            break;
        case META_REQUEST_OPTION_NONE:
        default:
            break;
        }
        /* Triggers "meta fetcher", eventually fetch meta on the network.
         * They are identical to "meta reader" expect that may actually
         * takes time. That's why they are running here.
         * The result of this fetch is not cached. */

        int i_ret = -1;

        if( e_pass == PASS1_LOCAL && ( p_fetcher->e_scope & FETCHER_SCOPE_LOCAL ) )
        {
            /* only fetch from local */
            p_fetcher->e_scope = FETCHER_SCOPE_LOCAL;
        }
        else if( e_pass == PASS2_NETWORK && ( p_fetcher->e_scope & FETCHER_SCOPE_NETWORK ) )
        {
            /* only fetch from network */
            p_fetcher->e_scope = FETCHER_SCOPE_NETWORK;
        }
        else
            p_fetcher->e_scope = 0;
        if ( p_fetcher->e_scope & FETCHER_SCOPE_ANY )
        {
            FetchMeta( p_fetcher, p_entry->p_item );
            i_ret = FindArt( p_fetcher, p_entry->p_item );
            switch( i_ret )
            {
            case 1: /* Found, need to dl */
                i_ret = DownloadArt( p_fetcher, p_entry->p_item );
                break;
            case 0: /* Is in cache */
                i_ret = VLC_SUCCESS;
                //ft
            default:// error
                break;
            }
        }

        p_fetcher->e_scope = e_prev_scope;
        /* */
        if ( i_ret != VLC_SUCCESS && (e_pass != PASS2_NETWORK) )
        {
            /* Move our entry to next pass queue */
            vlc_mutex_lock( &p_fetcher->lock );
            if ( p_fetcher->p_waiting_head[e_pass + 1] )
                p_fetcher->p_waiting_tail[e_pass + 1]->p_next = p_entry;
            else
                p_fetcher->p_waiting_head[e_pass + 1] = p_entry;
            p_fetcher->p_waiting_tail[e_pass + 1] = p_entry;
            vlc_mutex_unlock( &p_fetcher->lock );
        }
        else
        {
            /* */
            char *psz_name = input_item_GetName( p_entry->p_item );
            if( i_ret == VLC_SUCCESS ) /* Art is now in cache */
            {
                msg_Dbg( obj, "found art for %s in cache", psz_name );
                input_item_SetArtFetched( p_entry->p_item, true );
                var_SetAddress( obj, "item-change", p_entry->p_item );
            }
            else
            {
                msg_Dbg( obj, "art not found for %s", psz_name );
                input_item_SetArtNotFound( p_entry->p_item, true );
            }
            free( psz_name );
            vlc_gc_decref( p_entry->p_item );
            free( p_entry );
        }
    }
    return NULL;
}
