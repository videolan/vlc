/*****************************************************************************
 * loadsave.c : Playlist loading / saving functions
 *****************************************************************************
 * Copyright (C) 1999-2004 VideoLAN
 * $Id: loadsave.c,v 1.4 2004/01/11 00:45:06 zorglub Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/
#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/sout.h>

#include "stream_control.h"
#include "input_ext-intf.h"

#include "vlc_playlist.h"

#define PLAYLIST_FILE_HEADER  "# vlc playlist file version 0.5"


/*****************************************************************************
 * playlist_Import: load a playlist file.
 ****************************************************************************/
int playlist_Import( playlist_t * p_playlist, const char *psz_filename )
{
    playlist_item_t *p_item;
    char *psz_uri;
    int i_id;

    msg_Dbg( p_playlist, "clearing playlist");

    /* Create our "fake" playlist item */
    playlist_Clear( p_playlist );


    psz_uri = (char *)malloc(sizeof(char)*strlen(psz_filename) + 17 );
    sprintf( psz_uri, "file/playlist://%s", psz_filename);

    i_id = playlist_Add( p_playlist, psz_uri, psz_uri,
                  PLAYLIST_INSERT | PLAYLIST_GO , PLAYLIST_END);

    p_item = playlist_GetItemById( p_playlist, i_id );
    p_item->b_autodeletion = VLC_TRUE;

    //p_playlist->i_index = 0;

/*
 *     if( p_item )
    {
        p_playlist->p_input = input_CreateThread( p_playlist, p_item );
    }
    */

    return VLC_SUCCESS;
}

/*****************************************************************************
 * playlist_SaveFile: Save a playlist in a file.
 *****************************************************************************/
int playlist_Export( playlist_t * p_playlist, const char *psz_filename ,
                     const char *psz_type)
{
    extern int errno;
    module_t *p_module;
    playlist_export_t *p_export;

    msg_Info( p_playlist, "Saving playlist to file %s", psz_filename );

    /* Prepare the playlist_export_t structure */
    p_export = (playlist_export_t *)malloc( sizeof(playlist_export_t) );
    if( !p_export)
    {
        msg_Err( p_playlist, "Out of memory");
        return VLC_ENOMEM;
    }
    p_export->p_file = fopen( psz_filename, "wt" );
    if( !p_export->p_file )
    {
        msg_Err( p_playlist , "Could not create playlist file %s (%s)"
                , psz_filename, strerror(errno) );
        return -1;
    }

    p_playlist->p_private = (void *)p_export;
    /* Lock the playlist */
    vlc_mutex_lock( &p_playlist->object_lock );

    /* And call the module ! All work is done now */
    p_module = module_Need( p_playlist, "playlist export",  psz_type);
    if( !p_module )
    {
        msg_Warn( p_playlist, "Failed to export playlist" );
        vlc_mutex_unlock( &p_playlist->object_lock );
        return VLC_ENOOBJ;
    }
    module_Unneed( p_playlist , p_module );

    fclose( p_export->p_file );

    vlc_mutex_unlock( &p_playlist->object_lock );

    return VLC_SUCCESS;
}
