/*****************************************************************************
 * common.h: common definitions
 * Collection of useful common types and macros definitions
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: vlc_common.h,v 1.9 2002/07/12 21:57:25 massiot Exp $
 *
 * Authors: Samuel Hocevar <sam@via.ecp.fr>
 *          Vincent Seguin <seguin@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Compiler-specific workarounds
 *****************************************************************************/
#if defined( __BORLANDC__ )
#   undef HAVE_VARIADIC_MACROS
#   undef HAVE_STDINT_H
#endif

/*****************************************************************************
 * Required system headers
 *****************************************************************************/
#ifdef HAVE_STRING_H
#   include <string.h>                                         /* strerror() */
#endif

#ifdef HAVE_SYS_TYPES_H
#   include <sys/types.h>
#endif

/*****************************************************************************
 * Basic types definitions
 *****************************************************************************/
#ifdef HAVE_STDINT_H
#   include <stdint.h>
    typedef uint8_t             u8;
    typedef int8_t              s8;

    typedef uint16_t            u16;
    typedef int16_t             s16;

    typedef uint32_t            u32;
    typedef int32_t             s32;

    typedef uint64_t            u64;
    typedef int64_t             s64;
#else
    typedef unsigned char       u8;
    typedef signed char         s8;

    typedef unsigned short      u16;
    typedef signed short        s16;

    typedef unsigned int        u32;
    typedef signed int          s32;

#   if defined( _MSC_VER ) || ( defined( WIN32 ) && !defined( __MINGW32__ ) )
    typedef unsigned __int64    u64;
    typedef signed __int64      s64;
#   else
    typedef unsigned long long  u64;
    typedef signed long long    s64;
#   endif
#endif

typedef u8                      byte_t;

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

#if defined( WIN32 )
typedef int                 ssize_t;
#endif

/* Counter for statistics and profiling */
typedef unsigned long       count_t;

/* DCT elements types */
typedef s16                 dctelem_t;

/* Video buffer types */
typedef u8                  yuv_data_t;

/*****************************************************************************
 * mtime_t: high precision date or time interval
 *****************************************************************************
 * Store an high precision date or time interval. The maximum precision is the
 * micro-second, and a 64 bits integer is used to avoid any overflow (maximum
 * time interval is then 292271 years, which should be long enough for any
 * video). Date are stored as a time interval since a common date.
 * Note that date and time intervals can be manipulated using regular
 * arithmetic operators, and that no special functions are required.
 *****************************************************************************/
typedef s64 mtime_t;

/*****************************************************************************
 * Classes declaration
 *****************************************************************************/

/* Messages */
VLC_DECLARE_STRUCT(msg_bank)
VLC_DECLARE_STRUCT(msg_subscription)

/* Playlist */
VLC_DECLARE_STRUCT(playlist)
VLC_DECLARE_STRUCT(playlist_item)

/* Modules */
VLC_DECLARE_STRUCT(module_bank)
VLC_DECLARE_STRUCT(module)
VLC_DECLARE_STRUCT(module_config)
VLC_DECLARE_STRUCT(module_symbols)
VLC_DECLARE_STRUCT(module_functions)

/* Interface */
VLC_DECLARE_STRUCT(intf_thread)
VLC_DECLARE_STRUCT(intf_sys)
VLC_DECLARE_STRUCT(intf_console)
VLC_DECLARE_STRUCT(intf_msg)
VLC_DECLARE_STRUCT(intf_channel)

/* Input */
VLC_DECLARE_STRUCT(input_thread)
VLC_DECLARE_STRUCT(input_channel)
VLC_DECLARE_STRUCT(input_cfg)
VLC_DECLARE_STRUCT(input_area)
VLC_DECLARE_STRUCT(input_buffers)
VLC_DECLARE_STRUCT(input_socket)

/* Audio */
VLC_DECLARE_STRUCT(aout_thread)
VLC_DECLARE_STRUCT(aout_sys)
VLC_DECLARE_STRUCT(aout_fifo)

/* Video */
VLC_DECLARE_STRUCT(vout_thread)
VLC_DECLARE_STRUCT(vout_font)
VLC_DECLARE_STRUCT(vout_sys)
VLC_DECLARE_STRUCT(chroma_sys)
VLC_DECLARE_STRUCT(picture)
VLC_DECLARE_STRUCT(picture_sys)
VLC_DECLARE_STRUCT(picture_heap)
VLC_DECLARE_STRUCT(subpicture)
VLC_DECLARE_STRUCT(subpicture_sys)

/* Decoders */
VLC_DECLARE_STRUCT(decoder_fifo)

/* Misc */
VLC_DECLARE_STRUCT(macroblock)
VLC_DECLARE_STRUCT(data_packet)
VLC_DECLARE_STRUCT(data_buffer)
VLC_DECLARE_STRUCT(downmix)
VLC_DECLARE_STRUCT(imdct)
VLC_DECLARE_STRUCT(complex)
VLC_DECLARE_STRUCT(dm_par)
VLC_DECLARE_STRUCT(es_descriptor)
VLC_DECLARE_STRUCT(pgrm_descriptor)
VLC_DECLARE_STRUCT(stream_descriptor)
VLC_DECLARE_STRUCT(stream_position)
VLC_DECLARE_STRUCT(stream_ctrl)
VLC_DECLARE_STRUCT(pes_packet)
VLC_DECLARE_STRUCT(bit_stream)
VLC_DECLARE_STRUCT(network_socket)
VLC_DECLARE_STRUCT(iso639_lang)

/*****************************************************************************
 * Plug-in stuff
 *****************************************************************************/
#ifndef __PLUGIN__
#   define VLC_EXPORT( type, name, args ) type name args;
#else
#   define VLC_EXPORT( type, name, args ) ;
    extern module_symbols_t* p_symbols;
#endif

/*****************************************************************************
 * OS-specific headers and thread types
 *****************************************************************************/
#if defined( WIN32 )
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#endif

#include "vlc_threads.h"

/*****************************************************************************
 * Common structure members
 *****************************************************************************/

/* VLC_COMMON_MEMBERS : members common to all basic vlc objects */
#define VLC_COMMON_MEMBERS                                                  \
    int   i_object_id;                                                      \
    int   i_object_type;                                                    \
    char *psz_object_type;                                                  \
    char *psz_object_name;                                                  \
                                                                            \
    /* Thread properties, if any */                                         \
    vlc_bool_t   b_thread;                                                  \
    vlc_thread_t thread_id;                                                 \
                                                                            \
    /* Object access lock */                                                \
    vlc_mutex_t  object_lock;                                               \
    vlc_cond_t   object_wait;                                               \
                                                                            \
    volatile vlc_bool_t b_error;                    /* set by the object */ \
    volatile vlc_bool_t b_die;                     /* set by the outside */ \
    volatile vlc_bool_t b_dead;                     /* set by the object */ \
                                                                            \
    vlc_t *         p_vlc;                           /* root of all evil */ \
                                                                            \
    volatile int    i_refcount;                                             \
    vlc_object_t ** pp_parents;                           /* our parents */ \
    volatile int    i_parents;                                              \
    vlc_object_t ** pp_children;                         /* our children */ \
    volatile int    i_children;                                             \
                                                                            \
    /* Just a reminder so that people don't cast garbage */                 \
    int be_sure_to_add_VLC_COMMON_MEMBERS_to_struct;                        \

/* The real vlc_object_t type. Yes, it's that simple :-) */
struct vlc_object_s
{
    VLC_COMMON_MEMBERS
};

/* CAST_TO_VLC_OBJECT: attempt at doing a clever cast */
#define CAST_TO_VLC_OBJECT( x ) \
    ((vlc_object_t *)(x))+0*(x)->be_sure_to_add_VLC_COMMON_MEMBERS_to_struct

/*****************************************************************************
 * Macros and inline functions
 *****************************************************************************/
#ifdef NTOHL_IN_SYS_PARAM_H
#   include <sys/param.h>

#elif !defined(WIN32) /* NTOHL_IN_SYS_PARAM_H || WIN32 */
#   include <netinet/in.h>

#endif /* NTOHL_IN_SYS_PARAM_H || WIN32 */

/* CEIL: division with round to nearest greater integer */
#define CEIL(n, d)  ( ((n) / (d)) + ( ((n) % (d)) ? 1 : 0) )

/* PAD: PAD(n, d) = CEIL(n ,d) * d */
#define PAD(n, d)   ( ((n) % (d)) ? ((((n) / (d)) + 1) * (d)) : (n) )

/* __MAX and __MIN: self explanatory */
#ifndef __MAX
#   define __MAX(a, b)   ( ((a) > (b)) ? (a) : (b) )
#endif
#ifndef __MIN
#   define __MIN(a, b)   ( ((a) < (b)) ? (a) : (b) )
#endif

/* MSB (big endian)/LSB (little endian) conversions - network order is always
 * MSB, and should be used for both network communications and files. Note that
 * byte orders other than little and big endians are not supported, but only
 * the VAX seems to have such exotic properties. */
static inline u16 U16_AT( void * _p )
{
    u8 * p = (u8 *)_p;
    return ( ((u16)p[0] << 8) | p[1] );
}
static inline u32 U32_AT( void * _p )
{
    u8 * p = (u8 *)_p;
    return ( ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3] );
}
static inline u64 U64_AT( void * _p )
{
    u8 * p = (u8 *)_p;
    return ( ((u64)p[0] << 56) | ((u64)p[1] << 48) | ((u64)p[2] << 40) 
              | ((u64)p[3] << 32) | ((u64)p[4] << 24) | ((u64)p[5] << 16)
              | ((u64)p[6] << 8) | p[7] );
}
#if WORDS_BIGENDIAN
#   define hton16(i)   ( i )
#   define hton32(i)   ( i )
#   define hton64(i)   ( i )
#   define ntoh16(i)   ( i )
#   define ntoh32(i)   ( i )
#   define ntoh64(i)   ( i )
#else
#   define hton16(i)   U16_AT(&i)
#   define hton32(i)   U32_AT(&i)
#   define hton64(i)   U64_AT(&i)
#   define ntoh16(i)   U16_AT(&i)
#   define ntoh32(i)   U32_AT(&i)
#   define ntoh64(i)   U64_AT(&i)
#endif

/* Alignment of critical static data structures */
#ifdef ATTRIBUTE_ALIGNED_MAX
#   define ATTR_ALIGN(align) __attribute__ ((__aligned__ ((ATTRIBUTE_ALIGNED_MAX < align) ? ATTRIBUTE_ALIGNED_MAX : align)))
#else
#   define ATTR_ALIGN(align)
#endif

/* Alignment of critical dynamic data structure
 *
 * Not all platforms support memalign so we provide a vlc_memalign wrapper
 * void *vlc_memalign( size_t align, size_t size, void **pp_orig )
 * *pp_orig is the pointer that has to be freed afterwards.
 */
#if 0
#ifdef HAVE_POSIX_MEMALIGN
#   define vlc_memalign(align,size,pp_orig) \
    ( !posix_memalign( pp_orig, align, size ) ? *(pp_orig) : NULL )
#endif
#endif
#ifdef HAVE_MEMALIGN
    /* Some systems have memalign() but no declaration for it */
    void * memalign( size_t align, size_t size );

#   define vlc_memalign(pp_orig,align,size) \
    ( *(pp_orig) = memalign( align, size ) )

#else /* We don't have any choice but to align manually */
#   define vlc_memalign(pp_orig,align,size) \
    (( *(pp_orig) = malloc( size + align - 1 )) \
        ? (void *)( (((unsigned long)*(pp_orig)) + (unsigned long)(align-1) ) \
                       & (~(unsigned long)(align-1)) ) \
        : NULL )

#endif

/* strndup (defined in src/misc/extras.c) */
#ifndef HAVE_STRNDUP
char * strndup( const char *s, size_t n );
#endif


#define I64C(x)         x##LL

#ifdef WIN32
/* win32, cl and icl support */
#   if defined( _MSC_VER ) || !defined( __MINGW32__ )
#       define __attribute__(x)
#       define __inline__      __inline
#       define strncasecmp     strnicmp
#       define strcasecmp      stricmp
#       define S_IFBLK         0x3000  /* Block */
#       define S_ISBLK(m)      (0)
#       define S_ISCHR(m)      (0)
#       define S_ISFIFO(m)     (((m)&_S_IFMT) == _S_IFIFO)
#       define S_ISREG(m)      (((m)&_S_IFMT) == _S_IFREG)
#       undef I64C
#       define I64C(x)         x##i64
#   endif

/* several type definitions */
#   if defined( __MINGW32__ )
#       if !defined( _OFF_T_ )
typedef long long _off_t;
typedef _off_t off_t;
#           define _OFF_T_
#       else
#           define off_t long long
#       endif
#   endif

#   if defined( _MSC_VER )
#       if !defined( _OFF_T_DEFINED )
typedef __int64 off_t;
#           define _OFF_T_DEFINED
#       else
#           define off_t __int64
#       endif
#   endif

#   if defined( __BORLANDC__ )
#       undef off_t
#       define off_t unsigned __int64
#   endif

#   ifndef O_NONBLOCK
#       define O_NONBLOCK 0
#   endif

#   ifndef snprintf
#       define snprintf _snprintf  /* snprintf not defined in mingw32 (bug?) */
#   endif

#endif

/*****************************************************************************
 * CPU capabilities
 *****************************************************************************/
#define CPU_CAPABILITY_NONE    0
#define CPU_CAPABILITY_486     (1<<0)
#define CPU_CAPABILITY_586     (1<<1)
#define CPU_CAPABILITY_PPRO    (1<<2)
#define CPU_CAPABILITY_MMX     (1<<3)
#define CPU_CAPABILITY_3DNOW   (1<<4)
#define CPU_CAPABILITY_MMXEXT  (1<<5)
#define CPU_CAPABILITY_SSE     (1<<6)
#define CPU_CAPABILITY_ALTIVEC (1<<16)
#define CPU_CAPABILITY_FPU     (1<<31)

/*****************************************************************************
 * I18n stuff
 *****************************************************************************/
#ifndef PACKAGE /* Borland C++ uses this ! */
#define PACKAGE VLC_PACKAGE
#endif
#define VERSION VLC_VERSION

#if defined( ENABLE_NLS ) && defined ( HAVE_GETTEXT ) && !defined( __BORLANDC__ ) && !defined( MODULE_NAME_IS_gnome )
#   include <libintl.h>
#   undef _
#   define _(String) dgettext (PACKAGE, String)
#   ifdef gettext_noop
#       define N_(String) gettext_noop (String)
#   else
#       define N_(String) (String)
#   endif
#elif !defined( MODULE_NAME_IS_gnome )
#   define _(String) (String)
#   define N_(String) (String)
#endif

/*****************************************************************************
 * Plug-in stuff
 *****************************************************************************/
#include "vlc_symbols.h"
