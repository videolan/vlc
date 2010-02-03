/*****************************************************************************
 * b4s.c : B4S playlist format import
 *****************************************************************************
 * Copyright (C) 2005-2009 the VideoLAN team
 * $Id$
 *
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
#include <vlc_xml.h>

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
static int IsWhitespace( char *psz_string );

/*****************************************************************************
 * Import_B4S: main import function
 *****************************************************************************/
int Import_B4S( vlc_object_t *p_this )
{
    DEMUX_BY_EXTENSION_OR_FORCED_MSG( ".b4s", "b4s-open",
                                      "using B4S playlist reader" );
    p_demux->p_sys->psz_prefix = FindPrefix( p_demux );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
void Close_B4S( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    free( p_sys->psz_prefix );
    free( p_sys );
}

static int Demux( demux_t *p_demux )
{
    int i_ret = -1;

    xml_t *p_xml;
    xml_reader_t *p_xml_reader = NULL;
    char *psz_elname = NULL;
    input_item_t *p_input;
    char *psz_mrl = NULL, *psz_title = NULL, *psz_genre = NULL;
    char *psz_now = NULL, *psz_listeners = NULL, *psz_bitrate = NULL;
    input_item_node_t *p_subitems = NULL;

    input_item_t *p_current_input = GetCurrentItem(p_demux);

    p_xml = xml_Create( p_demux );
    if( !p_xml )
        goto end;

    psz_elname = stream_ReadLine( p_demux->s );
    free( psz_elname );
    psz_elname = NULL;

    p_xml_reader = xml_ReaderCreate( p_xml, p_demux->s );
    if( !p_xml_reader )
        goto end;

    /* xml */
    /* check root node */
    if( xml_ReaderRead( p_xml_reader ) != 1 )
    {
        msg_Err( p_demux, "invalid file (no root node)" );
        goto end;
    }

    if( xml_ReaderNodeType( p_xml_reader ) != XML_READER_STARTELEM ||
        ( psz_elname = xml_ReaderName( p_xml_reader ) ) == NULL ||
        strcmp( psz_elname, "WinampXML" ) )
    {
        msg_Err( p_demux, "invalid root node %i, %s",
                 xml_ReaderNodeType( p_xml_reader ), psz_elname );
        goto end;
    }
    FREENULL( psz_elname );

    /* root node should not have any attributes, and should only
     * contain the "playlist node */

    /* Skip until 1st child node */
    while( (i_ret = xml_ReaderRead( p_xml_reader )) == 1 &&
           xml_ReaderNodeType( p_xml_reader ) != XML_READER_STARTELEM );
    if( i_ret != 1 )
    {
        msg_Err( p_demux, "invalid file (no child node)" );
        goto end;
    }

    if( ( psz_elname = xml_ReaderName( p_xml_reader ) ) == NULL ||
        strcmp( psz_elname, "playlist" ) )
    {
        msg_Err( p_demux, "invalid child node %s", psz_elname );
        goto end;
    }
    FREENULL( psz_elname );

    // Read the attributes
    while( xml_ReaderNextAttr( p_xml_reader ) == VLC_SUCCESS )
    {
        char *psz_name = xml_ReaderName( p_xml_reader );
        char *psz_value = xml_ReaderValue( p_xml_reader );
        if( !psz_name || !psz_value )
        {
            free( psz_name );
            free( psz_value );
            goto end;
        }
        if( !strcmp( psz_name, "num_entries" ) )
        {
            msg_Dbg( p_demux, "playlist has %d entries", atoi(psz_value) );
        }
        else if( !strcmp( psz_name, "label" ) )
        {
            input_item_SetName( p_current_input, psz_value );
        }
        else
        {
            msg_Warn( p_demux, "stray attribute %s with value %s in element"
                      " 'playlist'", psz_name, psz_value );
        }
        free( psz_name );
        free( psz_value );
    }

    p_subitems = input_item_node_Create( p_current_input );

    while( (i_ret = xml_ReaderRead( p_xml_reader )) == 1 )
    {
        // Get the node type
        switch( xml_ReaderNodeType( p_xml_reader ) )
        {
            // Error
            case -1:
                goto end;

            case XML_READER_STARTELEM:
            {
                // Read the element name
                free( psz_elname );
                psz_elname = xml_ReaderName( p_xml_reader );
                if( !psz_elname )
                    goto end;

                // Read the attributes
                while( xml_ReaderNextAttr( p_xml_reader ) == VLC_SUCCESS )
                {
                    char *psz_name = xml_ReaderName( p_xml_reader );
                    char *psz_value = xml_ReaderValue( p_xml_reader );
                    if( !psz_name || !psz_value )
                    {
                        free( psz_name );
                        free( psz_value );
                        goto end;
                    }
                    if( !strcmp( psz_elname, "entry" ) &&
                        !strcmp( psz_name, "Playstring" ) )
                    {
                        psz_mrl = psz_value;
                    }
                    else
                    {
                        msg_Warn( p_demux, "unexpected attribure %s in element %s",
                                  psz_name, psz_elname );
                        free( psz_value );
                    }
                    free( psz_name );
                }
                break;
            }
            case XML_READER_TEXT:
            {
                char *psz_text = xml_ReaderValue( p_xml_reader );
                if( IsWhitespace( psz_text ) )
                {
                    free( psz_text );
                    break;
                }
                if( !strcmp( psz_elname, "Name" ) )
                {
                    psz_title = psz_text;
                }
                else if( !strcmp( psz_elname, "Genre" ) )
                {
                    psz_genre = psz_text;
                }
                else if( !strcmp( psz_elname, "Nowplaying" ) )
                {
                    psz_now = psz_text;
                }
                else if( !strcmp( psz_elname, "Listeners" ) )
                {
                    psz_listeners = psz_text;
                }
                else if( !strcmp( psz_elname, "Bitrate" ) )
                {
                    psz_bitrate = psz_text;
                }
                else if( !strcmp( psz_elname, "" ) )
                {
                    free( psz_text );
                }
                else
                {
                    msg_Warn( p_demux, "unexpected text in element '%s'",
                              psz_elname );
                    free( psz_text );
                }
                break;
            }
            // End element
            case XML_READER_ENDELEM:
            {
                // Read the element name
                free( psz_elname );
                psz_elname = xml_ReaderName( p_xml_reader );
                if( !psz_elname )
                    goto end;
                if( !strcmp( psz_elname, "entry" ) )
                {
                    p_input = input_item_New( p_demux, psz_mrl, psz_title );
                    if( psz_now )
                        input_item_SetNowPlaying( p_input, psz_now );
                    if( psz_genre )
                        input_item_SetGenre( p_input, psz_genre );
                    if( psz_listeners )
                        msg_Err( p_demux, "Unsupported meta listeners" );
                    if( psz_bitrate )
                        msg_Err( p_demux, "Unsupported meta bitrate" );

                    input_item_node_AppendItem( p_subitems, p_input );
                    vlc_gc_decref( p_input );
                    FREENULL( psz_title );
                    FREENULL( psz_mrl );
                    FREENULL( psz_genre );
                    FREENULL( psz_bitrate );
                    FREENULL( psz_listeners );
                    FREENULL( psz_now );
                }
                free( psz_elname );
                psz_elname = strdup( "" );

                break;
            }
        }
    }

    if( i_ret != 0 )
    {
        msg_Warn( p_demux, "error while parsing data" );
        i_ret = 0; /* Needed for correct operation of go back */
    }

end:
    free( psz_elname );

    if( p_subitems )
        input_item_node_PostAndDelete( p_subitems );

    vlc_gc_decref( p_current_input );
    if( p_xml_reader )
        xml_ReaderDelete( p_xml, p_xml_reader );
    if( p_xml )
        xml_Delete( p_xml );
    return i_ret;
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    VLC_UNUSED(p_demux); VLC_UNUSED(i_query); VLC_UNUSED(args);
    return VLC_EGENERIC;
}

static int IsWhitespace( char *psz_string )
{
    while( *psz_string )
    {
        if( *psz_string != ' ' && *psz_string != '\t' && *psz_string != '\r' &&
            *psz_string != '\n' )
        {
            return false;
        }
        psz_string++;
    }
    return true;
}
