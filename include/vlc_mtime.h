/*****************************************************************************
 * vlc_mtime.h: high resolution time management functions
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

/*****************************************************************************
 * LAST_MDATE: date which will never happen
 *****************************************************************************
 * This date can be used as a 'never' date, to mark missing events in a function
 * supposed to return a date, such as nothing to display in a function
 * returning the date of the first image to be displayed. It can be used in
 * comparaison with other values: all existing dates will be earlier.
 *****************************************************************************/
#define LAST_MDATE ((vlc_tick_t)((uint64_t)(-1)/2))

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

/*****************************************************************************
 * date_t: date incrementation without long-term rounding errors
 *****************************************************************************/
struct date_t
{
    vlc_tick_t  date;
    uint32_t i_divider_num;
    uint32_t i_divider_den;
    uint32_t i_remainder;
};

VLC_API void date_Init( date_t *, uint32_t, uint32_t );
VLC_API void date_Change( date_t *, uint32_t, uint32_t );
VLC_API void date_Set( date_t *, vlc_tick_t );
VLC_API vlc_tick_t date_Get( const date_t * );
VLC_API void date_Move( date_t *, vlc_tick_t );
VLC_API vlc_tick_t date_Increment( date_t *, uint32_t );
VLC_API vlc_tick_t date_Decrement( date_t *, uint32_t );
VLC_API uint64_t NTPtime64( void );
#endif /* !__VLC_MTIME_ */
