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
#include <vlc/vlc.h>
#include <vlc_playlist.h>
#include "playlist_internal.h"
#include "modules/configuration.h"
#include <vlc_charset.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

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

int playlist_MLLoad( playlist_t *p_playlist )
{
    const char *psz_homedir = p_playlist->p_libvlc->psz_homedir;
    char *psz_uri = NULL;
    input_item_t *p_input;

    if( !config_GetInt( p_playlist, "media-library") ) return VLC_SUCCESS;
    if( !psz_homedir )
    {
        msg_Err( p_playlist, "no home directory, cannot load media library") ;
        return VLC_EGENERIC;
    }

    if( asprintf( &psz_uri, "%s" DIR_SEP CONFIG_DIR DIR_SEP
                        "ml.xsp", psz_homedir ) == -1 )
    {
        psz_uri = NULL;
        goto error;
    }
    struct stat p_stat;
    /* checks if media library file is present */
    if( utf8_stat( psz_uri , &p_stat ) )
    {
        free( psz_uri );
        return VLC_EGENERIC;
    }
    free( psz_uri );

    if( asprintf( &psz_uri, "file/xspf-open://%s" DIR_SEP CONFIG_DIR DIR_SEP
                        "ml.xsp", psz_homedir ) == -1 )
    {
        psz_uri = NULL;
        goto error;
    }

    p_input = input_ItemNewExt( p_playlist, psz_uri,
                                _("Media Library"), 0, NULL, -1 );
    if( p_input == NULL )
        goto error;

    playlist_AddInput( p_playlist, p_input, PLAYLIST_APPEND, 0, VLC_FALSE,
                        VLC_FALSE );

    p_playlist->b_doing_ml = VLC_TRUE;
    stats_TimerStart( p_playlist, "ML Load", STATS_TIMER_ML_LOAD );
    input_Read( p_playlist, p_input, VLC_TRUE );
    stats_TimerStop( p_playlist,STATS_TIMER_ML_LOAD );
    p_playlist->b_doing_ml = VLC_FALSE;

    free( psz_uri );
    return VLC_SUCCESS;

error:
    free( psz_uri );
    return VLC_ENOMEM;
}

int playlist_MLDump( playlist_t *p_playlist )
{
    char *psz_homedir = p_playlist->p_libvlc->psz_homedir;
    if( !config_GetInt( p_playlist, "media-library") ) return VLC_SUCCESS;
    if( !psz_homedir )
    {
        msg_Err( p_playlist, "no home directory, cannot save media library") ;
        return VLC_EGENERIC;
    }

    char psz_dirname[ strlen( psz_homedir )
                      + sizeof( DIR_SEP CONFIG_DIR DIR_SEP "ml.xsl")];
    sprintf( psz_dirname, "%s" DIR_SEP CONFIG_DIR, psz_homedir );
    if( config_CreateDir( (vlc_object_t *)p_playlist, psz_dirname ) )
    {
        return VLC_EGENERIC;
    }

    strcat( psz_dirname, DIR_SEP "ml.xsp" );
    
    stats_TimerStart( p_playlist, "ML Dump", STATS_TIMER_ML_DUMP );
    playlist_Export( p_playlist, psz_dirname, p_playlist->p_ml_category,
                     "export-xspf" );
    stats_TimerStop( p_playlist, STATS_TIMER_ML_DUMP );

    return VLC_SUCCESS;
}
