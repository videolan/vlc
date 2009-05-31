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
 * Copyright (C) 1996, 1997, 1998, 1999, 2000 the VideoLAN team
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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
#define LAST_MDATE ((mtime_t)((uint64_t)(-1)/2))

/*****************************************************************************
 * MSTRTIME_MAX_SIZE: maximum possible size of mstrtime
 *****************************************************************************
 * This values is the maximal possible size of the string returned by the
 * mstrtime() function, including '-' and the final '\0'. It should be used to
 * allocate the buffer.
 *****************************************************************************/
#define MSTRTIME_MAX_SIZE 22

/* Well, Duh? But it does clue us in that we are converting from
   millisecond quantity to a second quantity or vice versa.
*/
#define MILLISECONDS_PER_SEC 1000

#define msecstotimestr(psz_buffer, msecs) \
  secstotimestr( psz_buffer, (msecs / (int) MILLISECONDS_PER_SEC) )

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
VLC_EXPORT( char *,  mstrtime, ( char *psz_buffer, mtime_t date ) );
VLC_EXPORT( mtime_t, mdate,    ( void ) );
VLC_EXPORT( void,    mwait,    ( mtime_t date ) );
VLC_EXPORT( void,    msleep,   ( mtime_t delay ) );
VLC_EXPORT( char *,  secstotimestr, ( char *psz_buffer, int secs ) );

#if defined (__GNUC__) && defined (__linux__)
# define VLC_HARD_MIN_SLEEP 1000 /* Linux has 100, 250, 300 or 1000Hz */
# define VLC_SOFT_MIN_SLEEP 9000000

static
__attribute__((unused))
__attribute__((noinline))
__attribute__((error("sorry, cannot sleep for such short a time")))
mtime_t impossible_delay( mtime_t delay )
{
    (void) delay;
    return VLC_HARD_MIN_SLEEP;
}

static
__attribute__((unused))
__attribute__((noinline))
__attribute__((warning("use proper event handling instead of short delay")))
mtime_t harmful_delay( mtime_t delay )
{
    return delay;
}

# define check_delay( d ) \
    ((__builtin_constant_p(d < VLC_HARD_MIN_SLEEP) \
   && (d < VLC_HARD_MIN_SLEEP)) \
       ? impossible_delay(d) \
       : ((__builtin_constant_p(d < VLC_SOFT_MIN_SLEEP) \
       && (d < VLC_SOFT_MIN_SLEEP)) \
           ? harmful_delay(d) \
           : d))

static
__attribute__((unused))
__attribute__((noinline))
__attribute__((error("deadlines can not be constant")))
mtime_t impossible_deadline( mtime_t deadline )
{
    return deadline;
}

# define check_deadline( d ) \
    (__builtin_constant_p(d) ? impossible_deadline(d) : d)
#else
# define check_delay(d) (d)
# define check_deadline(d) (d)
#endif

#define msleep(d) msleep(check_delay(d))
#define mwait(d) mwait(check_deadline(d))

/*****************************************************************************
 * date_t: date incrementation without long-term rounding errors
 *****************************************************************************/
struct date_t
{
    mtime_t  date;
    uint32_t i_divider_num;
    uint32_t i_divider_den;
    uint32_t i_remainder;
};

VLC_EXPORT( void,    date_Init,      ( date_t *, uint32_t, uint32_t ) );
VLC_EXPORT( void,    date_Change,    ( date_t *, uint32_t, uint32_t ) );
VLC_EXPORT( void,    date_Set,       ( date_t *, mtime_t ) );
VLC_EXPORT( mtime_t, date_Get,       ( const date_t * ) );
VLC_EXPORT( void,    date_Move,      ( date_t *, mtime_t ) );
VLC_EXPORT( mtime_t, date_Increment, ( date_t *, uint32_t ) );
VLC_EXPORT( mtime_t, date_Decrement, ( date_t *, uint32_t ) );
VLC_EXPORT( uint64_t, NTPtime64,     ( void ) );
#endif /* !__VLC_MTIME_ */
