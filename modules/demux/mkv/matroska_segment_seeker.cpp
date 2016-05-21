/*****************************************************************************
 * matroska_segment.hpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Filip Roséen <filip@videolabs.io>
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

#include "matroska_segment_seeker.hpp"
#include "matroska_segment.hpp"
#include "demux.hpp"
#include "Ebml_parser.hpp"
#include "Ebml_dispatcher.hpp"
#include "util.hpp"
#include "stream_io_callback.hpp"

#include <sstream>
#include <limits>

namespace { 
    template<class It, class T>
    It greatest_lower_bound( It beg, It end, T const& value )
    {
        It it = std::upper_bound( beg, end, value );
        if( it != beg ) --it;
        return it;
    }

    // std::prev and std::next exists in C++11, in order to avoid ambiguity due
    // to ADL and iterators being defined within namespace std, these two
    // function-names have been postfixed with an underscore.

    template<class It> It prev_( It it ) { return --it; }
    template<class It> It next_( It it ) { return ++it; }
}

SegmentSeeker::cluster_positions_t::iterator
SegmentSeeker::add_cluster_position( fptr_t fpos )
{
    cluster_positions_t::iterator insertion_point = std::upper_bound(
      _cluster_positions.begin(),
      _cluster_positions.end(),
      fpos
    );

    return _cluster_positions.insert( insertion_point, fpos );
}

SegmentSeeker::cluster_map_t::iterator
SegmentSeeker::add_cluster( KaxCluster * const p_cluster )
{
    Cluster cinfo = {
        /* fpos     */ p_cluster->GetElementPosition(),
        /* pts      */ mtime_t( p_cluster->GlobalTimecode() / INT64_C( 1000 ) ),
        /* duration */ mtime_t( -1 ),
        /* size     */ p_cluster->GetEndPosition() - p_cluster->GetElementPosition()
    };

    add_cluster_position( cinfo.fpos );

    cluster_map_t::iterator it = _clusters.lower_bound( cinfo.pts );

    if( it != _clusters.end() && it->second.pts == cinfo.pts )
    {
        // cluster already known
    }
    else
    {
        it = _clusters.insert( cluster_map_t::value_type( cinfo.pts, cinfo ) ).first;
    }

    // ------------------------------------------------------------------
    // IF we have two adjecent clusters, update duration where applicable
    // ------------------------------------------------------------------

    struct Duration {
        static void fix( Cluster& prev, Cluster& next )
        {
            if( ( prev.fpos + prev.size) == next.fpos )
                prev.duration = next.pts - prev.pts; 
        }
    };

    if( it != _clusters.begin() )
    {
        Duration::fix( prev_( it )->second, it->second );
    }

    if( it != _clusters.end() && next_( it ) != _clusters.end() )
    {
        Duration::fix( it->second, next_( it )->second );
    }

    return it;
}

void
SegmentSeeker::add_seekpoint( track_id_t track_id, int trust_level, fptr_t fpos, mtime_t pts )
{
    Seekpoint sp ( trust_level, fpos, pts );

    seekpoints_t&  seekpoints = _tracks_seekpoints[ track_id ];
    seekpoints_t::iterator it = std::lower_bound( seekpoints.begin(), seekpoints.end(), sp );

    if( it != seekpoints.end() && it->pts == sp.pts )
    {
        *it = sp;
    }
    else
    {
        seekpoints.insert( it, sp );
    }
}

void
SegmentSeeker::index_range( matroska_segment_c& ms, Range search_area, mtime_t max_pts )
{
    ranges_t areas_to_search = get_search_areas( search_area.start, search_area.end );

    for( ranges_t::const_iterator range_it = areas_to_search.begin(); range_it != areas_to_search.end(); ++range_it ) 
        index_unsearched_range( ms, *range_it, max_pts );
}

void
SegmentSeeker::index_unsearched_range( matroska_segment_c& ms, Range search_area, mtime_t max_pts )
{
    mkv_jump_to( ms, search_area.start );

    search_area.start = ms.es.I_O().getFilePointer();

    fptr_t  block_pos = search_area.start;
    mtime_t block_pts = std::numeric_limits<mtime_t>::max();

    while( block_pos < search_area.end )
    {
        KaxBlock * block;
        KaxSimpleBlock * simpleblock;

        bool     b_key_picture;
        bool     b_discardable_picture;
        int64_t  i_block_duration;

        matroska_segment_c::tracks_map_t::iterator i_track = ms.tracks.end();

        if( ms.BlockGet( block, simpleblock, &b_key_picture, &b_discardable_picture, &i_block_duration ) )
        {
            throw std::runtime_error( "Unable to BlockGet in SegmentSeeker::index_unsearched_range, EOF?" );
        }

        if( simpleblock ) {
            block_pos = simpleblock->GetElementPosition();
            block_pts = simpleblock->GlobalTimecode() / 1000;
        }
        else {
            block_pos = block->GetElementPosition();
            block_pts = block->GlobalTimecode() / 1000;
        }

        if( ! ms.FindTrackByBlock( &i_track, block, simpleblock ) )
        {
            if( b_key_picture )
            {
                add_seekpoint( i_track->first, Seekpoint::TRUSTED, block_pos, block_pts );
            }

            if( max_pts < block_pts )
                break;
        }

        delete block;
    }

    search_area.end = ms.es.I_O().getFilePointer();

    mark_range_as_searched( search_area );
}

void
SegmentSeeker::mark_range_as_searched( Range data )
{
    /* TODO: this is utterly ugly, we should do the insertion in-place */

    _ranges_searched.insert( std::upper_bound( _ranges_searched.begin(), _ranges_searched.end(), data ), data );

    {
        ranges_t merged;

        for( ranges_t::iterator it = _ranges_searched.begin(); it != _ranges_searched.end(); ++it )
        {
            if( merged.size() )
            {
                Range& last_entry = *merged.rbegin();

                if( last_entry.end+1 >= it->start && last_entry.end < it->end )
                {
                    last_entry.end = it->end;
                    continue;
                }

                if( it->start >= last_entry.start && it->end <= last_entry.end )
                {
                    last_entry.end = std::max( last_entry.end, it->end );
                    continue;
                }
            }

            merged.push_back( *it );
        }

        _ranges_searched = merged;
    }
}


SegmentSeeker::ranges_t
SegmentSeeker::get_search_areas( fptr_t start, fptr_t end ) const
{
    ranges_t areas_to_search;
    Range needle ( start, end );

    ranges_t::const_iterator it = greatest_lower_bound( _ranges_searched.begin(), _ranges_searched.end(), needle );

    for( ; it != _ranges_searched.end() && needle.start < needle.end; ++it )
    {
        if( needle.start < it->start )
        {
            areas_to_search.push_back( Range( needle.start, it->start ) );
        }

        if( needle.start <= it->end )
            needle.start = it->end + 1;
    }

    needle.start = std::max( needle.start, start );
    if( it == _ranges_searched.end() && needle.start < needle.end )
    {
        areas_to_search.push_back( needle );
    }

    return areas_to_search;
}

void
SegmentSeeker::mkv_jump_to( matroska_segment_c& ms, fptr_t fpos )
{
    fptr_t i_cluster_pos = -1;
    ms.cluster = NULL;

    {
        cluster_positions_t::iterator cluster_it = greatest_lower_bound(
          _cluster_positions.begin(), _cluster_positions.end(), fpos
        );

        ms.es.I_O().setFilePointer( *cluster_it );
        ms.ep->reconstruct( &ms.es, ms.segment, &ms.sys.demuxer );
    }

    while( ms.cluster == NULL || ms.cluster->GetEndPosition() < fpos )
    {
        ms.cluster    = static_cast<KaxCluster*>( ms.ep->Get() );
        i_cluster_pos = ms.cluster->GetElementPosition();

        add_cluster_position( i_cluster_pos );

        mark_range_as_searched( Range( i_cluster_pos, ms.es.I_O().getFilePointer() ) );
    }

    ms.ep->Down();

    /* read until cluster/timecode to initialize cluster */

    while( EbmlElement * el = ms.ep->Get() )
    {
        if( MKV_CHECKED_PTR_DECL( p_tc, KaxClusterTimecode, el ) )
        {
            p_tc->ReadData( ms.es.I_O(), SCOPE_ALL_DATA );
            ms.cluster->InitTimecode( static_cast<uint64>( *p_tc ), ms.i_timescale );
            break;
        }
    }

    /* TODO: add error handling; what if we never get a KaxCluster and/or KaxClusterTimecode? */

    mark_range_as_searched( Range( i_cluster_pos, ms.es.I_O().getFilePointer() ) );

    /* jump to desired position */

    ms.es.I_O().setFilePointer( fpos );
}

