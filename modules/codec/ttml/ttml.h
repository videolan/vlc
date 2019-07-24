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

int tt_OpenDemux( vlc_object_t* p_this );
void tt_CloseDemux( vlc_object_t* p_demux );

int  tt_OpenDecoder   ( vlc_object_t * );
void tt_CloseDecoder  ( vlc_object_t * );

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
};

typedef struct
{
    TT_NODE_BASE_MEMBERS
    char *psz_text;
} tt_textnode_t;

tt_node_t * tt_node_New( xml_reader_t* reader, tt_node_t* p_parent, const char* psz_node_name );
void tt_node_RecursiveDelete( tt_node_t *p_node );
int  tt_node_NameCompare( const char* psz_tagname, const char* psz_pattern );
bool tt_node_HasChild( const tt_node_t *p_node );

int tt_nodes_Read( xml_reader_t *p_reader, tt_node_t *p_root_node );

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
