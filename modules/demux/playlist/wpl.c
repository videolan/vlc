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

static int consume_tag( xml_reader_t* p_reader, char const* psz_tag )
{
    int i_type, i_depth = 0;
    char const *psz_name;

    if( xml_ReaderIsEmptyElement( p_reader ) == 1 )
        return VLC_SUCCESS;

    while( ( i_type = xml_ReaderNextNode( p_reader, &psz_name ) ) > 0 )
    {
        if( i_type == XML_READER_ENDELEM && !strcasecmp( psz_name, psz_tag ) )
        {
            if( --i_depth < 0 )
                return VLC_SUCCESS;
        }
        else if( i_type == XML_READER_STARTELEM && !strcasecmp( psz_name, psz_tag ) )
            ++i_depth;
    }

    return VLC_EGENERIC;
}

static int consume_volatile_tag( demux_t* p_demux, char const* psz_tag )
{
    char* psz_copy = strdup( psz_tag );
    int ret = VLC_ENOMEM;

    if( likely( psz_copy ) )
        ret = consume_tag( p_demux->p_sys->p_reader, psz_copy );

    free( psz_copy );
    return ret;
}

static void parse_meta( demux_t* p_demux, input_item_t* p_input )
{
    xml_reader_t* p_reader = p_demux->p_sys->p_reader;
    bool const b_empty = xml_ReaderIsEmptyElement( p_reader ) == 1;

    char *psz_meta_name = NULL, *psz_meta_content = NULL;
    char const *psz_attr, *psz_value;
    while( ( psz_attr = xml_ReaderNextAttr( p_reader, &psz_value ) ) )
    {
        if( psz_value == NULL )
            continue;

        if( !strcasecmp( psz_attr, "name" ) && !psz_meta_name )
            psz_meta_name = strdup( psz_value );
        else
            if( !strcasecmp( psz_attr, "content" ) && !psz_meta_content )
                psz_meta_content = strdup( psz_value );

        if( psz_meta_name && psz_meta_content )
            break;
    }

    if( b_empty == false )
        consume_tag( p_reader, "meta" );

    if( !psz_meta_name || !psz_meta_content )
        goto done;

    if( !strcasecmp( psz_meta_name, "TotalDuration" ) )
        input_item_SetDuration( p_input, atoll( psz_meta_content ) );
    else
        if( !strcasecmp( psz_meta_name, "Author" ) )
            input_item_SetPublisher( p_input, psz_meta_content );
    else
        if( !strcasecmp( psz_meta_name, "Rating" ) )
            input_item_SetRating( p_input, psz_meta_content );
    else
        if( !strcasecmp( psz_meta_name, "Genre" ) )
            input_item_SetGenre( p_input, psz_meta_content );
    else
        msg_Warn( p_demux, "ignoring unknown meta-attribute %s", psz_meta_name );

done:
    free( psz_meta_name );
    free( psz_meta_content );
}

static int parse_title_element( demux_t* p_demux, input_item_t* p_input )
{
    xml_reader_t* p_reader = p_demux->p_sys->p_reader;
    char const* psz_title;

    if( xml_ReaderIsEmptyElement( p_reader ) )
        return VLC_SUCCESS;

    if( xml_ReaderNextNode( p_reader, &psz_title ) != XML_READER_TEXT )
        return VLC_EGENERIC;

    input_item_SetTitle( p_input, psz_title );

    consume_tag( p_reader, "title" );
    return VLC_SUCCESS;
}

static void read_head( demux_t* p_demux, input_item_t* p_input )
{
    xml_reader_t* p_reader = p_demux->p_sys->p_reader;
    char const* psz_name;
    int i_type;

    while( ( i_type = xml_ReaderNextNode( p_reader, &psz_name ) ) > 0 )
    {
        if( i_type == XML_READER_ENDELEM && !strcasecmp( psz_name, "head" ) )
            break;

        if( i_type == XML_READER_STARTELEM )
        {
            if( !strcasecmp( psz_name, "meta" ) )
            {
                parse_meta( p_demux, p_input );
                continue;
            }

            if( !strcasecmp( psz_name, "title" ) )
            {
                if( parse_title_element( p_demux, p_input ) )
                    break;
                continue;
            }

            msg_Warn( p_demux, "skipping unknown tag <%s> in <head>", psz_name );
            consume_volatile_tag( p_demux, psz_name );
        }
    }
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

    if( xml_ReaderIsEmptyElement( p_sys->p_reader ) == 1 )
        return;

    while ( ( i_type = xml_ReaderNextNode( p_sys->p_reader, &psz_name ) ) > 0 )
    {
        if ( i_type == XML_READER_ENDELEM && !strcasecmp( psz_name, "seq" ) )
            break;

        if( i_type == XML_READER_STARTELEM )
        {
            if( !strcasecmp( psz_name, "media" ) )
            {
                const bool b_empty = xml_ReaderIsEmptyElement( p_sys->p_reader );

                const char *psz_attr = NULL, *psz_val = NULL;
                while ((psz_attr = xml_ReaderNextAttr( p_sys->p_reader, &psz_val )))
                {
                    if ( !psz_val || *psz_val == '\0' )
                        continue;
                    if (!strcasecmp( psz_attr, "src" ) )
                    {
                        char* mrl = ProcessMRL( psz_val, p_sys->psz_prefix );
                        if ( unlikely( !mrl ) )
                            return;
                        input_item_t* p_item = input_item_New( mrl, NULL );
                        if ( likely( p_item ) )
                        {
                            input_item_node_AppendItem( p_node, p_item );
                            input_item_Release( p_item );
                        }
                        free( mrl );
                    }
                }

                if( b_empty == false )
                    consume_tag( p_sys->p_reader, "media" );

                continue;
            }

            msg_Warn( p_demux, "skipping unknown tag <%s> in <seq>", psz_name );
            consume_volatile_tag( p_demux, psz_name );
        }
    }

    i_type = xml_ReaderNextNode( p_sys->p_reader, &psz_name );
    if ( i_type != XML_READER_ENDELEM || strcasecmp( psz_name, "body" ) )
        msg_Err( p_demux, "Expected closing <body> tag. Got: <%s> with type %d", psz_name, i_type );
}

static int Demux( demux_t* p_demux )
{
    const char* psz_name;
    int i_type;

    demux_sys_t* p_sys = p_demux->p_sys;
    p_sys->psz_prefix = FindPrefix( p_demux );
    if( unlikely(p_sys->psz_prefix == NULL) )
        return VLC_DEMUXER_EOF;

    if( xml_ReaderNextNode( p_sys->p_reader, &psz_name ) != XML_READER_STARTELEM ||
        strcasecmp( psz_name, "smil" ) || xml_ReaderIsEmptyElement( p_sys->p_reader ) == 1 )
    {
        return VLC_DEMUXER_EOF;
    }

    input_item_t* p_input = GetCurrentItem( p_demux );
    input_item_node_t* p_node = input_item_node_Create( p_input );

    if( unlikely( !p_node ) )
        return VLC_DEMUXER_EOF;

    while( ( i_type = xml_ReaderNextNode( p_sys->p_reader, &psz_name ) ) > 0 )
    {
        if( i_type == XML_READER_ENDELEM && !strcasecmp( psz_name, "smil" ) )
            break;

        if( i_type == XML_READER_STARTELEM )
        {
            if( !strcasecmp( psz_name, "head" ) )
            {
                read_head( p_demux, p_input );
                continue;
            }

            if( !strcasecmp( psz_name, "body" ) )
            {
                read_body( p_demux, p_node );
                continue;
            }

            msg_Warn( p_demux, "skipping unknown tag <%s> in <smil>", psz_name );
            consume_volatile_tag( p_demux, psz_name );
        }
    }

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
    uint8_t *p_peek;
    ssize_t i_peek = vlc_stream_Peek( p_demux->s, (const uint8_t **) &p_peek, 2048 );
    if( unlikely( i_peek <= 0 ) )
    {
        Close_WPL( p_this );
        return VLC_EGENERIC;
    }

    stream_t *p_probestream = vlc_stream_MemoryNew( p_demux->s, p_peek, i_peek, true );
    if( unlikely( !p_probestream ) )
    {
        Close_WPL( p_this );
        return VLC_EGENERIC;
    }

    p_sys->p_reader = xml_ReaderCreate( p_this, p_probestream );
    if ( !p_sys->p_reader )
    {
        msg_Err( p_demux, "Failed to create an XML reader" );
        Close_WPL( p_this );
        vlc_stream_Delete( p_probestream );
        return VLC_EGENERIC;
    }

    const int i_flags = p_sys->p_reader->obj.flags;
    p_sys->p_reader->obj.flags |= OBJECT_FLAGS_QUIET;
    const char* psz_name;
    int type = xml_ReaderNextNode( p_sys->p_reader, &psz_name );
    p_sys->p_reader->obj.flags = i_flags;
    if ( type != XML_READER_STARTELEM || strcasecmp( psz_name, "smil" ) )
    {
        msg_Err( p_demux, "Invalid WPL playlist. Root element should have been <smil>" );
        Close_WPL( p_this );
        vlc_stream_Delete( p_probestream );
        return VLC_EGENERIC;
    }

    p_sys->p_reader = xml_ReaderReset( p_sys->p_reader, p_demux->s );
    vlc_stream_Delete( p_probestream );

    msg_Dbg( p_demux, "Found valid WPL playlist" );

    return VLC_SUCCESS;
}

