/*****************************************************************************
 * matroska_segment.hpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
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

namespace mkv {

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
        /* pts      */ vlc_tick_t( VLC_TICK_FROM_NS( p_cluster->GlobalTimecode() ) ),
        /* duration */ vlc_tick_t( -1 ),
        /* size     */ p_cluster->IsFiniteSize()
            ? p_cluster->GetEndPosition() - p_cluster->GetElementPosition()
            : UINT64_MAX
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
SegmentSeeker::add_seekpoint( track_id_t track_id, Seekpoint sp )
{
    seekpoints_t&  seekpoints = _tracks_seekpoints[ track_id ];
    seekpoints_t::iterator it = std::lower_bound( seekpoints.begin(), seekpoints.end(), sp );

    if( it != seekpoints.end() && it->pts == sp.pts )
    {
        if (sp.trust_level <= it->trust_level)
            return;

        *it = sp;
    }
    else
    {
        seekpoints.insert( it, sp );
    }
}

SegmentSeeker::tracks_seekpoint_t
SegmentSeeker::find_greatest_seekpoints_in_range( fptr_t start_fpos, vlc_tick_t end_pts, track_ids_t const& filter_tracks )
{
    tracks_seekpoint_t tpoints;

    for( tracks_seekpoints_t::const_iterator it = _tracks_seekpoints.begin(); it != _tracks_seekpoints.end(); ++it )
    {
        if ( std::find( filter_tracks.begin(), filter_tracks.end(), it->first ) == filter_tracks.end() )
            continue;

        Seekpoint sp = get_first_seekpoint_around( end_pts, it->second );

        if( sp.fpos < start_fpos )
            continue;

        if( sp.pts > end_pts )
            continue;

        tpoints.insert( tracks_seekpoint_t::value_type( it->first, sp ) );
    }

    if (tpoints.empty())
    {
        // try a further pts
        for( tracks_seekpoints_t::const_iterator it = _tracks_seekpoints.begin(); it != _tracks_seekpoints.end(); ++it )
        {
            if ( std::find( filter_tracks.begin(), filter_tracks.end(), it->first ) == filter_tracks.end() )
                continue;

            Seekpoint sp = get_first_seekpoint_around( end_pts, it->second );

            if( sp.fpos < start_fpos )
                continue;

            tpoints.insert( tracks_seekpoint_t::value_type( it->first, sp ) );
        }
    }

    return tpoints;
}

SegmentSeeker::Seekpoint
SegmentSeeker::get_first_seekpoint_around( vlc_tick_t pts, seekpoints_t const& seekpoints,
                                           Seekpoint::TrustLevel trust_level )
{
    if( seekpoints.empty() )
    {
        return Seekpoint();
    }

    typedef seekpoints_t::const_iterator iterator;

    Seekpoint const needle ( std::numeric_limits<fptr_t>::max(), pts );

    iterator const it_begin  = seekpoints.begin();
    iterator const it_end    = seekpoints.end();
    iterator const it_middle = greatest_lower_bound( it_begin, it_end, needle );

    iterator it_before;

    // rewrind to _previous_ seekpoint with appropriate trust
    for( it_before = it_middle; it_before != it_begin; --it_before )
    {
        if( it_before->trust_level >= trust_level )
            return *it_before;
    }
    return *it_begin;
}

SegmentSeeker::seekpoint_pair_t
SegmentSeeker::get_seekpoints_around( vlc_tick_t pts, seekpoints_t const& seekpoints )
{
    if( seekpoints.empty() )
    {
        return seekpoint_pair_t();
    }

    typedef seekpoints_t::const_iterator iterator;

    Seekpoint const needle ( std::numeric_limits<fptr_t>::max(), pts );

    iterator const it_begin  = seekpoints.begin();
    iterator const it_end    = seekpoints.end();
    iterator const it_middle = greatest_lower_bound( it_begin, it_end, needle );

    if ( it_middle != it_end && (*it_middle).pts > pts)
        // found nothing low enough, use the first one
        return seekpoint_pair_t( *it_begin, Seekpoint() );

    iterator it_before = it_middle;
    iterator it_after = it_middle == it_end ? it_middle : next_( it_middle ) ;

    return seekpoint_pair_t( *it_before,
      it_after == it_end ? Seekpoint() : *it_after
    );
}

SegmentSeeker::seekpoint_pair_t
SegmentSeeker::get_seekpoints_around( vlc_tick_t target_pts, track_ids_t const& priority_tracks )
{
    seekpoint_pair_t points;

    if( _tracks_seekpoints.empty() )
        return points;

    { // locate the max/min seekpoints for priority_tracks //

        typedef track_ids_t::const_iterator track_iterator;

        track_iterator const begin = priority_tracks.begin();
        track_iterator const end   = priority_tracks.end();

        for( track_iterator it = begin; it != end; ++it )
        {
            seekpoint_pair_t track_points = get_seekpoints_around( target_pts, _tracks_seekpoints[ *it ] );

            if( it == begin ) {
                points = track_points;
                continue;
            }

            if( track_points.first.trust_level > Seekpoint::DISABLED &&
                points.first.fpos > track_points.first.fpos )
                points.first = track_points.first;

            if( track_points.second.trust_level > Seekpoint::DISABLED &&
                points.second.fpos < track_points.second.fpos )
                points.second = track_points.second;
        }
    }

    { // check if we got a cluster which is closer to target_pts than the found cues //

        cluster_map_t::iterator it = _clusters.lower_bound( target_pts );

        if( it != _clusters.begin() && --it != _clusters.end() )
        {
            Cluster const& cluster = it->second;

            if( cluster.fpos > points.first.fpos )
            {
                points.first = Seekpoint( cluster.fpos, cluster.pts );

                // do we need to update the max point? //

                if( points.second.fpos < points.first.fpos )
                    points.second = Seekpoint( cluster.fpos + cluster.size, cluster.pts + cluster.duration );
            }
        }
    }

    return points;
}

SegmentSeeker::tracks_seekpoint_t
SegmentSeeker::get_seekpoints( matroska_segment_c& ms, vlc_tick_t target_pts,
                               track_ids_t const& priority_tracks, track_ids_t const& filter_tracks )
{
    struct contains_all_of_t {
        bool operator()( tracks_seekpoint_t const& haystack, track_ids_t const& track_ids )
        {
            for( track_ids_t::const_iterator it = track_ids.begin(); it != track_ids.end(); ++it ) {
                if( haystack.find( *it ) == haystack.end() )
                    return false;
            }

            return true;
        }
    };

    for( vlc_tick_t needle_pts = target_pts; ; )
    {
        seekpoint_pair_t seekpoints = get_seekpoints_around( needle_pts, priority_tracks );

        Seekpoint const& start = seekpoints.first;
        Seekpoint const& end   = seekpoints.second;

        if ( start.fpos == std::numeric_limits<fptr_t>::max() )
            return tracks_seekpoint_t();

        if ( end.fpos != std::numeric_limits<fptr_t>::max() || !ms.b_cues )
            // do not read the whole (infinite?) file to get seek indexes
            index_range( ms, Range( start.fpos, end.fpos ), needle_pts );

        tracks_seekpoint_t tpoints = find_greatest_seekpoints_in_range( start.fpos, target_pts, filter_tracks );

        if( contains_all_of_t() ( tpoints, priority_tracks ) )
            return tpoints;

        needle_pts = start.pts - 1;
    }

    vlc_assert_unreachable();
}

void
SegmentSeeker::index_range( matroska_segment_c& ms, Range search_area, vlc_tick_t max_pts )
{
    ranges_t areas_to_search = get_search_areas( search_area.start, search_area.end );

    for( ranges_t::const_iterator range_it = areas_to_search.begin(); range_it != areas_to_search.end(); ++range_it ) 
        index_unsearched_range( ms, *range_it, max_pts );
}

void
SegmentSeeker::index_unsearched_range( matroska_segment_c& ms, Range search_area, vlc_tick_t max_pts )
{
    mkv_jump_to( ms, search_area.start );

    search_area.start = ms.es.I_O().getFilePointer();

    fptr_t  block_pos = search_area.start;
    vlc_tick_t block_pts;

    while( block_pos < search_area.end )
    {
        KaxBlock * block;
        KaxSimpleBlock * simpleblock;
        KaxBlockAdditions *additions;

        bool     b_key_picture;
        bool     b_discardable_picture;
        int64_t  i_block_duration;
        track_id_t track_id;

        if( ms.BlockGet( block, simpleblock, additions,
                         &b_key_picture, &b_discardable_picture, &i_block_duration ) )
            break;

        KaxInternalBlock& internal_block = simpleblock
            ? static_cast<KaxInternalBlock&>( *simpleblock )
            : static_cast<KaxInternalBlock&>( *block );

        block_pos = internal_block.GetElementPosition();
        block_pts = VLC_TICK_FROM_NS(internal_block.GlobalTimecode());
        track_id  = internal_block.TrackNum();

        bool const b_valid_track = ms.FindTrackByBlock( block, simpleblock ) != NULL;

        delete block;

        if( b_valid_track )
        {
            if( b_key_picture )
                add_seekpoint( track_id, Seekpoint( block_pos, block_pts ) );

            if( max_pts < block_pts )
                break;
        }
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

    if ( fpos != std::numeric_limits<SegmentSeeker::fptr_t>::max() )
    {
        ms.cluster = NULL;
        if ( !_cluster_positions.empty() )
        {
            cluster_positions_t::iterator cluster_it = greatest_lower_bound(
              _cluster_positions.begin(), _cluster_positions.end(), fpos
            );

            ms.es.I_O().setFilePointer( *cluster_it );
            ms.ep.reconstruct( &ms.es, ms.segment, &ms.sys.demuxer );
        }

        while( ms.cluster == NULL || (
              ms.cluster->IsFiniteSize() && ms.cluster->GetEndPosition() < fpos ) )
        {
            if( !( ms.cluster = static_cast<KaxCluster*>( ms.ep.Get() ) ) )
            {
                msg_Err( &ms.sys.demuxer, "unable to read KaxCluster during seek, giving up" );
                return;
            }

            i_cluster_pos = ms.cluster->GetElementPosition();

            add_cluster_position( i_cluster_pos );

            mark_range_as_searched( Range( i_cluster_pos, ms.es.I_O().getFilePointer() ) );
        }
    }
    else if (ms.cluster != NULL)
    {
        // make sure we start reading after the Cluster start
        ms.es.I_O().setFilePointer(ms.cluster->GetDataStart());
    }

    ms.ep.Down();

    /* read until cluster/timecode to initialize cluster */

    while( EbmlElement * el = ms.ep.Get() )
    {
        if( MKV_CHECKED_PTR_DECL( p_tc, KaxClusterTimecode, el ) )
        {
            p_tc->ReadData( ms.es.I_O(), SCOPE_ALL_DATA );
            ms.cluster->InitTimecode( static_cast<uint64>( *p_tc ), ms.i_timescale );
            add_cluster(ms.cluster);
            break;
        }
        else if( MKV_CHECKED_PTR_DECL( p_tc, EbmlCrc32, el ) )
        {
            p_tc->ReadData( ms.es.I_O(), SCOPE_ALL_DATA ); /* avoid a skip that may fail */
        }
    }

    /* TODO: add error handling; what if we never get a KaxCluster and/or KaxClusterTimecode? */

    mark_range_as_searched( Range( i_cluster_pos, ms.es.I_O().getFilePointer() ) );

    /* jump to desired position */

    if ( fpos != std::numeric_limits<SegmentSeeker::fptr_t>::max() )
        ms.es.I_O().setFilePointer( fpos );
}

} // namespace
