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
#include <vlc_events.h>
#include "playlist_internal.h"
#include "config/config.h"
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
        msg_Err( p_playlist , "could not create playlist file %s (%m)",
                 psz_filename );
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

/*****************************************************************************
 * A subitem has been added to the Media Library (Event Callback)
 *****************************************************************************/
static void input_item_subitem_added( const vlc_event_t * p_event,
                                      void * user_data )
{
    playlist_t *p_playlist = user_data;
    input_item_t *p_item = p_event->u.input_item_subitem_added.p_new_child;
    vlc_bool_t b_node = p_event->u.input_item_subitem_added.b_node;

    /* The media library input has one and only one option: "meta-file"
     * So we remove that unneeded option. */
    if( p_item->i_options == 1 )
    {
        free( p_item->ppsz_options[0] );
        p_item->i_options = 0;
    }

    playlist_AddInput( p_playlist, p_item, PLAYLIST_APPEND, PLAYLIST_END,
            VLC_FALSE, VLC_FALSE );

    if( b_node )
    {
        playlist_item_t *p_pl_item, *p_new_pl_item;
        p_pl_item = playlist_ItemFindFromInputAndRoot( p_playlist, p_item->i_id,
                                    p_playlist->p_root_category, VLC_FALSE );
        p_new_pl_item = playlist_ItemToNode( p_playlist, p_pl_item, VLC_FALSE );
        p_new_pl_item->p_input->i_type = ITEM_TYPE_NODE;
    }
}

int playlist_MLLoad( playlist_t *p_playlist )
{
    const char *psz_datadir = p_playlist->p_libvlc->psz_datadir;
    char *psz_uri = NULL;
    input_item_t *p_input;

    if( !config_GetInt( p_playlist, "media-library") ) return VLC_SUCCESS;
    if( !psz_datadir ) /* XXX: This should never happen */
    {
        msg_Err( p_playlist, "no data directory, cannot load media library") ;
        return VLC_EGENERIC;
    }

    if( asprintf( &psz_uri, "%s" DIR_SEP "ml.xspf", psz_datadir ) == -1 )
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

    if( asprintf( &psz_uri, "file/xspf-open://%s" DIR_SEP "ml.xspf",
                  psz_datadir ) == -1 )
    {
        psz_uri = NULL;
        goto error;
    }

    const char *const psz_option = "meta-file";
    /* that option has to be cleaned in input_item_subitem_added() */
    p_input = input_ItemNewExt( p_playlist, psz_uri,
                                _("Media Library"), 1, &psz_option, -1 );
    if( p_input == NULL )
        goto error;

    p_playlist->p_ml_onelevel->p_input =
    p_playlist->p_ml_category->p_input = p_input;

    vlc_event_attach( &p_input->event_manager, vlc_InputItemSubItemAdded,
                        input_item_subitem_added, p_playlist );

    p_playlist->b_doing_ml = VLC_TRUE;
    stats_TimerStart( p_playlist, "ML Load", STATS_TIMER_ML_LOAD );
    input_Read( p_playlist, p_input, VLC_TRUE );
    stats_TimerStop( p_playlist,STATS_TIMER_ML_LOAD );
    p_playlist->b_doing_ml = VLC_FALSE;

    vlc_event_detach( &p_input->event_manager, vlc_InputItemSubItemAdded,
                        input_item_subitem_added, p_playlist );

    free( psz_uri );
    return VLC_SUCCESS;

error:
    free( psz_uri );
    return VLC_ENOMEM;
}

int playlist_MLDump( playlist_t *p_playlist )
{
    char *psz_datadir = p_playlist->p_libvlc->psz_datadir;
    if( !config_GetInt( p_playlist, "media-library") ) return VLC_SUCCESS;
    if( !psz_datadir ) /* XXX: This should never happen */
    {
        msg_Err( p_playlist, "no data directory, cannot save media library") ;
        return VLC_EGENERIC;
    }

    char psz_dirname[ strlen( psz_datadir )
                      + sizeof( DIR_SEP "ml.xspf")];
    sprintf( psz_dirname, "%s", psz_datadir );
    if( config_CreateDir( (vlc_object_t *)p_playlist, psz_dirname ) )
    {
        return VLC_EGENERIC;
    }

    strcat( psz_dirname, DIR_SEP "ml.xspf" );

    stats_TimerStart( p_playlist, "ML Dump", STATS_TIMER_ML_DUMP );
    playlist_Export( p_playlist, psz_dirname, p_playlist->p_ml_category,
                     "export-xspf" );
    vlc_gc_decref( p_playlist->p_ml_category->p_input );
    stats_TimerStop( p_playlist, STATS_TIMER_ML_DUMP );

    return VLC_SUCCESS;
}
