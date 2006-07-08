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
#include "charset.h"

#define PLAYLIST_FILE_HEADER  "# vlc playlist file version 0.5"

/**
 * Import a playlist file at a given point of a given view
 * \param p_playlist the playlist to which the new items will be added
 * \param psz_filename the name of the playlistfile to import
 * \return VLC_SUCCESS on success
 */
int playlist_Import( playlist_t * p_playlist, const char *psz_filename,
                     playlist_item_t *p_root, vlc_bool_t b_only_there )
{
    char *psz_uri, *psz_opt;
    input_item_t *p_input;
    
    asprintf( &psz_uri, "file/playlist://%s", psz_filename );
    p_input = input_ItemNewExt( p_playlist, psz_uri, "playlist", 0, NULL, -1 );
    if( b_only_there )
    { 
        asprintf( &psz_opt, "parent-item=%i", p_root->i_id );
        vlc_input_item_AddOption( p_input, psz_opt );
        free( psz_opt );
    }
    if( p_root == p_playlist->p_ml_category )
        p_input->i_id = p_playlist->p_ml_category->p_input->i_id;
    input_Read( p_playlist, p_input, VLC_TRUE );
    free( psz_uri );
    
    return VLC_SUCCESS;
}

/**
 * Load a playlist file to the playlist. It will create a new node in 
 * category
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

    i_id = playlist_PlaylistAdd( p_playlist, psz_uri, psz_uri,
                  PLAYLIST_INSERT  , PLAYLIST_END);

    vlc_mutex_lock( &p_playlist->object_lock );
    p_item = playlist_ItemGetById( p_playlist, i_id );
    vlc_mutex_unlock( &p_playlist->object_lock );

    playlist_Play(p_playlist);

    return VLC_SUCCESS;
}

/**
 * Export a node of the playlist to a certain type of playlistfile
 *
 * \param p_playlist the playlist to export
 * \param psz_filename the location where the exported file will be saved
 * \param p_export_root the root node to export
 * \param psz_type the type of playlist file to create.
 * \return VLC_SUCCESS on success
 */
int playlist_Export( playlist_t * p_playlist, const char *psz_filename ,
                     playlist_item_t *p_export_root,const char *psz_type )
{
    module_t *p_module;
    playlist_export_t *p_export;

    if( p_export_root == NULL ) return VLC_EGENERIC;

    msg_Info( p_playlist, "saving %s to file %s",
                    p_export_root->p_input->psz_name, psz_filename );

    /* Prepare the playlist_export_t structure */
    p_export = (playlist_export_t *)malloc( sizeof(playlist_export_t) );
    if( !p_export)
    {
        msg_Err( p_playlist, "out of memory" );
        return VLC_ENOMEM;
    }
    p_export->psz_filename = NULL;
    if ( psz_filename )
        p_export->psz_filename = strdup( psz_filename );
    p_export->p_file = utf8_fopen( psz_filename, "wt" );
    if( !p_export->p_file )
    {
        msg_Err( p_playlist , "could not create playlist file %s"
                 " (%s)", psz_filename, strerror(errno) );
        return VLC_EGENERIC;
    }

    p_export->p_root = p_export_root;

    /* Lock the playlist */
    vlc_mutex_lock( &p_playlist->object_lock );
    p_playlist->p_private = (void *)p_export;

    /* And call the module ! All work is done now */
    p_module = module_Need( p_playlist, "playlist export", psz_type, VLC_TRUE);
    if( !p_module )
    {
        msg_Warn( p_playlist, "exporting playlist failed" );
        vlc_mutex_unlock( &p_playlist->object_lock );
        return VLC_ENOOBJ;
    }
    module_Unneed( p_playlist , p_module );

    /* Clean up */
    fclose( p_export->p_file );
    if ( p_export->psz_filename )
        free( p_export->psz_filename );
    free ( p_export );
    p_playlist->p_private = NULL;
    vlc_mutex_unlock( &p_playlist->object_lock );

    return VLC_SUCCESS;
}
