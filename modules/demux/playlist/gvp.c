/*****************************************************************************
 * gvp.c: Google Video Playlist demuxer
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
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

/**
 * Format seems to be:
 * gvp_version:<version> (1.1)
 * url:<the media's url>
 * docid:<integer>
 * duration:<integer ms ?>
 * title:<the title>
 * description:<desc line1>^M
 * description:<desc linei>^M
 * description:<desc final line (no ^M)>
 * lines starting with # are comments
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/intf.h>

#include <errno.h>                                                 /* ENOMEM */
#include "playlist.h"

#define MAX_LINE 1024

struct demux_sys_t
{
    playlist_t *p_playlist;
    playlist_item_t *p_current;
    playlist_item_t *p_item_in_category;
    int i_parent_id;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux);
static int Control( demux_t *p_demux, int i_query, va_list args );

/*****************************************************************************
 * Import_GVP: main import function
 *****************************************************************************/
int E_(Import_GVP)( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    byte_t *p_peek;

    CHECK_PEEK( p_peek, 12 );
    if( !POKE( p_peek, "gvp_version:", 12 ) )
    {
        return VLC_EGENERIC;
    }

    STANDARD_DEMUX_INIT_MSG(  "using Google Video Playlist (gvp) import" )
    p_demux->pf_control = Control;
    p_demux->pf_demux = Demux;
    MALLOC_ERR( p_demux->p_sys, demux_sys_t );
    p_demux->p_sys->p_playlist = NULL;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
void E_(Close_GVP)( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    if( p_sys->p_playlist )
        vlc_object_release( p_sys->p_playlist );
    free( p_sys );
}

static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    INIT_PLAYLIST_STUFF;

    p_sys->p_playlist = p_playlist;
    p_sys->p_current = p_current;
    p_sys->i_parent_id = i_parent_id;
    p_sys->p_item_in_category = p_item_in_category;

    char *psz_line;
    char *psz_attrvalue;

    char *psz_version = NULL;
    char *psz_url = NULL;
    char *psz_docid = NULL;
    int i_duration = -1;
    char *psz_title = NULL;
    char *psz_description = NULL;

    while( ( psz_line = stream_ReadLine( p_demux->s ) ) )
    {
        if( *psz_line == '#' )
        {
            /* This is a comment */
            free( psz_line );
            continue;
        }
        psz_attrvalue = strchr( psz_line, ':' );
        if( !psz_attrvalue )
        {
            msg_Dbg( p_demux, "Unable to parse line (%s)", psz_line );
            free( psz_line );
            continue;
        }
        *psz_attrvalue = '\0';
        psz_attrvalue++;
        if( !strcmp( psz_line, "gvp_version" ) )
        {
            psz_version = strdup( psz_attrvalue );
        }
        else if( !strcmp( psz_line, "url" ) )
        {
            psz_url = strdup( psz_attrvalue );
        }
        else if( !strcmp( psz_line, "docid" ) )
        {
            psz_docid = strdup( psz_attrvalue );
        }
        else if( !strcmp( psz_line, "duration" ) )
        {
            i_duration = atoi( psz_attrvalue );
        }
        else if( !strcmp( psz_line, "title" ) )
        {
            psz_title = strdup( psz_attrvalue );
        }
        else if( !strcmp( psz_line, "description" ) )
        {
            char *buf;
            if( !psz_description )
            {
                psz_description = strdup( psz_attrvalue );
            }
            else
            {
                /* handle multi-line descriptions */
                buf = malloc( strlen( psz_description )
                            + strlen( psz_attrvalue ) + 2 );
                sprintf( buf, "%s\n%s", psz_description, psz_attrvalue );
                free( psz_description );
                psz_description = buf;
            }
            /* remove ^M char at the end of the line (if any) */
            buf = psz_description + strlen( psz_description );
            if( buf != psz_description )
            {
                buf--;
                if( *buf == '\r' ) *buf = '\0';
            }
        }
        free( psz_line );
    }

    if( !psz_url )
    {
        msg_Err( p_demux, "URL not found" );
    }
    else
    {
        p_input = input_ItemNewExt( p_sys->p_playlist,
                                    psz_url, psz_title, 0, NULL, -1 );
#define SADD_INFO( type, field ) if( field ) { vlc_input_item_AddInfo( \
                    p_input, _("Google Video"), _(type), "%s", field ) ; }
        SADD_INFO( "gvp_version", psz_version );
        SADD_INFO( "docid", psz_docid );
        SADD_INFO( "description", psz_description );
        playlist_AddWhereverNeeded( p_sys->p_playlist, p_input,
                            p_sys->p_current, p_sys->p_item_in_category,
                            (p_sys->i_parent_id > 0 ) ? VLC_TRUE: VLC_FALSE,
                            PLAYLIST_APPEND );
    }

    HANDLE_PLAY_AND_RELEASE;

    free( psz_version );
    free( psz_url );
    free( psz_docid );
    free( psz_title );
    free( psz_description );

    p_sys->p_playlist = NULL;

    return VLC_SUCCESS;
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    return VLC_EGENERIC;
}
