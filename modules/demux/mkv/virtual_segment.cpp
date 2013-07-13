/*****************************************************************************
 * virtual_segment.cpp : virtual segment implementation in the MKV demuxer
 *****************************************************************************
 * Copyright © 2003-2011 VideoLAN and VLC authors
 * $Id$
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
#include <vector>

#include "demux.hpp"

/* FIXME move this */
matroska_segment_c * getSegmentbyUID( KaxSegmentUID * p_uid, std::vector<matroska_segment_c*> *segments )
{
    for( size_t i = 0; i < (*segments).size(); i++ )
    {
        if( (*segments)[i]->p_segment_uid &&
            *p_uid == *((*segments)[i]->p_segment_uid) )
            return (*segments)[i];
    }
    return NULL;
}

virtual_chapter_c * virtual_chapter_c::CreateVirtualChapter( chapter_item_c * p_chap,
                                                             matroska_segment_c * p_main_segment,
                                                             std::vector<matroska_segment_c*> * segments,
                                                             int64_t * usertime_offset, bool b_ordered)
{
    matroska_segment_c * p_segment = p_main_segment;

    if( !p_chap )
    {
        /* Dummy chapter use the whole segment */
        return new virtual_chapter_c( p_segment, NULL, 0, p_segment->i_duration*1000 );
    }

    int64_t start = ( b_ordered )? *usertime_offset : p_chap->i_start_time;
    int64_t stop = ( b_ordered )? ( *usertime_offset + p_chap->i_end_time - p_chap->i_start_time ) : p_chap->i_end_time;

    if( p_chap->p_segment_uid && 
       ( !( p_segment = getSegmentbyUID( (KaxSegmentUID*) p_chap->p_segment_uid,segments ) ) || !b_ordered ) )
    {
        msg_Warn( &p_main_segment->sys.demuxer,
                  "Couldn't find segment 0x%x or not ordered... - ignoring chapter %s",
                  *( (uint32_t *) p_chap->p_segment_uid->GetBuffer() ),p_chap->psz_name.c_str() );
        return NULL;
    }

    /* Preload segment */
    if ( !p_segment->b_preloaded )
        p_segment->Preload();

    virtual_chapter_c * p_vchap = new virtual_chapter_c( p_segment, p_chap, start, stop );

    if( !p_vchap )
        return NULL;

    int64_t tmp = *usertime_offset;

    for( size_t i = 0; i < p_chap->sub_chapters.size(); i++ )
    {
        virtual_chapter_c * p_vsubchap = CreateVirtualChapter( p_chap->sub_chapters[i], p_segment, segments, &tmp, b_ordered );

        if( p_vsubchap )
            p_vchap->sub_chapters.push_back( p_vsubchap );
    }

    if( tmp == *usertime_offset )
        *usertime_offset += p_chap->i_end_time - p_chap->i_start_time;
    else
        *usertime_offset = tmp;

    msg_Dbg( &p_main_segment->sys.demuxer,
             "Virtual chapter %s from %"PRId64" to %"PRId64" - " ,
             p_chap->psz_name.c_str(), p_vchap->i_virtual_start_time, p_vchap->i_virtual_stop_time );

    return p_vchap;
}

virtual_chapter_c::~virtual_chapter_c()
{
    for( size_t i = 0 ; i < sub_chapters.size(); i++ )
        delete sub_chapters[i];
}


virtual_edition_c::virtual_edition_c( chapter_edition_c * p_edit, std::vector<matroska_segment_c*> *opened_segments)
{
    bool b_fake_ordered = false;
    matroska_segment_c *p_main_segment = (*opened_segments)[0];
    p_edition = p_edit;
    b_ordered = false;

    int64_t usertime_offset = 0;

    /* ordered chapters */
    if( p_edition && p_edition->b_ordered )
    {
        b_ordered = true;
        for( size_t i = 0; i < p_edition->sub_chapters.size(); i++ )
        {
            virtual_chapter_c * p_vchap = virtual_chapter_c::CreateVirtualChapter( p_edition->sub_chapters[i],
                                                                                   p_main_segment, opened_segments,
                                                                                   &usertime_offset, b_ordered );
            if( p_vchap )
                chapters.push_back( p_vchap );
        }
        if( chapters.size() )
            i_duration = chapters[ chapters.size() - 1 ]->i_virtual_stop_time;
        else
            i_duration = 0; /* Empty ordered editions will be ignored */
    }
    else /* Not ordered or no edition at all */
    {
        matroska_segment_c * p_cur = p_main_segment;
        virtual_chapter_c * p_vchap = NULL;
        int64_t tmp = 0;

        /* check for prev linked segments */
        /* FIXME to avoid infinite recursion we limit to 10 prev should be better as parameter */
        for( int limit = 0; limit < 10 && p_cur->p_prev_segment_uid ; limit++ )
        {
            matroska_segment_c * p_prev = NULL;
            if( ( p_prev = getSegmentbyUID( p_cur->p_prev_segment_uid, opened_segments ) ) )
            {
                tmp = 0;
                msg_Dbg( &p_main_segment->sys.demuxer, "Prev segment 0x%x found\n",
                         *(int32_t*)p_cur->p_prev_segment_uid->GetBuffer() );

                /* Preload segment */
                if ( !p_prev->b_preloaded )
                    p_prev->Preload();

                /* Create virtual_chapter from the first edition if any */
                chapter_item_c * p_chap = ( p_prev->stored_editions.size() > 0 )? ((chapter_item_c *)p_prev->stored_editions[0]) : NULL;

                p_vchap = virtual_chapter_c::CreateVirtualChapter( p_chap, p_prev, opened_segments, &tmp, b_ordered );

                if( p_vchap )
                    chapters.insert( chapters.begin(), p_vchap );

                p_cur = p_prev;
                b_fake_ordered = true;
            }
            else /* segment not found */
                break;
        }

        tmp = 0;

        /* Append the main segment */
        p_vchap = virtual_chapter_c::CreateVirtualChapter( (chapter_item_c*) p_edit, p_main_segment,
                                                           opened_segments, &tmp, b_ordered );
        if( p_vchap )
            chapters.push_back( p_vchap );

        /* Append next linked segments */
        for( int limit = 0; limit < 10 && p_cur->p_next_segment_uid; limit++ )
        {
            matroska_segment_c * p_next = NULL;
            if( ( p_next = getSegmentbyUID( p_cur->p_next_segment_uid, opened_segments ) ) )
            {
                tmp = 0;
                msg_Dbg( &p_main_segment->sys.demuxer, "Next segment 0x%x found\n",
                         *(int32_t*) p_cur->p_next_segment_uid->GetBuffer() );

                /* Preload segment */
                if ( !p_next->b_preloaded )
                    p_next->Preload();

                /* Create virtual_chapter from the first edition if any */
                chapter_item_c * p_chap = ( p_next->stored_editions.size() > 0 )?( (chapter_item_c *)p_next->stored_editions[0] ) : NULL;

                 p_vchap = virtual_chapter_c::CreateVirtualChapter( p_chap, p_next, opened_segments, &tmp, b_ordered );

                if( p_vchap )
                    chapters.push_back( p_vchap );


                p_cur = p_next;
                b_fake_ordered = true;
            }
            else /* segment not found */
                break;
        }

        /* Retime chapters */
        retimeChapters();
        if(b_fake_ordered)
            b_ordered = true;
    }

#if MKV_DEBUG
    msg_Dbg( &p_main_segment->sys.demuxer, "-- RECAP-BEGIN --" );
    print();
    msg_Dbg( &p_main_segment->sys.demuxer, "-- RECAP-END --" );
#endif
}

virtual_edition_c::~virtual_edition_c()
{
    for( size_t i = 0; i < chapters.size(); i++ )
        delete chapters[i];
}

void virtual_edition_c::retimeSubChapters( virtual_chapter_c * p_vchap )
{
    int64_t stop_time = p_vchap->i_virtual_stop_time;
    for( size_t i = p_vchap->sub_chapters.size(); i-- > 0; )
    {
        virtual_chapter_c * p_vsubchap = p_vchap->sub_chapters[i];
        p_vsubchap->i_virtual_start_time += p_vchap->i_virtual_start_time;

        /*FIXME we artificially extend stop time if they were there before...*/
        /* Just for comfort*/
        p_vsubchap->i_virtual_stop_time = stop_time;
        stop_time = p_vsubchap->i_virtual_start_time;

        retimeSubChapters( p_vsubchap );
    }
}

void virtual_edition_c::retimeChapters()
{
    /* This function is just meant to be used on unordered chapters */
    if( b_ordered )
        return;

    i_duration = 0;

    /* On non ordered editions we have one top chapter == one segment */
    for( size_t i = 0; i < chapters.size(); i++ )
    {
        virtual_chapter_c * p_vchap = chapters[i];

        p_vchap->i_virtual_start_time = i_duration;
        i_duration += p_vchap->p_segment->i_duration * 1000;
        p_vchap->i_virtual_stop_time = i_duration;

        retimeSubChapters( p_vchap );
    }
}

virtual_segment_c::virtual_segment_c( std::vector<matroska_segment_c*> * p_opened_segments )
{
    /* Main segment */
    size_t i;
    matroska_segment_c *p_segment = (*p_opened_segments)[0];
    i_current_edition = 0;
    i_sys_title = 0;
    p_current_chapter = NULL;

    for( i = 0; i < p_segment->stored_editions.size(); i++ )
    {
        /* Create a virtual edition from opened */
        virtual_edition_c * p_vedition = new virtual_edition_c( p_segment->stored_editions[i], p_opened_segments );

        /* Ordered empty edition can happen when all chapters are 
         * on an other segment which couldn't be found... ignore it */
        if(p_vedition->b_ordered && p_vedition->i_duration == 0)
        {
            msg_Warn( &p_segment->sys.demuxer,
                      "Edition %s (%zu) links to other segments not found and is empty... ignoring it",
                       p_vedition->GetMainName().c_str(), i );
            delete p_vedition;
        }
        else
            editions.push_back( p_vedition );
    }
    /*if we don't have edition create a dummy one*/
    if( !p_segment->stored_editions.size() )
    {
        virtual_edition_c * p_vedition = new virtual_edition_c( NULL, p_opened_segments );
        editions.push_back( p_vedition );
    }

    /* Get the default edition, if there is none, use the first one */
    for( i = 0; i < editions.size(); i++)
    {
        if( editions[i]->p_edition && editions[i]->p_edition->b_default )
        {
            i_current_edition = i;
            break;
        }
    } 
    /* Set current chapter */
    p_current_chapter = editions[i_current_edition]->getChapterbyTimecode(0);

}

virtual_segment_c::~virtual_segment_c()
{
    for( size_t i = 0; i < editions.size(); i++ )
        delete editions[i];
}

virtual_chapter_c *virtual_segment_c::BrowseCodecPrivate( unsigned int codec_id,
                                    bool (*match)(const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size ),
                                    const void *p_cookie,
                                    size_t i_cookie_size )
{
    virtual_edition_c * p_ved = CurrentEdition();
    if( p_ved )
        return p_ved->BrowseCodecPrivate( codec_id, match, p_cookie, i_cookie_size );

    return NULL;
}


virtual_chapter_c * virtual_edition_c::BrowseCodecPrivate( unsigned int codec_id,
                                    bool (*match)(const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size ),
                                    const void *p_cookie,
                                    size_t i_cookie_size )
{
    if( !p_edition )
        return NULL;

    for( size_t i = 0; i < chapters.size(); i++ )
    {
        virtual_chapter_c * p_result = chapters[i]->BrowseCodecPrivate( codec_id, match, p_cookie, i_cookie_size );
        if( p_result )
            return p_result;
    }
    return NULL;
}



virtual_chapter_c * virtual_chapter_c::BrowseCodecPrivate( unsigned int codec_id,
                                    bool (*match)(const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size ),
                                    const void *p_cookie,
                                    size_t i_cookie_size )
{
    if( !p_chapter )
        return NULL;

    if( p_chapter->BrowseCodecPrivate( codec_id, match, p_cookie, i_cookie_size ) )
        return this;

    for( size_t i = 0; i < sub_chapters.size(); i++ )
    {
        virtual_chapter_c * p_result = sub_chapters[i]->BrowseCodecPrivate( codec_id, match, p_cookie, i_cookie_size );
        if( p_result )
            return p_result;
    }
    return NULL;
}

virtual_chapter_c* virtual_chapter_c::getSubChapterbyTimecode( int64_t time )
{
    for( size_t i = 0; i < sub_chapters.size(); i++ )
    {
        if( time >= sub_chapters[i]->i_virtual_start_time && time < sub_chapters[i]->i_virtual_stop_time )
            return sub_chapters[i]->getSubChapterbyTimecode( time );
    }

    return this;
}

virtual_chapter_c* virtual_edition_c::getChapterbyTimecode( int64_t time )
{
    for( size_t i = 0; i < chapters.size(); i++ )
    {
        if( time >= chapters[i]->i_virtual_start_time &&
            ( chapters[i]->i_virtual_stop_time < 0 || time < chapters[i]->i_virtual_stop_time ) )
            /*with the current implementation only the last chapter can have a negative virtual_stop_time*/
            return chapters[i]->getSubChapterbyTimecode( time );
    }

    return NULL;
}

bool virtual_segment_c::UpdateCurrentToChapter( demux_t & demux )
{
    demux_sys_t & sys = *demux.p_sys;
    virtual_chapter_c *p_cur_chapter;
    virtual_edition_c * p_cur_edition = editions[ i_current_edition ];

    bool b_has_seeked = false;

    p_cur_chapter = p_cur_edition->getChapterbyTimecode( sys.i_pts );

    /* we have moved to a new chapter */
    if ( p_cur_chapter != NULL && p_current_chapter != p_cur_chapter )
        {
            msg_Dbg( &demux, "NEW CHAPTER %"PRId64, sys.i_pts );
            if ( p_cur_edition->b_ordered )
            {
                /* FIXME EnterAndLeave has probably been broken for a long time */
                // Leave/Enter up to the link point
                b_has_seeked = p_cur_chapter->EnterAndLeave( p_current_chapter );
                if ( !b_has_seeked )
                {
                    // only physically seek if necessary
                    if ( p_current_chapter == NULL ||
                        ( p_current_chapter && p_current_chapter->p_segment != p_cur_chapter->p_segment ) ||
                        ( p_current_chapter->p_chapter->i_end_time != p_cur_chapter->p_chapter->i_start_time ))
                    {
                        Seek( demux, p_cur_chapter->i_virtual_start_time, 0, p_cur_chapter, -1 );
                        return true;
                    }
                }
                sys.i_start_pts = p_cur_chapter->i_virtual_start_time;;
            }

            p_current_chapter = p_cur_chapter;
            if ( p_cur_chapter->i_seekpoint_num > 0 )
            {
                demux.info.i_update |= INPUT_UPDATE_TITLE | INPUT_UPDATE_SEEKPOINT;
                demux.info.i_title = sys.i_current_title = i_sys_title;
                demux.info.i_seekpoint = p_cur_chapter->i_seekpoint_num - 1;
            }

            return b_has_seeked;
        }
        else if ( p_cur_chapter == NULL )
        {
            /* out of the scope of the data described by chapters, leave the edition */
            if ( p_cur_edition->b_ordered && p_current_chapter != NULL )
            {
                /* TODO */
                if ( !p_cur_edition->p_edition->EnterAndLeave( p_current_chapter->p_chapter, false ) )
                    p_current_chapter = NULL;
                else
                    return true;
            }
        }
    return false;
}

bool virtual_chapter_c::EnterAndLeave( virtual_chapter_c *p_item, bool b_enter )
{
    if( !p_chapter )
        return false;

    return p_chapter->EnterAndLeave( p_item->p_chapter, b_enter );
}

void virtual_segment_c::Seek( demux_t & demuxer, mtime_t i_date, mtime_t i_time_offset, 
                              virtual_chapter_c *p_chapter, int64_t i_global_position )
{
    demux_sys_t *p_sys = demuxer.p_sys;


    /* find the actual time for an ordered edition */
    if ( p_chapter == NULL )
        /* 1st, we need to know in which chapter we are */
        p_chapter = editions[ i_current_edition ]->getChapterbyTimecode( i_date );

    if ( p_chapter != NULL )
    {
        i_time_offset = p_chapter->i_virtual_start_time - ( ( p_chapter->p_chapter )? p_chapter->p_chapter->i_start_time : 0 );
        p_sys->i_chapter_time = i_time_offset - p_chapter->p_segment->i_start_time;
        if ( p_chapter->p_chapter && p_chapter->i_seekpoint_num > 0 )
        {
            demuxer.info.i_update |= INPUT_UPDATE_TITLE | INPUT_UPDATE_SEEKPOINT;
            demuxer.info.i_title = p_sys->i_current_title = i_sys_title;
            demuxer.info.i_seekpoint = p_chapter->i_seekpoint_num - 1;
        }

        if( p_current_chapter->p_segment != p_chapter->p_segment )
            ChangeSegment( p_current_chapter->p_segment, p_chapter->p_segment, i_date );
        p_current_chapter = p_chapter;

        p_chapter->p_segment->Seek( i_date, i_time_offset, i_global_position );
    }
}

virtual_chapter_c * virtual_chapter_c::FindChapter( int64_t i_find_uid )
{
    if( p_chapter && ( p_chapter->i_uid == i_find_uid ) )
        return this;

    for( size_t i = 0; i < sub_chapters.size(); i++ )
    {
        virtual_chapter_c * p_res = sub_chapters[i]->FindChapter( i_find_uid );
        if( p_res )
            return p_res;
    }

    return NULL;
}

virtual_chapter_c * virtual_segment_c::FindChapter( int64_t i_find_uid )
{
    virtual_edition_c * p_edition = editions[i_current_edition];

    for( size_t i = 0; p_edition->chapters.size(); i++ )
    {
        virtual_chapter_c * p_chapter = p_edition->chapters[i]->FindChapter( i_find_uid );
        if( p_chapter )
            return p_chapter;
    }
    return NULL;
}

int virtual_chapter_c::PublishChapters( input_title_t & title, int & i_user_chapters, int i_level )
{
    if ( p_chapter && ( !p_chapter->b_display_seekpoint || p_chapter->psz_name == "" ) )
    {
        p_chapter->psz_name = p_chapter->GetCodecName();
        if ( p_chapter->psz_name != "" )
            p_chapter->b_display_seekpoint = true;
    }

    if ( ( p_chapter && p_chapter->b_display_seekpoint &&
         ( ( sub_chapters.size() > 0 && i_virtual_start_time != sub_chapters[0]->i_virtual_start_time) ||
           sub_chapters.size() == 0 ) ) || !p_chapter )
    {
        seekpoint_t *sk = vlc_seekpoint_New();

        sk->i_time_offset = i_virtual_start_time;
        if( p_chapter )
            sk->psz_name = strdup( p_chapter->psz_name.c_str() );
        else
            sk->psz_name = strdup("dummy chapter");

        /* A start time of '0' is ok. A missing ChapterTime element is ok, too, because '0' is its default value. */
        title.i_seekpoint++;
        title.seekpoint = (seekpoint_t**)xrealloc( title.seekpoint,
                                 title.i_seekpoint * sizeof( seekpoint_t* ) );
        title.seekpoint[title.i_seekpoint-1] = sk;

        if ( (p_chapter && p_chapter->b_user_display ) ||  !p_chapter )
            i_user_chapters++;
    }
    i_seekpoint_num = i_user_chapters;

    for( size_t i = 0; i < sub_chapters.size(); i++ )
        sub_chapters[i]->PublishChapters( title, i_user_chapters, i_level + 1 );

    return i_user_chapters;
}


int virtual_edition_c::PublishChapters( input_title_t & title, int & i_user_chapters, int i_level )
{

    /* HACK for now don't expose edition as a seekpoint if its start time is the same than it's first chapter */
    if( chapters.size() > 0 &&
        chapters[0]->i_virtual_start_time && p_edition )
    {
        seekpoint_t *sk = vlc_seekpoint_New();

        sk->i_time_offset = 0;
        sk->psz_name = strdup( p_edition->psz_name.c_str() );

        title.i_seekpoint++;
        title.seekpoint = (seekpoint_t**)xrealloc( title.seekpoint,
                             title.i_seekpoint * sizeof( seekpoint_t* ) );
        title.seekpoint[title.i_seekpoint - 1] = sk;
        i_level++;

        i_user_chapters++;
        i_seekpoint_num = i_user_chapters;
    }

//    if( chapters.size() > 1 )
        for( size_t i = 0; i < chapters.size(); i++ )
            chapters[i]->PublishChapters( title, i_user_chapters, i_level );

    return i_user_chapters;
}

std::string virtual_edition_c::GetMainName()
{
    if( p_edition )
        return p_edition->GetMainName();

    return std::string("");
}

bool virtual_chapter_c::Enter( bool b_do_subs )
{
    if( p_chapter )
        return p_chapter->Enter( b_do_subs );
    return false;
}

bool virtual_chapter_c::Leave( bool b_do_subs )
{
    if( p_chapter )
        return p_chapter->Leave( b_do_subs );
    return false;
}

#if MKV_DEBUG
void virtual_chapter_c::print() 
{
    msg_Dbg( &p_segment->sys.demuxer, "*** chapter %"PRId64" - %"PRId64" (%u)",
             i_virtual_start_time, i_virtual_stop_time, sub_chapters.size() );
    for( size_t i = 0; i < sub_chapters.size(); i++ )
        sub_chapters[i]->print();
}
#endif

void virtual_segment_c::ChangeSegment( matroska_segment_c * p_old, matroska_segment_c * p_new, mtime_t i_start_time )
{
    size_t i, j;
    for( i = 0; i < p_new->tracks.size(); i++)
    {
        mkv_track_t *p_tk = p_new->tracks[i];
        es_format_t *p_nfmt = &p_tk->fmt;

        /* Let's only do that for audio and video for now */
        if( p_nfmt->i_cat == AUDIO_ES || p_nfmt->i_cat == VIDEO_ES )
        {
            
            /* check for a similar elementary stream */
            for( j = 0; j < p_old->tracks.size(); j++)
            {
                es_format_t * p_ofmt = &p_old->tracks[j]->fmt;

                if( !p_old->tracks[j]->p_es )
                    continue;

                if( ( p_nfmt->i_cat == p_ofmt->i_cat ) &&
                    ( p_nfmt->i_codec == p_ofmt->i_codec ) &&
                    ( p_nfmt->i_priority == p_ofmt->i_priority ) &&
                    ( p_nfmt->i_bitrate == p_ofmt->i_bitrate ) &&
                    ( p_nfmt->i_extra == p_ofmt->i_extra ) &&
                    ( (!p_nfmt->p_extra && !p_ofmt->p_extra) || 
                      !memcmp( p_nfmt->p_extra, p_ofmt->p_extra, p_nfmt->i_extra ) ) &&
                    !strcasecmp( p_nfmt->psz_language, p_ofmt->psz_language ) &&
                    ( ( p_nfmt->i_cat == AUDIO_ES && 
                        !memcmp( &p_nfmt->audio, &p_ofmt->audio, sizeof(audio_format_t) ) ) ||
                      ( p_nfmt->i_cat == VIDEO_ES && 
                        !memcmp( &p_nfmt->video, &p_ofmt->video, sizeof(video_format_t) ) ) ) )
                {
                    /* FIXME handle video palettes... */
                    msg_Warn( &p_old->sys.demuxer, "Reusing decoder of old track %zu for track %zu", j, i);
                    p_tk->p_es = p_old->tracks[j]->p_es;
                    p_old->tracks[j]->p_es = NULL;
                    break;
                }
            }
        }
    }
    p_new->Select( i_start_time );
    p_old->UnSelect();
}
