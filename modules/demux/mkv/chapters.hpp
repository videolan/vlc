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

namespace mkv {

class chapter_translation_c
{
public:
    chapter_translation_c()
        :p_translated(NULL)
    {}

    ~chapter_translation_c()
    {
        delete p_translated;
    }

    KaxChapterTranslateID  *p_translated;
    unsigned int           codec_id;
    std::vector<uint64_t>  editions;
};

class chapter_codec_cmds_c;
class chapter_item_c
{
public:
    chapter_item_c()
    :i_start_time(0)
    ,i_end_time(-1)
    ,p_segment_uid(NULL)
    ,p_segment_edition_uid(NULL)
    ,b_display_seekpoint(true)
    ,b_user_display(true)
    ,p_parent(NULL)
    ,b_is_leaving(false)
    {}

    virtual ~chapter_item_c();
    void Append( const chapter_item_c & edition );
    chapter_item_c * FindChapter( int64_t i_find_uid );
    virtual chapter_item_c *BrowseCodecPrivate( unsigned int codec_id,
                                    bool (*match)(const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size ),
                                    const void *p_cookie,
                                    size_t i_cookie_size );
    std::string                 GetCodecName( bool f_for_title = false ) const;
    bool                        ParentOf( const chapter_item_c & item ) const;
    int16                       GetTitleNumber( ) const;

    vlc_tick_t                  i_start_time, i_end_time;
    std::vector<chapter_item_c*> sub_chapters;
    KaxChapterSegmentUID        *p_segment_uid;
    KaxChapterSegmentEditionUID *p_segment_edition_uid;
    int64_t                     i_uid;
    bool                        b_display_seekpoint;
    bool                        b_user_display;
    std::string                 str_name;
    chapter_item_c              *p_parent;
    bool                        b_is_leaving;

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
