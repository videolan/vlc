/*****************************************************************************
 * m3u.c : M3U playlist export module
 *****************************************************************************
 * Copyright (C) 2004-2009 the VideoLAN team
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
#include <vlc_input_item.h>
#include <vlc_meta.h>
#include <vlc_charset.h>
#include <vlc_playlist_export.h>
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
static void DoExport( struct vlc_playlist_export *p_export,
                      int (*pf_fprintf) (FILE *, const char *, ...) )
{
    size_t prefix_len = -1;
    if( likely(p_export->base_url != NULL) )
    {
        const char *p = strrchr( p_export->base_url, '/' );
        assert(p != NULL);
        prefix_len = (p + 1) - p_export->base_url;
    }

    /* Write header */
    fputs( "#EXTM3U\n", p_export->file );

    /* Go through the playlist and add items */
    size_t count = vlc_playlist_view_Count(p_export->playlist_view);
    for( size_t i = 0; i < count; ++i )
    {
        vlc_playlist_item_t *item =
            vlc_playlist_view_Get(p_export->playlist_view, i);

        /* General info */
        input_item_t *media = vlc_playlist_item_GetMedia(item);

        char *psz_uri = input_item_GetURI(media);
        assert( psz_uri );

        char *psz_name = input_item_GetName(media);
        if( psz_name && strcmp( psz_uri, psz_name ) )
        {
            char *psz_artist = input_item_GetArtist(media);
            vlc_tick_t i_duration = input_item_GetDuration(media);
            if( psz_artist && *psz_artist )
            {
                /* write EXTINF with artist */
                pf_fprintf( p_export->file, "#EXTINF:%"PRIu64",%s - %s\n",
                            SEC_FROM_VLC_TICK(i_duration), psz_artist, psz_name);
            }
            else
            {
                /* write EXTINF without artist */
                pf_fprintf( p_export->file, "#EXTINF:%"PRIu64",%s\n",
                            SEC_FROM_VLC_TICK(i_duration), psz_name);
            }
            free( psz_artist );
        }
        free( psz_name );

        /* VLC specific options */
        vlc_mutex_lock(&media->lock);
        for( int j = 0; j < media->i_options; j++ )
        {
            pf_fprintf( p_export->file, "#EXTVLCOPT:%s\n",
                        media->ppsz_options[j][0] == ':' ?
                        media->ppsz_options[j] + 1 :
                        media->ppsz_options[j] );
        }
        vlc_mutex_unlock(&media->lock);

        /* We cannot really know if relative or absolute URL is better. As a
         * heuristic, we write a relative URL if the item is in the same
         * directory as the playlist, or a sub-directory thereof. */
        size_t skip = 0;
        if( likely(prefix_len != (size_t)-1)
         && !strncmp( p_export->base_url, psz_uri, prefix_len ) )
            skip = prefix_len;

        fprintf( p_export->file, "%s\n", psz_uri + skip );
        free( psz_uri );
    }
}

int Export_M3U( vlc_object_t *p_this )
{
    struct vlc_playlist_export *p_export = (struct vlc_playlist_export *) p_this;

    msg_Dbg( p_export, "saving using M3U format");

    DoExport(p_export, utf8_fprintf);
    return VLC_SUCCESS;
}

int Export_M3U8( vlc_object_t *p_this )
{
    struct vlc_playlist_export *p_export = (struct vlc_playlist_export *) p_this;

    msg_Dbg( p_export, "saving using M3U8 format");

    DoExport(p_export, fprintf);
    return VLC_SUCCESS;
}
