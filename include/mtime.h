/*****************************************************************************
 * mtime.h: high resolution time management functions
 * This header provides portable high precision time management functions,
 * which should be the only ones used in other segments of the program, since
 * functions like gettimeofday() and ftime() are not always supported.
 * Most functions are declared as inline or as macros since they are only
 * interfaces to system calls and have to be called frequently.
 * 'm' stands for 'micro', since maximum resolution is the microsecond.
 * Functions prototyped are implemented in interface/mtime.c.
 *****************************************************************************
 * Copyright (C) 1996, 1997, 1998, 1999, 2000 VideoLAN
 * $Id: mtime.h,v 1.10 2002/01/29 20:11:18 gbazin Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * LAST_MDATE: date which will never happen
 *****************************************************************************
 * This date can be used as a 'never' date, to mark missing events in a function
 * supposed to return a date, such as nothing to display in a function
 * returning the date of the first image to be displayed. It can be used in
 * comparaison with other values: all existing dates will be earlier.
 *****************************************************************************/
#define LAST_MDATE ((mtime_t)((u64)(-1)/2))

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
#ifndef PLUGIN
char *  mstrtime ( char *psz_buffer, mtime_t date );
mtime_t mdate    ( void );
void    mwait    ( mtime_t date );
void    msleep   ( mtime_t delay );
#else
#   define msleep    p_symbols->msleep
#   define mdate     p_symbols->mdate
#   define mstrtime  p_symbols->mstrtime
#endif

