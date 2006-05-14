/*****************************************************************************
 * m3u.c :  M3U playlist export module
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc_meta.h>

#include <errno.h>                                                 /* ENOMEM */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
int Export_M3U ( vlc_object_t * );

/*****************************************************************************
 * Export_M3U: main export function
 *****************************************************************************/
int Export_M3U( vlc_object_t *p_this )
{
    playlist_t *p_playlist = (playlist_t*)p_this;
    playlist_export_t *p_export = (playlist_export_t *)p_playlist->p_private;
    int i, j;

    msg_Dbg(p_playlist, "saving using M3U format");

    /* Write header */
    fprintf( p_export->p_file, "#EXTM3U\n" );

    /* Go through the playlist and add items */
    for( i = 0; i< p_export->p_root->i_children ; i++)
    {
        playlist_item_t *p_current = p_export->p_root->pp_children[i];
        if( p_current->i_flags & PLAYLIST_SAVE_FLAG )
            continue;

        /* General info */
        if( p_current->p_input->psz_name &&
             strcmp( p_current->p_input->psz_uri,
                     p_current->p_input->psz_name ) )
        {
            char *psz_artist = p_current->p_input->p_meta->psz_artist ?
                               strdup( p_current->p_input->p_meta->psz_artist ):
                               strdup( "" );
            if( psz_artist && *psz_artist )
            {
                /* write EXTINF with artist */
                fprintf( p_export->p_file, "#EXTINF:%i,%s - %s\n",
                          (int)( p_current->p_input->i_duration/1000000 ),
                          psz_artist,
                          p_current->p_input->psz_name);
            }
            else
            {
                /* write EXTINF without artist */
                fprintf( p_export->p_file, "#EXTINF:%i,%s\n",
                         (int)( p_current->p_input->i_duration/1000000 ),
                          p_current->p_input->psz_name);
            }
            if( psz_artist )
                free( psz_artist );
        }

        /* VLC specific options */
        for( j = 0; j < p_current->p_input->i_options; j++ )
        {
            fprintf( p_export->p_file, "#EXTVLCOPT:%s\n",
                     p_current->p_input->ppsz_options[j][0] == ':' ?
                     p_current->p_input->ppsz_options[j] + 1 :
                     p_current->p_input->ppsz_options[j] );
        }

        fprintf( p_export->p_file, "%s\n",
                 p_current->p_input->psz_uri );
    }
    return VLC_SUCCESS;
}
