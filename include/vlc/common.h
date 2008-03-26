/*****************************************************************************
 * vlc.h: global header for libvlc (old-style)
 *****************************************************************************
 * Copyright (C) 1998-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *          Derk-Jan Hartman <hartman at videolan dot org>
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

/**
 * \defgroup libvlc_old Libvlc Old
 * This is libvlc, the base library of the VLC program.
 * This is the legacy API. Please consider using the new libvlc API
 *
 * @{
 */


#ifndef _VLC_COMMON_H
#define _VLC_COMMON_H 1

#ifndef __cplusplus
# include <stdbool.h>
#endif

/*****************************************************************************
 * Shared library Export macros
 *****************************************************************************/
#ifndef VLC_PUBLIC_API
#  define VLC_PUBLIC_API extern
#endif

/*****************************************************************************
 * Compiler specific
 *****************************************************************************/

#ifndef VLC_DEPRECATED_API
# ifdef __LIBVLC__
/* Avoid unuseful warnings from libvlc with our deprecated APIs */
#    define VLC_DEPRECATED_API VLC_PUBLIC_API
# else /* __LIBVLC__ */
#  if defined(__GNUC__) && (__GNUC__ > 3 || __GNUC__ == 3 && __GNUC_MINOR__ > 0)
#    define VLC_DEPRECATED_API VLC_PUBLIC_API __attribute__((deprecated))
#  else
#    define VLC_DEPRECATED_API VLC_PUBLIC_API
#  endif
# endif /* __LIBVLC__ */
#endif

/*****************************************************************************
 * Types
 *****************************************************************************/

#if (defined( WIN32 ) || defined( UNDER_CE )) && !defined( __MINGW32__ )
typedef signed __int64 vlc_int64_t;
# else
typedef signed long long vlc_int64_t;
#endif

#endif /* _VLC_COMMON_H */
