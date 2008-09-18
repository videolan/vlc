/*****************************************************************************
 * mkv.cpp : matroska demuxer
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

#include "chapters.hpp"

#include "chapter_command.hpp"

chapter_item_c::~chapter_item_c()
{
    std::vector<chapter_codec_cmds_c*>::iterator index = codecs.begin();
    while ( index != codecs.end() )
    {
        delete (*index);
        index++;
    }
    std::vector<chapter_item_c*>::iterator index_ = sub_chapters.begin();
    while ( index_ != sub_chapters.end() )
    {
        delete (*index_);
        index_++;
    }
}

int chapter_item_c::PublishChapters( input_title_t & title, int & i_user_chapters, int i_level )
{
    // add support for meta-elements from codec like DVD Titles
    if ( !b_display_seekpoint || psz_name == "" )
    {
        psz_name = GetCodecName();
        if ( psz_name != "" )
            b_display_seekpoint = true;
    }

    if (b_display_seekpoint)
    {
        seekpoint_t *sk = vlc_seekpoint_New();

        sk->i_level = i_level;
        sk->i_time_offset = i_start_time;
        sk->psz_name = strdup( psz_name.c_str() );

        // A start time of '0' is ok. A missing ChapterTime element is ok, too, because '0' is its default value.
        title.i_seekpoint++;
        title.seekpoint = (seekpoint_t**)realloc( title.seekpoint, title.i_seekpoint * sizeof( seekpoint_t* ) );
        title.seekpoint[title.i_seekpoint-1] = sk;

        if ( b_user_display )
            i_user_chapters++;
    }

    for ( size_t i=0; i<sub_chapters.size() ; i++)
    {
        sub_chapters[i]->PublishChapters( title, i_user_chapters, i_level+1 );
    }

    i_seekpoint_num = i_user_chapters;

    return i_user_chapters;
}

chapter_item_c *chapter_item_c::BrowseCodecPrivate( unsigned int codec_id,
                                    bool (*match)(const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size ),
                                    const void *p_cookie,
                                    size_t i_cookie_size )
{
    // this chapter
    std::vector<chapter_codec_cmds_c*>::const_iterator index = codecs.begin();
    while ( index != codecs.end() )
    {
        if ( match( **index ,p_cookie, i_cookie_size ) )
            return this;
        index++;
    }
 
    // sub-chapters
    chapter_item_c *p_result = NULL;
    std::vector<chapter_item_c*>::const_iterator index2 = sub_chapters.begin();
    while ( index2 != sub_chapters.end() )
    {
        p_result = (*index2)->BrowseCodecPrivate( codec_id, match, p_cookie, i_cookie_size );
        if ( p_result != NULL )
            return p_result;
        index2++;
    }
 
    return p_result;
}

void chapter_item_c::Append( const chapter_item_c & chapter )
{
    // we are appending content for the same chapter UID
    size_t i;
    chapter_item_c *p_chapter;

    for ( i=0; i<chapter.sub_chapters.size(); i++ )
    {
        p_chapter = FindChapter( chapter.sub_chapters[i]->i_uid );
        if ( p_chapter != NULL )
        {
            p_chapter->Append( *chapter.sub_chapters[i] );
        }
        else
        {
            sub_chapters.push_back( chapter.sub_chapters[i] );
        }
    }

    i_user_start_time = min( i_user_start_time, chapter.i_user_start_time );
    i_user_end_time = max( i_user_end_time, chapter.i_user_end_time );
}

chapter_item_c * chapter_item_c::FindChapter( int64_t i_find_uid )
{
    size_t i;
    chapter_item_c *p_result = NULL;

    if ( i_uid == i_find_uid )
        return this;

    for ( i=0; i<sub_chapters.size(); i++)
    {
        p_result = sub_chapters[i]->FindChapter( i_find_uid );
        if ( p_result != NULL )
            break;
    }
    return p_result;
}

std::string chapter_item_c::GetCodecName( bool f_for_title ) const
{
    std::string result;

    std::vector<chapter_codec_cmds_c*>::const_iterator index = codecs.begin();
    while ( index != codecs.end() )
    {
        result = (*index)->GetCodecName( f_for_title );
        if ( result != "" )
            break;
        index++;
    }

    return result;
}

int16 chapter_item_c::GetTitleNumber( ) const
{
    int result = -1;

    std::vector<chapter_codec_cmds_c*>::const_iterator index = codecs.begin();
    while ( index != codecs.end() )
    {
        result = (*index)->GetTitleNumber( );
        if ( result >= 0 )
            break;
        index++;
    }

    return result;
}

int64_t chapter_item_c::RefreshChapters( bool b_ordered, int64_t i_prev_user_time )
{
    int64_t i_user_time = i_prev_user_time;
 
    // first the sub-chapters, and then ourself
    std::vector<chapter_item_c*>::iterator index = sub_chapters.begin();
    while ( index != sub_chapters.end() )
    {
        i_user_time = (*index)->RefreshChapters( b_ordered, i_user_time );
        index++;
    }

    if ( b_ordered )
    {
        // the ordered chapters always start at zero
        if ( i_prev_user_time == -1 )
        {
            if ( i_user_time == -1 )
                i_user_time = 0;
            i_prev_user_time = 0;
        }

        i_user_start_time = i_prev_user_time;
        if ( i_end_time != -1 && i_user_time == i_prev_user_time )
        {
            i_user_end_time = i_user_start_time - i_start_time + i_end_time;
        }
        else
        {
            i_user_end_time = i_user_time;
        }
    }
    else
    {
        if ( sub_chapters.begin() != sub_chapters.end() )
            std::sort( sub_chapters.begin(), sub_chapters.end(), chapter_item_c::CompareTimecode );
        i_user_start_time = i_start_time;
        if ( i_end_time != -1 )
            i_user_end_time = i_end_time;
        else if ( i_user_time != -1 )
            i_user_end_time = i_user_time;
        else
            i_user_end_time = i_user_start_time;
    }

    return i_user_end_time;
}

chapter_item_c *chapter_item_c::FindTimecode( mtime_t i_user_timecode, const chapter_item_c * p_current, bool & b_found )
{
    chapter_item_c *psz_result = NULL;

    if ( p_current == this )
        b_found = true;

    if ( i_user_timecode >= i_user_start_time &&
        ( i_user_timecode < i_user_end_time ||
          ( i_user_start_time == i_user_end_time && i_user_timecode == i_user_end_time )))
    {
        std::vector<chapter_item_c*>::iterator index = sub_chapters.begin();
        while ( index != sub_chapters.end() && ((p_current == NULL && psz_result == NULL) || (p_current != NULL && (!b_found || psz_result == NULL))))
        {
            psz_result = (*index)->FindTimecode( i_user_timecode, p_current, b_found );
            index++;
        }
 
        if ( psz_result == NULL )
            psz_result = this;
    }

    return psz_result;
}

bool chapter_item_c::ParentOf( const chapter_item_c & item ) const
{
    if ( &item == this )
        return true;

    std::vector<chapter_item_c*>::const_iterator index = sub_chapters.begin();
    while ( index != sub_chapters.end() )
    {
        if ( (*index)->ParentOf( item ) )
            return true;
        index++;
    }

    return false;
}

bool chapter_item_c::Enter( bool b_do_subs )
{
    bool f_result = false;
    std::vector<chapter_codec_cmds_c*>::iterator index = codecs.begin();
    while ( index != codecs.end() )
    {
        f_result |= (*index)->Enter();
        index++;
    }

    if ( b_do_subs )
    {
        // sub chapters
        std::vector<chapter_item_c*>::iterator index_ = sub_chapters.begin();
        while ( index_ != sub_chapters.end() )
        {
            f_result |= (*index_)->Enter( true );
            index_++;
        }
    }
    return f_result;
}

bool chapter_item_c::Leave( bool b_do_subs )
{
    bool f_result = false;
    b_is_leaving = true;
    std::vector<chapter_codec_cmds_c*>::iterator index = codecs.begin();
    while ( index != codecs.end() )
    {
        f_result |= (*index)->Leave();
        index++;
    }

    if ( b_do_subs )
    {
        // sub chapters
        std::vector<chapter_item_c*>::iterator index_ = sub_chapters.begin();
        while ( index_ != sub_chapters.end() )
        {
            f_result |= (*index_)->Leave( true );
            index_++;
        }
    }
    b_is_leaving = false;
    return f_result;
}

bool chapter_item_c::EnterAndLeave( chapter_item_c *p_item, bool b_final_enter )
{
    chapter_item_c *p_common_parent = p_item;

    // leave, up to a common parent
    while ( p_common_parent != NULL && !p_common_parent->ParentOf( *this ) )
    {
        if ( !p_common_parent->b_is_leaving && p_common_parent->Leave( false ) )
            return true;
        p_common_parent = p_common_parent->psz_parent;
    }

    // enter from the parent to <this>
    if ( p_common_parent != NULL )
    {
        do
        {
            if ( p_common_parent == this )
                return Enter( true );

            for ( size_t i = 0; i<p_common_parent->sub_chapters.size(); i++ )
            {
                if ( p_common_parent->sub_chapters[i]->ParentOf( *this ) )
                {
                    p_common_parent = p_common_parent->sub_chapters[i];
                    if ( p_common_parent != this )
                        if ( p_common_parent->Enter( false ) )
                            return true;

                    break;
                }
            }
        } while ( 1 );
    }

    if ( b_final_enter )
        return Enter( true );
    else
        return false;
}



/* Chapter Edition Class */
std::string chapter_edition_c::GetMainName() const
{
    if ( sub_chapters.size() )
    {
        return sub_chapters[0]->GetCodecName( true );
    }
    return "";
}

void chapter_edition_c::RefreshChapters( )
{
    chapter_item_c::RefreshChapters( b_ordered, -1 );
    b_display_seekpoint = false;
}

mtime_t chapter_edition_c::Duration() const
{
    mtime_t i_result = 0;
 
    if ( sub_chapters.size() )
    {
        std::vector<chapter_item_c*>::const_iterator index = sub_chapters.end();
        index--;
        i_result = (*index)->i_user_end_time;
    }
 
    return i_result;
}

chapter_item_c * chapter_edition_c::FindTimecode( mtime_t i_timecode, const chapter_item_c * p_current )
{
    if ( !b_ordered )
        p_current = NULL;
    bool b_found_current = false;
    return chapter_item_c::FindTimecode( i_timecode, p_current, b_found_current );
}


