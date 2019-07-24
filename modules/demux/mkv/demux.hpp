/*****************************************************************************
 * demux.hpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2004 VLC authors and VideoLAN
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

#ifndef VLC_MKV_DEMUX_HPP_
#define VLC_MKV_DEMUX_HPP_

#include "mkv.hpp"

#include "chapter_command.hpp"
#include "virtual_segment.hpp"
#include "dvd_types.hpp"
#include "events.hpp"

namespace mkv {

class virtual_segment_c;
class chapter_item_c;

struct demux_sys_t
{
public:
    demux_sys_t( demux_t & demux )
        :demuxer(demux)
        ,b_seekable(false)
        ,b_fastseekable(false)
        ,i_pts(VLC_TICK_INVALID)
        ,i_pcr(VLC_TICK_INVALID)
        ,i_start_pts(VLC_TICK_0)
        ,i_mk_chapter_time(0)
        ,meta(NULL)
        ,i_current_title(0)
        ,i_current_seekpoint(0)
        ,i_updates(0)
        ,p_current_vsegment(NULL)
        ,dvd_interpretor( *this )
        ,i_duration(-1)
        ,ev(&demux)
    {
        vlc_mutex_init( &lock_demuxer );
    }

    ~demux_sys_t();

    /* current data */
    demux_t                 & demuxer;
    bool                    b_seekable;
    bool                    b_fastseekable;

    vlc_tick_t              i_pts;
    vlc_tick_t              i_pcr;
    vlc_tick_t              i_start_pts;
    vlc_tick_t              i_mk_chapter_time;

    vlc_meta_t              *meta;

    std::vector<input_title_t*>      titles; // matroska editions
    size_t                           i_current_title;
    size_t                           i_current_seekpoint;
    unsigned                         i_updates;

    std::vector<matroska_stream_c*>  streams;
    std::vector<attachment_c*>       stored_attachments;
    std::vector<matroska_segment_c*> opened_segments;
    std::vector<virtual_segment_c*>  used_vsegments;
    virtual_segment_c                *p_current_vsegment;

    dvd_command_interpretor_c        dvd_interpretor;

    /* duration of the stream */
    vlc_tick_t              i_duration;

    matroska_segment_c *FindSegment( const EbmlBinary & uid ) const;
    virtual_chapter_c *BrowseCodecPrivate( unsigned int codec_id,
                                        bool (*match)(const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size ),
                                        const void *p_cookie,
                                        size_t i_cookie_size,
                                        virtual_segment_c * & p_vsegment_found );
    virtual_chapter_c *FindChapter( int64_t i_find_uid, virtual_segment_c * & p_vsegment_found );

    void PreloadFamily( const matroska_segment_c & of_segment );
    bool PreloadLinked();
    bool FreeUnused();
    bool PreparePlayback( virtual_segment_c & new_vsegment, vlc_tick_t i_mk_date );
    bool AnalyseAllSegmentsFound( demux_t *p_demux, matroska_stream_c * );
    void JumpTo( virtual_segment_c & vsegment, virtual_chapter_c & vchapter );

    uint8_t        palette[4][4];
    vlc_mutex_t    lock_demuxer;

    /* event */
    event_thread_t ev;
};

} // namespace

#endif
