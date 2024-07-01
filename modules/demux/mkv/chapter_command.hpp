/*****************************************************************************
 * chapter_command.hpp : matroska demuxer
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

#ifndef VLC_MKV_CHAPTER_COMMAND_HPP_
#define VLC_MKV_CHAPTER_COMMAND_HPP_

#include "mkv.hpp"

#include <memory>

struct vlc_spu_highlight_t;

namespace mkv {

class virtual_chapter_c;
class virtual_segment_c;

class chapter_codec_vm
{
public:
    virtual virtual_segment_c *GetCurrentVSegment() = 0;
    virtual virtual_chapter_c *FindVChapter( chapter_uid i_find_uid, virtual_segment_c * & p_vsegment_found ) = 0;
    virtual void JumpTo( virtual_segment_c &, virtual_chapter_c & ) = 0;

    virtual virtual_chapter_c *BrowseCodecPrivate( enum chapter_codec_id,
                                                   chapter_cmd_match match,
                                                   virtual_segment_c * & p_vsegment_found ) = 0;
    virtual void SetHighlight( vlc_spu_highlight_t & ) = 0;
};

enum NavivationKey {
    LEFT, RIGHT, UP, DOWN, OK, MENU, POPUP
};

class chapter_codec_cmds_c
{
public:
    chapter_codec_cmds_c( struct vlc_logger *log, chapter_codec_vm & vm_, enum chapter_codec_id codec_id)
    :i_codec_id( codec_id )
    ,l( log )
    ,vm( vm_ )
    {}

    virtual ~chapter_codec_cmds_c() = default;

    void SetPrivate( const KaxChapterProcessPrivate & private_data )
    {
        p_private_data = std::make_unique<KaxChapterProcessPrivate>(private_data);
    }

    void AddCommand( const KaxChapterProcessCommand & command );

    /// \return whether the codec has seeked in the files or not
    virtual bool Enter() { return false; }
    virtual bool Leave() { return false; }
    virtual std::string GetCodecName( bool ) const { return ""; }
    virtual int16_t GetTitleNumber() const { return -1; }

    const enum chapter_codec_id i_codec_id;

    std::unique_ptr<KaxChapterProcessPrivate> p_private_data;

protected:
    using ChapterProcess = std::vector<KaxChapterProcessData>;
    ChapterProcess enter_cmds;
    ChapterProcess during_cmds;
    ChapterProcess leave_cmds;

    struct vlc_logger *l;
    chapter_codec_vm & vm;
};
} // namespace

#endif
