/*****************************************************************************
 * chapters.hpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Steve Lhomme <steve.lhomme@free.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/* chapter_item, chapter_edition, and chapter_translation classes */

#ifndef _CHAPTER_H_
#define _CHAPTER_H_

#include "mkv.hpp"

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
    ,i_user_start_time(-1)
    ,i_user_end_time(-1)
    ,i_seekpoint_num(-1)
    ,b_display_seekpoint(true)
    ,b_user_display(false)
    ,psz_parent(NULL)
    ,b_is_leaving(false)
    {}

    virtual ~chapter_item_c();

    int64_t RefreshChapters( bool b_ordered, int64_t i_prev_user_time );
    int PublishChapters( input_title_t & title, int & i_user_chapters, int i_level = 0 );
    virtual chapter_item_c * FindTimecode( mtime_t i_timecode, const chapter_item_c * p_current, bool & b_found );
    void Append( const chapter_item_c & edition );
    chapter_item_c * FindChapter( int64_t i_find_uid );
    virtual chapter_item_c *BrowseCodecPrivate( unsigned int codec_id,
                                    bool (*match)(const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size ),
                                    const void *p_cookie,
                                    size_t i_cookie_size );
    std::string                 GetCodecName( bool f_for_title = false ) const;
    bool                        ParentOf( const chapter_item_c & item ) const;
    int16                       GetTitleNumber( ) const;
 
    int64_t                     i_start_time, i_end_time;
    int64_t                     i_user_start_time, i_user_end_time; /* the time in the stream when an edition is ordered */
    std::vector<chapter_item_c*> sub_chapters;
    int                         i_seekpoint_num;
    int64_t                     i_uid;
    bool                        b_display_seekpoint;
    bool                        b_user_display;
    std::string                 psz_name;
    chapter_item_c              *psz_parent;
    bool                        b_is_leaving;
 
    std::vector<chapter_codec_cmds_c*> codecs;

    static bool CompareTimecode( const chapter_item_c * itemA, const chapter_item_c * itemB )
    {
        return ( itemA->i_user_start_time < itemB->i_user_start_time || (itemA->i_user_start_time == itemB->i_user_start_time && itemA->i_user_end_time < itemB->i_user_end_time) );
    }

    bool Enter( bool b_do_subchapters );
    bool Leave( bool b_do_subchapters );
    bool EnterAndLeave( chapter_item_c *p_item, bool b_enter = true );
};

class chapter_edition_c : public chapter_item_c
{
public:
    chapter_edition_c()
    :b_ordered(false)
    {}
 
    void RefreshChapters( );
    mtime_t Duration() const;
    std::string GetMainName() const;
    chapter_item_c * FindTimecode( mtime_t i_timecode, const chapter_item_c * p_current );
 
    bool                        b_ordered;
};

#endif
