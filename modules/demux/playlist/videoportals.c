/*****************************************************************************
 * videoportals.c: Convert video webportal HTML urls to the appropriate video
 *                 stream url.
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea @t videolan d.t org>
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
#include <vlc/vlc.h>
#include <vlc_demux.h>

#include <errno.h>                                                 /* ENOMEM */
#include "playlist.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux);
static int Control( demux_t *p_demux, int i_query, va_list args );

struct demux_sys_t
{
    char *psz_url;
};

/*****************************************************************************
 * Import_VideoPortal: main import function
 *****************************************************************************/
int E_(Import_VideoPortal)( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    const char *psz_path = p_demux->psz_path;
    char *psz_cur;
    char *psz_url = NULL;

    /* YouTube */
    if( ( psz_cur = strstr( psz_path, "youtube.com" ) ) )
    {
        psz_cur += strlen( "youtube.com" );
        psz_cur = strchr( psz_cur, '/' );
        if( psz_cur )
        {
            psz_cur++;
            if( !strncmp( psz_cur, "watch?v=", strlen( "watch?v=" ) ) )
            {
                /* This is the webpage's url */
                psz_cur += strlen( "watch?v=" );
                asprintf( &psz_url, "http://www.youtube.com/v/%s", psz_cur );
            }
            else if( !strncmp( psz_cur, "p.swf", strlen( "p.swf" ) ) )
            {
                /* This is the swf flv player url (which we get after a
                 * redirect from the http://www.youtube.com/v/video_id url */
                char *video_id = strstr( psz_cur, "video_id=" );
                char *t = strstr( psz_cur, "t=" );
                if( video_id && t )
                {
                    char *psz_buf;
                    video_id += strlen( "video_id=" );
                    t += strlen( "t=" );
                    psz_buf = strchr( video_id, '&' );
                    if( psz_buf ) *psz_buf = '\0';
                    psz_buf = strchr( t, '&' );
                    if( psz_buf ) *psz_buf = '\0';
                    asprintf( &psz_url, "http://www.youtube.com/"
                              "get_video.php?video_id=%s&t=%s",
                              video_id, t );
                }
            }
        }
    }

    if( !psz_url )
    {
        return VLC_EGENERIC;
    }

    p_demux->p_sys = (demux_sys_t*)malloc( sizeof( demux_sys_t ) );
    if( !p_demux->p_sys )
    {
        free( psz_url );
        return VLC_ENOMEM;
    }
    p_demux->p_sys->psz_url = psz_url;

    p_demux->pf_control = Control;
    p_demux->pf_demux = Demux;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
void E_(Close_VideoPortal)( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    free( p_demux->p_sys->psz_url );
    free( p_demux->p_sys );
}

static int Demux( demux_t *p_demux )
{
    char *psz_url = p_demux->p_sys->psz_url;
    input_item_t *p_input;

    msg_Dbg( p_demux, "Redirecting %s to %s", p_demux->psz_path, psz_url );

    INIT_PLAYLIST_STUFF;

    p_input = input_ItemNewExt( p_playlist, psz_url, psz_url, 0, NULL, -1 );
    playlist_BothAddInput( p_playlist, p_input,
                           p_item_in_category,
                           PLAYLIST_APPEND | PLAYLIST_SPREPARSE,
                           PLAYLIST_END, NULL, NULL, VLC_FALSE );

    HANDLE_PLAY_AND_RELEASE;

    return -1; /* Needed for correct operation of go back */
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    return VLC_EGENERIC;
}
