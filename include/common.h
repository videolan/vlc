/*****************************************************************************
 * common.h: common definitions
 * Collection of useful common types and macros definitions
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: common.h,v 1.41 2001/09/28 09:57:08 massiot Exp $
 *
 * Authors: Samuel Hocevar <sam@via.ecp.fr>
 *          Vincent Seguin <seguin@via.ecp.fr>
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
 * required headers:
 *  config.h
 *****************************************************************************/

/*****************************************************************************
 * Basic types definitions
 *****************************************************************************/

#include "int_types.h"

typedef u8                  byte_t;

/* Boolean type */
#ifdef BOOLEAN_T_IN_SYS_TYPES_H
#   include <sys/types.h>
#elif defined(BOOLEAN_T_IN_PTHREAD_H)
#   include <pthread.h>
#elif defined(BOOLEAN_T_IN_CTHREADS_H)
#   include <cthreads.h>
#else
typedef int                 boolean_t;
#endif
#ifdef SYS_GNU
#   define _MACH_I386_BOOLEAN_H_
#endif

/* ptrdiff_t definition */
#ifdef HAVE_STDDEF_H
#   include <stddef.h>
#else
#   include <malloc.h>
#   ifndef _PTRDIFF_T
#       define _PTRDIFF_T
/* Not portable in a 64-bit environment. */
typedef int                 ptrdiff_t;
#   endif
#endif

/* Counter for statistics and profiling */
typedef unsigned long       count_t;

/* DCT elements types */
typedef s16                 dctelem_t;

/* Video buffer types */
typedef u8                  yuv_data_t;

/*****************************************************************************
 * Classes declaration
 *****************************************************************************/

/* Plugins */
struct plugin_bank_s;
struct plugin_info_s;

typedef struct plugin_bank_s *          p_plugin_bank_t;
typedef struct plugin_info_s *          p_plugin_info_t;

/* Plugins */
struct playlist_s;
struct playlist_item_s;

typedef struct playlist_s *             p_playlist_t;
typedef struct playlist_item_s *        p_playlist_item_t;

/* Interface */
struct intf_thread_s;
struct intf_sys_s;
struct intf_console_s;
struct intf_msg_s;
struct intf_channel_s;

typedef struct intf_thread_s *          p_intf_thread_t;
typedef struct intf_sys_s *             p_intf_sys_t;
typedef struct intf_console_s *         p_intf_console_t;
typedef struct intf_msg_s *             p_intf_msg_t;
typedef struct intf_channel_s *         p_intf_channel_t;

/* Input */
struct input_thread_s;
struct input_channel_s;
struct input_cfg_s;

typedef struct input_thread_s *         p_input_thread_t;
typedef struct input_channel_s *        p_input_channel_t;
typedef struct input_cfg_s *            p_input_cfg_t;

/* Audio */
struct aout_thread_s;
struct aout_sys_s;

typedef struct aout_thread_s *          p_aout_thread_t;
typedef struct aout_sys_s *             p_aout_sys_t;

/* Video */
struct vout_thread_s;
struct vout_font_s;
struct vout_sys_s;
struct vdec_thread_s;
struct vpar_thread_s;
struct video_parser_s;

typedef struct vout_thread_s *          p_vout_thread_t;
typedef struct vout_font_s *            p_vout_font_t;
typedef struct vout_sys_s *             p_vout_sys_t;
typedef struct vdec_thread_s *          p_vdec_thread_t;
typedef struct vpar_thread_s *          p_vpar_thread_t;
typedef struct video_parser_s *         p_video_parser_t;

/* Misc */
struct macroblock_s;
struct data_packet_s;
struct es_descriptor_s;
struct pgrm_descriptor_s;

/*****************************************************************************
 * Macros and inline functions
 *****************************************************************************/
#ifdef NTOHL_IN_SYS_PARAM_H
#   include <sys/param.h>
#elif defined(WIN32)
#   include <winsock.h>
#else
#   include <netinet/in.h>
#endif

/* CEIL: division with round to nearest greater integer */
#define CEIL(n, d)  ( ((n) / (d)) + ( ((n) % (d)) ? 1 : 0) )

/* PAD: PAD(n, d) = CEIL(n ,d) * d */
#define PAD(n, d)   ( ((n) % (d)) ? ((((n) / (d)) + 1) * (d)) : (n) )

/* MAX and MIN: self explanatory */
#ifndef MAX
#   define MAX(a, b)   ( ((a) > (b)) ? (a) : (b) )
#endif
#ifndef MIN
#   define MIN(a, b)   ( ((a) < (b)) ? (a) : (b) )
#endif

/* MSB (big endian)/LSB (little endian) conversions - network order is always
 * MSB, and should be used for both network communications and files. Note that
 * byte orders other than little and big endians are not supported, but only
 * the VAX seems to have such exotic properties - note that these 'functions'
 * needs <netinet/in.h> or the local equivalent. */
/* FIXME: hton64 should be declared as an extern inline function to avoid
 * border effects (see byteorder.h) */
#if WORDS_BIGENDIAN
#   define hton16      htons
#   define hton32      htonl
#   define hton64(i)   ( i )
#   define ntoh16      ntohs
#   define ntoh32      ntohl
#   define ntoh64(i)   ( i )
#else
#   define hton16      htons
#   define hton32      htonl
#   define hton64(i)   ( ((u64)(htonl((i) & 0xffffffff)) << 32) | htonl(((i) >> 32) & 0xffffffff ) )
#   define ntoh16      ntohs
#   define ntoh32      ntohl
#   define ntoh64      hton64
#endif

/* Macros with automatic casts */
#define U64_AT(p)   ( ntoh64 ( *( (u64 *)(p) ) ) )
#define U32_AT(p)   ( ntoh32 ( *( (u32 *)(p) ) ) )
#define U16_AT(p)   ( ntoh16 ( *( (u16 *)(p) ) ) )

/* Alignment of critical static data structures */
#ifdef ATTRIBUTE_ALIGNED_MAX
#   define ATTR_ALIGN(align) __attribute__ ((__aligned__ ((ATTRIBUTE_ALIGNED_MAX < align) ? ATTRIBUTE_ALIGNED_MAX : align)))
#else
#   define ATTR_ALIGN(align)
#endif

/* Alignment of critical dynamic data structure */
#ifdef HAVE_MEMALIGN
    /* Some systems have memalign() but no declaration for it */
    void * memalign( size_t align, size_t size );
#else
#   ifdef HAVE_VALLOC
        /* That's like using a hammer to kill a fly, but eh... */
#       define memalign(align,size) valloc(size)
#   else
        /* Assume malloc alignment is sufficient */
#       define memalign(align,size) malloc(size)
#   endif
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
#   define I64C(x)         x
#else
#   define I64C(x)         x##LL
#endif

#if defined( WIN32 )
#   ifndef _OFF_T_DEFINED
typedef __int64 off_t;
#       define _OFF_T_DEFINED
#   endif
#   ifndef snprintf
#       define snprintf _snprintf  /* snprintf not defined in mingw32 (bug?) */
#   endif
#endif
