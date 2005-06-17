/*****************************************************************************
 * playlist.c :  Playlist import module
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc_playlist.h>

#include "playlist.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define AUTOSTART_TEXT N_( "Auto start" )
#define AUTOSTART_LONGTEXT N_( "Automatically start the playlist when " \
    "it's loaded.\n" )

vlc_module_begin();
    add_shortcut( "playlist" );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );

    add_bool( "playlist-autostart", 1, NULL, AUTOSTART_TEXT, AUTOSTART_LONGTEXT,
              VLC_FALSE );

    set_description( _("Old playlist open") );
    add_shortcut( "old-open" );
    set_capability( "demux2", 10 );
    set_callbacks( E_(Import_Old), NULL );
#if 0
    add_submodule();
        set_description( _("Native playlist import") );
        add_shortcut( "playlist" );
        add_shortcut( "native-open" );
        set_capability( "demux2", 10 );
        set_callbacks( E_(Import_Native), E_(Close_Native) );
#endif
    add_submodule();
        set_description( _("M3U playlist import") );
        add_shortcut( "m3u-open" );
        set_capability( "demux2", 10 );
        set_callbacks( E_(Import_M3U), E_(Close_M3U) );
    add_submodule();
        set_description( _("PLS playlist import") );
        add_shortcut( "pls-open" );
        set_capability( "demux2", 10 );
        set_callbacks( E_(Import_PLS), E_(Close_PLS) );
    add_submodule();
        set_description( _("B4S playlist import") );
        add_shortcut( "b4s-open" );
        add_shortcut( "shout-b4s" );
        set_capability( "demux2", 10 );
        set_callbacks( E_(Import_B4S), E_(Close_B4S) );
vlc_module_end();


/**
 * Find directory part of the path to the playlist file, in case of
 * relative paths inside
 */
char *E_(FindPrefix)( demux_t *p_demux )
{
    char *psz_name;
    char *psz_path = strdup( p_demux->psz_path );

#ifndef WIN32
    psz_name = strrchr( psz_path, '/' );
#else
    psz_name = strrchr( psz_path, '\\' );
    if( !psz_name ) psz_name = strrchr( psz_path, '/' );
#endif
    if( psz_name ) psz_name[1] = '\0';
    else *psz_path = '\0';

    return psz_path;
}

/**
 * Add the directory part of the playlist file to the start of the
 * mrl, if the mrl is a relative file path
 */
char *E_(ProcessMRL)( char *psz_mrl, char *psz_prefix )
{
    /* Check for a protocol name.
     * for URL, we should look for "://"
     * for MRL (Media Resource Locator) ([[<access>][/<demux>]:][<source>]),
     * we should look for ":", so we end up looking simply for ":"
     * PB: on some file systems, ':' are valid characters though */

    /* Simple cases first */
    if( !psz_mrl || !*psz_mrl ) return NULL;
    if( !psz_prefix || !*psz_prefix ) return strdup( psz_mrl );

    /* Check if the line specifies an absolute path */
    if( *psz_mrl == '/' || *psz_mrl == '\\' ) return strdup( psz_mrl );

    /* Check if the line specifies an mrl/url
     * (and on win32, contains a drive letter) */
    if( strchr( psz_mrl, ':' ) ) return strdup( psz_mrl );

    /* This a relative path, prepend the prefix */
    asprintf( &psz_mrl, "%s%s", psz_prefix, psz_mrl );
    return psz_mrl;
}

vlc_bool_t E_(FindItem)( demux_t *p_demux, playlist_t *p_playlist,
                     playlist_item_t **pp_item )
{
     vlc_bool_t b_play = var_CreateGetBool( p_demux, "playlist-autostart" );

     if( b_play && p_playlist->status.p_item &&
             &p_playlist->status.p_item->input ==
                ((input_thread_t *)p_demux->p_parent)->input.p_item )
     {
         msg_Dbg( p_playlist, "starting playlist playback" );
         *pp_item = p_playlist->status.p_item;
         b_play = VLC_TRUE;
     }
     else
     {
         input_item_t *p_current = ( (input_thread_t*)p_demux->p_parent)->
                                                        input.p_item;
         *pp_item = playlist_LockItemGetByInput( p_playlist, p_current );
         if( !*pp_item )
         {
             msg_Dbg( p_playlist, "unable to find item in playlist");
         }
         msg_Dbg( p_playlist, "not starting playlist playback");
         b_play = VLC_FALSE;
     }
     return b_play;
}
