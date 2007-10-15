/*****************************************************************************
 * os_specific.h: OS specific features
 *****************************************************************************
 * Copyright (C) 2001 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@netcourrier.com>
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

#if !defined( __LIBVLC__ )
  #error You are not libvlc or one of its plugins. You cannot include this file
#endif

#ifndef _NEED_OS_SPECIFIC_H
#   define _NEED_OS_SPECIFIC_H 1
#endif

#if defined( SYS_BEOS )
/* Nothing at the moment, create beos_specific.h when needed */
#elif defined( __APPLE__ )
/* Nothing at the moment, create darwin_specific.h when needed */
#elif defined( WIN32 ) || defined( UNDER_CE )
VLC_EXPORT( const char * , system_VLCPath, (void));
#else
#   undef _NEED_OS_SPECIFIC_H
#endif

#   ifdef __cplusplus
extern "C" {
#   endif

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
#ifdef _NEED_OS_SPECIFIC_H
    void system_Init       ( libvlc_int_t *, int *, char *[] );
    void system_Configure  ( libvlc_int_t *, int *, char *[] );
    void system_End        ( libvlc_int_t * );
#else
#   define system_Init( a, b, c ) {}
#   define system_Configure( a, b, c ) {}
#   define system_End( a ) {}
#endif

#   ifdef __cplusplus
}
#   endif
