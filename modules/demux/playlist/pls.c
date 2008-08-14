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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_demux.h>

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
int Import_PLS( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    const uint8_t *p_peek;
    CHECK_PEEK( p_peek, 10 );

    if( POKE( p_peek, "[playlist]", 10 ) || POKE( p_peek, "[Reference]", 10 ) ||
        demux_IsPathExtension( p_demux, ".pls" )   || demux_IsForced( p_demux, "pls" ) )
    {
        ;
    }
    else return VLC_EGENERIC;

    STANDARD_DEMUX_INIT_MSG(  "found valid PLS playlist file");
    p_demux->p_sys->psz_prefix = FindPrefix( p_demux );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
void Close_PLS( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    free( p_demux->p_sys->psz_prefix );
    free( p_demux->p_sys );
}

static int Demux( demux_t *p_demux )
{
    mtime_t        i_duration = -1;
    char          *psz_name = NULL;
    char          *psz_line;
    char          *psz_mrl = NULL;
    char          *psz_mrl_orig = NULL;
    char          *psz_key;
    char          *psz_value;
    int            i_item = -1;
    int            i_new_item = 0;
    int            i_key_length;
    input_item_t *p_input;

    INIT_PLAYLIST_STUFF;

    while( ( psz_line = stream_ReadLine( p_demux->s ) ) )
    {
        if( !strncasecmp( psz_line, "[playlist]", sizeof("[playlist]")-1 ) ||
            !strncasecmp( psz_line, "[Reference]", sizeof("[Reference]")-1 ) )
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
        if( !strcasecmp( psz_key, "numberofentries" ) )
        {
            msg_Dbg( p_demux, "pls should have %d entries", atoi(psz_value) );
            free( psz_line);
            continue;
        }
        /* find the number part of of file1, title1 or length1 etc */
        i_key_length = strlen( psz_key );
        if( i_key_length >= 4 ) /* Ref1 type case */
        {
            i_new_item = atoi( psz_key + 3 );
            if( i_new_item == 0 && i_key_length >= 5 ) /* file1 type case */
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
                p_input = input_item_NewExt( p_demux, psz_mrl, psz_name,
                                            0, NULL, -1 );
                input_item_CopyOptions( p_current_input, p_input );
                input_item_AddSubItem( p_current_input, p_input );
                vlc_gc_decref( p_input );
            }
            else
            {
                msg_Warn( p_demux, "no file= part found for item %d", i_item );
            }
            free( psz_name );
            psz_name = NULL;
            i_duration = -1;
            i_item = i_new_item;
            i_new_item = 0;
        }
        if( !strncasecmp( psz_key, "file", sizeof("file") -1 ) ||
            !strncasecmp( psz_key, "Ref", sizeof("Ref") -1 ) )
        {
            free( psz_mrl_orig );
            psz_mrl_orig =
            psz_mrl = ProcessMRL( psz_value, p_demux->p_sys->psz_prefix );

            if( !strncasecmp( psz_key, "Ref", sizeof("Ref") -1 ) )
            {
                if( !strncasecmp( psz_mrl, "http://", sizeof("http://") -1 ) )
                {
                    psz_mrl++;
                    psz_mrl[0] = 'm';
                    psz_mrl[1] = 'm';
                    psz_mrl[2] = 's';
                }
            }
        }
        else if( !strncasecmp( psz_key, "title", sizeof("title") -1 ) )
        {
            free( psz_name );
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
        p_input = input_item_NewExt( p_demux, psz_mrl, psz_name,0, NULL, -1 );
        input_item_CopyOptions( p_current_input, p_input );
        input_item_AddSubItem( p_current_input, p_input );
        vlc_gc_decref( p_input );
        free( psz_mrl_orig );
        psz_mrl = NULL;
    }
    else
    {
        msg_Warn( p_demux, "no file= part found for item %d", i_item );
    }
    free( psz_name );
    psz_name = NULL;

    HANDLE_PLAY_AND_RELEASE;
    return 0; /* Needed for correct operation of go back */
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    VLC_UNUSED(p_demux); VLC_UNUSED(i_query); VLC_UNUSED(args);
    return VLC_EGENERIC;
}
