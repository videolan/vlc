/*****************************************************************************
 * m3u.c : M3U playlist format import
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id: m3u.c,v 1.3 2004/01/25 20:05:29 hartman Exp $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 * Authors: Sigmund Augdal <sigmunau@idi.ntnu.no>
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
#include "playlist.h"

struct demux_sys_t
{
    char *psz_prefix;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux);
static int Control( demux_t *p_demux, int i_query, va_list args );

/*****************************************************************************
 * Import_M3U: main import function
 *****************************************************************************/
int Import_M3U( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;

    uint8_t *p_peek;
    char    *psz_ext;

    if( stream_Peek( p_demux->s , &p_peek, 7 ) < 7 )
    {
        msg_Err( p_demux, "cannot peek" );
        return VLC_EGENERIC;
    }
    psz_ext = strrchr ( p_demux->psz_path, '.' );

    if( !strncmp( p_peek, "#EXTM3U", 7 ) )
    {
        ;
    }
    else if( ( psz_ext && !strcasecmp( psz_ext, ".m3u") ) ||
             ( p_demux->psz_demux && !strcmp(p_demux->psz_demux, "m3u") ) )
    {
        ;
    }
    else
    {
        msg_Warn(p_demux, "m3u import module discarded");
        return VLC_EGENERIC;
        
    }
    msg_Dbg( p_demux, "found valid M3U playlist file");

    p_demux->pf_control = Control;
    p_demux->pf_demux = Demux;
    p_demux->p_sys = malloc( sizeof(demux_sys_t) );
    if( p_demux->p_sys == NULL )
    {
        msg_Err( p_demux, "Out of memory" );
        return VLC_ENOMEM;
    }
    p_demux->p_sys->psz_prefix = FindPrefix( p_demux );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
void Close_M3U( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    if( p_demux->p_sys->psz_prefix )
    {
        free( p_demux->p_sys->psz_prefix );
    }
    free( p_demux->p_sys );
}


static int Demux( demux_t *p_demux )
{
    mtime_t        i_duration = -1;
    char          *psz_name = NULL;    
    char          *psz_line;
    char          *psz_parse;
    char          *psz_duration;
    char          *psz_mrl;
    playlist_t    *p_playlist;
    int            i_position;

    p_playlist = (playlist_t *) vlc_object_find( p_demux, VLC_OBJECT_PLAYLIST,
                                                 FIND_PARENT );
    if( !p_playlist )
    {
        msg_Err( p_demux, "can't find playlist" );
        return -1;
    }

    p_playlist->pp_items[p_playlist->i_index]->b_autodeletion = VLC_TRUE;
    i_position = p_playlist->i_index + 1;
    while( ( psz_line = stream_ReadLine( p_demux->s ) ) )
    {
        if( *psz_line == '#' )
        {
            /* parse extra info */
            psz_parse = psz_line;
            while( *psz_parse &&
                   strncasecmp( psz_parse, "EXTINF:", sizeof("EXTINF:") - 1 ) )
               psz_parse++;
            if( *psz_parse )
            {
                psz_parse += sizeof("EXTINF:") - 1;
                while( *psz_parse == '\t' || *psz_parse == ' ' )
                    psz_parse++;
                psz_duration = psz_parse;
                psz_parse = strchr( psz_parse, ',' );
                if ( psz_parse )
                {
                    *psz_parse = '\0';
                    psz_parse++;
                    psz_name = strdup( psz_parse );
                    i_duration = atoi( psz_duration );
                    if( i_duration != -1 )
                    {
                        i_duration *= 1000000;
                    }
                }
            }
        }
        else
        {
            psz_mrl = ProcessMRL( psz_line, p_demux->p_sys->psz_prefix );
            playlist_Add( p_playlist, psz_mrl, psz_name,
                          PLAYLIST_INSERT, i_position );
            playlist_SetDuration( p_playlist, i_position, i_duration );
            free( psz_mrl );
            i_position++;
            i_duration = -1;
            if( psz_name )
            {
                free( psz_name );
                psz_name = NULL;
            }
        }
        free( psz_line);
    }
    vlc_object_release( p_playlist );
    return VLC_SUCCESS;
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    return VLC_EGENERIC;
}
