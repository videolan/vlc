/*****************************************************************************
 * Time.hpp: Adaptative streaming time definitions
 *****************************************************************************
 * Copyright © 2015 - VideoLAN and VLC Authors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
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
#ifndef TIME_HPP
#define TIME_HPP

#include <vlc_common.h>
#include <list>

namespace adaptive
{

/* Scaled time */
using stime_t = int64_t;

class Timescale
{
    public:
        Timescale(uint64_t v = 0) : scale(v) {}

        vlc_tick_t ToTime(stime_t t) const
        {
            if( !scale ) return 0;
            stime_t v = t / scale;
            stime_t r = t % scale;
            return v * 1000000 + r * 1000000 / scale;
        }

        stime_t ToScaled(vlc_tick_t t) const
        {
            vlc_tick_t v = t / 1000000;
            vlc_tick_t r = t % 1000000;
            return v * scale + r * scale / 1000000;
        }

        bool isValid() const { return !!scale; }
        operator uint64_t() const { return scale; }

    private:
        uint64_t scale;
};

class SegmentTimes
{
    public:
        SegmentTimes()
        {
            demux = VLC_TS_INVALID;
            media = VLC_TS_INVALID;
            display = VLC_TS_INVALID;
        }
        SegmentTimes(vlc_tick_t a, vlc_tick_t b, vlc_tick_t c = VLC_TS_INVALID)
        {
            demux = a;
            media = b;
            display = c;
        }
        void offsetBy(vlc_tick_t v)
        {
            if(v == 0)
                return;
            if(demux != VLC_TS_INVALID)
                demux += v;
            if(media != VLC_TS_INVALID)
                media += v;
            if(display != VLC_TS_INVALID)
                display += v;
        }
        vlc_tick_t demux;
        vlc_tick_t media;
        vlc_tick_t display;
};

class Times
{
    public:
        Times()
        {
            continuous = VLC_TS_INVALID;
        }
        Times(const SegmentTimes &s, vlc_tick_t a)
        {
            segment = s;
            continuous = a;
        }
        void offsetBy(vlc_tick_t v)
        {
            if(continuous != VLC_TS_INVALID)
                continuous += v;
            segment.offsetBy(v);
        }
        vlc_tick_t continuous;
        SegmentTimes segment;
};

using SynchronizationReference = std::pair<uint64_t, Times>;

class SynchronizationReferences
{
    public:
        SynchronizationReferences() = default;
        void addReference(uint64_t seq, const Times &t)
        {
            for(auto &r : refs)
                if(r.first == seq)
                {
                    /* update reference when the timestamps are really old to prevent false roll */
                    constexpr vlc_tick_t quarterroll = (INT64_C(0x1FFFFFFFF) * 100 / 9) >> 2;
                    if(t.continuous - r.second.continuous > quarterroll)
                        r.second = t;
                    return;
                }
            while(refs.size() > 10)
                refs.pop_back();
            refs.push_front(SynchronizationReference(seq, t));
        }
        bool getReference(uint64_t seq, mtime_t,
                          SynchronizationReference &ref) const
        {
            for(auto t : refs)
            {
                if(t.first != seq)
                    continue;
                ref = t;
                return true;
            }
            return false;
        }

    private:
        std::list<SynchronizationReference> refs;
};

}

#endif // TIME_HPP

