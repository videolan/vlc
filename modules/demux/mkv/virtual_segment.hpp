/*****************************************************************************
 * virtual_segment.hpp : virtual segment implementation in the MKV demuxer
 *****************************************************************************
 * Copyright © 2003-2011 VideoLAN and VLC authors
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Steve Lhomme <steve.lhomme@free.fr>
 *          Denis Charmet <typx@dinauz.org>
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

#ifndef VLC_MKV_VIRTUAL_SEGMENT_HPP_
#define VLC_MKV_VIRTUAL_SEGMENT_HPP_

#include "mkv.hpp"

#include "matroska_segment.hpp"
#include "chapters.hpp"

namespace mkv {

/* virtual classes don't own anything but virtual elements so they shouldn't have to delete anything */

class virtual_chapter_c
{
public:
    virtual_chapter_c( matroska_segment_c &seg, chapter_item_c *p_chap, vlc_tick_t start, vlc_tick_t stop, std::vector<virtual_chapter_c *> & sub_chaps ):
        segment(seg), p_chapter(p_chap),
        i_mk_virtual_start_time(start), i_mk_virtual_stop_time(stop),
        sub_vchapters(sub_chaps)
    {}
    ~virtual_chapter_c();

    static virtual_chapter_c * CreateVirtualChapter( chapter_item_c * p_chap,
                                                     matroska_segment_c & main_segment,
                                                     std::vector<matroska_segment_c*> & segments,
                                                     vlc_tick_t & usertime_offset, bool b_ordered );

    virtual_chapter_c* getSubChapterbyTimecode( vlc_tick_t time );
    bool Leave( );
    bool EnterAndLeave( virtual_chapter_c *p_leaving_vchapter, bool b_enter = true );
    virtual_chapter_c * FindChapter( int64_t i_find_uid );
    int PublishChapters( input_title_t & title, int & i_user_chapters, int i_level, bool allow_no_name );

    virtual_chapter_c * BrowseCodecPrivate( unsigned int codec_id,
                                            bool (*match)( const chapter_codec_cmds_c &data,
                                                           const void *p_cookie,
                                                           size_t i_cookie_size ),
                                            const void *p_cookie,
                                            size_t i_cookie_size );
    bool Enter( bool b_do_subs );
    bool Leave( bool b_do_subs );

    static bool CompareTimecode( const virtual_chapter_c * itemA, const virtual_chapter_c * itemB )
    {
        return ( itemA->i_mk_virtual_start_time < itemB->i_mk_virtual_start_time );
    }

    bool ContainsTimestamp( vlc_tick_t i_pts );

    matroska_segment_c  &segment;
    chapter_item_c      *p_chapter;
    vlc_tick_t          i_mk_virtual_start_time;
    vlc_tick_t          i_mk_virtual_stop_time;
    int                 i_seekpoint_num;
    std::vector<virtual_chapter_c *> sub_vchapters;
#ifdef MKV_DEBUG
    void print();
#endif
};

class virtual_edition_c
{
public:
    virtual_edition_c( chapter_edition_c * p_edition, matroska_segment_c & main_segment, std::vector<matroska_segment_c*> & opened_segments );
    ~virtual_edition_c();
    std::vector<virtual_chapter_c*> vchapters;

    virtual_chapter_c* getChapterbyTimecode( vlc_tick_t time );
    std::string GetMainName();
    int PublishChapters( input_title_t & title, int & i_user_chapters, int i_level );
    virtual_chapter_c * BrowseCodecPrivate( unsigned int codec_id,
                                            bool (*match)( const chapter_codec_cmds_c &data,
                                                           const void *p_cookie,
                                                           size_t i_cookie_size ),
                                             const void *p_cookie, size_t i_cookie_size );

    bool                b_ordered;
    vlc_tick_t          i_duration;
    chapter_edition_c   *p_edition;
    int                 i_seekpoint_num;

private:
    void retimeChapters();
    void retimeSubChapters( virtual_chapter_c * p_vchap );
#ifdef MKV_DEBUG
    void print(){ for( size_t i = 0; i<vchapters.size(); i++ ) vchapters[i]->print(); }
#endif

};

// class holding hard-linked segment together in the playback order
class virtual_segment_c
{
public:
    virtual_segment_c( matroska_segment_c & segment, std::vector<matroska_segment_c*> & opened_segments );
    ~virtual_segment_c();
    std::vector<virtual_edition_c*> veditions;
    std::vector<virtual_edition_c*>::size_type i_current_edition;
    virtual_chapter_c               *p_current_vchapter;
    bool                            b_current_vchapter_entered;
    int                             i_sys_title;


    inline virtual_edition_c * CurrentEdition()
    {
        if( i_current_edition < veditions.size() )
            return veditions[i_current_edition];
        return NULL;
    }

    virtual_chapter_c * CurrentChapter() const
    {
        return p_current_vchapter;
    }

    matroska_segment_c * CurrentSegment() const
    {
        if ( !p_current_vchapter )
            return NULL;
        return &p_current_vchapter->segment;
    }

    inline vlc_tick_t Duration()
    {
        return veditions[i_current_edition]->i_duration;
    }

    inline std::vector<virtual_edition_c*>* Editions() { return &veditions; }

    virtual_chapter_c *BrowseCodecPrivate( unsigned int codec_id,
                                           bool (*match)( const chapter_codec_cmds_c &data,
                                                          const void *p_cookie,
                                                          size_t i_cookie_size ),
                                           const void *p_cookie,
                                           size_t i_cookie_size );

    virtual_chapter_c * FindChapter( int64_t i_find_uid );

    bool UpdateCurrentToChapter( demux_t & demux );
    bool Seek( demux_t & demuxer, vlc_tick_t i_mk_date, virtual_chapter_c *p_vchapter, bool b_precise = true );
private:
    void KeepTrackSelection( matroska_segment_c & old, matroska_segment_c & next );
};

} // namespace

#endif
