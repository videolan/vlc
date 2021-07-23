/*****************************************************************************
 * genttml.c : TTML formatter
 *****************************************************************************
 * Copyright (C) 2015-2018 VLC authors and VideoLAN
 * Copyright (C) 2017-2018 VideoLabs
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

//#define TTML_GEN_DEBUG

#include <vlc_common.h>
#include <vlc_strings.h>

#include <assert.h>
#include <stdlib.h>
#include <ctype.h>

#include "ttml.h"

char *tt_genTiming( tt_time_t t )
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

void tt_node_AttributesToText( struct vlc_memstream *p_stream, const tt_node_t* p_node )
{
    bool b_timed_node = false;
    const vlc_dictionary_t* p_attr_dict = &p_node->attr_dict;
    for (size_t i = 0; i < p_attr_dict->i_size; ++i)
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

void tt_node_ToText( struct vlc_memstream *p_stream, const tt_basenode_t *p_basenode,
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
