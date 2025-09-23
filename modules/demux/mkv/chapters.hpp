/*****************************************************************************
 * chapters.hpp : matroska demuxer
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

/* chapter_item, chapter_edition, and chapter_translation classes */

#ifndef VLC_MKV_CHAPTERS_HPP_
#define VLC_MKV_CHAPTERS_HPP_

#include "mkv.hpp"

#include <optional>
#include <limits>

namespace mkv {

class chapter_translation_c
{
public:
    chapter_translation_c()
        :p_translated(nullptr)
        ,codec_id(std::numeric_limits<unsigned int>::max())
    {}

    ~chapter_translation_c()
    {
        delete p_translated;
    }

    constexpr bool isValid() const {
        return p_translated != nullptr &&
               codec_id != std::numeric_limits<unsigned int>::max();
    }

    KaxChapterTranslateID  *p_translated;
    unsigned int           codec_id;
    std::vector<uint64_t>  editions;
};

class chapter_item_c
{
public:
    chapter_item_c()
    {}

    virtual ~chapter_item_c();
    void Append( const chapter_item_c & edition );
    chapter_item_c * FindChapter( chapter_uid i_find_uid );
    virtual chapter_item_c *BrowseCodecPrivate( chapter_codec_id codec_id,
                                                chapter_cmd_match match );
    std::string                 GetCodecName( bool f_for_title = false ) const;
    bool                        ParentOf( const chapter_item_c & item ) const;
    int16_t                     GetTitleNumber( ) const;

    vlc_tick_t                  i_start_time = 0;
    std::optional<vlc_tick_t>   i_end_time;
    std::vector<chapter_item_c*> sub_chapters;
    KaxChapterSegmentUID        *p_segment_uid = nullptr;
    KaxChapterSegmentEditionUID *p_segment_edition_uid = nullptr;
    chapter_uid                 i_uid = 0;
    bool                        b_display_seekpoint = true;
    bool                        b_user_display = true;
    std::string                 str_name;
    chapter_item_c              *p_parent = nullptr;
    bool                        b_is_leaving = false;

    std::vector<chapter_codec_cmds_c*> codecs;

    bool Enter( bool b_do_subchapters );
    bool Leave( bool b_do_subchapters );
    bool EnterAndLeave( chapter_item_c *p_leaving_chapter, bool b_enter = true );

  protected:
      bool EnterLeaveHelper_ (bool, bool(chapter_codec_cmds_c::*)(), bool(chapter_item_c::*)(bool));
};

class chapter_edition_c : public chapter_item_c
{
public:
    chapter_edition_c(): b_ordered(false), b_default(false), b_hidden(false)
    {}

    std::string GetMainName() const;
    bool                        b_ordered;
    bool                        b_default;
    bool                        b_hidden;
};

} // namespace

#endif
