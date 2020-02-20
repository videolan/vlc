
/*****************************************************************************
 * mkv.cpp : matroska demuxer
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

#include "demux.hpp"
#include "stream_io_callback.hpp"
#include "Ebml_parser.hpp"

#include <vlc_actions.h>

namespace mkv {

demux_sys_t::~demux_sys_t()
{
    size_t i;
    for ( i=0; i<streams.size(); i++ )
        delete streams[i];
    for ( i=0; i<opened_segments.size(); i++ )
        delete opened_segments[i];
    for ( i=0; i<used_vsegments.size(); i++ )
        delete used_vsegments[i];
    for ( i=0; i<stored_attachments.size(); i++ )
        delete stored_attachments[i];
    if( meta ) vlc_meta_Delete( meta );

    while( titles.size() )
    { vlc_input_title_Delete( titles.back() ); titles.pop_back();}
}


bool demux_sys_t::AnalyseAllSegmentsFound( demux_t *p_demux, matroska_stream_c *p_stream1 )
{
    int i_upper_lvl = 0;
    EbmlElement *p_l0;
    bool b_keep_stream = false;

    /* verify the EBML Header... it shouldn't be bigger than 1kB */
    p_l0 = p_stream1->estream.FindNextID(EBML_INFO(EbmlHead), 1024);
    if (p_l0 == NULL)
    {
        msg_Err( p_demux, "No EBML header found" );
        return false;
    }

    /* verify we can read this Segment */
    try
    {
        p_l0->Read( p_stream1->estream, EBML_CLASS_CONTEXT(EbmlHead), i_upper_lvl, p_l0, true);
    }
    catch(...)
    {
        msg_Err(p_demux, "EBML Header Read failed");
        return false;
    }

    EDocType doc_type = GetChild<EDocType>(*static_cast<EbmlHead*>(p_l0));
    if (std::string(doc_type) != "matroska" && std::string(doc_type) != "webm" )
    {
        msg_Err( p_demux, "Not a Matroska file : DocType = %s ", std::string(doc_type).c_str());
        return false;
    }

    EDocTypeReadVersion doc_read_version = GetChild<EDocTypeReadVersion>(*static_cast<EbmlHead*>(p_l0));
    if (uint64(doc_read_version) > 2)
    {
        msg_Err( p_demux, "matroska file needs version %" PRId64 " but only versions 1 & 2 supported", uint64(doc_read_version));
        return false;
    }

    delete p_l0;


    // find all segments in this file
    p_l0 = p_stream1->estream.FindNextID(EBML_INFO(KaxSegment), UINT64_MAX);
    if (p_l0 == NULL)
    {
        msg_Err( p_demux, "No segment found" );
        return false;
    }

    while (p_l0 != 0)
    {
        bool b_l0_handled = false;

        if ( MKV_IS_ID( p_l0, KaxSegment) )
        {
            matroska_segment_c *p_segment1 = new matroska_segment_c( *this, p_stream1->estream, (KaxSegment*)p_l0 );

            p_segment1->Preload();

            if ( !p_segment1->p_segment_uid ||
                 FindSegment( *p_segment1->p_segment_uid ) == NULL)
            {
                opened_segments.push_back( p_segment1 );
                b_keep_stream = true;
                p_stream1->segments.push_back( p_segment1 );
            }
            else
            {
                p_segment1->segment = NULL;
                delete p_segment1;
            }

            b_l0_handled = true;
        }

        if ( !b_seekable )
            break;

        EbmlElement* p_l0_prev = p_l0;

        if (p_l0->IsFiniteSize() )
        {
            p_l0->SkipData(p_stream1->estream, KaxMatroska_Context);
            p_l0 = p_stream1->estream.FindNextID(EBML_INFO(KaxSegment), UINT64_MAX);
        }
        else
        {
            p_l0 = NULL;
        }

        if( b_l0_handled == false )
            delete p_l0_prev;
    }

    if ( !b_keep_stream )
        return false;

    return true;
}

void demux_sys_t::PreloadFamily( const matroska_segment_c & of_segment )
{
    for (size_t i=0; i<opened_segments.size(); i++)
    {
        opened_segments[i]->PreloadFamily( of_segment );
    }
}

// preload all the linked segments for all preloaded segments
bool demux_sys_t::PreloadLinked()
{
    size_t i, j, ij = 0;
    virtual_segment_c *p_vseg;

    if ( unlikely(opened_segments.size() == 0) )
        return false;

    p_current_vsegment = new (std::nothrow) virtual_segment_c( *(opened_segments[0]), opened_segments );
    if ( !p_current_vsegment )
        return false;

    if ( unlikely(p_current_vsegment->CurrentEdition() == NULL) )
        return false;

    /* Set current chapter */
    p_current_vsegment->p_current_vchapter = p_current_vsegment->CurrentEdition()->getChapterbyTimecode(0);
    msg_Dbg( &demuxer, "NEW START CHAPTER uid=%" PRId64, p_current_vsegment->p_current_vchapter && p_current_vsegment->p_current_vchapter->p_chapter ?
                 p_current_vsegment->p_current_vchapter->p_chapter->i_uid : 0 );

    used_vsegments.push_back( p_current_vsegment );

    for ( i=1; i< opened_segments.size(); i++ )
    {
        /* add segments from the same family to used_segments */
        if ( opened_segments[0]->SameFamily( *(opened_segments[i]) ) )
        {
            virtual_segment_c *p_vsegment = new (std::nothrow) virtual_segment_c( *(opened_segments[i]), opened_segments );
            if ( likely(p_vsegment != NULL) )
                used_vsegments.push_back( p_vsegment );
        }
    }

    // publish all editions of all usable segment
    for ( i=0; i< used_vsegments.size(); i++ )
    {
        p_vseg = used_vsegments[i];
        if ( p_vseg->Editions() != NULL )
        {
            for ( j=0; j<p_vseg->Editions()->size(); j++ )
            {
                virtual_edition_c * p_ved = (*p_vseg->Editions())[j];
                input_title_t *p_title = vlc_input_title_New();
                int i_chapters;

                // TODO use a name for each edition, let the TITLE deal with a codec name
                if ( p_title->psz_name == NULL )
                {
                    if( p_ved->GetMainName().length() )
                        p_title->psz_name = strdup( p_ved->GetMainName().c_str() );
                    else
                    {
                        /* Check in tags if the edition has a name */

                        /* We use only the tags of the first segment as it contains the edition */
                        matroska_segment_c::tags_t const& tags = opened_segments[0]->tags;
                        uint64_t i_ed_uid = 0;
                        if( p_ved->p_edition )
                            i_ed_uid = (uint64_t) p_ved->p_edition->i_uid;

                        for( size_t k = 0; k < tags.size(); k++ )
                        {
                            if( tags[k].i_tag_type == EDITION_UID && tags[k].i_uid == i_ed_uid )
                                for( size_t l = 0; l < tags[k].simple_tags.size(); l++ )
                                {
                                    SimpleTag const& st = tags[k].simple_tags[l];
                                    if ( st.tag_name == "TITLE" )
                                    {
                                        msg_Dbg( &demuxer, "Using title \"%s\" from tag for edition %" PRIu64, st.value.c_str (), i_ed_uid );
                                        p_title->psz_name = strdup( st.value.c_str () );
                                        break;
                                    }
                                }
                        }

                        if( !p_title->psz_name &&
                            asprintf(&(p_title->psz_name), "%s %d", "Segment", (int)ij) == -1 )
                            p_title->psz_name = NULL;
                    }
                }

                ij++;
                i_chapters = 0;
                p_ved->PublishChapters( *p_title, i_chapters, 0 );

                // Input duration into i_length
                p_title->i_length = p_ved->i_duration;

                titles.push_back( p_title );
            }
        }
        p_vseg->i_sys_title = p_vseg->i_current_edition;
    }

    // TODO decide which segment should be first used (VMG for DVD)

    return true;
}

bool demux_sys_t::FreeUnused()
{
    auto sIt = std::remove_if(begin(streams), end(streams), [](const matroska_stream_c* p_s) {
        return !p_s->isUsed();
    });
    for (auto it = sIt; it != end(streams); ++it)
        delete *it;
    streams.erase(sIt, end(streams));

    auto sgIt = std::remove_if(begin(opened_segments), end(opened_segments),
                [](const matroska_segment_c* p_sg) {
        return !p_sg->b_preloaded;
    });
    for (auto it = sgIt; it != end(opened_segments); ++it)
        delete *it;
    opened_segments.erase(sgIt, end(opened_segments));

    return !streams.empty() && !opened_segments.empty();
}

bool demux_sys_t::PreparePlayback( virtual_segment_c & new_vsegment, vlc_tick_t i_mk_date )
{
    if ( p_current_vsegment != &new_vsegment )
    {
        if ( p_current_vsegment->CurrentSegment() != NULL )
            p_current_vsegment->CurrentSegment()->ESDestroy();

        p_current_vsegment = &new_vsegment;
        p_current_vsegment->CurrentSegment()->ESCreate();
        i_current_title = p_current_vsegment->i_sys_title;
    }
    if( !p_current_vsegment->CurrentSegment() )
        return false;
    if( !p_current_vsegment->CurrentSegment()->b_cues )
        msg_Warn( &p_current_vsegment->CurrentSegment()->sys.demuxer, "no cues/empty cues found->seek won't be precise" );

    i_duration = p_current_vsegment->Duration();

    /* add information */
    p_current_vsegment->CurrentSegment()->InformationCreate( );
    p_current_vsegment->CurrentSegment()->ESCreate( );

    /* Seek to the beginning */
    p_current_vsegment->Seek(p_current_vsegment->CurrentSegment()->sys.demuxer,
                             i_mk_date, p_current_vsegment->p_current_vchapter );

    return true;
}

void demux_sys_t::JumpTo( virtual_segment_c & vsegment, virtual_chapter_c & vchapter )
{
    if ( !vchapter.p_chapter || !vchapter.p_chapter->Enter( true ) )
    {
        // jump to the location in the found segment
        vsegment.Seek( demuxer, vchapter.i_mk_virtual_start_time, &vchapter );
    }
}

matroska_segment_c *demux_sys_t::FindSegment( const EbmlBinary & uid ) const
{
    for (size_t i=0; i<opened_segments.size(); i++)
    {
        if ( opened_segments[i]->p_segment_uid && *opened_segments[i]->p_segment_uid == uid )
            return opened_segments[i];
    }
    return NULL;
}

virtual_chapter_c *demux_sys_t::BrowseCodecPrivate( unsigned int codec_id,
                                        bool (*match)(const chapter_codec_cmds_c &data, const void *p_cookie, size_t i_cookie_size ),
                                        const void *p_cookie,
                                        size_t i_cookie_size,
                                        virtual_segment_c * &p_vsegment_found )
{
    virtual_chapter_c *p_result = NULL;
    for (size_t i=0; i<used_vsegments.size(); i++)
    {
        p_result = used_vsegments[i]->BrowseCodecPrivate( codec_id, match, p_cookie, i_cookie_size );
        if ( p_result != NULL )
        {
            p_vsegment_found = used_vsegments[i];
            break;
        }
    }
    return p_result;
}

virtual_chapter_c *demux_sys_t::FindChapter( int64_t i_find_uid, virtual_segment_c * & p_vsegment_found )
{
    virtual_chapter_c *p_result = NULL;
    for (size_t i=0; i<used_vsegments.size(); i++)
    {
        p_result = used_vsegments[i]->FindChapter( i_find_uid );
        if ( p_result != NULL )
        {
            p_vsegment_found = used_vsegments[i];
            break;
        }
    }
    return p_result;
}

} // namespace
