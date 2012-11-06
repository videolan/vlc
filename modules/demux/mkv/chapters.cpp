/*****************************************************************************
 * chapters.cpp : matroska demuxer
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

#include "chapters.hpp"

#include "chapter_command.hpp"

chapter_item_c::~chapter_item_c()
{
    if( p_segment_uid )
        delete p_segment_uid;
    if( p_segment_edition_uid )
        delete p_segment_edition_uid;
    vlc_delete_all( codecs );
    vlc_delete_all( sub_chapters );
}

chapter_item_c *chapter_item_c::BrowseCodecPrivate( unsigned int codec_id,
                                    bool (*match)(const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size ),
                                    const void *p_cookie,
                                    size_t i_cookie_size )
{
    VLC_UNUSED( codec_id );
    // this chapter
    std::vector<chapter_codec_cmds_c*>::const_iterator index = codecs.begin();
    while ( index != codecs.end() )
    {
        if ( match( **index ,p_cookie, i_cookie_size ) )
            return this;
        ++index;
    }
    return NULL;
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
        ++index;
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
        ++index;
    }

    return result;
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
        ++index;
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
        ++index;
    }

    if ( b_do_subs )
    {
        // sub chapters
        std::vector<chapter_item_c*>::iterator index_ = sub_chapters.begin();
        while ( index_ != sub_chapters.end() )
        {
            f_result |= (*index_)->Enter( true );
            ++index_;
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
        ++index;
    }

    if ( b_do_subs )
    {
        // sub chapters
        std::vector<chapter_item_c*>::iterator index_ = sub_chapters.begin();
        while ( index_ != sub_chapters.end() )
        {
            f_result |= (*index_)->Leave( true );
            ++index_;
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
        p_common_parent = p_common_parent->p_parent;
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

