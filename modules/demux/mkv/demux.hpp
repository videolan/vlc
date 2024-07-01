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
#include "chapter_command_dvd.hpp"
#include "chapter_command_script.hpp"
#include "events.hpp"

#include <memory>

#include <vlc_threads.h>

namespace mkv {

class virtual_segment_c;
class chapter_item_c;

struct demux_sys_t : public chapter_codec_vm
{
public:
    demux_sys_t( demux_t & demux, bool trust_cues )
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
        ,i_duration(-1)
        ,trust_cues(trust_cues)
        ,ev(&demux)
    {
        vlc_mutex_init( &lock_demuxer );
    }

    virtual ~demux_sys_t();

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
    std::vector<std::unique_ptr<input_attachment_t,
                    void(*)(input_attachment_t*)>> stored_attachments;
    std::vector<matroska_segment_c*> opened_segments;
    std::vector<virtual_segment_c*>  used_vsegments;

    /* duration of the stream */
    vlc_tick_t              i_duration;

    const bool              trust_cues;

    bool SegmentIsOpened( const EbmlBinary & uid ) const;

    // chapter_codec_vm
    virtual_chapter_c *BrowseCodecPrivate( chapter_codec_id codec_id,
                                           chapter_cmd_match match,
                                           virtual_segment_c * & p_vsegment_found ) override;
    void JumpTo( virtual_segment_c & vsegment, virtual_chapter_c & vchapter ) override;
    virtual_segment_c *GetCurrentVSegment() override
    {
        return p_current_vsegment;
    }
    virtual_chapter_c *FindVChapter( chapter_uid i_find_uid, virtual_segment_c * & p_vsegment_found ) override;
    void SetHighlight( vlc_spu_highlight_t & ) override;

    void PreloadFamily( const matroska_segment_c & of_segment );
    bool PreloadLinked();
    bool FreeUnused();
    bool PreparePlayback( virtual_segment_c & new_vsegment );
    bool AnalyseAllSegmentsFound( demux_t *p_demux, matroska_stream_c * );

    dvd_command_interpretor_c * GetDVDInterpretor()
    {
        if (!dvd_interpretor)
        {
            try {
                dvd_interpretor = std::make_unique<dvd_command_interpretor_c>( vlc_object_logger( &demuxer ), *this );
            } catch ( const std::bad_alloc & ) {
            }
        }
        return dvd_interpretor.get();
    }

    matroska_script_interpretor_c * GetMatroskaScriptInterpreter()
    {
        if (!ms_interpreter)
        {
            try {
                ms_interpreter = std::make_unique<matroska_script_interpretor_c> ( vlc_object_logger( &demuxer ), *this );
            } catch ( const std::bad_alloc & ) {
            }
        }

        return ms_interpreter.get();
    }

    uint8_t        palette[4][4];
    vlc_mutex_t    lock_demuxer;

    /* event */
    event_thread_t ev;

private:
    virtual_segment_c                *p_current_vsegment = nullptr;
    std::unique_ptr<dvd_command_interpretor_c> dvd_interpretor; // protected by lock_demuxer
    std::unique_ptr<matroska_script_interpretor_c> ms_interpreter;
};

} // namespace

#endif
