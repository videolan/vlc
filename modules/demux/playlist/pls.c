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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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
        msg_Err( p_demux, "out of memory" );
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
    int            i_position;
    int            i_item = -1;
    int            i_new_item = 0;
    int            i_key_length;

    INIT_PLAYLIST_STUFF;

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
                p_input = input_ItemNewExt( p_playlist, psz_mrl, psz_name,
                                            0, NULL, -1 );
                vlc_input_item_CopyOptions( p_current->p_input, p_input );
                playlist_AddWhereverNeeded( p_playlist, p_input, p_current,
                                p_item_in_category, (i_parent_id > 0 ) ?
                                VLC_TRUE: VLC_FALSE, PLAYLIST_APPEND );
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
        p_input = input_ItemNewExt( p_playlist, psz_mrl, psz_name,0, NULL, -1 );
        vlc_input_item_CopyOptions( p_current->p_input, p_input );
        playlist_AddWhereverNeeded( p_playlist, p_input, p_current,
                                p_item_in_category, (i_parent_id > 0 ) ?
                                VLC_TRUE: VLC_FALSE, PLAYLIST_APPEND );
        free( psz_mrl );
        psz_mrl = NULL;
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

    HANDLE_PLAY_AND_RELEASE;
    return VLC_SUCCESS;
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    return VLC_EGENERIC;
}
