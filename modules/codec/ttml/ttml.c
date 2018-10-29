/*****************************************************************************
 * ttml.c : TTML helpers
 *****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
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
#include <vlc_xml.h>
#include <vlc_strings.h>
#include <vlc_charset.h>

#include <assert.h>
#include <stdlib.h>
#include <limits.h>

#include "ttml.h"

#define ALIGN_TEXT N_("Subtitle justification")
#define ALIGN_LONGTEXT N_("Set the justification of subtitles")

/*****************************************************************************
 * Modules descriptor.
 *****************************************************************************/

vlc_module_begin ()
    set_capability( "spu decoder", 10 )
    set_shortname( N_("TTML decoder"))
    set_description( N_("TTML subtitles decoder") )
    set_callbacks( tt_OpenDecoder, tt_CloseDecoder )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_SCODEC )
    add_integer( "ttml-align", 0, ALIGN_TEXT, ALIGN_LONGTEXT, false )

    add_submodule()
        set_shortname( N_("TTML") )
        set_description( N_("TTML demuxer") )
        set_capability( "demux", 11 )
        set_category( CAT_INPUT )
        set_subcategory( SUBCAT_INPUT_DEMUX )
        set_callbacks( tt_OpenDemux, tt_CloseDemux )
        add_shortcut( "ttml" )

vlc_module_end ()


int tt_node_NameCompare( const char* psz_tagname, const char* psz_pattern )
{
    if( !strncasecmp( "tt:", psz_tagname, 3 ) )
        psz_tagname += 3;
    return strcasecmp( psz_tagname, psz_pattern );
}

bool tt_node_HasChild( const tt_node_t *p_node )
{
    return p_node->p_child;
}

static inline bool tt_ScanReset( unsigned *a, unsigned *b, unsigned *c,
                                 char *d,  unsigned *e )
{
    *a = *b = *c = *d = *e = 0;
    return false;
}

static tt_time_t tt_ParseTime( const char *s )
{
    tt_time_t t = {-1, 0};
    unsigned h1 = 0, m1 = 0, s1 = 0, d1 = 0;
    char c = 0;

    /* Clock time */
    if( sscanf( s, "%u:%2u:%2u%c%u",     &h1, &m1, &s1, &c, &d1 ) == 5 ||
                           tt_ScanReset( &h1, &m1, &s1, &c, &d1 )      ||
        sscanf( s, "%u:%2u:%2u",         &h1, &m1, &s1          ) == 3 ||
                           tt_ScanReset( &h1, &m1, &s1, &c, &d1 ) )
    {
        t.base = vlc_tick_from_sec(h1 * 3600 + m1 * 60 + s1);
        if( c == '.' && d1 > 0 )
        {
            unsigned i_den = 1;
            for( const char *p = strchr( s, '.' ) + 1; *p && (i_den < UINT_MAX / 10); p++ )
                i_den *= 10;
            t.base += vlc_tick_from_samples(d1, i_den);
        }
        else if( c == ':' )
        {
            t.frames = d1;
        }
    }
    else /* Offset Time */
    {
        char *psz_end = (char *) s;
        double v = us_strtod( s, &psz_end );
        if( psz_end != s && *psz_end )
        {
            if( *psz_end == 'm' )
            {
                if( *(psz_end + 1) == 's' )
                    t.base = VLC_TICK_FROM_MS(v);
                else
                    t.base = vlc_tick_from_sec(60 * v);
            }
            else if( *psz_end == 's' )
            {
                t.base = vlc_tick_from_sec(v);
            }
            else if( *psz_end == 'h' )
            {
                t.base = vlc_tick_from_sec(v * 3600);
            }
            else if( *psz_end == 'f' )
            {
                t.base = 0;
                t.frames = v;
            }
            //else if( *psz_end == 't' );
        }
    }

    return t;
}

bool tt_timings_Contains( const tt_timings_t *p_range, const tt_time_t *time )
{
    if( tt_time_Valid( &p_range->end ) &&
        tt_time_Compare( &p_range->end, time ) <= 0 )
        return false;

    if( tt_time_Valid( &p_range->begin ) &&
        tt_time_Compare( &p_range->begin, time ) > 0 )
        return false;

    return true;
}

static void tt_textnode_Delete( tt_textnode_t *p_node )
{
    free( p_node->psz_text );
    free( p_node );
}

static void tt_node_FreeDictValue( void* p_value, void* p_obj )
{
    VLC_UNUSED( p_obj );
    free( p_value );
}

static void tt_node_Delete( tt_node_t *p_node )
{
    free( p_node->psz_node_name );
    vlc_dictionary_clear( &p_node->attr_dict, tt_node_FreeDictValue, NULL );
    free( p_node );
}

void tt_node_RecursiveDelete( tt_node_t *p_node )
{
    for( ; p_node->p_child ; )
    {
        tt_basenode_t *p_child = p_node->p_child;
        p_node->p_child = p_child->p_next;

        if( p_child->i_type == TT_NODE_TYPE_TEXT )
            tt_textnode_Delete( (tt_textnode_t *) p_child );
        else
            tt_node_RecursiveDelete( (tt_node_t *) p_child );
    }
    tt_node_Delete( p_node );
}

static void tt_node_ParentAddChild( tt_node_t* p_parent, tt_basenode_t *p_child )
{
    tt_basenode_t **pp_node = &p_parent->p_child;
    while( *pp_node != NULL )
        pp_node = &((*pp_node)->p_next);
    *pp_node = p_child;
}

static tt_textnode_t *tt_textnode_New( tt_node_t *p_parent, const char *psz_text )
{
    tt_textnode_t *p_node = calloc( 1, sizeof( *p_node ) );
    if( !p_node )
        return NULL;
    p_node->i_type = TT_NODE_TYPE_TEXT;
    p_node->p_parent = p_parent;
    if( p_parent )
        tt_node_ParentAddChild( p_parent, (tt_basenode_t *) p_node );
    p_node->psz_text = strdup( psz_text );
    return p_node;
}

tt_node_t * tt_node_New( xml_reader_t* reader, tt_node_t* p_parent, const char* psz_node_name )
{
    tt_node_t *p_node = calloc( 1, sizeof( *p_node ) );
    if( !p_node )
        return NULL;

    p_node->i_type = TT_NODE_TYPE_ELEMENT;
    p_node->psz_node_name = strdup( psz_node_name );
    if( unlikely( p_node->psz_node_name == NULL ) )
    {
        free( p_node );
        return NULL;
    }
    vlc_dictionary_init( &p_node->attr_dict, 0 );
    tt_time_Init( &p_node->timings.begin );
    tt_time_Init( &p_node->timings.end );
    tt_time_Init( &p_node->timings.dur );
    p_node->p_parent = p_parent;
    if( p_parent )
        tt_node_ParentAddChild( p_parent, (tt_basenode_t *) p_node );

    const char* psz_value = NULL;
    for( const char* psz_key = xml_ReaderNextAttr( reader, &psz_value );
         psz_key != NULL;
         psz_key = xml_ReaderNextAttr( reader, &psz_value ) )
    {
        char *psz_val = strdup( psz_value );
        if( psz_val )
        {
            vlc_dictionary_insert( &p_node->attr_dict, psz_key, psz_val );

            if( !strcasecmp( psz_key, "begin" ) )
                p_node->timings.begin = tt_ParseTime( psz_val );
            else if( ! strcasecmp( psz_key, "end" ) )
                p_node->timings.end = tt_ParseTime( psz_val );
            else if( ! strcasecmp( psz_key, "dur" ) )
                p_node->timings.dur = tt_ParseTime( psz_val );
            else if( ! strcasecmp( psz_key, "timeContainer" ) )
                p_node->timings.i_type = strcmp( psz_val, "seq" ) ? TT_TIMINGS_PARALLEL
                                                                  : TT_TIMINGS_SEQUENTIAL;
        }
    }
    return p_node;
}
#if 0
static int tt_node_Skip( xml_reader_t *p_reader, const char *psz_skipped )
{
    size_t i_depth = 1;
    const char *psz_cur;

    /* need a copy as psz_skipped would point to next node after NextNode */
    char *psz_skip = strdup( psz_skipped );
    if(!psz_skip)
        return VLC_EGENERIC;

    for( ;; )
    {
        int i_type = xml_ReaderNextNode( p_reader, &psz_cur );
        switch( i_type )
        {
            case XML_READER_STARTELEM:
                if( i_depth == SIZE_MAX )
                {
                    free( psz_skip );
                    return VLC_EGENERIC;
                }
                if( !xml_ReaderIsEmptyElement( p_reader ) )
                    i_depth++;
                break;

            case XML_READER_ENDELEM:
                if( !strcmp( psz_cur, psz_skip ) )
                {
                    free( psz_skip );
                    if( i_depth != 1 )
                        return VLC_EGENERIC;
                    return VLC_SUCCESS;
                }
                else
                {
                    if( i_depth == 1 )
                    {
                        free( psz_skip );
                        return VLC_EGENERIC;
                    }
                    i_depth--;
                }
                break;

            default:
                if( i_type <= XML_READER_NONE )
                {
                    free( psz_skip );
                    return VLC_EGENERIC;
                }
                break;
        }
    }
    vlc_assert_unreachable();
    return VLC_EGENERIC;
}
#endif
int tt_nodes_Read( xml_reader_t *p_reader, tt_node_t *p_root_node )
{
    size_t i_depth = 0;
    tt_node_t *p_node = p_root_node;

    do
    {
        const char* psz_node_name;
        int i_type = xml_ReaderNextNode( p_reader, &psz_node_name );
        /* !warn read empty state now as attributes reading will **** it up */
        bool b_empty = xml_ReaderIsEmptyElement( p_reader );

        if( i_type <= XML_READER_NONE )
            break;

        switch( i_type )
        {
            default:
                break;

            case XML_READER_STARTELEM:
            {
                tt_node_t *p_newnode = tt_node_New( p_reader, p_node, psz_node_name );
                if( !p_newnode )
                    return VLC_EGENERIC;
                if( !b_empty )
                {
                    p_node = p_newnode;
                    i_depth++;
                }
                break;
            }

            case XML_READER_TEXT:
            {
                tt_textnode_t *p_textnode = tt_textnode_New( p_node, psz_node_name );
                VLC_UNUSED(p_textnode);
            }
            break;

            case XML_READER_ENDELEM:
            {
                if( strcmp( psz_node_name, p_node->psz_node_name ) )
                    return VLC_EGENERIC;

                if( i_depth == 0 )
                {
                    if( p_node != p_root_node )
                        return VLC_EGENERIC;
                    break; /* END */
                }
                i_depth--;
                p_node = p_node->p_parent;
                break;
            }
        }
    } while( 1 );

    return VLC_SUCCESS;
}


/* Timings storage */
static int tt_bsearch_searchkey_Compare( const void *key, const void *other )
{
    struct tt_searchkey *p_key = (struct tt_searchkey *) key;
    tt_time_t time = *((tt_time_t *) other);
    p_key->p_last = (tt_time_t *) other;
    return tt_time_Compare( &p_key->time, &time );
}

size_t tt_timings_FindLowerIndex( const tt_time_t *p_times, size_t i_times, tt_time_t time, bool *pb_found )
{
    size_t i_index = 0;
    if( p_times )
    {
        struct tt_searchkey key;
        key.time = time;
        key.p_last = NULL;

        tt_time_t *lookup = bsearch( &key, p_times, i_times,
                                     sizeof(tt_time_t), tt_bsearch_searchkey_Compare );
        if( lookup )
            key.p_last = lookup;
        *pb_found = !!lookup;

        /* Compute index from last visited */
        i_index = (key.p_last - p_times);
        if( tt_time_Compare( &p_times[i_index], &time ) < 0 )
            i_index++;
    }
    else *pb_found = false;
    return i_index;
}

static void tt_bsearch_Insert( tt_time_t **pp_times, size_t *pi_times, tt_time_t time )
{
    bool b_exists;
    size_t i_index = tt_timings_FindLowerIndex( *pp_times, *pi_times, time, &b_exists );
    if( b_exists )
        return;

    if( SIZE_MAX / sizeof(tt_time_t) < (*pi_times + 1) )
        return;

    tt_time_t *p_array = realloc( *pp_times, (*pi_times + 1) * sizeof(tt_time_t) );
    if( !p_array )
        return;
    *pp_times = p_array;

    if( *pi_times > 0 )
    {
        memmove( &p_array[i_index + 1],
                 &p_array[i_index],
                 (*pi_times - i_index) * sizeof(tt_time_t) );
    }

    p_array[i_index] = time;
    *pi_times += 1;
}


/* Timings storage */
static void tt_timings_MergeParallel( const tt_timings_t *p_ref, tt_timings_t *p_local )
{
    if( tt_time_Valid( &p_local->begin ) )
        p_local->begin = tt_time_Add( p_local->begin, p_ref->begin );
    else
        p_local->begin = p_ref->begin;

    if( tt_time_Valid( &p_local->end ) )
    {
        p_local->end = tt_time_Add( p_local->end, p_ref->begin );
    }
    else if( tt_time_Valid( &p_local->dur ) && tt_time_Valid( &p_local->begin ) )
    {
        p_local->end = tt_time_Add( p_local->begin, p_local->dur );
    }
    else p_local->end = p_ref->end;

    /* Enforce contained duration */

    if( tt_time_Valid( &p_ref->end ) && tt_time_Compare( &p_local->end, &p_ref->end ) > 0 )
        p_local->end = p_ref->end;

    /* Just for consistency */
    if( tt_time_Valid( &p_local->begin ) && tt_time_Valid( &p_local->end ) )
        p_local->dur = tt_time_Sub( p_local->end, p_local->begin );
}

static void tt_timings_MergeSequential( const tt_timings_t *p_restrict,
                                       const tt_timings_t *p_prevref, tt_timings_t *p_local )
{
    if( tt_time_Valid( &p_local->begin ) )
        p_local->begin = tt_time_Add( p_local->begin, p_prevref->end );
    else
        p_local->begin = p_prevref->end;

    if( tt_time_Valid( &p_local->end ) )
    {
        p_local->end = tt_time_Add( p_local->end, p_prevref->end );
    }
    else if( tt_time_Valid( &p_local->dur ) && tt_time_Valid( &p_local->begin ) )
    {
        p_local->end = tt_time_Add( p_local->begin, p_local->dur );
    }

    /* Enforce contained duration */
    if( tt_time_Valid( &p_restrict->end ) && tt_time_Compare( &p_local->end, &p_restrict->end ) > 0 )
        p_local->end = p_restrict->end;

    /* Just for consistency */
    if( tt_time_Valid( &p_local->begin ) && tt_time_Valid( &p_local->end ) )
        p_local->dur = tt_time_Sub( p_local->end, p_local->begin );
}

void tt_timings_Resolve( tt_basenode_t *p_child, const tt_timings_t *p_container_timings,
                         tt_time_t **pp_array, size_t *pi_count )
{
    const tt_node_t *p_prevnode = NULL;
    for(  ; p_child; p_child = p_child->p_next )
    {
        if( p_child->i_type != TT_NODE_TYPE_ELEMENT )
            continue;

        tt_node_t *p_childnode = (tt_node_t *) p_child;
        if( p_container_timings->i_type == TT_TIMINGS_SEQUENTIAL )
        {
            if( p_prevnode == NULL ) /* First */
                tt_timings_MergeParallel( p_container_timings, &p_childnode->timings );
            else
                tt_timings_MergeSequential( p_container_timings,
                                        &p_prevnode->timings, &p_childnode->timings );
        }
        else
        {
            tt_timings_MergeParallel( p_container_timings, &p_childnode->timings );
        }

        if( tt_time_Valid( &p_childnode->timings.begin ) )
            tt_bsearch_Insert( pp_array, pi_count, p_childnode->timings.begin );

        if( tt_time_Valid( &p_childnode->timings.end ) )
            tt_bsearch_Insert( pp_array, pi_count, p_childnode->timings.end );

        p_prevnode = p_childnode;

        tt_timings_Resolve( p_childnode->p_child, &p_childnode->timings,
                            pp_array, pi_count );
    }
}
