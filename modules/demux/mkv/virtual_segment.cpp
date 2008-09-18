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

#include "virtual_segment.hpp"

#include "demux.hpp"

void virtual_segment_c::PrepareChapters( )
{
    if ( linked_segments.size() == 0 )
        return;

    // !!! should be called only once !!!
    matroska_segment_c *p_segment;
    size_t i, j;

    // copy editions from the first segment
    p_segment = linked_segments[0];
    p_editions = &p_segment->stored_editions;

    for ( i=1 ; i<linked_segments.size(); i++ )
    {
        p_segment = linked_segments[i];
        // FIXME assume we have the same editions in all segments
        for (j=0; j<p_segment->stored_editions.size(); j++)
        {
            if( j >= p_editions->size() ) /* Protect against broken files (?) */
                break;
            (*p_editions)[j]->Append( *p_segment->stored_editions[j] );
        }
    }
}

bool virtual_segment_c::UpdateCurrentToChapter( demux_t & demux )
{
    demux_sys_t & sys = *demux.p_sys;
    chapter_item_c *psz_curr_chapter;
    bool b_has_seeked = false;

    /* update current chapter/seekpoint */
    if ( p_editions->size() )
    {
        /* 1st, we need to know in which chapter we are */
        psz_curr_chapter = (*p_editions)[i_current_edition]->FindTimecode( sys.i_pts, psz_current_chapter );

        /* we have moved to a new chapter */
        if (psz_curr_chapter != NULL && psz_current_chapter != psz_curr_chapter)
        {
            if ( (*p_editions)[i_current_edition]->b_ordered )
            {
                // Leave/Enter up to the link point
                b_has_seeked = psz_curr_chapter->EnterAndLeave( psz_current_chapter );
                if ( !b_has_seeked )
                {
                    // only physically seek if necessary
                    if ( psz_current_chapter == NULL || (psz_current_chapter->i_end_time != psz_curr_chapter->i_start_time) )
                        Seek( demux, sys.i_pts, 0, psz_curr_chapter, -1 );
                }
            }
 
            if ( !b_has_seeked )
            {
                psz_current_chapter = psz_curr_chapter;
                if ( psz_curr_chapter->i_seekpoint_num > 0 )
                {
                    demux.info.i_update |= INPUT_UPDATE_TITLE | INPUT_UPDATE_SEEKPOINT;
                    demux.info.i_title = sys.i_current_title = i_sys_title;
                    demux.info.i_seekpoint = psz_curr_chapter->i_seekpoint_num - 1;
                }
            }

            return true;
        }
        else if (psz_curr_chapter == NULL)
        {
            // out of the scope of the data described by chapters, leave the edition
            if ( (*p_editions)[i_current_edition]->b_ordered && psz_current_chapter != NULL )
            {
                if ( !(*p_editions)[i_current_edition]->EnterAndLeave( psz_current_chapter, false ) )
                    psz_current_chapter = NULL;
                else
                    return true;
            }
        }
    }
    return false;
}

chapter_item_c *virtual_segment_c::BrowseCodecPrivate( unsigned int codec_id,
                                    bool (*match)(const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size ),
                                    const void *p_cookie,
                                    size_t i_cookie_size )
{
    // FIXME don't assume it is the first edition
    std::vector<chapter_edition_c*>::iterator index = p_editions->begin();
    if ( index != p_editions->end() )
    {
        chapter_item_c *p_result = (*index)->BrowseCodecPrivate( codec_id, match, p_cookie, i_cookie_size );
        if ( p_result != NULL )
            return p_result;
    }
    return NULL;
}


void virtual_segment_c::Sort()
{
    // keep the current segment index
    matroska_segment_c *p_segment = linked_segments[i_current_segment];

    std::sort( linked_segments.begin(), linked_segments.end(), matroska_segment_c::CompareSegmentUIDs );

    for ( i_current_segment=0; i_current_segment<linked_segments.size(); i_current_segment++)
        if ( linked_segments[i_current_segment] == p_segment )
            break;
}

size_t virtual_segment_c::AddSegment( matroska_segment_c *p_segment )
{
    size_t i;
    // check if it's not already in here
    for ( i=0; i<linked_segments.size(); i++ )
    {
        if ( linked_segments[i]->p_segment_uid != NULL
            && p_segment->p_segment_uid != NULL
            && *p_segment->p_segment_uid == *linked_segments[i]->p_segment_uid )
            return 0;
    }

    // find possible mates
    for ( i=0; i<linked_uids.size(); i++ )
    {
        if (   (p_segment->p_segment_uid != NULL && *p_segment->p_segment_uid == linked_uids[i])
            || (p_segment->p_prev_segment_uid != NULL && *p_segment->p_prev_segment_uid == linked_uids[i])
            || (p_segment->p_next_segment_uid !=NULL && *p_segment->p_next_segment_uid == linked_uids[i]) )
        {
            linked_segments.push_back( p_segment );

            AppendUID( p_segment->p_prev_segment_uid );
            AppendUID( p_segment->p_next_segment_uid );

            return 1;
        }
    }
    return 0;
}

void virtual_segment_c::PreloadLinked( )
{
    for ( size_t i=0; i<linked_segments.size(); i++ )
    {
        linked_segments[i]->Preload( );
    }
    i_current_edition = linked_segments[0]->i_default_edition;
}

mtime_t virtual_segment_c::Duration() const
{
    mtime_t i_duration;
    if ( linked_segments.size() == 0 )
        i_duration = 0;
    else {
        matroska_segment_c *p_last_segment = linked_segments[linked_segments.size()-1];
//        p_last_segment->ParseCluster( );

        i_duration = p_last_segment->i_start_time / 1000 + p_last_segment->i_duration;
    }
    return i_duration;
}

void virtual_segment_c::AppendUID( const EbmlBinary * p_UID )
{
    if ( p_UID == NULL )
        return;
    if ( p_UID->GetBuffer() == NULL )
        return;

    for (size_t i=0; i<linked_uids.size(); i++)
    {
        if ( *p_UID == linked_uids[i] )
            return;
    }
    linked_uids.push_back( *(KaxSegmentUID*)(p_UID) );
}

void virtual_segment_c::Seek( demux_t & demuxer, mtime_t i_date, mtime_t i_time_offset, chapter_item_c *psz_chapter, int64_t i_global_position )
{
    demux_sys_t *p_sys = demuxer.p_sys;
    size_t i;

    // find the actual time for an ordered edition
    if ( psz_chapter == NULL )
    {
        if ( Edition() && Edition()->b_ordered )
        {
            /* 1st, we need to know in which chapter we are */
            psz_chapter = (*p_editions)[i_current_edition]->FindTimecode( i_date, psz_current_chapter );
        }
    }

    if ( psz_chapter != NULL )
    {
        psz_current_chapter = psz_chapter;
        p_sys->i_chapter_time = i_time_offset = psz_chapter->i_user_start_time - psz_chapter->i_start_time;
        if ( psz_chapter->i_seekpoint_num > 0 )
        {
            demuxer.info.i_update |= INPUT_UPDATE_TITLE | INPUT_UPDATE_SEEKPOINT;
            demuxer.info.i_title = p_sys->i_current_title = i_sys_title;
            demuxer.info.i_seekpoint = psz_chapter->i_seekpoint_num - 1;
        }
    }

    // find the best matching segment
    for ( i=0; i<linked_segments.size(); i++ )
    {
        if ( i_date < linked_segments[i]->i_start_time )
            break;
    }

    if ( i > 0 )
        i--;

    if ( i_current_segment != i  )
    {
        linked_segments[i_current_segment]->UnSelect();
        linked_segments[i]->Select( i_date );
        i_current_segment = i;
    }

    linked_segments[i]->Seek( i_date, i_time_offset, i_global_position );
}

chapter_item_c *virtual_segment_c::FindChapter( int64_t i_find_uid )
{
    // FIXME don't assume it is the first edition
    std::vector<chapter_edition_c*>::iterator index = p_editions->begin();
    if ( index != p_editions->end() )
    {
        chapter_item_c *p_result = (*index)->FindChapter( i_find_uid );
        if ( p_result != NULL )
            return p_result;
    }
    return NULL;
}
