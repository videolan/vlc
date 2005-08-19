/*****************************************************************************
 * m3u.c : M3U playlist format import
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Sigmund Augdal <sigmunau@idi.ntnu.no>
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
static void parseEXTINF( char *psz_string, char **ppsz_author, char **ppsz_name, int *pi_duration );

/*****************************************************************************
 * Import_M3U: main import function
 *****************************************************************************/
int E_(Import_M3U)( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;

    uint8_t *p_peek;
    char    *psz_ext;

    if( stream_Peek( p_demux->s , &p_peek, 7 ) < 7 )
    {
        return VLC_EGENERIC;
    }
    psz_ext = strrchr ( p_demux->psz_path, '.' );

    if( !strncmp( (char *)p_peek, "#EXTM3U", 7 ) )
    {
        ;
    }
    else if( ( psz_ext && !strcasecmp( psz_ext, ".m3u") ) ||
             ( psz_ext && !strcasecmp( psz_ext, ".ram") ) ||
             ( psz_ext && !strcasecmp( psz_ext, ".rm") ) ||
             /* A .ram file can contain a single rtsp link */
             ( p_demux->psz_demux && !strcmp(p_demux->psz_demux, "m3u") ) )
    {
        ;
    }
    else
    {
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
    p_demux->p_sys->psz_prefix = E_(FindPrefix)( p_demux );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
void E_(Close_M3U)( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;

    if( p_demux->p_sys->psz_prefix ) free( p_demux->p_sys->psz_prefix );
    free( p_demux->p_sys );
}

static int Demux( demux_t *p_demux )
{
    playlist_t *p_playlist;
    char       *psz_line;

    char       *psz_name = NULL;
    char       *psz_author = NULL;
    int        i_parsed_duration = 0;
    mtime_t    i_duration = -1;
    char       **ppsz_options = NULL;
    int        i_options = 0, i;

    playlist_item_t *p_item, *p_current;

    vlc_bool_t b_play;

    vlc_bool_t b_cleanup = VLC_FALSE;

    p_playlist = (playlist_t *) vlc_object_find( p_demux, VLC_OBJECT_PLAYLIST,
                                                 FIND_PARENT );
    if( !p_playlist )
    {
        msg_Err( p_demux, "can't find playlist" );
        return -1;
    }

    b_play = E_(FindItem)( p_demux, p_playlist, &p_current );

    playlist_ItemToNode( p_playlist, p_current );
    p_current->input.i_type = ITEM_TYPE_PLAYLIST;

    psz_line = stream_ReadLine( p_demux->s );
    while( psz_line )
    {
        char *psz_parse = psz_line;

        /* Skip leading tabs and spaces */
        while( *psz_parse == ' ' || *psz_parse == '\t' ||
               *psz_parse == '\n' || *psz_parse == '\r' ) psz_parse++;

        if( *psz_parse == '#' )
        {
            /* Parse extra info */

            /* Skip leading tabs and spaces */
            while( *psz_parse == ' ' || *psz_parse == '\t' ||
                   *psz_parse == '\n' || *psz_parse == '\r' ||
                   *psz_parse == '#' ) psz_parse++;

            if( !*psz_parse ) goto error;

            if( !strncasecmp( psz_parse, "EXTINF:", sizeof("EXTINF:") -1 ) )
            {
                /* Extended info */
                psz_parse += sizeof("EXTINF:") - 1;
                parseEXTINF( psz_parse, &psz_author, &psz_name, &i_parsed_duration );
                i_duration = i_parsed_duration * 1000000;
                if ( psz_name )
                    psz_name = strdup( psz_name );
                if ( psz_author )
                    psz_author = strdup( psz_author );
            }
            else if( !strncasecmp( psz_parse, "EXTVLCOPT:",
                                   sizeof("EXTVLCOPT:") -1 ) )

            {
                /* VLC Option */
                char *psz_option;
                psz_parse += sizeof("EXTVLCOPT:") -1;
                if( !*psz_parse ) goto error;

                psz_option = strdup( psz_parse );
                if( psz_option )
                    INSERT_ELEM( ppsz_options, i_options, i_options,
                                 psz_option );
            }
        }
        else if( *psz_parse )
        {
            char *psz_mrl;
            if( !psz_name || !*psz_name )
            {
                /* Use filename as name for relative entries */
                psz_name = strdup( psz_parse );
            }

            psz_mrl = E_(ProcessMRL)( psz_parse, p_demux->p_sys->psz_prefix );

            b_cleanup = VLC_TRUE;
            if( !psz_mrl ) goto error;

            EnsureUTF8( psz_name );
            EnsureUTF8( psz_mrl );

            p_item = playlist_ItemNew( p_playlist, psz_mrl, psz_name );
            for( i = 0; i< i_options; i++ )
            {
                EnsureUTF8( ppsz_options[i] );
                playlist_ItemAddOption( p_item, ppsz_options[i] );
            }
            p_item->input.i_duration = i_duration;
            if ( psz_author )
                vlc_input_item_AddInfo( &p_item->input, _("Meta-information"),
                                        _("Artist"), "%s", psz_author );
            playlist_NodeAddItem( p_playlist, p_item,
                                  p_current->pp_parents[0]->i_view,
                                  p_current, PLAYLIST_APPEND,
                                  PLAYLIST_END );

            /* We need to declare the parents of the node as the
             *                  * same of the parent's ones */
            playlist_CopyParents( p_current, p_item );

            vlc_input_item_CopyOptions( &p_current->input,
                                        &p_item->input );

            free( psz_mrl );
        }

 error:

        /* Fetch another line */
        free( psz_line );
        psz_line = stream_ReadLine( p_demux->s );
        if( !psz_line ) b_cleanup = VLC_TRUE;

        if( b_cleanup )
        {
            /* Cleanup state */
            while( i_options-- ) free( ppsz_options[i_options] );
            if( ppsz_options ) free( ppsz_options );
            ppsz_options = NULL; i_options = 0;
            if( psz_name ) free( psz_name );
            psz_name = NULL;
            if ( psz_author ) free( psz_author );
            psz_author = NULL;
            i_parsed_duration = 0;
            i_duration = -1;

            b_cleanup = VLC_FALSE;
        }
    }

    /* Go back and play the playlist */
    if( b_play && p_playlist->status.p_item &&
        p_playlist->status.p_item->i_children > 0 )
    {
        playlist_Control( p_playlist, PLAYLIST_VIEWPLAY,
                          p_playlist->status.i_view,
                          p_playlist->status.p_item,
                          p_playlist->status.p_item->pp_children[0] );
    }

    vlc_object_release( p_playlist );
    return VLC_SUCCESS;
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    return VLC_EGENERIC;
}

static void parseEXTINF(char *psz_string, char **ppsz_author, 
                        char **ppsz_name, int *pi_duration)
{
    char *end=NULL;
    char *psz_item=NULL;
    char *pos;

    end = psz_string + strlen( psz_string );

    /* ignore whitespaces */
    for (; psz_string < end && ( *psz_string == '\t' || *psz_string == ' ' ); 
         psz_string++ );

    /* read all digits */
    psz_item = psz_string;
    while ( psz_string < end && *psz_string >= '0' && *psz_string <= '9' )
    {
        psz_string++;
    }
    if ( *psz_item >= '0' && *psz_item <= '9' && *psz_string == ',' )
    {
        *psz_string = '\0';
        *pi_duration = atoi(psz_item);
    }
    else
    {
        return;
    }
    if ( psz_string < end )               /* continue parsing if possible */
    {
        psz_string++;
    }

    /* read the author */
    /* parse the author until unescaped comma is reached */
    psz_item = pos = psz_string;
    while( psz_string < end && *psz_string != ',' )
    {
        if( *psz_string == '\\' )
            psz_string++;                 /* Skip escape character */
        *pos++ = *psz_string++;
    }
    *pos = '\0';                          /* terminate the item */
    *ppsz_author = psz_item;

    if( psz_string < end )               /* continue parsing if possible */
        psz_string++;
    /* the title doesn't need to be escaped */
    *ppsz_name = psz_string;

    if( !**ppsz_name )
    {
        /* Assume a syntax without author name */
        *ppsz_name = *ppsz_author;
        *ppsz_author = NULL;
    }

    return;
}

