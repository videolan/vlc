/*****************************************************************************
 * preparse.c: Preparser thread.
 *****************************************************************************
 * Copyright © 1999-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Clément Stenac <zorglub@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_playlist.h>

#include "art.h"
#include "fetcher.h"
#include "playlist_internal.h"

static void *Thread( void * );

playlist_fetcher_t *playlist_fetcher_New( playlist_t *p_playlist )
{
    // Secondary Preparse
    playlist_fetcher_t *p_fetcher = malloc( sizeof(*p_fetcher) );
    if( !p_fetcher )
        return NULL;

    p_fetcher->p_playlist = p_playlist;
    vlc_mutex_init( &p_fetcher->lock );
    vlc_cond_init( &p_fetcher->wait );
    p_fetcher->i_waiting = 0;
    p_fetcher->pp_waiting = NULL;
    p_fetcher->i_art_policy = var_CreateGetInteger( p_playlist, "album-art" );
    ARRAY_INIT( p_fetcher->albums );

    if( vlc_clone( &p_fetcher->thread, Thread, p_fetcher,
                   VLC_THREAD_PRIORITY_LOW ) )
    {
        msg_Err( p_playlist, "cannot spawn secondary preparse thread" );
        free( p_fetcher );
        return NULL;
    }

    return p_fetcher;
}

void playlist_fetcher_Push( playlist_fetcher_t *p_fetcher, input_item_t *p_item )
{
    vlc_gc_incref( p_item );

    vlc_mutex_lock( &p_fetcher->lock );
    INSERT_ELEM( p_fetcher->pp_waiting, p_fetcher->i_waiting,
                 p_fetcher->i_waiting, p_item );
    vlc_cond_signal( &p_fetcher->wait );
    vlc_mutex_unlock( &p_fetcher->lock );
}

void playlist_fetcher_Delete( playlist_fetcher_t *p_fetcher )
{
    /* Destroy the item meta-infos fetcher */
    vlc_cancel( p_fetcher->thread );
    vlc_join( p_fetcher->thread, NULL );

    while( p_fetcher->i_waiting > 0 )
    {   /* Any left-over unparsed item? */
        vlc_gc_decref( p_fetcher->pp_waiting[0] );
        REMOVE_ELEM( p_fetcher->pp_waiting, p_fetcher->i_waiting, 0 );
    }
    vlc_cond_destroy( &p_fetcher->wait );
    vlc_mutex_destroy( &p_fetcher->lock );
    free( p_fetcher );
}

/**
 * This function locates the art associated to an input item.
 * Return codes:
 *   0 : Art is in cache or is a local file
 *   1 : Art found, need to download
 *  -X : Error/not found
 */
static int FindArt( playlist_fetcher_t *p_fetcher, input_item_t *p_item )
{
    playlist_t *p_playlist = p_fetcher->p_playlist;
    int i_ret = VLC_EGENERIC;
    module_t *p_module;
    char *psz_title, *psz_artist, *psz_album;

    psz_artist = input_item_GetArtist( p_item );
    psz_album = input_item_GetAlbum( p_item );
    psz_title = input_item_GetTitle( p_item );
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
                msg_Dbg( p_playlist, " %s - %s has already been searched",
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
                else
                {
                    return VLC_EGENERIC;
                }
            }
        FOREACH_END();
    }
    free( psz_artist );
    free( psz_album );

    playlist_FindArtInCache( p_item );

    char *psz_arturl = input_item_GetArtURL( p_item );
    if( psz_arturl )
    {
        /* We already have an URL */
        if( !strncmp( psz_arturl, "file://", strlen( "file://" ) ) )
        {
            free( psz_arturl );
            return 0; /* Art is in cache, no need to go further */
        }

        free( psz_arturl );

        /* Art need to be put in cache */
        return 1;
    }

    PL_LOCK;
    p_playlist->p_private = p_item;
    psz_album = input_item_GetAlbum( p_item );
    psz_artist = input_item_GetArtist( p_item );
    psz_title = input_item_GetTitle( p_item );
    if( !psz_title )
        psz_title = input_item_GetName( p_item );

    if( psz_album && psz_artist )
    {
        msg_Dbg( p_playlist, "searching art for %s - %s",
             psz_artist, psz_album );
    }
    else
    {
        msg_Dbg( p_playlist, "searching art for %s",
             psz_title );
    }
    free( psz_title );

    p_module = module_need( p_playlist, "art finder", NULL, false );

    if( p_module )
        i_ret = 1;
    else
        msg_Dbg( p_playlist, "unable to find art" );

    /* Record this album */
    if( psz_artist && psz_album )
    {
        playlist_album_t a;
        a.psz_artist = psz_artist;
        a.psz_album = psz_album;
        a.psz_arturl = input_item_GetArtURL( p_item );
        a.b_found = (i_ret == VLC_EGENERIC ? false : true );
        ARRAY_APPEND( p_fetcher->albums, a );
    }
    else
    {
        free( psz_artist );
        free( psz_album );
    }

    if( p_module )
        module_unneed( p_playlist, p_module );
    p_playlist->p_private = NULL;
    PL_UNLOCK;

    return i_ret;
}


static void *Thread( void *p_data )
{
    playlist_fetcher_t *p_fetcher = p_data;
    playlist_t *p_playlist = p_fetcher->p_playlist;
    playlist_private_t *p_sys = pl_priv( p_playlist );

    for( ;; )
    {
        input_item_t *p_item;

        /* */
        vlc_mutex_lock( &p_fetcher->lock );
        mutex_cleanup_push( &p_fetcher->lock );

        while( p_fetcher->i_waiting == 0 )
            vlc_cond_wait( &p_fetcher->wait, &p_fetcher->lock );

        p_item = p_fetcher->pp_waiting[0];
        REMOVE_ELEM( p_fetcher->pp_waiting, p_fetcher->i_waiting, 0 );
        vlc_cleanup_run( );

        if( !p_item )
            continue;

        /* */
        int canc = vlc_savecancel();
        {
            int i_ret;

            /* Check if it is not yet preparsed and if so wait for it
             * (at most 0.5s)
             * (This can happen if we fetch art on play)
             * FIXME this doesn't work if we need to fetch meta before art...
             */
            for( i_ret = 0; i_ret < 10 && !input_item_IsPreparsed( p_item ); i_ret++ )
            {
                bool b_break;
                PL_LOCK;
                b_break = ( !p_sys->p_input || input_GetItem(p_sys->p_input) != p_item  ||
                            p_sys->p_input->b_die || p_sys->p_input->b_eof || p_sys->p_input->b_error );
                PL_UNLOCK;
                if( b_break )
                    break;
                msleep( 50000 );
            }

            i_ret = FindArt( p_fetcher, p_item );
            if( i_ret == 1 )
            {
                PL_DEBUG( "downloading art for %s", p_item->psz_name );
                if( playlist_DownloadArt( p_playlist, p_item ) )
                    input_item_SetArtNotFound( p_item, true );
                else {
                    input_item_SetArtFetched( p_item, true );
                    var_SetInteger( p_playlist, "item-change",
                                    p_item->i_id );
                }
            }
            else if( i_ret == 0 ) /* Was in cache */
            {
                PL_DEBUG( "found art for %s in cache", p_item->psz_name );
                input_item_SetArtFetched( p_item, true );
                var_SetInteger( p_playlist, "item-change", p_item->i_id );
            }
            else
            {
                PL_DEBUG( "art not found for %s", p_item->psz_name );
                input_item_SetArtNotFound( p_item, true );
            }
            vlc_gc_decref( p_item );
        }
        vlc_restorecancel( canc );

        int i_activity = var_GetInteger( p_playlist, "activity" );
        if( i_activity < 0 ) i_activity = 0;
        /* Sleep at least 1ms */
        msleep( (i_activity+1) * 1000 );
    }
    return NULL;
}


