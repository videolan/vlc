/*****************************************************************************
 * common.h: common definitions
 * Collection of useful common types and macros definitions
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: vlc_common.h,v 1.67 2003/06/05 11:52:19 gbazin Exp $
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
 * Required vlc headers
 *****************************************************************************/
#if defined( __BORLANDC__ )
#   undef PACKAGE
#endif

#include "config.h"

#if defined( __BORLANDC__ )
#   undef HAVE_VARIADIC_MACROS
#   undef HAVE_STDINT_H
#   undef HAVE_INTTYPES_H
#   undef off_t
#endif

#include "vlc_config.h"
#include "modules_inner.h"

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
#if defined( HAVE_STDINT_H )
#   include <stdint.h>
#elif defined( HAVE_INTTYPES_H )
#   include <inttypes.h>
#elif defined( SYS_CYGWIN )
#   include <sys/types.h>
    /* Cygwin only defines half of these... */
    typedef u_int8_t            uint8_t;
    typedef u_int16_t           uint16_t;
    typedef u_int32_t           uint32_t;
    typedef u_int64_t           uint64_t;
#else
    /* Fallback types (very x86-centric, sorry) */
    typedef unsigned char       uint8_t;
    typedef signed char         int8_t;
    typedef unsigned short      uint16_t;
    typedef signed short        int16_t;
    typedef unsigned int        uint32_t;
    typedef signed int          int32_t;
#   if defined( _MSC_VER ) \
      || defined( UNDER_CE ) \
      || ( defined( WIN32 ) && !defined( __MINGW32__ ) )
    typedef unsigned __int64    uint64_t;
    typedef signed __int64      int64_t;
#   else
    typedef unsigned long long  uint64_t;
    typedef signed long long    int64_t;
#   endif
#endif

typedef uint8_t                 byte_t;

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
#   include <malloc.h>
#endif

#if defined( WIN32 ) || defined( UNDER_CE )
typedef int                 ssize_t;
#endif

/* Counter for statistics and profiling */
typedef unsigned long       count_t;

/* DCT elements types */
typedef int16_t             dctelem_t;

/* Video buffer types */
typedef uint8_t             yuv_data_t;

/* Audio volume */
typedef uint16_t            audio_volume_t;

#ifndef HAVE_SOCKLEN_T
typedef int                 socklen_t;
#endif

/*****************************************************************************
 * Old types definitions
 *****************************************************************************
 * We still provide these types because most of the VLC code uses them
 * instead of the C9x types. They should be removed when the transition is
 * complete (probably in 10 years).
 *****************************************************************************/
typedef uint8_t    u8;
typedef int8_t     s8;
typedef uint16_t   u16;
typedef int16_t    s16;
typedef uint32_t   u32;
typedef int32_t    s32;
typedef uint64_t   u64;
typedef int64_t    s64;

/*****************************************************************************
 * mtime_t: high precision date or time interval
 *****************************************************************************
 * Store a high precision date or time interval. The maximum precision is the
 * microsecond, and a 64 bits integer is used to avoid overflows (maximum
 * time interval is then 292271 years, which should be long enough for any
 * video). Dates are stored as microseconds since a common date (usually the
 * epoch). Note that date and time intervals can be manipulated using regular
 * arithmetic operators, and that no special functions are required.
 *****************************************************************************/
typedef int64_t mtime_t;

/*****************************************************************************
 * The vlc_fourcc_t type.
 *****************************************************************************
 * See http://www.webartz.com/fourcc/ for a very detailed list.
 *****************************************************************************/
typedef uint32_t vlc_fourcc_t;

#ifdef WORDS_BIGENDIAN
#   define VLC_FOURCC( a, b, c, d ) \
        ( ((uint32_t)d) | ( ((uint32_t)c) << 8 ) \
           | ( ((uint32_t)b) << 16 ) | ( ((uint32_t)a) << 24 ) )
#   define VLC_TWOCC( a, b ) \
        ( (uint16_t)(b) | ( (uint16_t)(a) << 8 ) )

#else
#   define VLC_FOURCC( a, b, c, d ) \
        ( ((uint32_t)a) | ( ((uint32_t)b) << 8 ) \
           | ( ((uint32_t)c) << 16 ) | ( ((uint32_t)d) << 24 ) )
#   define VLC_TWOCC( a, b ) \
        ( (uint16_t)(a) | ( (uint16_t)(b) << 8 ) )

#endif

/*****************************************************************************
 * Classes declaration
 *****************************************************************************/

/* Internal types */
typedef struct libvlc_t libvlc_t;
typedef struct vlc_t vlc_t;
typedef struct variable_t variable_t;

/* Messages */
typedef struct msg_bank_t msg_bank_t;
typedef struct msg_subscription_t msg_subscription_t;

/* Playlist */
typedef struct playlist_t playlist_t;
typedef struct playlist_item_t playlist_item_t;

/* Modules */
typedef struct module_bank_t module_bank_t;
typedef struct module_t module_t;
typedef struct module_config_t module_config_t;
typedef struct module_symbols_t module_symbols_t;

/* Interface */
typedef struct intf_thread_t intf_thread_t;
typedef struct intf_sys_t intf_sys_t;
typedef struct intf_console_t intf_console_t;
typedef struct intf_msg_t intf_msg_t;
typedef struct intf_channel_t intf_channel_t;

/* Input */
typedef struct input_thread_t input_thread_t;
typedef struct input_channel_t input_channel_t;
typedef struct input_area_t input_area_t;
typedef struct input_buffers_t input_buffers_t;
typedef struct input_socket_t input_socket_t;
typedef struct input_info_t input_info_t;
typedef struct input_info_category_t input_info_category_t;
typedef struct access_sys_t access_sys_t;
typedef struct demux_sys_t demux_sys_t;
typedef struct es_descriptor_t es_descriptor_t;
typedef struct es_sys_t es_sys_t;
typedef struct pgrm_descriptor_t pgrm_descriptor_t;
typedef struct pgrm_sys_t pgrm_sys_t;
typedef struct stream_descriptor_t stream_descriptor_t;
typedef struct stream_sys_t stream_sys_t;

/* Audio */
typedef struct aout_instance_t aout_instance_t;
typedef struct aout_sys_t aout_sys_t;
typedef struct aout_fifo_t aout_fifo_t;
typedef struct aout_input_t aout_input_t;
typedef struct aout_buffer_t aout_buffer_t;
typedef struct audio_sample_format_t audio_sample_format_t;
typedef struct audio_date_t audio_date_t;

/* Video */
typedef struct vout_thread_t vout_thread_t;
typedef struct vout_font_t vout_font_t;
typedef struct vout_sys_t vout_sys_t;
typedef struct chroma_sys_t chroma_sys_t;
typedef struct picture_t picture_t;
typedef struct picture_sys_t picture_sys_t;
typedef struct picture_heap_t picture_heap_t;
typedef struct subpicture_t subpicture_t;
typedef struct subpicture_sys_t subpicture_sys_t;
typedef struct vout_synchro_t vout_synchro_t;

/* Stream output */
typedef struct sout_instance_t sout_instance_t;
typedef struct sout_fifo_t sout_fifo_t;
typedef struct sout_input_t sout_input_t;
typedef struct sout_packetizer_input_t sout_packetizer_input_t;
typedef struct sout_buffer_t sout_buffer_t;
typedef struct sout_access_out_t sout_access_out_t;
typedef struct sout_mux_t sout_mux_t;
typedef struct sout_stream_t    sout_stream_t;
typedef struct sout_cfg_t       sout_cfg_t;
typedef struct sout_format_t    sout_format_t;
typedef struct sap_session_t    sap_session_t;
/* Decoders */
typedef struct decoder_fifo_t decoder_fifo_t;

/* Misc */
typedef struct data_packet_t data_packet_t;
typedef struct data_buffer_t data_buffer_t;
typedef struct stream_position_t stream_position_t;
typedef struct stream_ctrl_t stream_ctrl_t;
typedef struct pes_packet_t pes_packet_t;
typedef struct bit_stream_t bit_stream_t;
typedef struct network_socket_t network_socket_t;
typedef struct iso639_lang_t iso639_lang_t;

/*****************************************************************************
 * Variable callbacks
 *****************************************************************************/
typedef int ( * vlc_callback_t ) ( vlc_object_t *,      /* variable's object */
                                   char const *,            /* variable name */
                                   vlc_value_t,                 /* old value */
                                   vlc_value_t,                 /* new value */
                                   void * );                /* callback data */

/*****************************************************************************
 * Plug-in stuff
 *****************************************************************************/
#ifndef __PLUGIN__
#   define VLC_EXPORT( type, name, args ) type name args
#else
#   define VLC_EXPORT( type, name, args ) struct _u_n_u_s_e_d_
    extern module_symbols_t* p_symbols;
#endif

/*****************************************************************************
 * OS-specific headers and thread types
 *****************************************************************************/
#if defined( WIN32 ) || defined( UNDER_CE )
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   define IS_WINNT ( GetVersion() < 0x80000000 )
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
    /* Object properties */                                                 \
    volatile vlc_bool_t b_error;                    /* set by the object */ \
    volatile vlc_bool_t b_die;                     /* set by the outside */ \
    volatile vlc_bool_t b_dead;                     /* set by the object */ \
    volatile vlc_bool_t b_attached;                 /* set by the object */ \
                                                                            \
    /* Object variables */                                                  \
    vlc_mutex_t     var_lock;                                               \
    int             i_vars;                                                 \
    variable_t *    p_vars;                                                 \
                                                                            \
    /* Stuff related to the libvlc structure */                             \
    libvlc_t *      p_libvlc;                        /* root of all evil */ \
    vlc_t *         p_vlc;                     /* (root of all evil) - 1 */ \
                                                                            \
    volatile int    i_refcount;                           /* usage count */ \
    vlc_object_t *  p_parent;                              /* our parent */ \
    vlc_object_t ** pp_children;                         /* our children */ \
    volatile int    i_children;                                             \
                                                                            \
    /* Private data */                                                      \
    void *          p_private;                                              \
                                                                            \
    /* Just a reminder so that people don't cast garbage */                 \
    int be_sure_to_add_VLC_COMMON_MEMBERS_to_struct;                        \

/* VLC_OBJECT: attempt at doing a clever cast */
#define VLC_OBJECT( x ) \
    ((vlc_object_t *)(x))+0*(x)->be_sure_to_add_VLC_COMMON_MEMBERS_to_struct

/*****************************************************************************
 * Macros and inline functions
 *****************************************************************************/
#ifdef NTOHL_IN_SYS_PARAM_H
#   include <sys/param.h>

#elif !defined(WIN32) && !defined( UNDER_CE )
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

/* Dynamic array handling: realloc array, move data, increment position */
#define INSERT_ELEM( p_ar, i_oldsize, i_pos, elem )                           \
    do                                                                        \
    {                                                                         \
        if( i_oldsize )                                                       \
        {                                                                     \
            (p_ar) = realloc( p_ar, ((i_oldsize) + 1) * sizeof( *(p_ar) ) );  \
        }                                                                     \
        else                                                                  \
        {                                                                     \
            (p_ar) = malloc( ((i_oldsize) + 1) * sizeof( *(p_ar) ) );         \
        }                                                                     \
        if( (i_oldsize) - (i_pos) )                                           \
        {                                                                     \
            memmove( (p_ar) + (i_pos) + 1,                                    \
                     (p_ar) + (i_pos),                                        \
                     ((i_oldsize) - (i_pos)) * sizeof( *(p_ar) ) );           \
        }                                                                     \
        (p_ar)[i_pos] = elem;                                                 \
        (i_oldsize)++;                                                        \
    }                                                                         \
    while( 0 )

#define REMOVE_ELEM( p_ar, i_oldsize, i_pos )                                 \
    do                                                                        \
    {                                                                         \
        if( (i_oldsize) - (i_pos) - 1 )                                       \
        {                                                                     \
            memmove( (p_ar) + (i_pos),                                        \
                     (p_ar) + (i_pos) + 1,                                    \
                     ((i_oldsize) - (i_pos) - 1) * sizeof( *(p_ar) ) );       \
        }                                                                     \
        if( i_oldsize > 1 )                                                   \
        {                                                                     \
            (p_ar) = realloc( p_ar, ((i_oldsize) - 1) * sizeof( *(p_ar) ) );  \
        }                                                                     \
        else                                                                  \
        {                                                                     \
            free( p_ar );                                                     \
            (p_ar) = NULL;                                                    \
        }                                                                     \
        (i_oldsize)--;                                                        \
    }                                                                         \
    while( 0 )


/* MSB (big endian)/LSB (little endian) conversions - network order is always
 * MSB, and should be used for both network communications and files. Note that
 * byte orders other than little and big endians are not supported, but only
 * the VAX seems to have such exotic properties. */
static inline uint16_t U16_AT( void * _p )
{
    uint8_t * p = (uint8_t *)_p;
    return ( ((uint16_t)p[0] << 8) | p[1] );
}
static inline uint32_t U32_AT( void * _p )
{
    uint8_t * p = (uint8_t *)_p;
    return ( ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
              | ((uint32_t)p[2] << 8) | p[3] );
}
static inline uint64_t U64_AT( void * _p )
{
    uint8_t * p = (uint8_t *)_p;
    return ( ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48)
              | ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32)
              | ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16)
              | ((uint64_t)p[6] << 8) | p[7] );
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

/* Format string sanity checks */
#ifdef HAVE_ATTRIBUTE_FORMAT
#   define ATTRIBUTE_FORMAT(x,y) __attribute__ ((format(printf,x,y)))
#else
#   define ATTRIBUTE_FORMAT(x,y)
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

/* Stuff defined in src/extras/libc.c */
#ifndef HAVE_STRDUP
#   define strdup vlc_strdup
    VLC_EXPORT( char *, vlc_strdup, ( const char *s ) );
#elif !defined(__PLUGIN__)
#   define vlc_strdup NULL
#endif

#ifndef HAVE_STRNDUP
#   if defined(STRNDUP_IN_GNOME_H) && \
        (defined(MODULE_NAME_IS_gnome)||defined(MODULE_NAME_IS_gnome_main)||\
         defined(MODULE_NAME_IS_gnome2)||defined(MODULE_NAME_IS_gnome2_main))
        /* Do nothing: gnome.h defines strndup for us */
#   else
#       define strndup vlc_strndup
        VLC_EXPORT( char *, vlc_strndup, ( const char *s, size_t n ) );
#   endif
#elif !defined(__PLUGIN__)
#   define vlc_strndup NULL
#endif

#ifndef HAVE_ATOF
#   define atof vlc_atof
    VLC_EXPORT( double, vlc_atof, ( const char *nptr ) );
#elif !defined(__PLUGIN__)
#   define vlc_atof NULL
#endif

#ifndef HAVE_GETENV
#   define getenv vlc_getenv
    VLC_EXPORT( char *, vlc_getenv, ( const char *name ) );
#elif !defined(__PLUGIN__)
#   define vlc_getenv NULL
#endif

#ifndef HAVE_STRCASECMP
#   ifdef HAVE_STRICMP
#       define strcasecmp stricmp
#       if !defined(__PLUGIN__)
#           define vlc_strcasecmp NULL
#       endif
#   elif !defined(__PLUGIN__)
#       define strcasecmp vlc_strcasecmp
        VLC_EXPORT( int, vlc_strcasecmp, ( const char *s1, const char *s2 ) );
#   endif
#elif !defined(__PLUGIN__)
#   define vlc_strcasecmp NULL
#endif

#ifndef HAVE_STRNCASECMP
#   ifdef HAVE_STRNICMP
#       define strncasecmp strnicmp
#       if !defined(__PLUGIN__)
#           define vlc_strncasecmp NULL
#       endif
#   elif !defined(__PLUGIN__)
#       define strncasecmp vlc_strncasecmp
        VLC_EXPORT( int, vlc_strncasecmp, ( const char *s1, const char *s2, size_t n ) );
#   endif
#elif !defined(__PLUGIN__)
#   define vlc_strncasecmp NULL
#endif

/* Format type specifiers for 64 bits numbers */
#if !defined(WIN32) && !defined(UNDER_CE)
#   define I64Fd "%lld"
#   define I64Fi "%lli"
#   define I64Fo "%llo"
#   define I64Fu "%llu"
#   define I64Fx "%llx"
#   define I64FX "%llX"
#else
#   define I64Fd "%I64d"
#   define I64Fi "%I64i"
#   define I64Fo "%I64o"
#   define I64Fu "%I64u"
#   define I64Fx "%I64x"
#   define I64FX "%I64X"
#endif /* defined(WIN32)||defined(UNDER_CE) */

/* 64 bits integer constant suffix */
#if defined( __MINGW32__ ) || (!defined(WIN32) && !defined(UNDER_CE))
#   define I64C(x)         x##LL
#else
#   define I64C(x)         x##i64
#endif /* defined(WIN32)||defined(UNDER_CE) */

#if defined(WIN32) || defined(UNDER_CE)
/* win32, cl and icl support */
#   if defined( _MSC_VER ) || !defined( __MINGW32__ )
#       define __attribute__(x)
#       define __inline__      __inline
#       define S_IFBLK         0x3000  /* Block */
#       define S_ISBLK(m)      (0)
#       define S_ISCHR(m)      (0)
#       define S_ISFIFO(m)     (((m)&_S_IFMT) == _S_IFIFO)
#       define S_ISREG(m)      (((m)&_S_IFMT) == _S_IFREG)
#   endif

/* several type definitions */
#   if defined( __MINGW32__ )
#       if !defined( _OFF_T_ )
typedef long long _off_t;
typedef _off_t off_t;
#           define _OFF_T_
#       else
#           ifdef off_t
#               undef off_t
#           endif
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

#   ifndef alloca
#       define alloca _alloca
#   endif

    /* These two are not defined in mingw32 (bug?) */
#   ifndef snprintf
#       define snprintf _snprintf
#   endif
#   ifndef vsnprintf
#       define vsnprintf _vsnprintf
#   endif

#endif

/* lseek (defined in src/extras/libc.c) */
#ifndef HAVE_LSEEK
#   define lseek vlc_lseek
    VLC_EXPORT( off_t, vlc_lseek, ( int fildes, off_t offset, int whence ) );
#elif !defined(__PLUGIN__)
#   define vlc_lseek NULL
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
VLC_EXPORT( char *, vlc_dgettext, ( const char *package, const char *msgid ) );

#if defined( ENABLE_NLS ) && \
     (defined(MODULE_NAME_IS_gnome)||defined(MODULE_NAME_IS_gnome_main)||\
      defined(MODULE_NAME_IS_gnome2)||defined(MODULE_NAME_IS_gnome2_main))
    /* Declare nothing: gnome.h will do it for us */
#elif defined( ENABLE_NLS )
#if defined( HAVE_INCLUDED_GETTEXT )
#   include "libintl.h"
#else
#   include <libintl.h>
#endif
#   undef _
#if defined( __BORLANDC__ )
#define _(String) vlc_dgettext (PACKAGE_TARNAME, String)
#else
#   define _(String) vlc_dgettext (PACKAGE, String)
#endif
#   define N_(String) ((char*)(String))
#else
#   define _(String) ((char*)(String))
#   define N_(String) ((char*)(String))
#endif

/*****************************************************************************
 * Additional vlc stuff
 *****************************************************************************/
#include "vlc_symbols.h"
#include "os_specific.h"
#include "vlc_messages.h"
#include "variables.h"
#include "vlc_objects.h"
#include "vlc_threads_funcs.h"
#include "mtime.h"
#include "modules.h"
#include "main.h"
#include "configuration.h"

#if defined( __BORLANDC__ )
#   undef PACKAGE
#   define PACKAGE
#endif

