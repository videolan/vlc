/*****************************************************************************
 * timestamps.h: MPEG TS/PS Timestamps helpers
 *****************************************************************************
 * Copyright (C) 2004-2016 VLC authors and VideoLAN
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/
#ifndef VLC_MPEG_TIMESTAMPS_H
#define VLC_MPEG_TIMESTAMPS_H

#ifndef __cplusplus
_Static_assert(CLOCK_FREQ == 1000000, "FROM|TO_SCALE_NZ not matching CLOCK_FREQ");
#endif

#define FROM_SCALE_NZ(x) (((vlc_tick_t)(x) * 100 / 9))
#define TO_SCALE_NZ(x)   ((x) * 9 / 100)

#define FROM_SCALE(x) (VLC_TICK_0 + FROM_SCALE_NZ(x))
#define TO_SCALE(x)   TO_SCALE_NZ((x) - VLC_TICK_0)

typedef int64_t ts_90khz_t;
#define TS_90KHZ_INVALID -1

#define TS_33BITS_ROLL_NZ      FROM_SCALE_NZ(0x1FFFFFFFF)
#define TS_33BITS_HALF_ROLL_NZ FROM_SCALE_NZ(0x0FFFFFFFF)

static inline vlc_tick_t TimeStampWrapAround( vlc_tick_t i_past_pcr, vlc_tick_t i_time )
{
    if( i_past_pcr == VLC_TICK_INVALID || i_time >= i_past_pcr )
        return i_time;

    vlc_tick_t delta = i_past_pcr - i_time;
    if( delta >= TS_33BITS_HALF_ROLL_NZ )
    {
        vlc_tick_t rolls = (delta + TS_33BITS_ROLL_NZ - 1) / TS_33BITS_ROLL_NZ;
        i_time += rolls * TS_33BITS_ROLL_NZ;
    }

    return i_time;
}

#endif
