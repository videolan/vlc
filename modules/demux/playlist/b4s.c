/*****************************************************************************
 * b4s.c : B4S playlist format import
 *****************************************************************************
 * Copyright (C) 2005-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux);
static bool IsWhitespace( const char *psz_string );

/*****************************************************************************
 * Import_B4S: main import function
 *****************************************************************************/
int Import_B4S( vlc_object_t *p_this )
{
    demux_t *demux = (demux_t *)p_this;

    if( !demux_IsPathExtension( demux, ".b4s" )
     && !demux_IsForced( demux, "b4s-open" ) )
        return VLC_EGENERIC;

    demux->pf_demux = Demux;
    demux->pf_control = Control;

    return VLC_SUCCESS;
}

static int Demux( demux_t *p_demux )
{
    int i_ret = -1;

    xml_reader_t *p_xml_reader = NULL;
    char *psz_elname = NULL;
    const char *node;
    input_item_t *p_input;
    char *psz_mrl = NULL, *psz_title = NULL, *psz_genre = NULL;
    char *psz_now = NULL, *psz_listeners = NULL, *psz_bitrate = NULL;
    input_item_node_t *p_subitems = NULL;

    input_item_t *p_current_input = GetCurrentItem(p_demux);

    free( stream_ReadLine( p_demux->s ) );

    p_xml_reader = xml_ReaderCreate( p_demux, p_demux->s );
    if( !p_xml_reader )
        return -1;

    /* xml */
    /* check root node */
    if( xml_ReaderNextNode( p_xml_reader, &node ) != XML_READER_STARTELEM )
    {
        msg_Err( p_demux, "invalid file (no root node)" );
        goto end;
    }

    if( strcmp( node, "WinampXML" ) )
    {
        msg_Err( p_demux, "invalid root node: %s", node );
        goto end;
    }

    /* root node should not have any attributes, and should only
     * contain the "playlist node */

    /* Skip until 1st child node */
    while( (i_ret = xml_ReaderNextNode( p_xml_reader, &node )) != XML_READER_STARTELEM )
        if( i_ret <= 0 )
        {
            msg_Err( p_demux, "invalid file (no child node)" );
            goto end;
        }

    if( strcmp( node, "playlist" ) )
    {
        msg_Err( p_demux, "invalid child node %s", node );
        goto end;
    }

    // Read the attributes
    const char *attr, *value;
    while( (attr = xml_ReaderNextAttr( p_xml_reader, &value )) != NULL )
    {
        if( !strcmp( attr, "num_entries" ) )
            msg_Dbg( p_demux, "playlist has %d entries", atoi(value) );
        else if( !strcmp( attr, "label" ) )
            input_item_SetName( p_current_input, value );
        else
            msg_Warn( p_demux, "stray attribute %s with value %s in element"
                      " <playlist>", attr, value );
    }

    p_subitems = input_item_node_Create( p_current_input );

    while( (i_ret = xml_ReaderNextNode( p_xml_reader, &node )) > 0 )
    {
        // Get the node type
        switch( i_ret )
        {
            case XML_READER_STARTELEM:
            {
                // Read the element name
                free( psz_elname );
                psz_elname = strdup( node );
                if( unlikely(!psz_elname) )
                    goto end;

                // Read the attributes
                while( (attr = xml_ReaderNextAttr( p_xml_reader, &value )) )
                {
                    if( !strcmp( psz_elname, "entry" ) &&
                        !strcmp( attr, "Playstring" ) )
                    {
                        free( psz_mrl );
                        psz_mrl = strdup( value );
                    }
                    else
                    {
                        msg_Warn( p_demux, "unexpected attribute %s in <%s>",
                                  attr, psz_elname );
                    }
                }
                break;
            }

            case XML_READER_TEXT:
            {
                char **p;

                if( psz_elname == NULL )
                    break;
                if( IsWhitespace( node ) )
                    break;
                if( !strcmp( psz_elname, "Name" ) )
                    p = &psz_title;
                else if( !strcmp( psz_elname, "Genre" ) )
                    p = &psz_genre;
                else if( !strcmp( psz_elname, "Nowplaying" ) )
                    p = &psz_now;
                else if( !strcmp( psz_elname, "Listeners" ) )
                    p = &psz_listeners;
                else if( !strcmp( psz_elname, "Bitrate" ) )
                    p = &psz_bitrate;
                else
                {
                    msg_Warn( p_demux, "unexpected text in element <%s>",
                              psz_elname );
                    break;
                }
                free( *p );
                *p = strdup( node );
                break;
            }

            // End element
            case XML_READER_ENDELEM:
            {
                // Read the element name
                if( !strcmp( node, "entry" ) )
                {
                    p_input = input_item_New( psz_mrl, psz_title );
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
                FREENULL( psz_elname );
                break;
            }
        }
    }

    if( i_ret < 0 )
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
        xml_ReaderDelete( p_xml_reader );
    return i_ret;
}

static bool IsWhitespace( const char *psz_string )
{
    psz_string += strspn( psz_string, " \t\r\n" );
    return !*psz_string;
}
