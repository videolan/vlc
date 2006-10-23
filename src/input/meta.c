/*****************************************************************************
 * meta.c : Metadata handling
 *****************************************************************************
 * Copyright (C) 1998-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea@videolan.org>
 *          Clément Stenac <zorglub@videolan.org
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

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc_meta.h>
#include "vlc_playlist.h"
#include "charset.h"
#include "../playlist/playlist_internal.h"

#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif

int input_FindArtInCache( playlist_t *p_playlist, input_item_t *p_item );

vlc_bool_t input_MetaSatisfied( playlist_t *p_playlist, input_item_t *p_item,
                                uint32_t *pi_mandatory, uint32_t *pi_optional )
{
    *pi_mandatory = VLC_META_ENGINE_TITLE | VLC_META_ENGINE_ARTIST;

    uint32_t i_meta = input_CurrentMetaFlags( p_item->p_meta );
    *pi_mandatory &= ~i_meta;
    *pi_optional = 0; /// Todo
    return *pi_mandatory ? VLC_FALSE:VLC_TRUE;
}

int input_MetaFetch( playlist_t *p_playlist, input_item_t *p_item )
{
    struct meta_engine_t *p_me;
    uint32_t i_mandatory, i_optional;

    if( !p_item->p_meta ) return VLC_EGENERIC;

    input_MetaSatisfied( p_playlist, p_item, &i_mandatory, &i_optional );
    // Meta shouldn't magically appear
    assert( i_mandatory );

    /* FIXME: object creation is overkill, use p_private */
    p_me = vlc_object_create( p_playlist, VLC_OBJECT_META_ENGINE );
    p_me->i_flags |= OBJECT_FLAGS_NOINTERACT;
    p_me->i_flags |= OBJECT_FLAGS_QUIET;
    p_me->i_mandatory = i_mandatory;
    p_me->i_optional = i_optional;

    p_me->p_item = p_item;
    p_me->p_module = module_Need( p_me, "meta fetcher", 0, VLC_FALSE );
    if( !p_me->p_module )
    {
        vlc_object_destroy( p_me );
        return VLC_EGENERIC;
    }
    module_Unneed( p_me, p_me->p_module );
    vlc_object_destroy( p_me );
    return VLC_SUCCESS;
}

/* Return codes:
 *   0 : Art is in cache
 *   1 : Art found, need to download
 *  -X : Error/not found
 */
int input_ArtFind( playlist_t *p_playlist, input_item_t *p_item )
{
    int i_ret = VLC_EGENERIC;
    module_t *p_module;
    if( !p_item->p_meta || !p_item->p_meta->psz_album ||
                           !p_item->p_meta->psz_artist )
        return VLC_EGENERIC;

    /* If we already checked this album in this session, skip */
    FOREACH_ARRAY( playlist_album_t album, p_playlist->p_fetcher->albums )
        if( !strcmp( album.psz_artist, p_item->p_meta->psz_artist ) &&
            !strcmp( album.psz_album, p_item->p_meta->psz_album ) )
        {
            msg_Dbg( p_playlist, " %s - %s has already been searched",
                     p_item->p_meta->psz_artist,  p_item->p_meta->psz_album );
            if( album.b_found )
            {
                /* Actually get URL from cache */
                input_FindArtInCache( p_playlist, p_item );
                return 0;
            }
            else
                return VLC_EGENERIC;
        }
    FOREACH_END();

    input_FindArtInCache( p_playlist, p_item );
    if( !EMPTY_STR( p_item->p_meta->psz_arturl ) )
        return 0;

    PL_LOCK;
    p_playlist->p_private = p_item;
    msg_Dbg( p_playlist, "searching art for %s - %s",
             p_item->p_meta->psz_artist,  p_item->p_meta->psz_album );
    p_module = module_Need( p_playlist, "art finder", 0, VLC_FALSE );

    if( p_module )
        i_ret = 1;
    else
        msg_Dbg( p_playlist, "unable to find art" );

    /* Record this album */
    playlist_album_t a;
    a.psz_artist = strdup( p_item->p_meta->psz_artist );
    a.psz_album = strdup( p_item->p_meta->psz_album );
    a.b_found = (i_ret == VLC_EGENERIC ? VLC_FALSE : VLC_TRUE );
    ARRAY_APPEND( p_playlist->p_fetcher->albums, a );

    if( p_module )
        module_Unneed( p_playlist, p_module );
    p_playlist->p_private = NULL;
    PL_UNLOCK;

    return i_ret;
}

#ifndef MAX_PATH
#   define MAX_PATH 250
#endif
int input_FindArtInCache( playlist_t *p_playlist, input_item_t *p_item )
{
    char *psz_artist;
    char *psz_album;
    char psz_filename[MAX_PATH+1];
    int i;
    struct stat a;
    const char *ppsz_type[] = { ".jpg", ".png", ".gif", ".bmp", "" };

    if( !p_item->p_meta ) return VLC_EGENERIC;

    psz_artist = p_item->p_meta->psz_artist;
    psz_album = p_item->p_meta->psz_album;

    for( i = 0; i < 5; i++ )
    {
        snprintf( psz_filename, MAX_PATH,
                  "file://%s" DIR_SEP CONFIG_DIR DIR_SEP "art"
                  DIR_SEP "%s" DIR_SEP "%s" DIR_SEP "art%s",
                  p_playlist->p_libvlc->psz_homedir,
                  psz_artist, psz_album, ppsz_type[i] );

        /* Check if file exists */
        if( utf8_stat( psz_filename+7, &a ) == 0 )
        {
            vlc_meta_SetArtURL( p_item->p_meta, psz_filename );
            return VLC_SUCCESS;
        }
    }
    return VLC_EGENERIC;
}

/**
 * Download the art using the URL or an art downloaded
 * This function should be called only if data is not already in cache
 */
int input_DownloadAndCacheArt( playlist_t *p_playlist, input_item_t *p_item )
{
    int i_status = VLC_EGENERIC;
    stream_t *p_stream;
    char psz_filename[MAX_PATH+1], psz_dir[MAX_PATH+1];
    char *psz_artist;
    char *psz_album;
    char *psz_type;
    psz_artist = p_item->p_meta->psz_artist;
    psz_album = p_item->p_meta->psz_album;

    assert( p_item->p_meta && !EMPTY_STR(p_item->p_meta->psz_arturl) );

    /* FIXME: use an alternate saving filename scheme if we don't have
     * the artist or album name */
    if( !p_item->p_meta->psz_artist || !p_item->p_meta->psz_album )
        return VLC_EGENERIC;

    psz_type = strrchr( p_item->p_meta->psz_arturl, '.' );

    /* Todo: get a helper to do this */
    snprintf( psz_filename, MAX_PATH,
              "file://%s" DIR_SEP CONFIG_DIR DIR_SEP "art"
              DIR_SEP "%s" DIR_SEP "%s" DIR_SEP "art%s",
              p_playlist->p_libvlc->psz_homedir,
              psz_artist, psz_album, psz_type );

    snprintf( psz_dir, MAX_PATH, "%s" DIR_SEP CONFIG_DIR,
              p_playlist->p_libvlc->psz_homedir );
    utf8_mkdir( psz_dir );
    snprintf( psz_dir, MAX_PATH, "%s" DIR_SEP CONFIG_DIR DIR_SEP "art",
              p_playlist->p_libvlc->psz_homedir );
    utf8_mkdir( psz_dir );
    snprintf( psz_dir, MAX_PATH, "%s" DIR_SEP CONFIG_DIR DIR_SEP
              "art" DIR_SEP "%s",
                 p_playlist->p_libvlc->psz_homedir, psz_artist );
    utf8_mkdir( psz_dir );
    snprintf( psz_dir, MAX_PATH, "%s" DIR_SEP CONFIG_DIR DIR_SEP
              "art" DIR_SEP "%s" DIR_SEP "%s",
                      p_playlist->p_libvlc->psz_homedir,
                      psz_artist, psz_album );
    utf8_mkdir( psz_dir );

    if( !strncmp( p_item->p_meta->psz_arturl , "APIC", 4 ) )
    {
        msg_Warn( p_playlist, "APIC fetch not supported yet" );
        return VLC_EGENERIC;
    }

    p_stream = stream_UrlNew( p_playlist, p_item->p_meta->psz_arturl );
    if( p_stream )
    {
        void *p_buffer = malloc( 1<<16 );
        long int l_read;
        FILE *p_file = utf8_fopen( psz_filename+7, "w" );
        while( ( l_read = stream_Read( p_stream, p_buffer, 1<<16 ) ) )
        {
            fwrite( p_buffer, l_read, 1, p_file );
        }
        free( p_buffer );
        fclose( p_file );
        stream_Delete( p_stream );
        msg_Dbg( p_playlist, "album art saved to %s\n", psz_filename );
        free( p_item->p_meta->psz_arturl );
        p_item->p_meta->psz_arturl = strdup( psz_filename );
        i_status = VLC_SUCCESS;
    }
    return i_status;
}

uint32_t input_CurrentMetaFlags( vlc_meta_t *p_meta )
{
    uint32_t i_meta = 0;

#define CHECK( a, b ) \
    if( p_meta->psz_ ## a && *p_meta->psz_ ## a ) \
        i_meta |= VLC_META_ENGINE_ ## b;

    CHECK( title, TITLE )
    CHECK( artist, ARTIST )
    CHECK( album, COLLECTION )
#if 0
    /* As this is not used at the moment, don't uselessly check for it.
     * Re-enable this when it is used */
    CHECK( genre, GENRE )
    CHECK( copyright, COPYRIGHT )
    CHECK( tracknum, SEQ_NUM )
    CHECK( description, DESCRIPTION )
    CHECK( rating, RATING )
    CHECK( date, DATE )
    CHECK( url, URL )
    CHECK( language, LANGUAGE )
#endif
    CHECK( arturl, ART_URL )

    return i_meta;
}
