/*****************************************************************************
 * html.c : HTML playlist export module
 *****************************************************************************
 * Copyright (C) 2008-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Rémi Duraffort <ivoire@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_playlist.h>
#include <vlc_input.h>
#include <vlc_strings.h>

#include <assert.h>


// Export the playlist in HTML
int Export_HTML( vlc_object_t *p_this );


/**
 * Recursively follow the playlist
 * @param p_export: the export structure
 * @param p_root: the current node
 */
static void DoChildren( playlist_export_t *p_export, playlist_item_t *p_root )
{
    /* Go through the playlist and add items */
    for( int i = 0; i < p_root->i_children ; i++)
    {
        playlist_item_t *p_current = p_root->pp_children[i];
        assert( p_current );

        if( p_current->i_flags & PLAYLIST_SAVE_FLAG )
            continue;

        if( p_current->i_children >= 0 )
        {
            DoChildren( p_export, p_current );
            continue;
        }

        char* psz_name = NULL;
        char *psz_tmp = input_item_GetName( p_current->p_input );
        if( psz_tmp )
            psz_name = convert_xml_special_chars( psz_tmp );
        free( psz_tmp );

        if( psz_name )
        {
            char* psz_artist = NULL;
            psz_tmp = input_item_GetArtist( p_current->p_input );
            if( psz_tmp )
                psz_artist = convert_xml_special_chars( psz_tmp );
            free( psz_tmp );

            mtime_t i_duration = input_item_GetDuration( p_current->p_input );
            int min = ( i_duration / 1000000 ) / 60;
            int sec = ( i_duration / 1000000 ) - min * 60;

            // Print the artist if we have one
            if( psz_artist && *psz_artist )
                fprintf( p_export->p_file, "    <li>%s - %s (%02d:%02d)</li>\n", psz_artist, psz_name, min, sec );
            else
                fprintf( p_export->p_file, "    <li>%s (%2d:%2d)</li>\n", psz_name, min, sec );

            free( psz_artist );
        }
        free( psz_name );
    }
}


/**
 * Export the playlist as an HTML page
 * @param p_this: the playlist
 * @return VLC_SUCCESS if everything goes fine
 */
int Export_HTML( vlc_object_t *p_this )
{
    playlist_export_t *p_export = (playlist_export_t *)p_this;

    msg_Dbg( p_export, "saving using HTML format" );

    /* Write header */
    fprintf( p_export->p_file, "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n"
"<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" \"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">\n"
"<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\">\n"
"<head>\n"
"  <meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />\n"
"  <meta name=\"Generator\" content=\"VLC media player\" />\n"
"  <meta name=\"Author\" content=\"VLC, http://www.videolan.org/vlc/\" />\n"
"  <title>VLC generated playlist</title>\n"
"  <style type=\"text/css\">\n"
"    body {\n"
"      background-color: #E4F3FF;\n"
"      font-family: sans-serif, Helvetica, Arial;\n"
"      font-size: 13px;\n"
"    }\n"
"    h1 {\n"
"      color: #2D58AE;\n"
"      font-size: 25px;\n"
"    }\n"
"    hr {\n"
"      color: #555555;\n"
"    }\n"
"  </style>\n"
"</head>\n\n"
"<body>\n"
"  <h1>Playlist</h1>\n"
"  <hr />\n"
"  <ol>\n" );

    // Call the playlist constructor
    DoChildren( p_export, p_export->p_root );

    // Print the footer
    fprintf( p_export->p_file, "  </ol>\n"
"  <hr />\n"
"</body>\n"
"</html>" );
    return VLC_SUCCESS;
}

