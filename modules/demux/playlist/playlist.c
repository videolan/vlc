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

#include "playlist.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    add_shortcut( "playlist" );

    set_description( _("Old playlist open") );
    add_shortcut( "old-open" );
    set_capability( "demux2" , 10 );
    set_callbacks( Import_Old , NULL );

    add_submodule();
    set_description( _("M3U playlist import") );
    add_shortcut( "m3u-open" );
    set_capability( "demux2" , 10 );
    set_callbacks( Import_M3U , Close_M3U );

    add_submodule();
    set_description( _("PLS playlist import") );
    add_shortcut( "pls-open" );
    set_capability( "demux2" , 10 );
    set_callbacks( Import_PLS , Close_PLS );
vlc_module_end();


/**
 * Find directory part of the path to the playlist file, in case of
 * relative paths inside
 */
char *FindPrefix( demux_t *p_demux )
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
char *ProcessMRL( char *psz_mrl, char *psz_prefix )
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
