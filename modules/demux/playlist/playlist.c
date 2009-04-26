/*****************************************************************************
 * playlist.c :  Playlist import module
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>

#include "playlist.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define AUTOSTART_TEXT N_( "Auto start" )
#define AUTOSTART_LONGTEXT N_( "Automatically start playing the playlist " \
                "content once it's loaded." )

#define SHOW_ADULT_TEXT N_( "Show shoutcast adult content" )
#define SHOW_ADULT_LONGTEXT N_( "Show NC17 rated video streams when " \
                "using shoutcast video playlists." )

#define SKIP_ADS_TEXT N_( "Skip ads" )
#define SKIP_ADS_LONGTEXT N_( "Use playlist options usually used to prevent " \
    "ads skipping to detect ads and prevent adding them to the playlist." )

vlc_module_begin ()
    add_shortcut( "playlist" )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )

    add_bool( "playlist-autostart", 1, NULL,
              AUTOSTART_TEXT, AUTOSTART_LONGTEXT, false )

    add_integer( "parent-item", 0, NULL, NULL, NULL, true )
        change_internal ()

    add_bool( "playlist-skip-ads", 1, NULL,
              SKIP_ADS_TEXT, SKIP_ADS_LONGTEXT, false )

    set_shortname( N_("Playlist") )
    set_description( N_("Playlist") )
    add_submodule ()
        set_description( N_("M3U playlist import") )
        add_shortcut( "playlist" )
        add_shortcut( "m3u-open" )
        set_capability( "demux", 10 )
        set_callbacks( Import_M3U, Close_M3U )
    add_submodule ()
        set_description( N_("RAM playlist import") )
        add_shortcut( "playlist" )
        add_shortcut( "ram-open" )
        set_capability( "demux", 10 )
        set_callbacks( Import_RAM, Close_RAM )
    add_submodule ()
        set_description( N_("PLS playlist import") )
        add_shortcut( "playlist" )
        add_shortcut( "pls-open" )
        set_capability( "demux", 10 )
        set_callbacks( Import_PLS, Close_PLS )
    add_submodule ()
        set_description( N_("B4S playlist import") )
        add_shortcut( "playlist" )
        add_shortcut( "b4s-open" )
        add_shortcut( "shout-b4s" )
        set_capability( "demux", 10 )
        set_callbacks( Import_B4S, Close_B4S )
    add_submodule ()
        set_description( N_("DVB playlist import") )
        add_shortcut( "playlist" )
        add_shortcut( "dvb-open" )
        set_capability( "demux", 10 )
        set_callbacks( Import_DVB, Close_DVB )
    add_submodule ()
        set_description( N_("Podcast parser") )
        add_shortcut( "playlist" )
        add_shortcut( "podcast" )
        set_capability( "demux", 10 )
        set_callbacks( Import_podcast, Close_podcast )
    add_submodule ()
        set_description( N_("XSPF playlist import") )
        add_shortcut( "playlist" )
        add_shortcut( "xspf-open" )
        set_capability( "demux", 10 )
        set_callbacks( Import_xspf, Close_xspf )
    add_submodule ()
        set_description( N_("New winamp 5.2 shoutcast import") )
        add_shortcut( "playlist" )
        add_shortcut( "shout-winamp" )
        set_capability( "demux", 10 )
        set_callbacks( Import_Shoutcast, Close_Shoutcast )
        add_bool( "shoutcast-show-adult", false, NULL,
                   SHOW_ADULT_TEXT, SHOW_ADULT_LONGTEXT, false )
    add_submodule ()
        set_description( N_("ASX playlist import") )
        add_shortcut( "playlist" )
        add_shortcut( "asx-open" )
        set_capability( "demux", 10 )
        set_callbacks( Import_ASX, Close_ASX )
    add_submodule ()
        set_description( N_("Kasenna MediaBase parser") )
        add_shortcut( "playlist" )
        add_shortcut( "sgimb" )
        set_capability( "demux", 10 )
        set_callbacks( Import_SGIMB, Close_SGIMB )
    add_submodule ()
        set_description( N_("QuickTime Media Link importer") )
        add_shortcut( "playlist" )
        add_shortcut( "qtl" )
        set_capability( "demux", 10 )
        set_callbacks( Import_QTL, Close_QTL )
    add_submodule ()
        set_description( N_("Google Video Playlist importer") )
        add_shortcut( "playlist" )
        add_shortcut( "gvp" )
        set_capability( "demux", 10 )
        set_callbacks( Import_GVP, Close_GVP )
    add_submodule ()
        set_description( N_("Dummy ifo demux") )
        add_shortcut( "playlist" )
        set_capability( "demux", 12 )
        set_callbacks( Import_IFO, Close_IFO )
    add_submodule ()
        set_description( N_("iTunes Music Library importer") )
        add_shortcut( "playlist" )
        add_shortcut( "itml" )
        set_capability( "demux", 10 )
        set_callbacks( Import_iTML, Close_iTML )
vlc_module_end ()


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
    if( asprintf( &psz_mrl, "%s%s", psz_prefix, psz_mrl ) != -1 )
        return psz_mrl;
    else
        return NULL;
}
