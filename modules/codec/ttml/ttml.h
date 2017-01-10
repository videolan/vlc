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

int OpenDemux( vlc_object_t* p_this );
void CloseDemux( demux_t* p_demux );

int  OpenDecoder   ( vlc_object_t * );
void CloseDecoder  ( vlc_object_t * );

enum
{
    TT_TIMINGS_UNSPEC = 0,
    TT_TIMINGS_PARALLEL,
    TT_TIMINGS_SEQUENTIAL,
};

typedef struct
{
    uint8_t i_type;
    int64_t i_begin;
    int64_t i_end;
    int64_t i_dur;
} tt_timings_t;

struct tt_searchkey
{
    int64_t i_time;
    int64_t *p_last;
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
                         int64_t **pp_array, size_t *pi_count );
bool tt_timings_Contains( const tt_timings_t *p_range, int64_t i_time );
size_t tt_timings_FindLowerIndex( const int64_t *p_times, size_t i_times, int64_t i_time, bool *pb_found );




