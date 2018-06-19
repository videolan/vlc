/*****************************************************************************
 * vlc_tick.h: high resolution time management functions
 *****************************************************************************
 * This header provides portable high precision time management functions,
 * which should be the only ones used in other segments of the program, since
 * functions like gettimeofday() and ftime() are not always supported.
 * Most functions are declared as inline or as macros since they are only
 * interfaces to system calls and have to be called frequently.
 * 'm' stands for 'micro', since maximum resolution is the microsecond.
 * Functions prototyped are implemented in interface/mtime.c.
 *****************************************************************************
 * Copyright (C) 1996, 1997, 1998, 1999, 2000 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
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

#ifndef __VLC_MTIME_H
# define __VLC_MTIME_H 1

/**
 * High precision date or time interval
 *
 * Store a high precision date or time interval. The maximum precision is the
 * microsecond, and a 64 bits integer is used to avoid overflows (maximum
 * time interval is then 292271 years, which should be long enough for any
 * video). Dates are stored as microseconds since a common date (usually the
 * epoch). Note that date and time intervals can be manipulated using regular
 * arithmetic operators, and that no special functions are required.
 */
typedef int64_t vlc_tick_t;
typedef vlc_tick_t mtime_t; /* deprecated, use vlc_tick_t */

/*****************************************************************************
 * MSTRTIME_MAX_SIZE: maximum possible size of mstrtime
 *****************************************************************************
 * This values is the maximal possible size of the string returned by the
 * mstrtime() function, including '-' and the final '\0'. It should be used to
 * allocate the buffer.
 *****************************************************************************/
#define MSTRTIME_MAX_SIZE 22

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
VLC_API char * secstotimestr( char *psz_buffer, int32_t secs );

/**
 * \defgroup date Timestamps, error-free
 * These functions support generating timestamps without long term rounding
 * errors due to sample rate conversions.
 * \ingroup input
 * @{
 */
/**
 * Timestamps without long-term rounding errors
 */
struct date_t
{
    vlc_tick_t  date;
    uint32_t i_divider_num;
    uint32_t i_divider_den;
    uint32_t i_remainder;
};

/**
 * Initializes a date_t.
 *
 * \param date date to initialize [OUT]
 * \param num divider (sample rate) numerator
 * \param den divider (sample rate) denominator
 */
VLC_API void date_Init(date_t *restrict date, uint32_t num, uint32_t den);

/**
 * Changes the rate of a date_t.
 *
 * \param date date to change
 * \param num divider (sample rate) numerator
 * \param den divider (sample rate) denominator
 */
VLC_API void date_Change(date_t *restrict date, uint32_t num, uint32_t den);

/**
 * Sets the exact timestamp of a date_t.
 *
 * \param date date to set the timestamp into
 * \param value date value
 */
static inline void date_Set(date_t *restrict date, vlc_tick_t value)
{
    date->date = value;
    date->i_remainder = 0;
}

/**
 * Gets the current timestamp from a date_t.
 *
 * \param date date to fetch the timestamp from
 * \return date value
 */
VLC_USED static inline vlc_tick_t date_Get(const date_t *restrict date)
{
    return date->date;
}

/**
 * Increments a date.
 *
 * Moves the date_t timestamp forward by a given number of samples.
 *
 * \param date date to move forward
 * \param count number of samples
 * \return timestamp value after incrementing
 */
VLC_API vlc_tick_t date_Increment(date_t *restrict date, uint32_t count);

/**
 * Decrements a date.
 *
 * Moves the date_t timestamp backward by a given number of samples.
 *
 * \param date date to move backward
 * \param count number of samples
 * \return date value
 */
VLC_API vlc_tick_t date_Decrement(date_t *restrict date, uint32_t count);

/** @} */

VLC_API uint64_t NTPtime64( void );
#endif /* !__VLC_MTIME_ */
