/*****************************************************************************
 * old.c : Old playlist format import
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/intf.h>

#include <errno.h>                                                 /* ENOMEM */

#define PLAYLIST_FILE_HEADER "# vlc playlist file version 0.5"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux);
static int Control( demux_t *p_demux, int i_query, va_list args );

/*****************************************************************************
 * Import_Old : main import function
 *****************************************************************************/
int E_(Import_Old)( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    uint8_t *p_peek;

    if( stream_Peek( p_demux->s, &p_peek, 31 ) < 31 ) return VLC_EGENERIC;

    if( strncmp( p_peek, PLAYLIST_FILE_HEADER , 31 ) ) return VLC_EGENERIC;

    msg_Dbg( p_demux, "found valid old playlist file");

    p_demux->pf_control = Control;
    p_demux->pf_demux = Demux;

    return VLC_SUCCESS;
}

static int Demux( demux_t *p_demux)
{
    char *psz_line;
    /* Attach playlist and start reading data */
    playlist_t *p_playlist;

    p_playlist = (playlist_t*)vlc_object_find( p_demux,
                         VLC_OBJECT_PLAYLIST, FIND_PARENT );
    if( !p_playlist )
    {
        msg_Err( p_demux, "cannot attach playlist" );
        return VLC_EGENERIC;
    }

    p_playlist->pp_items[p_playlist->i_index]->b_autodeletion = VLC_TRUE;
    while( ( psz_line = stream_ReadLine( p_demux->s) ) != NULL )
    {
        if( ( psz_line[0] == '#' ) || (psz_line[0] == '\r') ||
            ( psz_line[0] == '\n') || (psz_line[0] == (char)0) )
        {
            continue;
        }
        /* Remove end of line */
        if( psz_line[strlen(psz_line) -1 ] == '\n' ||
            psz_line[strlen(psz_line) -1 ] == '\r' )
        {
            psz_line[ strlen(psz_line) -1 ] = (char)0;
            if( psz_line[strlen(psz_line) - 1 ] == '\r' )
                psz_line[strlen(psz_line) - 1 ] = (char)0;
        }
        playlist_Add( p_playlist, psz_line, psz_line, PLAYLIST_APPEND,
                      PLAYLIST_END );

        free( psz_line );
    }

    p_demux->b_die = VLC_TRUE;
    vlc_object_release( p_playlist );
    return VLC_SUCCESS;
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    return VLC_EGENERIC;
}
