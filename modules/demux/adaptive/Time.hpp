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

/* Scaled time */
typedef int64_t stime_t;

class Timescale
{
    public:
        Timescale(uint64_t v = 0) : scale(v) {}

        vlc_tick_t ToTime(stime_t t) const
        {
            if( !scale ) return 0;
            stime_t v = t / scale;
            stime_t r = t % scale;
            return v * CLOCK_FREQ + r * CLOCK_FREQ / scale;
        }

        stime_t ToScaled(vlc_tick_t t) const
        {
            int64_t v = t / CLOCK_FREQ;
            int64_t r = t % CLOCK_FREQ;
            return v * scale + r * scale / CLOCK_FREQ;
        }

        bool isValid() const { return !!scale; }
        operator uint64_t() const { return scale; }

    private:
        uint64_t scale;
};

#endif // TIME_HPP

