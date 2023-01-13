/*****************************************************************************
 * vlc_tick.h: high resolution time management functions
 *****************************************************************************
 * This header provides portable high precision time management functions,
 * which should be the only ones used in other segments of the program, since
 * functions like gettimeofday() and ftime() are not always supported.
 * Most functions are declared as inline or as macros since they are only
 * interfaces to system calls and have to be called frequently.
 *****************************************************************************
 * Copyright (C) 1996, 1997, 1998, 1999, 2000 VLC authors and VideoLAN
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

struct timespec;

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

#define VLC_TICK_MIN INT64_MIN
#define VLC_TICK_MAX INT64_MAX

/*
 * vlc_tick_t <> seconds (sec) conversions
 */
#define VLC_TICK_FROM_SEC(sec)   (CLOCK_FREQ * (sec))
#define SEC_FROM_VLC_TICK(vtk)   ((vtk) / CLOCK_FREQ)

#ifdef __cplusplus
#include <type_traits>

template <typename T>
static inline auto vlc_tick_from_sec(T sec)
    -> typename std::enable_if<std::is_integral<T>::value, vlc_tick_t>::type
{
    return CLOCK_FREQ * sec;
}

/* seconds in floating point */
static inline vlc_tick_t vlc_tick_from_sec(double secf)
{
    return (vlc_tick_t)(CLOCK_FREQ * secf); /* TODO use llround ? */
}
#else /* !__cplusplus */
static inline vlc_tick_t vlc_tick_from_seci(int64_t sec)
{
    return CLOCK_FREQ * sec;
}
/* seconds in floating point */
static inline vlc_tick_t vlc_tick_from_secf(double secf)
{
    return (vlc_tick_t)(CLOCK_FREQ * secf); /* TODO use llround ? */
}

#define vlc_tick_from_sec(sec) _Generic((sec), \
        double:  vlc_tick_from_secf(sec), \
        float:   vlc_tick_from_secf(sec), \
        default: vlc_tick_from_seci(sec) )
#endif /* !__cplusplus */

/* seconds in floating point from vlc_tick_t */
static inline double secf_from_vlc_tick(vlc_tick_t vtk)
{
    return (double)vtk / (double)CLOCK_FREQ;
}

static inline vlc_tick_t vlc_tick_rate_duration(float frame_rate)
{
    return (vlc_tick_t)(CLOCK_FREQ / frame_rate);
}

/*
 * samples<>vlc_tick_t
 */
static inline vlc_tick_t vlc_tick_from_samples(int64_t samples, unsigned samp_rate)
{
    return CLOCK_FREQ * samples / samp_rate;
}
static inline int64_t samples_from_vlc_tick(vlc_tick_t t, unsigned samp_rate)
{
    return t * samp_rate / CLOCK_FREQ;
}


static inline vlc_tick_t vlc_tick_from_frac(uint64_t num, uint64_t den)
{
    lldiv_t d = lldiv (num, den);
    return vlc_tick_from_sec( d.quot ) + vlc_tick_from_samples(d.rem, den);
}


/*
 * vlc_tick_t <> milliseconds (ms) conversions
 */
#if (CLOCK_FREQ % 1000) == 0
#define VLC_TICK_FROM_MS(ms)  ((CLOCK_FREQ / INT64_C(1000)) * (ms))
#define MS_FROM_VLC_TICK(vtk) ((vtk) / (CLOCK_FREQ / INT64_C(1000)))
#elif (1000 % CLOCK_FREQ) == 0
#define VLC_TICK_FROM_MS(ms)  ((ms)  / (INT64_C(1000) / CLOCK_FREQ))
#define MS_FROM_VLC_TICK(vtk) ((vtk) * (INT64_C(1000) / CLOCK_FREQ))
#else /* rounded overflowing conversion */
#define VLC_TICK_FROM_MS(ms)  (CLOCK_FREQ * (ms) / 1000)
#define MS_FROM_VLC_TICK(vtk) ((vtk) * 1000 / CLOCK_FREQ)
#endif /* CLOCK_FREQ / 1000 */


/*
 * vlc_tick_t <> microseconds (us) conversions
 */
#if (CLOCK_FREQ % 1000000) == 0
#define VLC_TICK_FROM_US(us)    ((CLOCK_FREQ / INT64_C(1000000)) * (us))
#define US_FROM_VLC_TICK(vtk)   ((vtk) / (CLOCK_FREQ / INT64_C(1000000)))
#elif (1000000 % CLOCK_FREQ) == 0
#define VLC_TICK_FROM_US(us)    ((us)  / (INT64_C(1000000) / CLOCK_FREQ))
#define US_FROM_VLC_TICK(vtk)   ((vtk) * (INT64_C(1000000) / CLOCK_FREQ))
#else /* rounded overflowing conversion */
#define VLC_TICK_FROM_US(us)    (CLOCK_FREQ * (us) / INT64_C(1000000))
#define US_FROM_VLC_TICK(vtk)   ((vtk) * INT64_C(1000000) / CLOCK_FREQ)
#endif /* CLOCK_FREQ / 1000000 */


/*
 * vlc_tick_t <> nanoseconds (ns) conversions
 */
#if (CLOCK_FREQ % 1000000000) == 0
#define VLC_TICK_FROM_NS(ns)    ((ns)  * (CLOCK_FREQ / (INT64_C(1000000000))))
#define NS_FROM_VLC_TICK(vtk)   ((vtk) / (CLOCK_FREQ / (INT64_C(1000000000))))
#elif (1000000000 % CLOCK_FREQ) == 0
#define VLC_TICK_FROM_NS(ns)    ((ns)  / (INT64_C(1000000000) / CLOCK_FREQ))
#define NS_FROM_VLC_TICK(vtk)   ((vtk) * (INT64_C(1000000000) / CLOCK_FREQ))
#else /* rounded overflowing conversion */
#define VLC_TICK_FROM_NS(ns)    (CLOCK_FREQ * (ns) / INT64_C(1000000000))
#define NS_FROM_VLC_TICK(vtk)   ((vtk) * INT64_C(1000000000) / CLOCK_FREQ)
#endif /* CLOCK_FREQ / 1000000000 */


/*
 * msftime_t is a time with 100ns resolutions, typically used by Microsoft
 */
typedef int64_t msftime_t;

#define MSFTIME_FROM_SEC(sec)       (INT64_C(10000000) * (sec))  /* seconds in msftime_t */
#define MSFTIME_FROM_MS(sec)        (INT64_C(10000) * (sec))     /* milliseconds in msftime_t */

#if (CLOCK_FREQ % 10000000) == 0
#define VLC_TICK_FROM_MSFTIME(msft) ((msft) * (CLOCK_FREQ / INT64_C(10000000))
#define MSFTIME_FROM_VLC_TICK(vtk)  ((vtk)  / (CLOCK_FREQ / INT64_C(10000000))
#elif (10000000 % CLOCK_FREQ) == 0
#define VLC_TICK_FROM_MSFTIME(msft) ((msft) / (INT64_C(10000000) / CLOCK_FREQ))
#define MSFTIME_FROM_VLC_TICK(vtk)  ((vtk)  * (INT64_C(10000000) / CLOCK_FREQ))
#else /* rounded overflowing conversion */
#define VLC_TICK_FROM_MSFTIME(msft) (CLOCK_FREQ * (msft) / INT64_C(10000000))
#define MSFTIME_FROM_VLC_TICK(vtk)  ((vtk)  * INT64_C(10000000) / CLOCK_FREQ)
#endif /* CLOCK_FREQ / 10000000 */

#define vlc_tick_from_timeval(tv) \
    (vlc_tick_from_sec( (tv)->tv_sec ) + VLC_TICK_FROM_US( (tv)->tv_usec ))

#define vlc_tick_from_timespec(tv) \
    (vlc_tick_from_sec( (tv)->tv_sec ) + VLC_TICK_FROM_NS( (tv)->tv_nsec ))

/**
 * Converts a VLC tick to a POSIX time specification.
 *
 * \param ts [out] storage space for the time specification
 * \param tick VLC tick
 * \return @c ts
 */
VLC_API struct timespec *vlc_tick_to_timespec(struct timespec *restrict ts,
                                              vlc_tick_t tick);

/*****************************************************************************
 * MSTRTIME_MAX_SIZE: maximum possible size of vlc_tick_to_str
 *****************************************************************************
 * This values is the maximal possible size of the string returned by the
 * vlc_tick_to_str() function, including '-' and the final '\0'. It should be
 * used to allocate the buffer.
 *****************************************************************************/
#define MSTRTIME_MAX_SIZE 22

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

/**
 * Convert seconds to a time in the format h:mm:ss.
 *
 * This function is provided for any interface function which need to print a
 * time string in the format h:mm:ss
 * date.
 * \param ticks the time to be converted
 * \param psz_buffer should be a buffer at least MSTRTIME_MAX_SIZE characters
 * \return psz_buffer is returned so this can be used as printf parameter.
 */
VLC_API char * vlc_tick_to_str( char *psz_buffer, vlc_tick_t ticks );

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

/**
 * @return NTP 64-bits timestamp in host byte order.
 */
VLC_API uint64_t NTPtime64( void );
#endif /* !__VLC_MTIME_ */
