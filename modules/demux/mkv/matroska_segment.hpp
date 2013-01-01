/*****************************************************************************
 * matroska_segment.hpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2004 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Steve Lhomme <steve.lhomme@free.fr>
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

#ifndef _MATROSKA_SEGMENT_HPP_
#define _MATROSKA_SEGMENT_HPP_

#include "mkv.hpp"

class EbmlParser;

class chapter_edition_c;
class chapter_translation_c;
class chapter_item_c;

struct mkv_track_t;
struct mkv_index_t;

typedef enum
{
    WHOLE_SEGMENT,
    TRACK_UID,
    EDITION_UID,
    CHAPTER_UID,
    ATTACHMENT_UID
} tag_target_type;

class SimpleTag
{
public:
    SimpleTag():
        psz_tag_name(NULL), psz_lang(NULL), b_default(true), p_value(NULL){}
    ~SimpleTag();
    char *psz_tag_name;
    char *psz_lang; /* NULL value means "undf" */
    bool b_default;
    char * p_value;
    std::vector<SimpleTag *> sub_tags;
};

class Tag
{
public:
    Tag():i_tag_type(WHOLE_SEGMENT),i_target_type(50),i_uid(0){}
    ~Tag();
    tag_target_type i_tag_type;
    uint64_t        i_target_type;
    uint64_t        i_uid;
    std::vector<SimpleTag*> simple_tags;
};

class matroska_segment_c
{
public:
    matroska_segment_c( demux_sys_t & demuxer, EbmlStream & estream );
    virtual ~matroska_segment_c();

    KaxSegment              *segment;
    EbmlStream              & es;

    /* time scale */
    uint64_t                i_timescale;

    /* duration of the segment */
    mtime_t                 i_duration;
    mtime_t                 i_start_time;

    /* all tracks */
    std::vector<mkv_track_t*> tracks;

    /* from seekhead */
    int                     i_seekhead_count;
    int64_t                 i_seekhead_position;
    int64_t                 i_cues_position;
    int64_t                 i_tracks_position;
    int64_t                 i_info_position;
    int64_t                 i_chapters_position;
    int64_t                 i_tags_position;
    int64_t                 i_attachments_position;

    KaxCluster              *cluster;
    uint64                  i_block_pos;
    uint64                  i_cluster_pos;
    int64_t                 i_start_pos;
    KaxSegmentUID           *p_segment_uid;
    KaxPrevUID              *p_prev_segment_uid;
    KaxNextUID              *p_next_segment_uid;

    bool                    b_cues;
    int                     i_index;
    int                     i_index_max;
    mkv_index_t             *p_indexes;

    /* info */
    char                    *psz_muxing_application;
    char                    *psz_writing_application;
    char                    *psz_segment_filename;
    char                    *psz_title;
    char                    *psz_date_utc;

    /* !!!!! GCC 3.3 bug on Darwin !!!!! */
    /* when you remove this variable the compiler issues an atomicity error */
    /* this variable only works when using std::vector<chapter_edition_c> */
    std::vector<chapter_edition_c*> stored_editions;
    int                             i_default_edition;

    std::vector<chapter_translation_c*> translations;
    std::vector<KaxSegmentFamily*>  families;
    std::vector<Tag *>              tags;

    demux_sys_t                    & sys;
    EbmlParser                     *ep;
    bool                           b_preloaded;
    bool                           b_ref_external_segments;

    bool Preload();
    bool PreloadFamily( const matroska_segment_c & segment );
    void InformationCreate();
    void Seek( mtime_t i_date, mtime_t i_time_offset, int64_t i_global_position );
    int BlockGet( KaxBlock * &, KaxSimpleBlock * &, bool *, bool *, int64_t *);

    int BlockFindTrackIndex( size_t *pi_track,
                             const KaxBlock *, const KaxSimpleBlock * );

    bool Select( mtime_t i_start_time );
    void UnSelect();

    static bool CompareSegmentUIDs( const matroska_segment_c * item_a, const matroska_segment_c * item_b );

private:
    void LoadCues( KaxCues *cues );
    void LoadTags( KaxTags *tags );
    bool LoadSeekHeadItem( const EbmlCallbacks & ClassInfos, int64_t i_element_position );
    void ParseInfo( KaxInfo *info );
    void ParseAttachments( KaxAttachments *attachments );
    void ParseChapters( KaxChapters *chapters );
    void ParseSeekHead( KaxSeekHead *seekhead );
    void ParseTracks( KaxTracks *tracks );
    void ParseChapterAtom( int i_level, KaxChapterAtom *ca, chapter_item_c & chapters );
    void ParseTrackEntry( KaxTrackEntry *m );
    void ParseCluster( bool b_update_start_time = true );
    SimpleTag * ParseSimpleTags( KaxTagSimple *tag, int level = 50 );
    void IndexAppendCluster( KaxCluster *cluster );
    int32_t TrackInit( mkv_track_t * p_tk );
    void ComputeTrackPriority();
};


#endif
