/*****************************************************************************
 * m3u.c : M3U playlist export module
 *****************************************************************************
 * Copyright (C) 2004-2009 the VideoLAN team
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
#include <vlc_playlist.h>
#include <vlc_input.h>
#include <vlc_meta.h>
#include <vlc_charset.h>
#include <vlc_url.h>

#include <assert.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
int Export_M3U ( vlc_object_t * );
int Export_M3U8( vlc_object_t * );

/*****************************************************************************
 * Export_M3U: main export function
 *****************************************************************************/
static void DoChildren( playlist_export_t *p_export, playlist_item_t *p_root,
                        int (*pf_fprintf) (FILE *, const char *, ...) )
{
    /* Write header */
    fputs( "#EXTM3U\n", p_export->p_file );

    /* Go through the playlist and add items */
    for( int i = 0; i< p_root->i_children ; i++)
    {
        playlist_item_t *p_current = p_root->pp_children[i];
        assert( p_current );

        if( p_current->i_flags & PLAYLIST_SAVE_FLAG )
            continue;

        if( p_current->i_children >= 0 )
        {
            DoChildren( p_export, p_current, pf_fprintf );
            continue;
        }

        /* General info */

        char *psz_uri = input_item_GetURI( p_current->p_input );

        assert( psz_uri );

        char *psz_name = input_item_GetName( p_current->p_input );
        if( psz_name && strcmp( psz_uri, psz_name ) )
        {
            char *psz_artist = input_item_GetArtist( p_current->p_input );
            if( psz_artist == NULL ) psz_artist = strdup( "" );
            mtime_t i_duration = input_item_GetDuration( p_current->p_input );
            if( psz_artist && *psz_artist )
            {
                /* write EXTINF with artist */
                pf_fprintf( p_export->p_file, "#EXTINF:%"PRIu64",%s - %s\n",
                            i_duration / CLOCK_FREQ, psz_artist, psz_name);
            }
            else
            {
                /* write EXTINF without artist */
                pf_fprintf( p_export->p_file, "#EXTINF:%"PRIu64",%s\n",
                            i_duration / CLOCK_FREQ, psz_name);
            }
            free( psz_artist );
        }
        free( psz_name );

        /* VLC specific options */
        vlc_mutex_lock( &p_current->p_input->lock );
        for( int j = 0; j < p_current->p_input->i_options; j++ )
        {
            pf_fprintf( p_export->p_file, "#EXTVLCOPT:%s\n",
                        p_current->p_input->ppsz_options[j][0] == ':' ?
                        p_current->p_input->ppsz_options[j] + 1 :
                        p_current->p_input->ppsz_options[j] );
        }
        vlc_mutex_unlock( &p_current->p_input->lock );

        /* Stupid third party players don't understand file: URIs. */
        char *psz_path = make_path( psz_uri );
        if( psz_path != NULL )
        {
            free( psz_uri );
            psz_uri = psz_path;
        }
        fprintf( p_export->p_file, "%s\n", psz_uri );
        free( psz_uri );
    }
}

int Export_M3U( vlc_object_t *p_this )
{
    playlist_export_t *p_export = (playlist_export_t *)p_this;

    msg_Dbg( p_export, "saving using M3U format");

    DoChildren( p_export, p_export->p_root, utf8_fprintf );
    return VLC_SUCCESS;
}

int Export_M3U8( vlc_object_t *p_this )
{
    playlist_export_t *p_export = (playlist_export_t *)p_this;

    msg_Dbg( p_export, "saving using M3U8 format");

    DoChildren( p_export, p_export->p_root, fprintf );
    return VLC_SUCCESS;
}
