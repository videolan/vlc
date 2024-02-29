/*****************************************************************************
 * ttml.h : TTML helpers
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

#include <vlc_tick.h>
#include <vlc_arrays.h>
#include <vlc_memstream.h>
#include <vlc_list.h>

int tt_OpenDemux( vlc_object_t* p_this );
void tt_CloseDemux( vlc_object_t* p_demux );

int  tt_OpenDecoder   ( vlc_object_t * );

int  tt_OpenEncoder   ( vlc_object_t * );

enum
{
    TT_TIMINGS_UNSPEC = 0,
    TT_TIMINGS_PARALLEL,
    TT_TIMINGS_SEQUENTIAL,
};

#define TT_FRAME_RATE 30

typedef struct
{
    vlc_tick_t base;
    unsigned frames;
    //unsigned ticks;
} tt_time_t;

typedef struct
{
    uint8_t i_type;
    tt_time_t begin;
    tt_time_t end;
    tt_time_t dur;
} tt_timings_t;

struct tt_searchkey
{
    tt_time_t time;
    tt_time_t *p_last;
};

/* namespaces */
#define TT_NS             "http://www.w3.org/ns/ttml"
#define TT_NS_PARAMETER   TT_NS "#parameter"
#define TT_NS_STYLING     TT_NS "#styling"
#define TT_NS_METADATA    TT_NS "#metadata"
#define TT_NS_PROFILE     TT_NS "/profile/"
#define TT_NS_FEATURE     TT_NS "/feature/"
#define TT_NS_EXTENSION   TT_NS "/extension/"
#define TT_NS_XML             "http://www.w3.org/XML/1998/namespace"
#define TT_NS_SMPTE_TT_EXT    "http://www.smpte-ra.org/schemas/2052-1/2010/smpte-tt"

typedef struct
{
    struct vlc_list nodes;
} tt_namespaces_t;

void tt_namespaces_Init( tt_namespaces_t *nss );
void tt_namespaces_Clean( tt_namespaces_t *nss );
void tt_namespaces_Register( tt_namespaces_t *nss, const char *psz_prefix,
                             const char *psz_uri );
const char * tt_namespaces_GetURI( const tt_namespaces_t *nss,
                                   const char *psz_qn ); /* qn or prefix */
const char * tt_namespaces_GetPrefix( const tt_namespaces_t *nss,
                                      const char *psz_uri );

enum
{
    TT_NODE_TYPE_ELEMENT,
    TT_NODE_TYPE_TEXT,
};

typedef struct tt_basenode_t tt_basenode_t;
typedef struct tt_node_t tt_node_t;

#define TT_NODE_BASE_MEMBERS \
    uint8_t i_type;\
    tt_node_t *p_parent;\
    tt_basenode_t *p_next;

struct tt_basenode_t
{
    TT_NODE_BASE_MEMBERS
};

struct tt_node_t
{
    TT_NODE_BASE_MEMBERS
    tt_basenode_t *p_child;
    char *psz_node_name;
    tt_timings_t timings;
    vlc_dictionary_t attr_dict;
    char *psz_namespace;
};

typedef struct
{
    TT_NODE_BASE_MEMBERS
    char *psz_text;
} tt_textnode_t;

static inline const char *tt_LocalName( const char *psz_qname )
{
    const char *psz_local = strchr( psz_qname, ':' );
    return psz_local ? psz_local + 1 : psz_qname;
}

tt_textnode_t *tt_textnode_New( tt_node_t *p_parent, const char *psz_text );
tt_textnode_t *tt_subtextnode_New( tt_node_t *p_parent, const char *psz_text, size_t );
tt_node_t * tt_node_New( tt_node_t* p_parent, const char* psz_node_name, const char *psz_namespace );
tt_node_t * tt_node_NewRead( xml_reader_t* reader, tt_namespaces_t *, tt_node_t* p_parent,
                             const char* psz_node_name, const char *psz_namespace );
void tt_node_RecursiveDelete( tt_node_t *p_node );
bool tt_node_Match( const tt_node_t *p_node, const char* psz_name, const char* psz_namespace );
const char * tt_node_GetAttribute( tt_namespaces_t *, const tt_node_t *p_node,
                                   const char *psz_name, const char *psz_namespace );
bool tt_node_HasChild( const tt_node_t *p_node );
int  tt_node_AddAttribute( tt_node_t *p_node, const char *key, const char *value );
void tt_node_RemoveAttribute( tt_node_t *p_node, const char *key );

int tt_nodes_Read( xml_reader_t *p_reader, tt_namespaces_t *, tt_node_t *p_root_node );

void tt_timings_Resolve( tt_basenode_t *p_child, const tt_timings_t *p_container_timings,
                         tt_time_t **pp_array, size_t *pi_count );
bool tt_timings_Contains( const tt_timings_t *p_range, const tt_time_t * );
size_t tt_timings_FindLowerIndex( const tt_time_t *p_times, size_t i_times, tt_time_t time, bool *pb_found );

static inline void tt_time_Init( tt_time_t *t )
{
    t->base = -1;
    t->frames = 0;
}

static inline tt_time_t tt_time_Create( vlc_tick_t i )
{
    tt_time_t t;
    t.base = i;
    t.frames = 0;
    return t;
}

static inline bool tt_time_Valid( const tt_time_t *t )
{
    return t->base != -1;
}

static inline vlc_tick_t tt_time_Convert( const tt_time_t *t )
{
    if( !tt_time_Valid( t ) )
        return VLC_TICK_INVALID;
    else
        return t->base + vlc_tick_from_samples( t->frames, TT_FRAME_RATE);
}

static inline int tt_time_Compare( const tt_time_t *t1, const tt_time_t *t2 )
{
    vlc_tick_t ttt1 = tt_time_Convert( t1 );
    vlc_tick_t ttt2 = tt_time_Convert( t2 );
    if (ttt1 < ttt2)
        return -1;
    return ttt1 > ttt2;
}

static inline tt_time_t tt_time_Add( tt_time_t t1, tt_time_t t2 )
{
    t1.base += t2.base;
    t1.frames += t2.frames;
    t1.base += vlc_tick_from_samples( t1.frames, TT_FRAME_RATE );
    t1.frames = t1.frames % TT_FRAME_RATE;
    return t1;
}

static inline tt_time_t tt_time_Sub( tt_time_t t1, tt_time_t t2 )
{
    if( t2.frames > t1.frames )
    {
        unsigned diff = 1 + (t2.frames - t1.frames) / TT_FRAME_RATE;
        t1.base -= vlc_tick_from_sec( diff );
        t1.frames += diff * TT_FRAME_RATE;
    }
    t1.frames -= t2.frames;
    t1.base -= t2.base;
    return t1;
}

/* Encoding */

char *tt_genTiming( tt_time_t t );
void tt_node_AttributesToText( struct vlc_memstream *p_stream, const tt_node_t* p_node );
void tt_node_ToText( struct vlc_memstream *p_stream, const tt_basenode_t *p_basenode,
                     const tt_time_t *playbacktime );
