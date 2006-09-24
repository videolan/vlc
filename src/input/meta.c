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

#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif

int __input_MetaFetch( vlc_object_t *p_parent, input_item_t *p_item )
{
    struct meta_engine_t *p_me;

    /* FIXME: don't launch any module if we already have all the needed
     * info. Easiest way to do this would be to add a dummy module.
     * I'll do that later */

    p_me = vlc_object_create( p_parent, VLC_OBJECT_META_ENGINE );
    p_me->i_flags |= OBJECT_FLAGS_NOINTERACT;
    p_me->i_mandatory =   VLC_META_ENGINE_TITLE
                        | VLC_META_ENGINE_ARTIST;
    p_me->i_optional = 0;
/*
    if( var_CreateGetInteger( p_parent, "album-art" ) != ALBUM_ART_NEVER )
    {
        p_me->i_mandatory |= VLC_META_ENGINE_ART_URL;
    }
    else
    {
        p_me->i_optional |= VLC_META_ENGINE_ART_URL;
    }
*/
    p_me->p_item = p_item;
    p_me->p_module = module_Need( p_me, "meta fetcher", 0, VLC_FALSE );
    vlc_object_attach( p_me, p_parent );
    if( !p_me->p_module )
    {
        msg_Err( p_parent, "no suitable meta engine module" );
        vlc_object_detach( p_me );
        vlc_object_destroy( p_me );
        return VLC_EGENERIC;
    }

    module_Unneed( p_me, p_me->p_module );

    vlc_object_destroy( p_me );

    return VLC_SUCCESS;
}

#ifndef MAX_PATH
#   define MAX_PATH 250
#endif
int input_FindArt( vlc_object_t *p_parent, input_item_t *p_item )
{
    char *psz_artist;
    char *psz_album;
    char *psz_type;
    char psz_filename[MAX_PATH];
    int i_ret;
    struct stat a;

    if( !p_item->p_meta ) return VLC_EGENERIC;

    psz_artist = p_item->p_meta->psz_artist;
    psz_album = p_item->p_meta->psz_album;

    //FIXME !!!!!
    psz_type = strdup( "jpg" );

    snprintf( psz_filename, MAX_PATH,
              "file://%s" DIR_SEP CONFIG_DIR DIR_SEP "art"
              DIR_SEP "%s" DIR_SEP "%s" DIR_SEP "art%s",
              p_parent->p_libvlc->psz_homedir,
              psz_artist, psz_album, psz_type );

    /* Check if file exists */
    i_ret = utf8_stat( psz_filename+7, &a );
    if( i_ret == 0 )
    {
        msg_Dbg( p_parent, "album art %s already exists in cache"
                         , psz_filename );
        return VLC_SUCCESS;
    }
    else
    {
        /* Use a art finder module to find the URL */
        return VLC_EGENERIC;
    }
}

/**
 * Download the art using the URL or an art downloaded
 * This function should be called only if data is not already in cache
 */
int input_DownloadAndCacheArt( vlc_object_t *p_parent, input_item_t *p_item )
{
    int i_status = VLC_EGENERIC;
    int i_ret;
    struct stat a;
    stream_t *p_stream;
    char psz_filename[MAX_PATH], psz_dir[MAX_PATH];
    char *psz_artist;
    char *psz_album;
    char *psz_type;
    psz_artist = p_item->p_meta->psz_artist;
    psz_album = p_item->p_meta->psz_album;

    /* You dummy ! How am I supposed to download NULL ? */
    if( !p_item->p_meta || !p_item->p_meta->psz_arturl
                        || !*p_item->p_meta->psz_arturl )
        return VLC_EGENERIC;

    /* Todo: get a helper to do this */
    snprintf( psz_filename, MAX_PATH,
              "file://%s" DIR_SEP CONFIG_DIR DIR_SEP "art"
              DIR_SEP "%s" DIR_SEP "%s" DIR_SEP "art%s",
              p_parent->p_libvlc->psz_homedir,
              psz_artist, psz_album, psz_type );

    snprintf( psz_dir, MAX_PATH, "%s" DIR_SEP CONFIG_DIR,
              p_parent->p_libvlc->psz_homedir );
    utf8_mkdir( psz_dir );
    snprintf( psz_dir, MAX_PATH, "%s" DIR_SEP CONFIG_DIR DIR_SEP "art",
              p_parent->p_libvlc->psz_homedir );
    utf8_mkdir( psz_dir );
    snprintf( psz_dir, MAX_PATH, "%s" DIR_SEP CONFIG_DIR DIR_SEP
              "art" DIR_SEP "%s",
                 p_parent->p_libvlc->psz_homedir, psz_artist );
    utf8_mkdir( psz_dir );
    snprintf( psz_dir, MAX_PATH, "%s" DIR_SEP CONFIG_DIR DIR_SEP
              "art" DIR_SEP "%s" DIR_SEP "%s",
                      p_parent->p_libvlc->psz_homedir,
                      psz_artist, psz_album );
    utf8_mkdir( psz_dir );

    /* Todo: check for stuff that needs a downloader module */
    p_stream = stream_UrlNew( p_parent, p_item->p_meta->psz_arturl );

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
        msg_Dbg( p_parent, "Album art saved to %s\n", psz_filename );
        free( p_item->p_meta->psz_arturl );
        p_item->p_meta->psz_arturl = strdup( psz_filename );
        i_status = VLC_SUCCESS;
    }
    return i_status;
}
