/*****************************************************************************
 * loadsave.c : Playlist loading / saving functions
 *****************************************************************************
 * Copyright (C) 1999-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */
#include <errno.h>

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/sout.h>
#include <vlc/input.h>

#include "vlc_playlist.h"

#define PLAYLIST_FILE_HEADER  "# vlc playlist file version 0.5"

/**
 * Import a certain playlist file into the library
 * This file will get inserted as a new category
 *
 * XXX: TODO
 * \param p_playlist the playlist to which the new items will be added
 * \param psz_filename the name of the playlistfile to import
 * \return VLC_SUCCESS on success
 */
int playlist_Import( playlist_t * p_playlist, const char *psz_filename )
{
    playlist_item_t *p_item;
    char *psz_uri;
    int i_id;

    msg_Info( p_playlist, "clearing playlist");
    playlist_Clear( p_playlist );


    psz_uri = (char *)malloc(sizeof(char)*strlen(psz_filename) + 17 );
    sprintf( psz_uri, "file/playlist://%s", psz_filename);

    i_id = playlist_Add( p_playlist, psz_uri, psz_uri,
                  PLAYLIST_INSERT  , PLAYLIST_END);

    vlc_mutex_lock( &p_playlist->object_lock );
    p_item = playlist_ItemGetById( p_playlist, i_id );
    p_item->b_autodeletion = VLC_TRUE;
    vlc_mutex_unlock( &p_playlist->object_lock );

    playlist_Play(p_playlist);

    return VLC_SUCCESS;
}

/**
 * Load a certain playlist file into the playlist
 * This file will replace the contents of the "current" view
 *
 * \param p_playlist the playlist to which the new items will be added
 * \param psz_filename the name of the playlistfile to import
 * \return VLC_SUCCESS on success
 */
int playlist_Load( playlist_t * p_playlist, const char *psz_filename )
{
    playlist_item_t *p_item;
    char *psz_uri;
    int i_id;

    msg_Info( p_playlist, "clearing playlist");
    playlist_Clear( p_playlist );


    psz_uri = (char *)malloc(sizeof(char)*strlen(psz_filename) + 17 );
    sprintf( psz_uri, "file/playlist://%s", psz_filename);

    i_id = playlist_Add( p_playlist, psz_uri, psz_uri,
                  PLAYLIST_INSERT  , PLAYLIST_END);

    vlc_mutex_lock( &p_playlist->object_lock );
    p_item = playlist_ItemGetById( p_playlist, i_id );
    p_item->b_autodeletion = VLC_TRUE;
    vlc_mutex_unlock( &p_playlist->object_lock );

    playlist_Play(p_playlist);

    return VLC_SUCCESS;
}

/**
 * Export a playlist to a certain type of playlistfile
 *
 * \param p_playlist the playlist to export
 * \param psz_filename the location where the exported file will be saved
 * \param psz_type the type of playlist file to create.
 * \return VLC_SUCCESS on success
 */
int playlist_Export( playlist_t * p_playlist, const char *psz_filename ,
                     const char *psz_type)
{
    module_t *p_module;
    playlist_export_t *p_export;

    msg_Info( p_playlist, "saving playlist to file %s", psz_filename );

    /* Prepare the playlist_export_t structure */
    p_export = (playlist_export_t *)malloc( sizeof(playlist_export_t) );
    if( !p_export)
    {
        msg_Err( p_playlist, "out of memory");
        return VLC_ENOMEM;
    }
    p_export->p_file = fopen( psz_filename, "wt" );
    if( !p_export->p_file )
    {
        msg_Err( p_playlist , "could not create playlist file %s"
                 " (%s)", psz_filename, strerror(errno) );
        return VLC_EGENERIC;
    }

    p_playlist->p_private = (void *)p_export;
    /* Lock the playlist */
    vlc_mutex_lock( &p_playlist->object_lock );

    /* And call the module ! All work is done now */
    p_module = module_Need( p_playlist, "playlist export", psz_type, VLC_TRUE);
    if( !p_module )
    {
        msg_Warn( p_playlist, "failed to export playlist" );
        vlc_mutex_unlock( &p_playlist->object_lock );
        return VLC_ENOOBJ;
    }
    module_Unneed( p_playlist , p_module );

    fclose( p_export->p_file );

    vlc_mutex_unlock( &p_playlist->object_lock );

    return VLC_SUCCESS;
}
