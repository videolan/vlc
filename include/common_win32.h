/*****************************************************************************
 * common_win32.h: common definitions specific to Win32
 * Collection of useful common types and macros definitions
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: common_win32.h,v 1.1 2001/11/25 22:52:21 gbazin Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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

/* The ntoh* and hton* bytes swapping functions are provided by winsock
 * but for conveniency and speed reasons it is better to implement them
 * ourselves. ( several plugins use them and it is too much assle to link
 * winsock with each of them ;-)
 */
#undef ntoh32(x)
#undef ntoh16(x)
#undef ntoh64(x)
#undef hton32(x)
#undef hton16(x)
#undef hton64(x)

#ifdef WORDS_BIGENDIAN
#   define ntoh32(x)       (x)
#   define ntoh16(x)       (x)
#   define ntoh64(x)       (x)
#   define hton32(x)       (x)
#   define hton16(x)       (x)
#   define hton64(x)       (x)
#else
#   define ntoh32(x)     __bswap_32 (x)
#   define ntoh16(x)     __bswap_16 (x)
#   define ntoh64(x)     __bswap_32 (x)
#   define hton32(x)     __bswap_32 (x)
#   define hton16(x)     __bswap_16 (x)
#   define hton64(x)     __bswap_32 (x)
#endif

/* win32, cl and icl support */
#if defined( _MSC_VER )
#   define __attribute__(x)
#   define __inline__      __inline
#   define strncasecmp     strnicmp
#   define strcasecmp      stricmp
#   define S_ISBLK(m)      (0)
#   define S_ISCHR(m)      (0)
#   define S_ISFIFO(m)     (((m)&_S_IFMT) == _S_IFIFO)
#   define S_ISREG(m)      (((m)&_S_IFMT) == _S_IFREG)
#   undef I64C(x)
#   define I64C(x)         x##i64
#endif

/* several type definitions */
#if defined( __MINGW32__ )
#   if !defined( _OFF_T_ )
typedef long long _off_t;
typedef _off_t off_t;
#       define _OFF_T_
#   else
#       define off_t long long
#   endif
#endif

#if defined( _MSC_VER )
#   if !defined( _OFF_T_DEFINED )
typedef __int64 off_t;
#       define _OFF_T_DEFINED
#   else
#       define off_t __int64
#   endif
#   define stat _stati64
#endif

#ifndef snprintf
#   define snprintf _snprintf  /* snprintf not defined in mingw32 (bug?) */
#endif
