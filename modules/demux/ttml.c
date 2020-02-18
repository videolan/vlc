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
#include <vlc_memstream.h>
#include <vlc_es_out.h>
#include <vlc_charset.h>          /* FromCharset */

#include <assert.h>
#include <stdlib.h>
#include <ctype.h>

#include "../codec/ttml/ttml.h"

//#define TTML_DEMUX_DEBUG

typedef struct
{
    xml_t*          p_xml;
    xml_reader_t*   p_reader;
    es_out_id_t*    p_es;
    vlc_tick_t      i_next_demux_time;
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
        tt_time_t *p_array;
        size_t   i_count;
        size_t   i_current;
    } times;
} demux_sys_t;

static char *tt_genTiming( tt_time_t t )
{
    if( !tt_time_Valid( &t ) )
        t.base = 0;
    unsigned f = t.base % CLOCK_FREQ;
    t.base /= CLOCK_FREQ;
    unsigned h = t.base / 3600;
    unsigned m = t.base % 3600 / 60;
    unsigned s = t.base % 60;

    int i_ret;
    char *psz;
    if( f )
    {
        const char *lz = "000000";
        const char *psz_lz = &lz[6];
        /* add leading zeroes */
        for( unsigned i=10*f; i<CLOCK_FREQ; i *= 10 )
            psz_lz--;
        /* strip trailing zeroes */
        for( ; f > 0 && (f % 10) == 0; f /= 10 );
        i_ret = asprintf( &psz, "%02u:%02u:%02u.%s%u",
                                 h, m, s, psz_lz, f );
    }
    else if( t.frames )
    {
        i_ret = asprintf( &psz, "%02u:%02u:%02u:%s%u",
                                 h, m, s, t.frames < 10 ? "0" : "", t.frames );
    }
    else
    {
        i_ret = asprintf( &psz, "%02u:%02u:%02u",
                                 h, m, s );
    }

    return i_ret < 0 ? NULL : psz;
}

static void tt_MemstreamPutEntities( struct vlc_memstream *p_stream, const char *psz )
{
    char *psz_entities = vlc_xml_encode( psz );
    if( psz_entities )
    {
        vlc_memstream_puts( p_stream, psz_entities );
        free( psz_entities );
    }
}

static void tt_node_AttributesToText( struct vlc_memstream *p_stream, const tt_node_t* p_node )
{
    bool b_timed_node = false;
    const vlc_dictionary_t* p_attr_dict = &p_node->attr_dict;
    for( int i = 0; i < p_attr_dict->i_size; ++i )
    {
        for ( vlc_dictionary_entry_t* p_entry = p_attr_dict->p_entries[i];
                                      p_entry != NULL; p_entry = p_entry->p_next )
        {
            const char *psz_value = NULL;

            if( !strcmp(p_entry->psz_key, "begin") ||
                !strcmp(p_entry->psz_key, "end") ||
                !strcmp(p_entry->psz_key, "dur") )
            {
                b_timed_node = true;
                /* will remove duration */
                continue;
            }
            else if( !strcmp(p_entry->psz_key, "timeContainer") )
            {
                /* also remove sequential timings info (all abs now) */
                continue;
            }
            else
            {
                psz_value = p_entry->p_value;
            }

            if( psz_value == NULL )
                continue;

            vlc_memstream_printf( p_stream, " %s=\"", p_entry->psz_key );
            tt_MemstreamPutEntities( p_stream, psz_value );
            vlc_memstream_putc( p_stream, '"' );
        }
    }

    if( b_timed_node )
    {
        if( tt_time_Valid( &p_node->timings.begin ) )
        {
            char *psz = tt_genTiming( p_node->timings.begin );
            vlc_memstream_printf( p_stream, " begin=\"%s\"", psz );
            free( psz );
        }

        if( tt_time_Valid( &p_node->timings.end ) )
        {
            char *psz = tt_genTiming( p_node->timings.end );
            vlc_memstream_printf( p_stream, " end=\"%s\"", psz );
            free( psz );
        }
    }
}

static void tt_node_ToText( struct vlc_memstream *p_stream, const tt_basenode_t *p_basenode,
                            const tt_time_t *playbacktime )
{
    if( p_basenode->i_type == TT_NODE_TYPE_ELEMENT )
    {
        const tt_node_t *p_node = (const tt_node_t *) p_basenode;

        if( tt_time_Valid( playbacktime ) &&
           !tt_timings_Contains( &p_node->timings, playbacktime ) )
            return;

        vlc_memstream_putc( p_stream, '<' );
        tt_MemstreamPutEntities( p_stream, p_node->psz_node_name );

        tt_node_AttributesToText( p_stream, p_node );

        if( tt_node_HasChild( p_node ) )
        {
            vlc_memstream_putc( p_stream, '>' );

#ifdef TTML_DEMUX_DEBUG
            vlc_memstream_printf( p_stream, "<!-- starts %ld ends %ld -->",
                                  tt_time_Convert( &p_node->timings.begin ),
                                  tt_time_Convert( &p_node->timings.end ) );
#endif

            for( const tt_basenode_t *p_child = p_node->p_child;
                                   p_child; p_child = p_child->p_next )
            {
                tt_node_ToText( p_stream, p_child, playbacktime );
            }

            vlc_memstream_puts( p_stream, "</" );
            tt_MemstreamPutEntities( p_stream, p_node->psz_node_name );
            vlc_memstream_putc( p_stream, '>' );
        }
        else
            vlc_memstream_puts( p_stream, "/>" );
    }
    else
    {
        const tt_textnode_t *p_textnode = (const tt_textnode_t *) p_basenode;
        tt_MemstreamPutEntities( p_stream, p_textnode->psz_text );
    }
}

static int Control( demux_t* p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    vlc_tick_t i64;
    double *pf, f;
    bool b;

    switch( i_query )
    {
        case DEMUX_CAN_SEEK:
            *va_arg( args, bool * ) = true;
            return VLC_SUCCESS;
        case DEMUX_GET_TIME:
            *va_arg( args, vlc_tick_t * ) = p_sys->i_next_demux_time;
            return VLC_SUCCESS;
        case DEMUX_SET_TIME:
            if( p_sys->times.i_count )
            {
                tt_time_t t = tt_time_Create( va_arg( args, vlc_tick_t ) - VLC_TICK_0 );
                size_t i_index = tt_timings_FindLowerIndex( p_sys->times.p_array,
                                                            p_sys->times.i_count, t, &b );
                p_sys->times.i_current = i_index;
                p_sys->b_first_time = true;
                return VLC_SUCCESS;
            }
            break;
        case DEMUX_SET_NEXT_DEMUX_TIME:
            p_sys->i_next_demux_time = va_arg( args, vlc_tick_t );
            p_sys->b_slave = true;
            return VLC_SUCCESS;
        case DEMUX_GET_LENGTH:
            if( p_sys->times.i_count )
            {
                tt_time_t t = tt_time_Sub( p_sys->times.p_array[p_sys->times.i_count - 1],
                                           p_sys->temporal_extent.begin );
                *va_arg( args, vlc_tick_t * ) = tt_time_Convert( &t );
                return VLC_SUCCESS;
            }
            break;
        case DEMUX_GET_POSITION:
            pf = va_arg( args, double * );
            if( p_sys->times.i_current >= p_sys->times.i_count )
            {
                *pf = 1.0;
            }
            else if( p_sys->times.i_count > 0 )
            {
                i64 = tt_time_Convert( &p_sys->times.p_array[p_sys->times.i_count - 1] );
                *pf = (double) p_sys->i_next_demux_time / (i64 + VLC_TICK_FROM_MS(500));
            }
            else
            {
                *pf = 0.0;
            }
            return VLC_SUCCESS;
        case DEMUX_SET_POSITION:
            f = va_arg( args, double );
            if( p_sys->times.i_count )
            {
                i64 = f * tt_time_Convert( &p_sys->times.p_array[p_sys->times.i_count - 1] );
                tt_time_t t = tt_time_Create( i64 );
                size_t i_index = tt_timings_FindLowerIndex( p_sys->times.p_array,
                                                            p_sys->times.i_count, t, &b );
                p_sys->times.i_current = i_index;
                p_sys->b_first_time = true;
                return VLC_SUCCESS;
            }
            break;
        case DEMUX_CAN_PAUSE:
        case DEMUX_SET_PAUSE_STATE:
        case DEMUX_CAN_CONTROL_PACE:
            return demux_vaControlHelper( p_demux->s, 0, -1, 0, 1, i_query, args );

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
           tt_time_Convert( &p_sys->times.p_array[p_sys->times.i_current] ) <= p_sys->i_next_demux_time )
    {
        const vlc_tick_t i_playbacktime =
                tt_time_Convert( &p_sys->times.p_array[p_sys->times.i_current] );
        const vlc_tick_t i_playbackendtime =
                tt_time_Convert( &p_sys->times.p_array[p_sys->times.i_current + 1] ) - 1;

        if ( !p_sys->b_slave && p_sys->b_first_time )
        {
            es_out_SetPCR( p_demux->out, VLC_TICK_0 + i_playbacktime );
            p_sys->b_first_time = false;
        }

        struct vlc_memstream stream;

        if( vlc_memstream_open( &stream ) )
            return VLC_DEMUXER_EGENERIC;

        tt_node_ToText( &stream, (tt_basenode_t *) p_sys->p_rootnode,
                        &p_sys->times.p_array[p_sys->times.i_current] );

        if( vlc_memstream_close( &stream ) == VLC_SUCCESS )
        {
            block_t* p_block = block_heap_Alloc( stream.ptr, stream.length );
            if( p_block )
            {
                p_block->i_dts =
                    p_block->i_pts = VLC_TICK_0 + i_playbacktime;
                p_block->i_length = i_playbackendtime - i_playbacktime;

                es_out_Send( p_demux->out, p_sys->p_es, p_block );
            }
        }

        p_sys->times.i_current++;
    }

    if ( !p_sys->b_slave )
    {
        es_out_SetPCR( p_demux->out, VLC_TICK_0 + p_sys->i_next_demux_time );
        p_sys->i_next_demux_time += VLC_TICK_FROM_MS(125);
    }

    if( p_sys->times.i_current + 1 >= p_sys->times.i_count )
        return VLC_DEMUXER_EOF;

    return VLC_DEMUXER_SUCCESS;
}

int tt_OpenDemux( vlc_object_t* p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;

    const uint8_t *p_peek;
    ssize_t i_peek = vlc_stream_Peek( p_demux->s, &p_peek, 2048 );
    if( unlikely( i_peek <= 32 ) )
        return VLC_EGENERIC;

    const char *psz_xml = (const char *) p_peek;
    size_t i_xml  = i_peek;

    /* Try to probe without xml module/loading the full document */
    char *psz_alloc = NULL;
    switch( GetQWBE(p_peek) )
    {
        /* See RFC 3023 Part 4 */
        case UINT64_C(0xFFFE3C003F007800): /* UTF16 BOM<? */
        case UINT64_C(0xFFFE3C003F007400): /* UTF16 BOM<t */
        case UINT64_C(0xFEFF003C003F0078): /* UTF16 BOM<? */
        case UINT64_C(0xFEFF003C003F0074): /* UTF16 BOM<t */
            psz_alloc = FromCharset( "UTF-16", p_peek, i_peek );
            break;
        case UINT64_C(0x3C003F0078006D00): /* UTF16-LE <?xm */
        case UINT64_C(0x3C003F0074007400): /* UTF16-LE <tt */
            psz_alloc = FromCharset( "UTF-16LE", p_peek, i_peek );
            break;
        case UINT64_C(0x003C003F0078006D): /* UTF16-BE <?xm */
        case UINT64_C(0x003C003F00740074): /* UTF16-BE <tt */
            psz_alloc = FromCharset( "UTF-16BE", p_peek, i_peek );
            break;
        case UINT64_C(0xEFBBBF3C3F786D6C): /* UTF8 BOM<?xml */
        case UINT64_C(0x3C3F786D6C207665): /* UTF8 <?xml ve */
        case UINT64_C(0xEFBBBF3C74742078): /* UTF8 BOM<tt x*/
            break;
        default:
            if(GetDWBE(p_peek) != UINT32_C(0x3C747420)) /* tt node without xml document marker */
                return VLC_EGENERIC;
    }

    if( psz_alloc )
    {
        psz_xml = psz_alloc;
        i_xml = strlen( psz_alloc );
    }

    /* Simplified probing. Valid TTML must have a namespace declaration */
    const char *psz_tt = strnstr( psz_xml, "tt", i_xml );
    if( !psz_tt || psz_tt == psz_xml ||
        ((size_t)(&psz_tt[2] - (const char*)p_peek)) == i_xml || isalpha(psz_tt[2]) ||
        (psz_tt[-1] != ':' && psz_tt[-1] != '<') )
    {
        free( psz_alloc );
        return VLC_EGENERIC;
    }
    else
    {
        const char * const rgsz[] =
        {
            "=\"http://www.w3.org/ns/ttml\"",
            "=\"http://www.w3.org/2004/11/ttaf1\"",
            "=\"http://www.w3.org/2006/04/ttaf1\"",
            "=\"http://www.w3.org/2006/10/ttaf1\"",
        };
        const char *psz_ns = NULL;
        for( size_t i=0; i<ARRAY_SIZE(rgsz) && !psz_ns; i++ )
        {
            psz_ns = strnstr( psz_xml, rgsz[i],
                              i_xml - (psz_tt - psz_xml) );
        }
        free( psz_alloc );
        if( !psz_ns )
            return VLC_EGENERIC;
    }

    p_demux->p_sys = p_sys = calloc( 1, sizeof( *p_sys ) );
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;

    p_sys->b_first_time = true;
    p_sys->temporal_extent.i_type = TT_TIMINGS_PARALLEL;
    tt_time_Init( &p_sys->temporal_extent.begin );
    tt_time_Init( &p_sys->temporal_extent.end );
    tt_time_Init( &p_sys->temporal_extent.dur );
    p_sys->temporal_extent.begin.base = 0;

    p_sys->p_xml = xml_Create( p_demux );
    if( !p_sys->p_xml )
        goto error;

    p_sys->p_reader = xml_ReaderCreate( p_sys->p_xml, p_demux->s );
    if( !p_sys->p_reader )
        goto error;

#ifndef TTML_DEMUX_DEBUG
    p_sys->p_reader->obj.logger = NULL;
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

        tt_time_t t;
        tt_time_Init( &t );
        tt_node_ToText( &stream, (tt_basenode_t*)p_sys->p_rootnode, &t /* invalid */ );

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
    fmt.i_id = 0;
    p_sys->p_es = es_out_Add( p_demux->out, &fmt );
    if( !p_sys->p_es )
        goto error;

    es_format_Clean( &fmt );

    return VLC_SUCCESS;

error:
    tt_CloseDemux( p_this );

    return VLC_EGENERIC;
}

void tt_CloseDemux( vlc_object_t* p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
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
