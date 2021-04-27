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

namespace adaptive
{

/* Scaled time */
using stime_t = int64_t;

class Timescale
{
    public:
        Timescale(uint64_t v = 0) : scale(v) {}

        mtime_t ToTime(stime_t t) const
        {
            if( !scale ) return 0;
            stime_t v = t / scale;
            stime_t r = t % scale;
            return v * 1000000 + r * 1000000 / scale;
        }

        stime_t ToScaled(mtime_t t) const
        {
            mtime_t v = t / 1000000;
            mtime_t r = t % 1000000;
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
        SegmentTimes(mtime_t a, mtime_t b, mtime_t c = VLC_TS_INVALID)
        {
            demux = a;
            media = b;
            display = c;
        }
        void offsetBy(mtime_t v)
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
        mtime_t demux;
        mtime_t media;
        mtime_t display;
};

class Times
{
    public:
        Times()
        {
            continuous = VLC_TS_INVALID;
        }
        Times(const SegmentTimes &s, mtime_t a)
        {
            segment = s;
            continuous = a;
        }
        void offsetBy(mtime_t v)
        {
            if(continuous != VLC_TS_INVALID)
                continuous += v;
            segment.offsetBy(v);
        }
        mtime_t continuous;
        SegmentTimes segment;
};

}

#endif // TIME_HPP

