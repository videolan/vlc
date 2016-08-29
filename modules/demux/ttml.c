/*****************************************************************************
 * ttml.c : TTML subtitles demux
 *****************************************************************************
 * Copyright (C) 2015 VLC authors and VideoLAN
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
 *          Sushma Reddy <sushma.reddy@research.iiit.ac.in>
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
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_xml.h>
#include <vlc_strings.h>
#include <vlc_memory.h>
#include <vlc_es_out.h>

static int Open( vlc_object_t* p_this );
static void Close( demux_t* p_demux );

vlc_module_begin ()
    set_shortname( N_("TTML") )
    set_description( N_("TTML demuxer") )
    set_capability( "demux", 2 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_callbacks( Open, Close )
    add_shortcut( "ttml", "subtitle" )
vlc_module_end ();


typedef struct
{
   int64_t i_start;
   int64_t i_stop;
   char *psz_text;
} subtitle_t;

struct demux_sys_t
{
    xml_t*          p_xml;
    xml_reader_t*   p_reader;
    subtitle_t*     subtitle;
    es_out_id_t*    p_es;
    int64_t         i_length;
    int64_t         i_next_demux_time;
    int             i_subtitle;
    int             i_subtitles;
    char*           psz_head;
    size_t          i_head_len;
    bool            b_has_head;
};

typedef struct node_t
{
    struct node_t* p_parent;
    char* psz_styleid;
    char* psz_node_name;
    char* psz_begin;
    char* psz_end;
    vlc_dictionary_t attr_dict;
} node_t;

static int Control( demux_t* p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int64_t *pi64, i64;
    double *pf, f;

    switch( i_query )
    {
        case DEMUX_CAN_SEEK:
            *va_arg( args, bool * ) = true;
            return VLC_SUCCESS;
        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_sys->i_subtitle < p_sys->i_subtitles )
                *pi64 = p_sys->subtitle[p_sys->i_subtitle].i_start;
            else
                *pi64 = p_sys->i_length;
            return VLC_SUCCESS;
        case DEMUX_SET_TIME:
            i64 = (int64_t)va_arg( args, int64_t );
            p_sys->i_subtitle = 0;
            while( p_sys->i_subtitle < p_sys->i_subtitles )
            {
                const subtitle_t *p_subtitle = &p_sys->subtitle[p_sys->i_subtitle];

                if( p_subtitle->i_start > i64 )
                    break;
                if( p_subtitle->i_stop > p_subtitle->i_start && p_subtitle->i_stop > i64 )
                    break;

                p_sys->i_subtitle++;
            }

            if( p_sys->i_subtitle >= p_sys->i_subtitles )
                return VLC_EGENERIC;
            return VLC_SUCCESS;
        case DEMUX_SET_NEXT_DEMUX_TIME:
            i64 = (int64_t)va_arg( args, int64_t );
            p_sys->i_next_demux_time = i64;
            return VLC_SUCCESS;
        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = p_sys->i_length;
            return VLC_SUCCESS;
        case DEMUX_GET_POSITION:
            pf = (double*)va_arg( args, double * );
            if( p_sys->i_subtitle >= p_sys->i_subtitles )
            {
                *pf = 1.0;
            }
            else if( p_sys->i_subtitles > 0 )
            {
                *pf = (double)p_sys->subtitle[p_sys->i_subtitle].i_start /
                      (double)p_sys->i_length;
            }
            else
            {
                *pf = 0.0;
            }
            return VLC_SUCCESS;
        case DEMUX_SET_POSITION:
            f = (double)va_arg( args, double );
            i64 = f * p_sys->i_length;

            p_sys->i_subtitle = 0;
            while( p_sys->i_subtitle < p_sys->i_subtitles &&
                   p_sys->subtitle[p_sys->i_subtitle].i_start < i64 )
            {
                p_sys->i_subtitle++;
            }
            if( p_sys->i_subtitle >= p_sys->i_subtitles )
                return VLC_EGENERIC;
            return VLC_SUCCESS;
        case DEMUX_GET_PTS_DELAY:
        case DEMUX_GET_FPS:
        case DEMUX_GET_META:
        case DEMUX_GET_ATTACHMENTS:
        case DEMUX_GET_TITLE_INFO:
        case DEMUX_HAS_UNSUPPORTED_META:
        case DEMUX_CAN_RECORD:
            return VLC_EGENERIC;
        default:
            msg_Err( p_demux, "unknown query %d in subtitle control", i_query );
            return VLC_EGENERIC;
    }
    return VLC_EGENERIC;
}

static int Convert_time( int64_t *timing_value, const char *s )
{
    int h1 = 0;
    int m1 = 0;
    int s1 = 0;
    int d1 = 0;

    if ( sscanf( s, "%d.%ds", &s1, &d1) == 2 ||
         sscanf( s, "%d:%d:%d%*[,.]%d", &h1, &m1, &s1, &d1 ) == 4 ||
         sscanf( s, "%d:%d:%d", &h1, &m1, &s1) == 3 )
    {
        (*timing_value) = ( (int64_t)h1 * 3600 * 1000 +
                            (int64_t)m1 * 60 * 1000 +
                            (int64_t)s1 * 1000 +
                            (int64_t)d1 ) * 1000;

        return VLC_SUCCESS;
    }

    return VLC_EGENERIC;
}

static char* Append( char* psz_old, const char* psz_format, ... )
{
    va_list ap;
    char* psz_new;
    va_start (ap, psz_format);
    int ret = vasprintf( &psz_new, psz_format, ap );
    va_end (ap);
    if ( ret < 0 )
    {
        free( psz_old );
        return NULL;
    }
    char* psz_concat;
    ret = asprintf( &psz_concat, "%s%s", psz_old, psz_new );
    free( psz_old );
    free( psz_new );
    if( ret < 0 )
        return NULL;
    return psz_concat;
}

static void FreeDictAttrValue( void* p_attr_value, void* p_obj )
{
    VLC_UNUSED( p_obj );
    free( p_attr_value );
}

static void ClearNode( node_t* p_node )
{
    free( p_node->psz_styleid );
    free( p_node->psz_node_name );
    free( p_node->psz_begin );
    free( p_node->psz_end );
    vlc_dictionary_clear( &p_node->attr_dict, FreeDictAttrValue, NULL );
    free( p_node );
}

static void ClearNodeStack( node_t* p_node )
{
    while( p_node != NULL )
    {
        node_t* p_parent = p_node->p_parent;
        ClearNode( p_node );
        p_node = p_parent;
    }
}

static int MergeAttrDict ( vlc_dictionary_t* p_dest, const vlc_dictionary_t* p_src )
{
    char** pp_src_keys = vlc_dictionary_all_keys( p_src );
    if( unlikely( pp_src_keys == NULL ) )
        return VLC_ENOMEM;

    for( int i = 0; pp_src_keys[i] != NULL; i++)
    {
        if( !vlc_dictionary_value_for_key( p_dest, pp_src_keys[i] ) )
            vlc_dictionary_insert( p_dest, pp_src_keys[i], vlc_dictionary_value_for_key( p_src, pp_src_keys[i] ) );

        free( pp_src_keys[i] );
    }
    free( pp_src_keys );
    return VLC_SUCCESS;
}

static int MergeStyles(char** pp_dest, char* p_src)
{
    if( p_src )
    {
        char *p_tmp = NULL;
        if( asprintf( &p_tmp, "%s %s", p_src, *pp_dest ) < 0 )
            return VLC_ENOMEM;

        free( *pp_dest );
        *pp_dest = p_tmp;
    }
    return VLC_SUCCESS;
}

static int MergeNodeWithParents( node_t* p_node )
{
    node_t *parent = p_node->p_parent;
    while( parent != NULL)
    {
        if( MergeAttrDict( &p_node->attr_dict, &parent->attr_dict ) != VLC_SUCCESS ||
            MergeStyles( &p_node->psz_styleid, parent->psz_styleid ) != VLC_SUCCESS )
            return VLC_EGENERIC;

        parent = parent->p_parent;
    }
    return VLC_SUCCESS;
}

static char* NodeToStr( const node_t* p_node )
{   
    char* psz_text = NULL;

    if( asprintf( &psz_text, "<%s", p_node->psz_node_name ) < 0 )
        return NULL;

    const vlc_dictionary_t* p_attr_dict = &p_node->attr_dict;
    char** pp_keys = vlc_dictionary_all_keys( p_attr_dict );
    if( unlikely( pp_keys == NULL ) )
    {
        free( psz_text );
        return NULL;
    }

    for( int i = 0; pp_keys[i] != NULL; i++)
    {
        psz_text = Append( psz_text, " %s=\"%s\"", pp_keys[i], vlc_dictionary_value_for_key( p_attr_dict, pp_keys[i] ) );
        free( pp_keys[i] );
    }
    free( pp_keys );
    if( p_node->psz_styleid )
        psz_text = Append( psz_text, " style=\"%s\"", p_node->psz_styleid );

    if( !strcasecmp( p_node->psz_node_name, "br" ) || !strcasecmp( p_node->psz_node_name, "tt:br" ) )
        psz_text = Append( psz_text, "/>" );
    else
        psz_text = Append( psz_text, ">" );

    return psz_text;
}

static int ReadAttrNode( xml_reader_t* reader, node_t* p_node, const char* psz_node_name )
{
    const char* psz_attr_value = NULL;

    vlc_dictionary_init( &p_node->attr_dict, 0 );

    const char* psz_attr_name = xml_ReaderNextAttr( reader, &psz_attr_value );
    p_node->psz_node_name = strdup( psz_node_name );
    if( unlikely( p_node->psz_node_name == NULL ) )
        return VLC_ENOMEM;

    while( psz_attr_value && psz_attr_name )
    {
        char* psz_value = strdup( psz_attr_value );
        if( unlikely( psz_value == NULL ) )
            return VLC_ENOMEM;

        if( !strcasecmp( psz_attr_name, "style" ) )
            p_node->psz_styleid = psz_value;
        else if( !strcasecmp( psz_attr_name, "begin" ) )
            p_node->psz_begin = psz_value;
        else if( ! strcasecmp( psz_attr_name, "end" ) )
            p_node->psz_end = psz_value;
        else
            vlc_dictionary_insert( &p_node->attr_dict, psz_attr_name, psz_value );

        psz_attr_name = xml_ReaderNextAttr(reader, &psz_attr_value );
    }
    return VLC_SUCCESS;
}

static int ReadTTML( demux_t* p_demux )
{
    demux_sys_t* p_sys = p_demux->p_sys;
    node_t* p_parent_node = NULL;
    char* psz_text = NULL;
    const char* psz_node_name;
    int i_max_sub = 0;
    int i_type;

    do
    {
        i_type = xml_ReaderNextNode( p_sys->p_reader, &psz_node_name );
        if( i_type <= XML_READER_NONE )
            break;

        if( i_type == XML_READER_STARTELEM && ( !strcasecmp( psz_node_name, "head" ) || !strcasecmp( psz_node_name, "tt:head" ) ) )
        {
            p_sys->b_has_head = true;
            /* we don't want to parse the head content here */
            while( i_type != XML_READER_ENDELEM || ( strcasecmp( psz_node_name, "head" ) && strcasecmp( psz_node_name, "tt:head" ) ) )
            {
                i_type = xml_ReaderNextNode( p_sys->p_reader, &psz_node_name );
            }
        }
        else if( i_type == XML_READER_STARTELEM && ( strcasecmp( psz_node_name, "tt" ) && strcasecmp( psz_node_name, "tt:tt") ) )
        {
            node_t* p_node = calloc( 1, sizeof( *p_node ) );
            if( unlikely( p_node == NULL ) )
            {
                ClearNodeStack( p_parent_node );
                return VLC_ENOMEM;
            }

            if( ReadAttrNode( p_sys->p_reader, p_node, psz_node_name ) != VLC_SUCCESS )
            {
                ClearNode( p_node );
                ClearNodeStack( p_parent_node );
                return VLC_ENOMEM;
            }

            p_node->p_parent = p_parent_node;

            /* only in leaf p nodes */
            if( !strcasecmp( psz_node_name, "p" ) || !strcasecmp( psz_node_name, "tt:p" ) )
            {
                if( MergeNodeWithParents( p_node ) != VLC_SUCCESS )
                {
                    ClearNodeStack( p_node );
                    return VLC_ENOMEM;
                }

                psz_text = NodeToStr( p_node );
                if( unlikely( psz_text == NULL ) )
                    goto error;

                if( p_node->psz_begin && p_node->psz_end )
                {
                    if( p_sys->i_subtitles >= i_max_sub )
                    {
                        i_max_sub += 500;
                        p_sys->subtitle = realloc_or_free( p_sys->subtitle,
                                sizeof( *p_sys->subtitle ) * i_max_sub );
                        if( unlikely( p_sys->subtitle == NULL ) )
                            goto error;
                    }
                    subtitle_t *p_subtitle = &p_sys->subtitle[p_sys->i_subtitles];

                    Convert_time( &p_subtitle->i_start, p_node->psz_begin );
                    Convert_time( &p_subtitle->i_stop, p_node->psz_end );

                    /*
                    * we drop metadata tag inside p tags to avoid conflict as
                    * they are unecessary.
                    */
                    i_type = xml_ReaderNextNode( p_sys->p_reader, &psz_node_name );

                    while( i_type > XML_READER_NONE && ( i_type != XML_READER_ENDELEM
                            || ( strcmp( psz_node_name, "p" ) && strcmp( psz_node_name, "tt:p" ) ) )
                          )
                    {
                        if( i_type == XML_READER_TEXT && psz_node_name != NULL )
                        {
                            psz_text = Append( psz_text, "%s", psz_node_name );
                            if( unlikely( psz_text == NULL ) )
                                goto error;
                        }
                        else if( i_type == XML_READER_STARTELEM )
                        {
                            node_t* p_subnode = calloc( 1, sizeof( *p_subnode ) );
                            if( unlikely( p_subnode == NULL ) )
                                goto error;

                            if( ReadAttrNode( p_sys->p_reader, p_subnode, psz_node_name) != VLC_SUCCESS )
                            {
                                ClearNode( p_subnode );
                                goto error;
                            }

                            char* psz_subnode_text = NodeToStr( p_subnode );
                            if( unlikely( psz_subnode_text == NULL ) )
                            {
                                ClearNode( p_subnode );
                                goto error;
                            }

                            psz_text = Append( psz_text, "%s", psz_subnode_text );
                            free( psz_subnode_text);
                            ClearNode( p_subnode );

                            if( unlikely( psz_text == NULL ) )
                                goto error;

                        }
                        else if( i_type == XML_READER_ENDELEM )
                        {
                            psz_text = Append( psz_text, " </%s>", psz_node_name );
                            if( unlikely( psz_text == NULL ) )
                                goto error;

                        }
                        i_type = xml_ReaderNextNode( p_sys->p_reader, &psz_node_name );
                    }
                    psz_text = Append( psz_text, "</%s>", psz_node_name );
                    if( unlikely( psz_text == NULL ) )
                        goto error;

                    p_subtitle->psz_text = psz_text;
                    p_sys->i_subtitles++;
                }
                else
                    goto error;
            }
            else
                p_parent_node = p_node;
        }
        /*  end tag after a p tag but inside the body */
        else if( i_type == XML_READER_ENDELEM && ( strcasecmp( psz_node_name, "body" ) && strcasecmp( psz_node_name, "tt:body" ) ) )
        {
            node_t* p_parent = p_parent_node->p_parent;
            ClearNode( p_parent_node );
            p_parent_node = p_parent;
        }
    } while( i_type != XML_READER_ENDELEM || ( strcasecmp( psz_node_name, "tt" ) && strcasecmp( psz_node_name, "tt:tt" ) ) );
    ClearNodeStack( p_parent_node );
    return VLC_SUCCESS;

error:
    free( psz_text );
    ClearNodeStack( p_parent_node );
    return VLC_EGENERIC;
}

static int Demux( demux_t* p_demux )
{
    demux_sys_t* p_sys = p_demux->p_sys;
    if( p_sys->i_subtitle >= p_sys->i_subtitles )
        return 0;

    while ( p_sys->i_subtitle < p_sys->i_subtitles &&
            p_sys->subtitle[p_sys->i_subtitle].i_start < p_sys->i_next_demux_time )
    {
        const subtitle_t* p_subtitle = &p_sys->subtitle[p_sys->i_subtitle];

        block_t* p_block = block_Alloc( strlen( p_subtitle->psz_text ) + 1 );
        if ( unlikely( p_block == NULL ) )
            return -1;

        p_block->i_dts =
        p_block->i_pts = VLC_TS_0 + p_subtitle->i_start;

        if( p_subtitle->i_stop >= 0 && p_subtitle->i_stop >= p_subtitle->i_start )
            p_block->i_length = p_subtitle->i_stop - p_subtitle->i_start;

        memcpy( p_block->p_buffer, p_subtitle->psz_text, p_block->i_buffer );

        es_out_Send( p_demux->out, p_sys->p_es, p_block );

        p_sys->i_subtitle++;
    }
    p_sys->i_next_demux_time = 0;
    return 1;
}

static void ParseHead( demux_t* p_demux )
{
    demux_sys_t* p_sys = p_demux->p_sys;
    char buff[1025];
    char* psz_head = NULL;
    size_t i_head_len = 0; // head tags size, in bytes
    size_t i_size; // allocated buffer size
    ssize_t i_read;

    // Rewind since the XML parser will have consumed the entire file.
    vlc_stream_Seek( p_demux->s, 0 );

    while ( ( i_read = vlc_stream_Read( p_demux->s, (void*)buff, 1024 ) ) > 0 )
    {
        ssize_t i_offset = -1;
        // Ensure we can use strstr
        buff[i_read] = 0;

        if ( psz_head == NULL )
        {
            // Seek to the opening <head> tag if we haven't seen it already
            const char* psz_head_begin = strstr( buff, "<head>" );
            if ( psz_head_begin == NULL )
                psz_head_begin = strstr( buff, "<tt:head>" );
            if ( psz_head_begin == NULL )
                continue;
            i_head_len = i_read - ( psz_head_begin - buff );
            i_size = i_head_len;
            psz_head = malloc( i_size * sizeof( *psz_head ) );
            if ( unlikely( psz_head == NULL ) )
                return;
            memcpy( psz_head, psz_head_begin, i_head_len );
            // Avoid copying the head tag again once we search for the end tag.
            i_offset = psz_head_begin - buff;
        }
        if ( psz_head != NULL )
        {
            size_t i_end_tag_len = strlen( "</head>" );
            // Or copy until the end of the head tag once we've seen the opening one
            const char* psz_end_head = strstr( buff, "</head>" );
            if ( psz_end_head == NULL )
            {
                psz_end_head = strstr( buff, "</tt:head>" );
                i_end_tag_len = strlen( "</tt:head>" );
            }
            // Check if we need to extend the buffer first
            size_t i_to_copy = i_read;
            if ( psz_end_head != NULL )
                i_to_copy = psz_end_head - buff + i_end_tag_len;
            if ( i_size < i_head_len + i_to_copy )
            {
                i_size = __MAX(i_size * 2, i_head_len + i_to_copy);
                psz_head = realloc_or_free( psz_head, i_size );
                if ( unlikely( psz_head == NULL ) )
                    return;
            }

            if ( psz_end_head == NULL )
            {
                // If we already copied the begin tag, we don't need to append this again.
                if ( i_offset != -1 )
                    continue;
                // Otherwise, simply append the entire buffer
                memcpy( psz_head + i_head_len, buff, i_to_copy );
                i_head_len += i_to_copy;
            }
            else
            {
                if ( i_offset == -1 )
                {
                    memcpy( psz_head + i_head_len, buff, i_to_copy );
                    i_head_len += i_to_copy;
                }
                else
                {
                    // If the buffer we originally copied already contains the end tag, no need to copy again
                    // Though if we already have the </head> in our buffer, we need to adjust the total size
                    i_head_len = psz_end_head - buff + i_end_tag_len + i_offset;
                }
                p_sys->psz_head = psz_head;
                p_sys->i_head_len = i_head_len;
                break;
            }
        }
    }
}

static int Open( vlc_object_t* p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    p_demux->p_sys = p_sys = calloc( 1, sizeof( *p_sys ) );
    if ( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;

    uint8_t *p_peek;
    ssize_t i_peek = vlc_stream_Peek( p_demux->s, (const uint8_t **) &p_peek, 2048 );

    if( unlikely( i_peek <= 0 ) )
    {
        Close( p_demux );
        return VLC_EGENERIC;
    }

    stream_t *p_probestream = vlc_stream_MemoryNew( p_demux->s, p_peek, i_peek, true );
    if( unlikely( !p_probestream ) )
    {
        Close( p_demux );
        return VLC_EGENERIC;
    }

    p_sys->p_xml = xml_Create( p_demux );
    if ( !p_sys->p_xml )
    {
        Close( p_demux );
        vlc_stream_Delete( p_probestream );
        return VLC_EGENERIC;
    }
    p_sys->p_reader = xml_ReaderCreate( p_sys->p_xml, p_probestream );
    if ( !p_sys->p_reader )
    {
        Close( p_demux );
        vlc_stream_Delete( p_probestream );
        return VLC_EGENERIC;
    }

    const int i_flags = p_sys->p_reader->obj.flags;
    p_sys->p_reader->obj.flags |= OBJECT_FLAGS_QUIET;
    const char* psz_name;
    int i_type = xml_ReaderNextNode( p_sys->p_reader, &psz_name );
    p_sys->p_reader->obj.flags = i_flags;
    if ( i_type != XML_READER_STARTELEM || ( strcmp( psz_name, "tt" ) && strcmp( psz_name, "tt:tt" ) ) )
    {
        Close( p_demux );
        vlc_stream_Delete( p_probestream );
        return VLC_EGENERIC;
    }

    p_sys->p_reader = xml_ReaderReset( p_sys->p_reader, p_demux->s );
    vlc_stream_Delete( p_probestream );
    if ( !p_sys->p_reader )
    {
        Close( p_demux );
        return VLC_EGENERIC;
    }

    if ( ReadTTML( p_demux ) != VLC_SUCCESS )
    {
        Close( p_demux );
        return VLC_EGENERIC;
    }
    if ( p_sys->b_has_head )
        ParseHead( p_demux );

    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;

    es_format_t fmt;
    es_format_Init( &fmt, SPU_ES, VLC_CODEC_TTML );
    if ( p_sys->i_head_len > 0 )
    {
        fmt.p_extra = p_sys->psz_head;
        fmt.i_extra = p_sys->i_head_len;
    }
    p_sys->p_es = es_out_Add( p_demux->out, &fmt );
    es_format_Clean( &fmt );

    if ( p_sys->i_subtitles > 0 )
        p_sys->i_length = p_sys->subtitle[ p_sys->i_subtitles - 1 ].i_stop;
    else
        p_sys->i_length = 0;

    return VLC_SUCCESS;
}

static void Close( demux_t* p_demux )
{
    demux_sys_t* p_sys = p_demux->p_sys;
    if ( p_sys->p_es )
        es_out_Del( p_demux->out, p_sys->p_es );
    if ( p_sys->p_reader )
        xml_ReaderDelete( p_sys->p_reader );
    if ( p_sys->p_xml )
        xml_Delete( p_sys->p_xml );
    for ( int i = 0; i < p_sys->i_subtitles; ++i )
    {
        free( p_sys->subtitle[i].psz_text );
    }
    free( p_sys->subtitle );
    free( p_sys );
}
