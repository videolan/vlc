/*****************************************************************************
 * ttml.c : TTML subtitles demux
 *****************************************************************************
 * Copyright (C) 2015-2017 VLC authors and VideoLAN
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
#include <vlc_demux.h>
#include <vlc_xml.h>
#include <vlc_strings.h>
#include <vlc_memory.h>
#include <vlc_memstream.h>
#include <vlc_es_out.h>

#include <assert.h>
#include <stdlib.h>

#include "../codec/ttml/ttml.h"

//#define TTML_DEMUX_DEBUG

struct demux_sys_t
{
    xml_t*          p_xml;
    xml_reader_t*   p_reader;
    es_out_id_t*    p_es;
    int64_t         i_next_demux_time;
    bool            b_slave;
    bool            b_first_time;

    tt_node_t         *p_rootnode;

    tt_timings_t    temporal_extent;

    /*
     * All timings are stored unique and ordered.
     * Being begin or end times of sub sequence,
     * we use them as 'point of change' for output filtering.
    */
    struct
    {
        int64_t *p_array;
        size_t   i_count;
        size_t   i_current;
    } times;
};

static char *tt_genTiming( int64_t i_time )
{
    if( i_time < 0 )
        i_time = 0;
    char *psz;
    unsigned h, m, s, f;
    f = (i_time % CLOCK_FREQ) / 10000;
    i_time /= CLOCK_FREQ;
    h = i_time / 3600;
    m = (i_time - h) / 3600;
    s = (i_time - h - m);

    if( asprintf( &psz, "%2.2u:%2.2u:%2.2u.%2.2u",
                        h, m, s, f ) < 0 )
        psz = NULL;

    return psz;
}

static void tt_node_AttributesToText( struct vlc_memstream *p_stream, const tt_node_t* p_node )
{
    const vlc_dictionary_t* p_attr_dict = &p_node->attr_dict;
    for( int i = 0; i < p_attr_dict->i_size; ++i )
    {
        for ( vlc_dictionary_entry_t* p_entry = p_attr_dict->p_entries[i];
                                      p_entry != NULL; p_entry = p_entry->p_next )
        {
            const char *psz_value = NULL;
            char *psz_alloc = NULL;

            if( !strcmp(p_entry->psz_key, "begin") )
            {
                if( p_node->timings.i_begin != -1 )
                    psz_value = psz_alloc = tt_genTiming( p_node->timings.i_begin );
            }
            else if( !strcmp(p_entry->psz_key, "end") )
            {
                if( p_node->timings.i_end != -1 )
                    psz_value = psz_alloc = tt_genTiming( p_node->timings.i_end );
            }
            else if( !strcmp(p_entry->psz_key, "dur") )
            {
                /* remove */
                continue;
            }
            else
            {
                psz_value = (char const*)p_entry->p_value;
            }

            if( psz_value == NULL )
                continue;

            vlc_memstream_printf( p_stream, " %s=\"%s\"",
                                  p_entry->psz_key, psz_value );

            free( psz_alloc );
        }
    }
}

static void tt_node_ToText( struct vlc_memstream *p_stream, const tt_basenode_t *p_basenode,
                            int64_t i_playbacktime )
{
    if( p_basenode->i_type == TT_NODE_TYPE_ELEMENT )
    {
        const tt_node_t *p_node = (const tt_node_t *) p_basenode;

        if( i_playbacktime != -1 &&
           !tt_timings_Contains( &p_node->timings, i_playbacktime ) )
            return;

        vlc_memstream_putc( p_stream, '<' );
        vlc_memstream_puts( p_stream, p_node->psz_node_name );

        tt_node_AttributesToText( p_stream, p_node );

        if( tt_node_HasChild( p_node ) )
        {
            vlc_memstream_putc( p_stream, '>' );

#ifdef TTML_DEMUX_DEBUG
            vlc_memstream_printf( p_stream, "<!-- starts %ld ends %ld -->",
                                  p_node->timings.i_begin,
                                  p_node->timings.i_end );
#endif

            for( const tt_basenode_t *p_child = p_node->p_child;
                                   p_child; p_child = p_child->p_next )
            {
                tt_node_ToText( p_stream, p_child, i_playbacktime );
            }

            vlc_memstream_printf( p_stream, "</%s>", p_node->psz_node_name );
        }
        else
            vlc_memstream_puts( p_stream, "/>" );
    }
    else
    {
        const tt_textnode_t *p_textnode = (const tt_textnode_t *) p_basenode;
        vlc_memstream_puts( p_stream, p_textnode->psz_text );
    }
}

static int Control( demux_t* p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int64_t *pi64, i64;
    double *pf, f;
    bool b;

    switch( i_query )
    {
        case DEMUX_CAN_SEEK:
            *va_arg( args, bool * ) = true;
            return VLC_SUCCESS;
        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = p_sys->i_next_demux_time;
            return VLC_SUCCESS;
        case DEMUX_SET_TIME:
            i64 = (int64_t)va_arg( args, int64_t );
            if( p_sys->times.i_count )
            {
                size_t i_index = tt_timings_FindLowerIndex( p_sys->times.p_array,
                                                            p_sys->times.i_count, i64, &b );
                p_sys->times.i_current = i_index;
                p_sys->b_first_time = true;
                return VLC_SUCCESS;
            }
            break;
        case DEMUX_SET_NEXT_DEMUX_TIME:
            i64 = (int64_t)va_arg( args, int64_t );
            p_sys->i_next_demux_time = i64;
            p_sys->b_slave = true;
            return VLC_SUCCESS;
        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_sys->times.i_count )
            {
                *pi64 = p_sys->times.p_array[p_sys->times.i_count - 1] -
                        p_sys->temporal_extent.i_begin;
                return VLC_SUCCESS;
            }
            break;
        case DEMUX_GET_POSITION:
            pf = (double*)va_arg( args, double * );
            if( p_sys->times.i_current >= p_sys->times.i_count )
            {
                *pf = 1.0;
            }
            else if( p_sys->times.i_count > 0 )
            {
                *pf = (double) p_sys->i_next_demux_time /
                      (p_sys->times.p_array[p_sys->times.i_count - 1] + 0.5);
            }
            else
            {
                *pf = 0.0;
            }
            return VLC_SUCCESS;
        case DEMUX_SET_POSITION:
            f = (double)va_arg( args, double );
            if( p_sys->times.i_count )
            {
                i64 = f * p_sys->times.p_array[p_sys->times.i_count - 1];
                size_t i_index = tt_timings_FindLowerIndex( p_sys->times.p_array,
                                                            p_sys->times.i_count, i64, &b );
                p_sys->times.i_current = i_index;
                p_sys->b_first_time = true;
                return VLC_SUCCESS;
            }
            break;
        case DEMUX_GET_PTS_DELAY:
        case DEMUX_GET_FPS:
        case DEMUX_GET_META:
        case DEMUX_GET_ATTACHMENTS:
        case DEMUX_GET_TITLE_INFO:
        case DEMUX_HAS_UNSUPPORTED_META:
        case DEMUX_CAN_RECORD:
        default:
            break;
    }

    return VLC_EGENERIC;
}

static int ReadTTML( demux_t* p_demux )
{
    demux_sys_t* p_sys = p_demux->p_sys;
    const char* psz_node_name;

    do
    {
        int i_type = xml_ReaderNextNode( p_sys->p_reader, &psz_node_name );
        bool b_empty = xml_ReaderIsEmptyElement( p_sys->p_reader );

        if( i_type <= XML_READER_NONE )
            break;

        switch(i_type)
        {
            default:
                break;

            case XML_READER_STARTELEM:
                if( tt_node_NameCompare( psz_node_name, "tt" ) ||
                    p_sys->p_rootnode != NULL )
                    return VLC_EGENERIC;

                p_sys->p_rootnode = tt_node_New( p_sys->p_reader, NULL, psz_node_name );
                if( b_empty )
                    break;
                if( !p_sys->p_rootnode ||
                    tt_nodes_Read( p_sys->p_reader, p_sys->p_rootnode ) != VLC_SUCCESS )
                    return VLC_EGENERIC;
                break;

            case XML_READER_ENDELEM:
                if( !p_sys->p_rootnode ||
                    tt_node_NameCompare( psz_node_name, p_sys->p_rootnode->psz_node_name ) )
                    return VLC_EGENERIC;
                break;
        }

    } while( 1 );

    if( p_sys->p_rootnode == NULL )
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

static int Demux( demux_t* p_demux )
{
    demux_sys_t* p_sys = p_demux->p_sys;

    /* Last one must be an end time */
    while( p_sys->times.i_current + 1 < p_sys->times.i_count &&
           p_sys->times.p_array[p_sys->times.i_current] <= p_sys->i_next_demux_time )
    {
        const int64_t i_playbacktime = p_sys->times.p_array[p_sys->times.i_current];
        const int64_t i_playbackendtime = p_sys->times.p_array[p_sys->times.i_current + 1] - 1;

        if ( !p_sys->b_slave && p_sys->b_first_time )
        {
            es_out_Control( p_demux->out, ES_OUT_SET_PCR, VLC_TS_0 + i_playbacktime );
            p_sys->b_first_time = false;
        }

        struct vlc_memstream stream;

        if( vlc_memstream_open( &stream ) )
            return VLC_DEMUXER_EGENERIC;

        tt_node_ToText( &stream, (tt_basenode_t *) p_sys->p_rootnode, i_playbacktime );

        if( vlc_memstream_close( &stream ) == VLC_SUCCESS )
        {
            block_t* p_block = block_heap_Alloc( stream.ptr, stream.length );
            if( p_block )
            {
                p_block->i_dts =
                    p_block->i_pts = VLC_TS_0 + i_playbacktime;
                p_block->i_length = i_playbackendtime - i_playbacktime;

                es_out_Send( p_demux->out, p_sys->p_es, p_block );
            }
        }

        p_sys->times.i_current++;
    }

    if ( !p_sys->b_slave )
    {
        es_out_Control( p_demux->out, ES_OUT_SET_PCR, VLC_TS_0 + p_sys->i_next_demux_time );
        p_sys->i_next_demux_time += CLOCK_FREQ / 8;
    }

    if( p_sys->times.i_current + 1 >= p_sys->times.i_count )
        return VLC_DEMUXER_EOF;

    return VLC_DEMUXER_SUCCESS;
}

int OpenDemux( vlc_object_t* p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;

    uint8_t *p_peek;
    ssize_t i_peek = vlc_stream_Peek( p_demux->s, (const uint8_t **) &p_peek, 2048 );
    if( unlikely( i_peek <= 0 ) )
        return VLC_EGENERIC;

    /* Simplified probing. Valid TTML must have a namespace declaration */
    const char *psz_tt = strnstr( (const char*) p_peek, "tt ", i_peek );
    if( !psz_tt || (ptrdiff_t)psz_tt == (ptrdiff_t)p_peek ||
        (psz_tt[-1] != ':' && psz_tt[-1] != '<') )
    {
        return VLC_EGENERIC;
    }
    else
    {
        const char *psz_ns = strnstr( (const char*) p_peek, "=\"http://www.w3.org/ns/ttml\"",
                                      i_peek -( (ptrdiff_t)psz_tt - (ptrdiff_t)p_peek ) );
        if( !psz_ns )
            return VLC_EGENERIC;
    }

    p_demux->p_sys = p_sys = calloc( 1, sizeof( *p_sys ) );
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;

    p_sys->b_first_time = true;
    p_sys->temporal_extent.i_type = TT_TIMINGS_PARALLEL;
    p_sys->temporal_extent.i_begin = 0;
    p_sys->temporal_extent.i_end = -1;
    p_sys->temporal_extent.i_dur = -1;

    p_sys->p_xml = xml_Create( p_demux );
    if( !p_sys->p_xml )
        goto error;

    p_sys->p_reader = xml_ReaderCreate( p_sys->p_xml, p_demux->s );
    if( !p_sys->p_reader )
        goto error;

#ifndef TTML_DEMUX_DEBUG
    p_sys->p_reader->obj.flags |= OBJECT_FLAGS_QUIET;
#endif

    if( ReadTTML( p_demux ) != VLC_SUCCESS )
        goto error;

    tt_timings_Resolve( (tt_basenode_t *) p_sys->p_rootnode, &p_sys->temporal_extent,
                        &p_sys->times.p_array, &p_sys->times.i_count );

#ifdef TTML_DEMUX_DEBUG
    {
        struct vlc_memstream stream;

        if( vlc_memstream_open( &stream ) )
            goto error;

        tt_node_ToText( &stream, (tt_basenode_t*)p_sys->p_rootnode, -1 );

        vlc_memstream_putc( &stream, '\0' );

        if( vlc_memstream_close( &stream ) == VLC_SUCCESS )
        {
            msg_Dbg( p_demux, "%s", stream.ptr );
            free( stream.ptr );
        }
    }
#endif

    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;

    es_format_t fmt;
    es_format_Init( &fmt, SPU_ES, VLC_CODEC_TTML );
    p_sys->p_es = es_out_Add( p_demux->out, &fmt );
    if( !p_sys->p_es )
        goto error;

    es_format_Clean( &fmt );

    return VLC_SUCCESS;

error:
    CloseDemux( p_demux );

    return VLC_EGENERIC;
}

void CloseDemux( demux_t* p_demux )
{
    demux_sys_t* p_sys = p_demux->p_sys;

    if( p_sys->p_rootnode )
        tt_node_RecursiveDelete( p_sys->p_rootnode );

    if( p_sys->p_es )
        es_out_Del( p_demux->out, p_sys->p_es );

    if( p_sys->p_reader )
        xml_ReaderDelete( p_sys->p_reader );

    if( p_sys->p_xml )
        xml_Delete( p_sys->p_xml );

    free( p_sys->times.p_array );

    free( p_sys );
}
