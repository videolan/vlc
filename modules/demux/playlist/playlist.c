/*****************************************************************************
 * playlist.c :  Playlist import module
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id: playlist.c,v 1.4 2004/01/26 20:48:51 fenrir Exp $
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
    if ( ! psz_name ) psz_name = strrchr( psz_path, '/' );
#endif
    if( psz_name ) *psz_name = '\0';
    else *psz_path = '\0';
    return psz_path;
}

/**
 * Add the directory part of the playlist file to the start of the
 * mrl, if the mrl is a relative file path
 */
char *ProcessMRL( char *psz_mrl, char *psz_prefix )
{
    char *psz_name;
    /* check for a protocol name */
    /* for URL, we should look for "://"
     * for MRL (Media Resource Locator) ([[<access>][/<demux>]:][<source>]),
     * we should look for ":"
     * so we end up looking simply for ":"*/
    /* PB: on some file systems, ':' are valid characters though*/
    psz_name = psz_mrl;
    while( *psz_name && *psz_name!=':' )
    {
        psz_name++;
    }
#ifdef WIN32
    if ( *psz_name && ( psz_name == psz_mrl + 1 ) )
    {
        /* if it is not an URL,
         * as it is unlikely to be an MRL (PB: if it is ?)
         * it should be an absolute file name with the drive letter */
        if ( *(psz_name+1) == '/' )/* "*:/" */
        {
            if ( *(psz_name+2) != '/' )/* not "*://" */
                while ( *psz_name ) *psz_name++;/* so now (*psz_name==0) */
        }
        else while ( *psz_name ) *psz_name++;/* "*:*"*/
    }
#endif

    /* if the line doesn't specify a protocol name,
     * check if the line has an absolute or relative path */
#ifndef WIN32
    if( !*psz_name && *psz_mrl != '/' )
         /* If this line doesn't begin with a '/' */
#else
    if( !*psz_name
            && *psz_mrl!='/'
            && *psz_mrl!='\\'
            && *(psz_mrl+1)!=':' )
         /* if this line doesn't begin with
          *  "/" or "\" or "*:" or "*:\" or "*:/" or "\\" */
#endif
    {
#ifndef WIN32
        psz_name = malloc( strlen(psz_prefix) + strlen(psz_mrl) + 2 );
        sprintf( psz_name, "%s/%s", psz_prefix, psz_mrl );
#else
        if ( *p_m3u->psz_prefix != '\0' )
        {
            psz_name = malloc( strlen(psz_prefix) + strlen(psz_mrl) + 2 );
            sprintf( psz_name, "%s\\%s", psz_prefix, psz_mrl );
        }
        else psz_name = strdup( psz_mrl );
#endif
    }
    else
    {
        psz_name = strdup( psz_mrl );
    }
    return psz_name;
}

