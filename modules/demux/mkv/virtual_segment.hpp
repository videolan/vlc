
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

#ifndef _VIRTUAL_SEGMENT_HPP_
#define _VIRTUAL_SEGMENT_HPP_

#include "mkv.hpp"

#include "matroska_segment.hpp"
#include "chapters.hpp"

// class holding hard-linked segment together in the playback order
class virtual_segment_c
{
public:
    virtual_segment_c( matroska_segment_c *p_segment )
        :p_editions(NULL)
        ,i_sys_title(0)
        ,i_current_segment(0)
        ,i_current_edition(-1)
        ,psz_current_chapter(NULL)
    {
        linked_segments.push_back( p_segment );

        AppendUID( p_segment->p_segment_uid );
        AppendUID( p_segment->p_prev_segment_uid );
        AppendUID( p_segment->p_next_segment_uid );
    }

    void Sort();
    size_t AddSegment( matroska_segment_c *p_segment );
    void PreloadLinked( );
    mtime_t Duration( ) const;
    void Seek( demux_t & demuxer, mtime_t i_date, mtime_t i_time_offset, chapter_item_c *psz_chapter, int64_t i_global_position );

    inline chapter_edition_c *Edition()
    {
        if ( i_current_edition >= 0 && size_t(i_current_edition) < p_editions->size() )
            return (*p_editions)[i_current_edition];
        return NULL;
    }

    matroska_segment_c * Segment() const
    {
        if ( linked_segments.size() == 0 || i_current_segment >= linked_segments.size() )
            return NULL;
        return linked_segments[i_current_segment];
    }

    inline chapter_item_c *CurrentChapter() {
        return psz_current_chapter;
    }

    bool SelectNext()
    {
        if ( i_current_segment < linked_segments.size()-1 )
        {
            i_current_segment++;
            return true;
        }
        return false;
    }

    bool FindUID( KaxSegmentUID & uid ) const
    {
        for ( size_t i=0; i<linked_uids.size(); i++ )
        {
            if ( linked_uids[i] == uid )
                return true;
        }
        return false;
    }

    bool UpdateCurrentToChapter( demux_t & demux );
    void PrepareChapters( );

    chapter_item_c *BrowseCodecPrivate( unsigned int codec_id,
                                        bool (*match)(const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size ),
                                        const void *p_cookie,
                                        size_t i_cookie_size );
    chapter_item_c *FindChapter( int64_t i_find_uid );

    std::vector<chapter_edition_c*>  *p_editions;
    int                              i_sys_title;

protected:
    std::vector<matroska_segment_c*> linked_segments;
    std::vector<KaxSegmentUID>       linked_uids;
    size_t                           i_current_segment;

    int                              i_current_edition;
    chapter_item_c                   *psz_current_chapter;

    void                             AppendUID( const EbmlBinary * UID );
};

#endif
