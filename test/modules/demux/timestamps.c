/*****************************************************************************
 * timestamps.c:
 *****************************************************************************
 * Copyright © 2025 VideoLabs, VideoLAN and VLC Authors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_tick.h>

#include "../../../modules/demux/mpeg/timestamps.h"

#define ASSERT(a) do {\
if(!(a)) { \
        fprintf(stderr, "failed line %d\n", __LINE__); \
        return 1; } \
} while(0)

int main(void)
{
    /* Should not wrap without reference */
    vlc_tick_t ts1 = 50;
    vlc_tick_t ts2 = TimeStampWrapAround(VLC_TICK_INVALID, ts1);
    ASSERT(ts2 == ts1);

    ts1 = TS_33BITS_ROLL_NZ * 3/4;
    ts2 = TimeStampWrapAround(VLC_TICK_INVALID, ts1);
    ASSERT(ts2 == ts1);

    /* Should not wrap */
    ts1 = VLC_TICK_0 + TS_33BITS_HALF_ROLL_NZ;
    ts2 = TimeStampWrapAround(VLC_TICK_0, ts1);
    ASSERT(ts2 == ts1);

    ts1 = VLC_TICK_0 + TS_33BITS_ROLL_NZ;
    ts2 = TimeStampWrapAround(VLC_TICK_0, ts1);
    ASSERT(ts2 == ts1);

    ts1 = VLC_TICK_0 + TS_33BITS_ROLL_NZ;
    ts2 = TimeStampWrapAround(ts1, ts1);
    ASSERT(ts2 == ts1);

    ts1 = VLC_TICK_0 + TS_33BITS_ROLL_NZ;
    ts2 = TimeStampWrapAround(ts1 - 100, ts1);
    ASSERT(ts2 == ts1);

    ts1 = VLC_TICK_0 + TS_33BITS_ROLL_NZ;
    ts2 = TimeStampWrapAround(VLC_TICK_0 + TS_33BITS_HALF_ROLL_NZ, ts1);
    ASSERT(ts2 == ts1);

    /* Should wrap */
    ts1 = VLC_TICK_0;
    ts2 = TimeStampWrapAround(VLC_TICK_0 + TS_33BITS_HALF_ROLL_NZ, ts1);
    ASSERT(ts2 > ts1);
    ASSERT(ts2 == ts1 + TS_33BITS_ROLL_NZ);

    ts1 = VLC_TICK_0;
    ts2 = TimeStampWrapAround(VLC_TICK_0 + TS_33BITS_ROLL_NZ * 3/4, ts1);
    ASSERT(ts2 == ts1 + TS_33BITS_ROLL_NZ);

    ts1 = VLC_TICK_0;
    ts2 = TimeStampWrapAround(VLC_TICK_0 + TS_33BITS_ROLL_NZ, ts1);
    ASSERT(ts2 == ts1 + TS_33BITS_ROLL_NZ);

    ts1 = VLC_TICK_0 + TS_33BITS_HALF_ROLL_NZ;
    ts2 = TimeStampWrapAround(VLC_TICK_0 + TS_33BITS_ROLL_NZ, ts1);
    ASSERT(ts2 == ts1 + TS_33BITS_ROLL_NZ);

    /* Should wrap multiple times */
    ts1 = VLC_TICK_0;
    ts2 = TimeStampWrapAround(VLC_TICK_0 + TS_33BITS_ROLL_NZ * 2, ts1);
    ASSERT(ts2 > ts1);
    ASSERT(ts2 == ts1 + TS_33BITS_ROLL_NZ * 2);

    ts1 = VLC_TICK_0 + TS_33BITS_HALF_ROLL_NZ;
    ts2 = TimeStampWrapAround(VLC_TICK_0 + TS_33BITS_ROLL_NZ * 5, ts1);
    ASSERT(ts2 > ts1);
    ASSERT(ts2 == ts1 + TS_33BITS_ROLL_NZ * 5);

    return 0;
}
