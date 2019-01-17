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

#ifndef MKV_MATROSKA_SEGMENT_SEEKER_HPP_
#define MKV_MATROSKA_SEGMENT_SEEKER_HPP_

#include "mkv.hpp"

#include <algorithm>
#include <vector>
#include <map>
#include <limits>

namespace mkv {

class matroska_segment_c;

class SegmentSeeker
{
    public:
        typedef uint64_t fptr_t;
        typedef mkv_track_t::track_id_t track_id_t;

        struct Range
        {
            Range (fptr_t start, fptr_t end)
                : start( start ), end( end )
            { }

            fptr_t start, end;

            bool operator<( Range const& rhs ) const
            {
                return start < rhs.start;
            }
        };

        struct Seekpoint
        {
            enum TrustLevel {
                TRUSTED = +3,
                QUESTIONABLE = +2,
                DISABLED = -1,
            };

            Seekpoint( fptr_t fpos, vlc_tick_t pts, TrustLevel trust_level = TRUSTED )
                : fpos( fpos ), pts( pts ), trust_level( trust_level )
            { }

            Seekpoint()
                : Seekpoint( std::numeric_limits<fptr_t>::max(), -1, DISABLED )
            { }

            bool operator<( Seekpoint const& rhs ) const
            {
                return pts < rhs.pts;
            }

            fptr_t fpos;
            vlc_tick_t pts;
            TrustLevel trust_level;
        };

        struct Cluster {
            fptr_t  fpos;
            vlc_tick_t pts;
            vlc_tick_t duration;
            fptr_t  size;
        };

    public:
        typedef std::vector<track_id_t> track_ids_t;
        typedef std::vector<Range> ranges_t;
        typedef std::vector<Seekpoint> seekpoints_t;
        typedef std::vector<fptr_t> cluster_positions_t;

        typedef std::map<track_id_t, Seekpoint> tracks_seekpoint_t;
        typedef std::map<track_id_t, seekpoints_t> tracks_seekpoints_t;
        typedef std::map<vlc_tick_t, Cluster> cluster_map_t;

        typedef std::pair<Seekpoint, Seekpoint> seekpoint_pair_t;

        void add_seekpoint( track_id_t, Seekpoint );

        seekpoint_pair_t get_seekpoints_around( vlc_tick_t, seekpoints_t const& );
        Seekpoint get_first_seekpoint_around( vlc_tick_t, seekpoints_t const&, Seekpoint::TrustLevel = Seekpoint::TRUSTED );
        seekpoint_pair_t get_seekpoints_around( vlc_tick_t, track_ids_t const& );

        tracks_seekpoint_t get_seekpoints( matroska_segment_c&, vlc_tick_t, track_ids_t const&, track_ids_t const& );
        tracks_seekpoint_t find_greatest_seekpoints_in_range( fptr_t , vlc_tick_t, track_ids_t const& filter_tracks );

        cluster_positions_t::iterator add_cluster_position( fptr_t pos );
        cluster_map_t      ::iterator add_cluster( KaxCluster * const );

        void mkv_jump_to( matroska_segment_c&, fptr_t );

        void index_range( matroska_segment_c& matroska_segment, Range search_area, vlc_tick_t max_pts );
        void index_unsearched_range( matroska_segment_c& matroska_segment, Range search_area, vlc_tick_t max_pts );

        void mark_range_as_searched( Range );
        ranges_t get_search_areas( fptr_t start, fptr_t end ) const;

    public:
        ranges_t            _ranges_searched;
        tracks_seekpoints_t _tracks_seekpoints;
        cluster_positions_t _cluster_positions;
        cluster_map_t       _clusters;
};

} // namespace

#endif /* include-guard */
