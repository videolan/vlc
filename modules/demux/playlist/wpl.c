/*****************************************************************************
 * wpl.c : WPL playlist import
 *****************************************************************************
 * Copyright (C) 2015 VLC authors and VideoLAN
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_demux.h>
#include <vlc_xml.h>

#include "playlist.h"

struct demux_sys_t
{
    xml_reader_t* p_reader;
    char* psz_prefix;
};

static void read_head( demux_t* p_demux, input_item_t* p_input )
{
    demux_sys_t* p_sys = p_demux->p_sys;
    const char* psz_name;
    int i_type;

    do
    {
        i_type = xml_ReaderNextNode( p_sys->p_reader, &psz_name );
        if ( !strcasecmp( psz_name, "meta" ) )
        {
            char* psz_attribute_name = NULL;
            char* psz_attribute_value = NULL;
            while (!psz_attribute_name || !psz_attribute_value)
            {
                const char* psz_attr = NULL;
                const char* psz_val = NULL;
                psz_attr = xml_ReaderNextAttr( p_sys->p_reader, &psz_val );
                if ( !psz_attr || !psz_val )
                    break;
                if ( !strcasecmp( psz_attr, "name" ) )
                    psz_attribute_name = strdup( psz_val );
                else if ( !strcasecmp( psz_attr, "content" ) )
                    psz_attribute_value = strdup( psz_val );
            }
            if ( psz_attribute_name && psz_attribute_value )
            {
                if ( !strcasecmp( psz_attribute_name, "TotalDuration" ) )
                    input_item_SetDuration( p_input, atoll( psz_attribute_value ) );
                else if ( !strcasecmp( psz_attribute_name, "Author" ) )
                    input_item_SetPublisher( p_input, psz_attribute_value );
                else if ( !strcasecmp( psz_attribute_name, "Rating" ) )
                    input_item_SetRating( p_input, psz_attribute_value );
                else if ( !strcasecmp( psz_attribute_name, "Genre" ) )
                    input_item_SetGenre( p_input, psz_attribute_value );
            }
            free( psz_attribute_name );
            free( psz_attribute_value );
        }
        else if ( !strcasecmp( psz_name, "title" ) )
        {
            const char* psz_title;
            int i_type = xml_ReaderNextNode( p_sys->p_reader, &psz_title );
            if ( i_type == XML_READER_TEXT && psz_title != NULL )
                input_item_SetTitle( p_input, psz_title );
        }
    } while ( i_type != XML_READER_ENDELEM || strcasecmp( psz_name, "head" ) );
}

static void read_body( demux_t* p_demux, input_item_node_t* p_node )
{
    demux_sys_t* p_sys = p_demux->p_sys;
    const char* psz_name;
    int i_type;

    i_type = xml_ReaderNextNode( p_sys->p_reader, &psz_name );
    if ( i_type != XML_READER_STARTELEM || strcasecmp( psz_name, "seq" ) )
    {
        msg_Err( p_demux, "Expected opening <seq> tag. Got <%s> with type %d", psz_name, i_type );
        return;
    }

    do
    {
        i_type = xml_ReaderNextNode( p_sys->p_reader, &psz_name );
        if ( !strcasecmp( psz_name, "media" ) )
        {
            const char* psz_attr = NULL;
            const char* psz_val = NULL;
            while ((psz_attr = xml_ReaderNextAttr( p_sys->p_reader, &psz_val )))
            {
                if ( !psz_val )
                    continue;
                if (!strcasecmp( psz_attr, "src" ) )
                {
                    char* mrl = ProcessMRL( psz_val, p_sys->psz_prefix );
                    if ( unlikely( !mrl ) )
                        return;
                    input_item_t* p_item = input_item_NewExt( mrl, NULL, 0, NULL, 0, -1 );
                    if ( likely( p_item ) )
                    {
                        input_item_node_AppendItem( p_node, p_item );
                        input_item_Release( p_item );
                    }
                    free( mrl );
                }
            }
        }
    } while ( i_type != XML_READER_ENDELEM || strcasecmp( psz_name, "seq" ) );

    i_type = xml_ReaderNextNode( p_sys->p_reader, &psz_name );
    if ( i_type != XML_READER_ENDELEM || strcasecmp( psz_name, "body" ) )
        msg_Err( p_demux, "Expected closing <body> tag. Got: <%s> with type %d", psz_name, i_type );
}

static int Demux( demux_t* p_demux )
{
    const char* psz_name;
    int i_type;

    demux_sys_t* p_sys = p_demux->p_sys;
    input_item_t* p_input = GetCurrentItem( p_demux );
    input_item_node_t* p_node = input_item_node_Create( p_input );
    p_sys->psz_prefix = FindPrefix( p_demux );

    do
    {
        i_type = xml_ReaderNextNode( p_sys->p_reader, &psz_name );
        if ( i_type == XML_READER_STARTELEM && !strcasecmp( psz_name, "head" ) )
            read_head( p_demux, p_input );
        else if ( i_type == XML_READER_STARTELEM && !strcasecmp( psz_name, "body" ) )
            read_body( p_demux, p_node );
    } while (i_type != XML_READER_ENDELEM || strcasecmp( psz_name, "smil" ) );

    input_item_node_PostAndDelete( p_node );
    input_item_Release( p_input );
    return 0;
}

void Close_WPL( vlc_object_t* p_this )
{
    demux_t *p_demux = (demux_t*)p_this;
    demux_sys_t* p_sys = p_demux->p_sys;

    free( p_sys->psz_prefix );
    if ( p_sys->p_reader )
        xml_ReaderDelete( p_sys->p_reader );
    free( p_sys );
}

int Import_WPL( vlc_object_t* p_this )
{
    demux_t* p_demux = (demux_t*)p_this;

    CHECK_FILE();
    if( !demux_IsPathExtension( p_demux, ".wpl" ) &&
        !demux_IsPathExtension( p_demux, ".zpl" ) )
        return VLC_EGENERIC;

    DEMUX_INIT_COMMON();

    demux_sys_t* p_sys = p_demux->p_sys;

    p_sys->p_reader = xml_ReaderCreate( p_this, p_demux->s );
    if ( !p_sys->p_reader )
    {
        msg_Err( p_demux, "Failed to create an XML reader" );
        Close_WPL( p_this );
        return VLC_EGENERIC;
    }

    const char* psz_name;
    int type = xml_ReaderNextNode( p_sys->p_reader, &psz_name );
    if ( type != XML_READER_STARTELEM || strcasecmp( psz_name, "smil" ) )
    {
        msg_Err( p_demux, "Invalid WPL playlist. Root element should have been <smil>" );
        Close_WPL( p_this );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_demux, "Found valid WPL playlist" );

    return VLC_SUCCESS;
}

