/*****************************************************************************
 * m3u.c : M3U playlist format import
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id: m3u.c,v 1.1 2004/01/11 00:45:06 zorglub Exp $
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
#include <vlc/intf.h>

#include <errno.h>                                                 /* ENOMEM */


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
int Import_M3U ( vlc_object_t * );
static int Demux( demux_t *p_demux);
static int Control( demux_t *p_demux, int i_query, va_list args );

/*****************************************************************************
 * Import_Old : main import function
 *****************************************************************************/
int Import_M3U( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;

    uint8_t *p_peek;

    if( stream_Peek( p_demux->s , &p_peek, 7 ) < 7 )
    {
        msg_Err( p_demux, "cannot peek" );
        return VLC_EGENERIC;
    }

    if( strncmp( p_peek, "#EXTM3U", 7 ) )
    {
        msg_Warn(p_demux, "m3u import module discarded: invalid file");
        return VLC_EGENERIC;
    }
    msg_Info( p_demux, "Found valid M3U playlist file");

    p_demux->pf_control = Control;
    p_demux->pf_demux = Demux;

    return VLC_SUCCESS;
}

static int Demux( demux_t *p_demux)
{
    msg_Warn(p_demux, "Not yet implemented" );
    playlist_t *p_playlist = (playlist_t *)vlc_object_find( p_demux,
                    VLC_OBJECT_PLAYLIST, FIND_PARENT );

    p_playlist->pp_items[p_playlist->i_index]->b_autodeletion = VLC_TRUE;
    vlc_object_release( p_playlist );
    return VLC_SUCCESS;
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    return VLC_EGENERIC;
}
