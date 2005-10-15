/*****************************************************************************
 * pls.c : PLS playlist format import
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
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

/*****************************************************************************
 * Import_PLS: main import function
 *****************************************************************************/
int E_(Import_PLS)( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;

    uint8_t *p_peek;
    char    *psz_ext;

    if( stream_Peek( p_demux->s , &p_peek, 7 ) < 7 ) return VLC_EGENERIC;
    psz_ext = strrchr ( p_demux->psz_path, '.' );

    if( !strncasecmp( (char *)p_peek, "[playlist]", 10 ) )
    {
        ;
    }
    else if( ( psz_ext && !strcasecmp( psz_ext, ".pls") ) ||
             ( p_demux->psz_demux && !strcmp(p_demux->psz_demux, "pls") ) )
    {
        ;
    }
    else return VLC_EGENERIC;

    msg_Dbg( p_demux, "found valid PLS playlist file");

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
void E_(Close_PLS)( vlc_object_t *p_this )
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
    char          *psz_mrl = NULL;
    char          *psz_key;
    char          *psz_value;
    playlist_t    *p_playlist;
    int            i_position;
    int            i_item = -1;
    int            i_new_item = 0;
    int            i_key_length;
    playlist_item_t *p_parent;
    vlc_bool_t b_play;

    p_playlist = (playlist_t *) vlc_object_find( p_demux, VLC_OBJECT_PLAYLIST,
                                                 FIND_PARENT );
    if( !p_playlist )
    {
        msg_Err( p_demux, "can't find playlist" );
        return -1;
    }

    b_play = E_(FindItem)( p_demux, p_playlist, &p_parent );
    p_parent->input.i_type = ITEM_TYPE_PLAYLIST;

    /* Change the item to a node */
    if( p_parent->i_children == -1)
    {
        playlist_ItemToNode( p_playlist,p_parent );
    }

    while( ( psz_line = stream_ReadLine( p_demux->s ) ) )
    {
        if( !strncasecmp( psz_line, "[playlist]", sizeof("[playlist]")-1 ) )
        {
            free( psz_line );
            continue;
        }
        psz_key = psz_line;
        psz_value = strchr( psz_line, '=' );
        if( psz_value )
        {
            *psz_value='\0';
            psz_value++;
        }
        else
        {
            msg_Warn( p_demux, "invalid line in pls file" );
            free( psz_line );
            continue;
        }
        if( !strcasecmp( psz_key, "version" ) )
        {
            msg_Dbg( p_demux, "pls file version: %s", psz_value );
            free( psz_line );
            continue;
        }
        /* find the number part of of file1, title1 or length1 etc */
        i_key_length = strlen( psz_key );
        if( i_key_length >= 5 ) /* file1 type case */
        {
            i_new_item = atoi( psz_key + 4 );
            if( i_new_item == 0 && i_key_length >= 6 ) /* title1 type case */
            {
                i_new_item = atoi( psz_key + 5 );
                if( i_new_item == 0 && i_key_length >= 7 ) /* length1 type case */
                {
                    i_new_item = atoi( psz_key + 6 );
                }
            }
        }
        if( i_new_item == 0 )
        {
            msg_Warn( p_demux, "couldn't find number of items" );
            free( psz_line );
            continue;
        }
        if( i_item == -1 )
        {
            i_item = i_new_item;
        }
        /* we found a new item, insert the previous */
        if( i_item != i_new_item )
        {
            if( psz_mrl )
            {
                playlist_item_t *p_item = playlist_ItemNew( p_playlist, psz_mrl,
                                                            psz_name );

                playlist_NodeAddItem( p_playlist,p_item,
                                      p_parent->pp_parents[0]->i_view,
                                      p_parent,
                                      PLAYLIST_APPEND, PLAYLIST_END );

                playlist_CopyParents( p_parent, p_item );
                if( i_duration != -1 )
                {
                    //playlist_SetDuration( p_playlist, i_position, i_duration );
                }
                i_position++;
                free( psz_mrl );
                psz_mrl = NULL;

                vlc_input_item_CopyOptions( &p_parent->input,
                                            &p_item->input );
            }
            else
            {
                msg_Warn( p_demux, "no file= part found for item %d", i_item );
            }
            if( psz_name )
            {
                free( psz_name );
                psz_name = NULL;
            }
            i_duration = -1;
            i_item = i_new_item;
            i_new_item = 0;
        }
        if( !strncasecmp( psz_key, "file", sizeof("file") -1 ) )
        {
            psz_mrl = E_(ProcessMRL)( psz_value, p_demux->p_sys->psz_prefix );
        }
        else if( !strncasecmp( psz_key, "title", sizeof("title") -1 ) )
        {
            psz_name = strdup( psz_value );
        }
        else if( !strncasecmp( psz_key, "length", sizeof("length") -1 ) )
        {
            i_duration = atoi( psz_value );
            if( i_duration != -1 )
            {
                i_duration *= 1000000;
            }
        }
        else
        {
            msg_Warn( p_demux, "unknown key found in pls file: %s", psz_key );
        }
        free( psz_line );
    }
    /* Add last object */
    if( psz_mrl )
    {
        playlist_item_t *p_item = playlist_ItemNew( p_playlist, psz_mrl,
                                                    psz_name );

        playlist_NodeAddItem( p_playlist,p_item,
                              p_parent->pp_parents[0]->i_view,
                              p_parent,
                              PLAYLIST_APPEND, PLAYLIST_END );

        playlist_CopyParents( p_parent, p_item );
        if( i_duration != -1 )
        {
            //playlist_SetDuration( p_playlist, i_position, i_duration );
        }
        free( psz_mrl );
        psz_mrl = NULL;

        vlc_input_item_CopyOptions( &p_parent->input,
                                    &p_item->input );
    }
    else
    {
        msg_Warn( p_demux, "no file= part found for item %d", i_item );
    }
    if( psz_name )
    {
        free( psz_name );
        psz_name = NULL;
    }

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
