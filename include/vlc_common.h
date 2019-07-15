/*****************************************************************************
 * vlc_common.h: common definitions
 * Collection of useful common types and macros definitions
 *****************************************************************************
 * Copyright (C) 1998-2011 VLC authors and VideoLAN
 *
 * Authors: Samuel Hocevar <sam@via.ecp.fr>
 *          Vincent Seguin <seguin@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Rémi Denis-Courmont
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

/**
 * \file
 * This file is a collection of common definitions and types
 */

#ifndef VLC_COMMON_H
# define VLC_COMMON_H 1

/*****************************************************************************
 * Required vlc headers
 *****************************************************************************/
#include "vlc_config.h"

/*****************************************************************************
 * Required system headers
 *****************************************************************************/
#include <stdlib.h>
#include <stdarg.h>

#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <stddef.h>

#ifndef __cplusplus
# include <stdbool.h>
#endif

/*****************************************************************************
 * Compilers definitions
 *****************************************************************************/
/* Helper for GCC version checks */
#ifdef __GNUC__
# define VLC_GCC_VERSION(maj,min) \
    ((__GNUC__ > (maj)) || (__GNUC__ == (maj) && __GNUC_MINOR__ >= (min)))
#else
# define VLC_GCC_VERSION(maj,min) (0)
#endif

/* Try to fix format strings for all versions of mingw and mingw64 */
#if defined( _WIN32 ) && defined( __USE_MINGW_ANSI_STDIO )
 #undef PRId64
 #define PRId64 "lld"
 #undef PRIi64
 #define PRIi64 "lli"
 #undef PRIu64
 #define PRIu64 "llu"
 #undef PRIo64
 #define PRIo64 "llo"
 #undef PRIx64
 #define PRIx64 "llx"
 #define snprintf __mingw_snprintf
 #define vsnprintf __mingw_vsnprintf
 #define swprintf _snwprintf
#endif

/* Function attributes for compiler warnings */
#ifdef __GNUC__
# define VLC_DEPRECATED __attribute__((deprecated))
# if VLC_GCC_VERSION(6,0)
#  define VLC_DEPRECATED_ENUM __attribute__((deprecated))
# else
#  define VLC_DEPRECATED_ENUM
# endif

# if defined( _WIN32 ) && !defined( __clang__ )
#  define VLC_FORMAT(x,y) __attribute__ ((format(gnu_printf,x,y)))
# else
#  define VLC_FORMAT(x,y) __attribute__ ((format(printf,x,y)))
# endif
# define VLC_FORMAT_ARG(x) __attribute__ ((format_arg(x)))
# define VLC_MALLOC __attribute__ ((malloc))
# define VLC_USED __attribute__ ((warn_unused_result))

#else
# define VLC_DEPRECATED
# define VLC_DEPRECATED_ENUM
# define VLC_FORMAT(x,y)
# define VLC_FORMAT_ARG(x)
# define VLC_MALLOC
# define VLC_USED
#endif


/* Branch prediction */
#ifdef __GNUC__
# define likely(p)     __builtin_expect(!!(p), 1)
# define unlikely(p)   __builtin_expect(!!(p), 0)
# define unreachable() __builtin_unreachable()
#else
# define likely(p)     (!!(p))
# define unlikely(p)   (!!(p))
# define unreachable() ((void)0)
#endif

#define vlc_assert_unreachable() (assert(!"unreachable"), unreachable())

/* Linkage */
#ifdef __cplusplus
# define VLC_EXTERN extern "C"
#else
# define VLC_EXTERN
#endif

#if defined (_WIN32) && defined (DLL_EXPORT)
# define VLC_EXPORT __declspec(dllexport)
#elif defined (__GNUC__)
# define VLC_EXPORT __attribute__((visibility("default")))
#else
# define VLC_EXPORT
#endif

#define VLC_API VLC_EXTERN VLC_EXPORT


/*****************************************************************************
 * Basic types definitions
 *****************************************************************************/
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
typedef vlc_tick_t mtime_t; /* deprecated, use vlc_tick_t */

#define VLC_TICK_INVALID  VLC_TS_INVALID
#define VLC_TICK_0        VLC_TS_0

/**
 * The vlc_fourcc_t type.
 *
 * See http://www.webartz.com/fourcc/ for a very detailed list.
 */
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

/**
 * Translate a vlc_fourcc into its string representation. This function
 * assumes there is enough room in psz_fourcc to store 4 characters in.
 *
 * \param fcc a vlc_fourcc_t
 * \param psz_fourcc string to store string representation of vlc_fourcc in
 */
static inline void vlc_fourcc_to_char( vlc_fourcc_t fcc, char *psz_fourcc )
{
    memcpy( psz_fourcc, &fcc, 4 );
}

/*****************************************************************************
 * Classes declaration
 *****************************************************************************/

/* Internal types */
typedef struct vlc_list_t vlc_list_t;
typedef struct vlc_object_t vlc_object_t;
typedef struct libvlc_int_t libvlc_int_t;
typedef struct date_t date_t;

/* Playlist */

typedef struct playlist_t playlist_t;
typedef struct playlist_item_t playlist_item_t;
typedef struct services_discovery_t services_discovery_t;
typedef struct services_discovery_sys_t services_discovery_sys_t;
typedef struct vlc_renderer_discovery_t vlc_renderer_discovery_t;
typedef struct vlc_renderer_item_t vlc_renderer_item_t;

/* Modules */
typedef struct module_t module_t;
typedef struct module_config_t module_config_t;

typedef struct config_category_t config_category_t;

/* Input */
typedef struct input_thread_t input_thread_t;
typedef struct input_item_t input_item_t;
typedef struct input_item_node_t input_item_node_t;
typedef struct access_sys_t access_sys_t;
typedef struct stream_t     stream_t;
typedef struct stream_sys_t stream_sys_t;
typedef struct demux_t  demux_t;
typedef struct demux_sys_t demux_sys_t;
typedef struct es_out_t     es_out_t;
typedef struct es_out_id_t  es_out_id_t;
typedef struct es_out_sys_t es_out_sys_t;
typedef struct seekpoint_t seekpoint_t;
typedef struct info_t info_t;
typedef struct info_category_t info_category_t;
typedef struct input_attachment_t input_attachment_t;

/* Format */
typedef struct audio_format_t audio_format_t;
typedef struct video_format_t video_format_t;
typedef struct subs_format_t subs_format_t;
typedef struct es_format_t es_format_t;
typedef struct video_palette_t video_palette_t;

/* Audio */
typedef struct audio_output audio_output_t;
typedef struct aout_sys_t aout_sys_t;
typedef audio_format_t audio_sample_format_t;

/* Video */
typedef struct vout_thread_t vout_thread_t;
typedef struct vlc_viewpoint_t vlc_viewpoint_t;

typedef video_format_t video_frame_format_t;
typedef struct picture_t picture_t;
typedef struct picture_sys_t picture_sys_t;

/* Subpictures */
typedef struct spu_t spu_t;
typedef struct subpicture_t subpicture_t;
typedef struct subpicture_region_t subpicture_region_t;

typedef struct image_handler_t image_handler_t;

/* Stream output */
typedef struct sout_instance_t sout_instance_t;

typedef struct sout_input_t sout_input_t;
typedef struct sout_packetizer_input_t sout_packetizer_input_t;

typedef struct sout_access_out_t sout_access_out_t;
typedef struct sout_access_out_sys_t   sout_access_out_sys_t;

typedef struct sout_mux_t sout_mux_t;
typedef struct sout_mux_sys_t sout_mux_sys_t;

typedef struct sout_stream_t    sout_stream_t;
typedef struct sout_stream_sys_t sout_stream_sys_t;

typedef struct config_chain_t       config_chain_t;
typedef struct session_descriptor_t session_descriptor_t;

/* Decoders */
typedef struct decoder_t         decoder_t;
typedef struct decoder_sys_t     decoder_sys_t;
typedef struct decoder_synchro_t decoder_synchro_t;

/* Encoders */
typedef struct encoder_t      encoder_t;
typedef struct encoder_sys_t  encoder_sys_t;

/* Filters */
typedef struct filter_t filter_t;
typedef struct filter_sys_t filter_sys_t;

/* Network */
typedef struct vlc_url_t vlc_url_t;

/* Misc */
typedef struct iso639_lang_t iso639_lang_t;

/* block */
typedef struct block_t      block_t;
typedef struct block_fifo_t block_fifo_t;

/* Hashing */
typedef struct md5_s md5_t;

/* XML */
typedef struct xml_t xml_t;
typedef struct xml_sys_t xml_sys_t;
typedef struct xml_reader_t xml_reader_t;
typedef struct xml_reader_sys_t xml_reader_sys_t;

/* vod server */
typedef struct vod_t     vod_t;
typedef struct vod_sys_t vod_sys_t;
typedef struct vod_media_t vod_media_t;

/* VLM */
typedef struct vlm_t         vlm_t;
typedef struct vlm_message_t vlm_message_t;

/* misc */
typedef struct vlc_meta_t    vlc_meta_t;
typedef struct input_stats_t input_stats_t;
typedef struct addon_entry_t addon_entry_t;

/* Update */
typedef struct update_t update_t;

/**
 * VLC value structure
 */
typedef union
{
    int64_t         i_int;
    bool            b_bool;
    float           f_float;
    char *          psz_string;
    void *          p_address;
    vlc_list_t *    p_list;
    struct { int32_t x; int32_t y; } coords;

} vlc_value_t;

/**
 * VLC list structure
 */
struct vlc_list_t
{
    int          i_type;
    int          i_count;
    vlc_value_t *p_values;
};

/*****************************************************************************
 * Error values (shouldn't be exposed)
 *****************************************************************************/
#define VLC_SUCCESS        (-0) /**< No error */
#define VLC_EGENERIC       (-1) /**< Unspecified error */
#define VLC_ENOMEM         (-2) /**< Not enough memory */
#define VLC_ETIMEOUT       (-3) /**< Timeout */
#define VLC_ENOMOD         (-4) /**< Module not found */
#define VLC_ENOOBJ         (-5) /**< Object not found */
#define VLC_ENOVAR         (-6) /**< Variable not found */
#define VLC_EBADVAR        (-7) /**< Bad variable value */
#define VLC_ENOITEM        (-8) /**< Item not found */

/*****************************************************************************
 * Variable callbacks: called when the value is modified
 *****************************************************************************/
typedef int ( * vlc_callback_t ) ( vlc_object_t *,      /* variable's object */
                                   char const *,            /* variable name */
                                   vlc_value_t,                 /* old value */
                                   vlc_value_t,                 /* new value */
                                   void * );                /* callback data */

/*****************************************************************************
 * List callbacks: called when elements are added/removed from the list
 *****************************************************************************/
typedef int ( * vlc_list_callback_t ) ( vlc_object_t *,      /* variable's object */
                                        char const *,            /* variable name */
                                        int,                  /* VLC_VAR_* action */
                                        vlc_value_t *,      /* new/deleted value  */
                                        void *);                 /* callback data */

/*****************************************************************************
 * OS-specific headers and thread types
 *****************************************************************************/
#if defined( _WIN32 )
#   include <malloc.h>
#   ifndef PATH_MAX
#       define PATH_MAX MAX_PATH
#   endif
#   include <windows.h>
#endif

#ifdef __APPLE__
#include <sys/syslimits.h>
#include <AvailabilityMacros.h>
#endif

#ifdef __OS2__
#   define OS2EMX_PLAIN_CHAR
#   define INCL_BASE
#   define INCL_PM
#   include <os2safe.h>
#   include <os2.h>
#endif

#include "vlc_mtime.h"
#include "vlc_threads.h"

/**
 * Common structure members
 *****************************************************************************/

/**
 * VLC object common members
 *
 * Common public properties for all VLC objects.
 * Object also have private properties maintained by the core, see
 * \ref vlc_object_internals_t
 */
struct vlc_common_members
{
    /** Object type name
     *
     * A constant string identifying the type of the object (for logging)
     */
    const char *object_type;

    /** Log messages header
     *
     * Human-readable header for log messages. This is not thread-safe and
     * only used by VLM and Lua interfaces.
     */
    char *header;

    int  flags;

    /** Module probe flag
     *
     * A boolean during module probing when the probe is "forced".
     * See \ref module_need().
     */
    bool force;

    /** LibVLC instance
     *
     * Root VLC object of the objects tree that this object belongs in.
     */
    libvlc_int_t *libvlc;

    /** Parent object
     *
     * The parent VLC object in the objects tree. For the root (the LibVLC
     * instance) object, this is NULL.
     */
    vlc_object_t *parent;
};

/**
 * Backward compatibility macro
 */
#define VLC_COMMON_MEMBERS struct vlc_common_members obj;

/**
 * Type-safe vlc_object_t cast
 *
 * This macro attempts to cast a pointer to a compound type to a
 * \ref vlc_object_t pointer in a type-safe manner.
 * It checks if the compound type actually starts with an embedded
 * \ref vlc_object_t structure.
 */
#if !defined(__cplusplus)
# define VLC_OBJECT(x) \
    _Generic((x)->obj, \
        struct vlc_common_members: (vlc_object_t *)(&(x)->obj), \
        const struct vlc_common_members: (const vlc_object_t *)(&(x)->obj) \
    )
#else
# define VLC_OBJECT( x ) ((vlc_object_t *)&(x)->obj)
#endif

/*****************************************************************************
 * Macros and inline functions
 *****************************************************************************/

/* __MAX and __MIN: self explanatory */
#ifndef __MAX
#   define __MAX(a, b)   ( ((a) > (b)) ? (a) : (b) )
#endif
#ifndef __MIN
#   define __MIN(a, b)   ( ((a) < (b)) ? (a) : (b) )
#endif

/* clip v in [min, max] */
#define VLC_CLIP(v, min, max)    __MIN(__MAX((v), (min)), (max))

VLC_USED
static inline int64_t GCD ( int64_t a, int64_t b )
{
    while( b )
    {
        int64_t c = a % b;
        a = b;
        b = c;
    }
    return a;
}

/* function imported from libavutil/common.h */
VLC_USED
static inline uint8_t clip_uint8_vlc( int32_t a )
{
    if( a&(~255) ) return (-a)>>31;
    else           return a;
}

/** Count leading zeroes */
VLC_USED
static inline unsigned (clz)(unsigned x)
{
#ifdef __GNUC__
    return __builtin_clz (x);
#else
    unsigned i = sizeof (x) * 8;

    while (x)
    {
        x >>= 1;
        i--;
    }
    return i;
#endif
}

#define clz8( x ) (clz(x) - ((sizeof(unsigned) - sizeof (uint8_t)) * 8))
#define clz16( x ) (clz(x) - ((sizeof(unsigned) - sizeof (uint16_t)) * 8))
/* XXX: this assumes that int is 32-bits or more */
#define clz32( x ) (clz(x) - ((sizeof(unsigned) - sizeof (uint32_t)) * 8))

/** Count trailing zeroes */
VLC_USED
static inline unsigned (ctz)(unsigned x)
{
#ifdef __GNUC__
    return __builtin_ctz (x);
#else
    unsigned i = sizeof (x) * 8;

    while (x)
    {
        x <<= 1;
        i--;
    }
    return i;
#endif
}

#if !defined(__NetBSD__)
/** Bit weight */
VLC_USED
static inline unsigned (popcount)(unsigned x)
{
#ifdef __GNUC__
    return __builtin_popcount (x);
#else
    unsigned count = 0;
    while (x)
    {
        count += x & 1;
        x = x >> 1;
    }
    return count;
#endif
}

/** Bit weight of long long */
VLC_USED
static inline int (popcountll)(unsigned long long x)
{
#ifdef __GNUC__
    return __builtin_popcountll(x);
#else
    int count = 0;
    while (x)
    {
        count += x & 1;
        x = x >> 1;
    }
    return count;
#endif
}
#endif

VLC_USED
static inline unsigned (parity)(unsigned x)
{
#ifdef __GNUC__
    return __builtin_parity (x);
#else
    for (unsigned i = 4 * sizeof (x); i > 0; i /= 2)
        x ^= x >> i;
    return x & 1;
#endif
}

#if !defined(__NetBSD__)
/** Byte swap (16 bits) */
VLC_USED
static inline uint16_t (bswap16)(uint16_t x)
{
    return (x << 8) | (x >> 8);
}

/** Byte swap (32 bits) */
VLC_USED
static inline uint32_t (bswap32)(uint32_t x)
{
#if defined (__GNUC__) || defined(__clang__)
    return __builtin_bswap32 (x);
#else
    return ((x & 0x000000FF) << 24)
         | ((x & 0x0000FF00) <<  8)
         | ((x & 0x00FF0000) >>  8)
         | ((x & 0xFF000000) >> 24);
#endif
}

/** Byte swap (64 bits) */
VLC_USED
static inline uint64_t (bswap64)(uint64_t x)
{
#if defined (__GNUC__) || defined(__clang__)
    return __builtin_bswap64 (x);
#elif !defined (__cplusplus)
    return ((x & 0x00000000000000FF) << 56)
         | ((x & 0x000000000000FF00) << 40)
         | ((x & 0x0000000000FF0000) << 24)
         | ((x & 0x00000000FF000000) <<  8)
         | ((x & 0x000000FF00000000) >>  8)
         | ((x & 0x0000FF0000000000) >> 24)
         | ((x & 0x00FF000000000000) >> 40)
         | ((x & 0xFF00000000000000) >> 56);
#else
    return ((x & 0x00000000000000FFULL) << 56)
         | ((x & 0x000000000000FF00ULL) << 40)
         | ((x & 0x0000000000FF0000ULL) << 24)
         | ((x & 0x00000000FF000000ULL) <<  8)
         | ((x & 0x000000FF00000000ULL) >>  8)
         | ((x & 0x0000FF0000000000ULL) >> 24)
         | ((x & 0x00FF000000000000ULL) >> 40)
         | ((x & 0xFF00000000000000ULL) >> 56);
#endif
}
#endif

/* Integer overflow */
static inline bool uadd_overflow(unsigned a, unsigned b, unsigned *res)
{
#if VLC_GCC_VERSION(5,0) || defined(__clang__)
     return __builtin_uadd_overflow(a, b, res);
#else
     *res = a + b;
     return (a + b) < a;
#endif
}

static inline bool uaddl_overflow(unsigned long a, unsigned long b,
                                  unsigned long *res)
{
#if VLC_GCC_VERSION(5,0) || defined(__clang__)
     return __builtin_uaddl_overflow(a, b, res);
#else
     *res = a + b;
     return (a + b) < a;
#endif
}

static inline bool uaddll_overflow(unsigned long long a, unsigned long long b,
                                   unsigned long long *res)
{
#if VLC_GCC_VERSION(5,0) || defined(__clang__)
     return __builtin_uaddll_overflow(a, b, res);
#else
     *res = a + b;
     return (a + b) < a;
#endif
}

#ifndef __cplusplus
# define add_overflow(a,b,r) \
    _Generic(*(r), \
        unsigned: uadd_overflow(a, b, (unsigned *)(r)), \
        unsigned long: uaddl_overflow(a, b, (unsigned long *)(r)), \
        unsigned long long: uaddll_overflow(a, b, (unsigned long long *)(r)))
#else
static inline bool add_overflow(unsigned a, unsigned b, unsigned *res)
{
    return uadd_overflow(a, b, res);
}

static inline bool add_overflow(unsigned long a, unsigned long b,
                                unsigned long *res)
{
    return uaddl_overflow(a, b, res);
}

static inline bool add_overflow(unsigned long long a, unsigned long long b,
                                unsigned long long *res)
{
    return uaddll_overflow(a, b, res);
}
#endif

#if !(VLC_GCC_VERSION(5,0) || defined(__clang__))
# include <limits.h>
#endif

static inline bool umul_overflow(unsigned a, unsigned b, unsigned *res)
{
#if VLC_GCC_VERSION(5,0) || defined(__clang__)
     return __builtin_umul_overflow(a, b, res);
#else
     *res = a * b;
     return b > 0 && a > (UINT_MAX / b);
#endif
}

static inline bool umull_overflow(unsigned long a, unsigned long b,
                                  unsigned long *res)
{
#if VLC_GCC_VERSION(5,0) || defined(__clang__)
     return __builtin_umull_overflow(a, b, res);
#else
     *res = a * b;
     return b > 0 && a > (ULONG_MAX / b);
#endif
}

static inline bool umulll_overflow(unsigned long long a, unsigned long long b,
                                   unsigned long long *res)
{
#if VLC_GCC_VERSION(5,0) || defined(__clang__)
     return __builtin_umulll_overflow(a, b, res);
#else
     *res = a * b;
     return b > 0 && a > (ULLONG_MAX / b);
#endif
}

#ifndef __cplusplus
#define mul_overflow(a,b,r) \
    _Generic(*(r), \
        unsigned: umul_overflow(a, b, (unsigned *)(r)), \
        unsigned long: umull_overflow(a, b, (unsigned long *)(r)), \
        unsigned long long: umulll_overflow(a, b, (unsigned long long *)(r)))
#else
static inline bool mul_overflow(unsigned a, unsigned b, unsigned *res)
{
    return umul_overflow(a, b, res);
}

static inline bool mul_overflow(unsigned long a, unsigned long b,
                                unsigned long *res)
{
    return umull_overflow(a, b, res);
}

static inline bool mul_overflow(unsigned long long a, unsigned long long b,
                                unsigned long long *res)
{
    return umulll_overflow(a, b, res);
}
#endif

/* Free and set set the variable to NULL */
#define FREENULL(a) do { free( a ); a = NULL; } while(0)

#define EMPTY_STR(str) (!str || !*str)

VLC_API char const * vlc_error( int ) VLC_USED;

#include <vlc_arrays.h>

/* MSB (big endian)/LSB (little endian) conversions - network order is always
 * MSB, and should be used for both network communications and files. */

#ifdef WORDS_BIGENDIAN
# define hton16(i) ((uint16_t)(i))
# define hton32(i) ((uint32_t)(i))
# define hton64(i) ((uint64_t)(i))
#else
# define hton16(i) bswap16(i)
# define hton32(i) bswap32(i)
# define hton64(i) bswap64(i)
#endif
#define ntoh16(i) hton16(i)
#define ntoh32(i) hton32(i)
#define ntoh64(i) hton64(i)

/** Reads 16 bits in network byte order */
VLC_USED
static inline uint16_t U16_AT (const void *p)
{
    uint16_t x;

    memcpy (&x, p, sizeof (x));
    return ntoh16 (x);
}

/** Reads 32 bits in network byte order */
VLC_USED
static inline uint32_t U32_AT (const void *p)
{
    uint32_t x;

    memcpy (&x, p, sizeof (x));
    return ntoh32 (x);
}

/** Reads 64 bits in network byte order */
VLC_USED
static inline uint64_t U64_AT (const void *p)
{
    uint64_t x;

    memcpy (&x, p, sizeof (x));
    return ntoh64 (x);
}

#define GetWBE(p)  U16_AT(p)
#define GetDWBE(p) U32_AT(p)
#define GetQWBE(p) U64_AT(p)

/** Reads 16 bits in little-endian order */
VLC_USED
static inline uint16_t GetWLE (const void *p)
{
    uint16_t x;

    memcpy (&x, p, sizeof (x));
#ifdef WORDS_BIGENDIAN
    x = bswap16 (x);
#endif
    return x;
}

/** Reads 32 bits in little-endian order */
VLC_USED
static inline uint32_t GetDWLE (const void *p)
{
    uint32_t x;

    memcpy (&x, p, sizeof (x));
#ifdef WORDS_BIGENDIAN
    x = bswap32 (x);
#endif
    return x;
}

/** Reads 64 bits in little-endian order */
VLC_USED
static inline uint64_t GetQWLE (const void *p)
{
    uint64_t x;

    memcpy (&x, p, sizeof (x));
#ifdef WORDS_BIGENDIAN
    x = bswap64 (x);
#endif
    return x;
}

/** Writes 16 bits in network byte order */
static inline void SetWBE (void *p, uint16_t w)
{
    w = hton16 (w);
    memcpy (p, &w, sizeof (w));
}

/** Writes 32 bits in network byte order */
static inline void SetDWBE (void *p, uint32_t dw)
{
    dw = hton32 (dw);
    memcpy (p, &dw, sizeof (dw));
}

/** Writes 64 bits in network byte order */
static inline void SetQWBE (void *p, uint64_t qw)
{
    qw = hton64 (qw);
    memcpy (p, &qw, sizeof (qw));
}

/** Writes 16 bits in little endian order */
static inline void SetWLE (void *p, uint16_t w)
{
#ifdef WORDS_BIGENDIAN
    w = bswap16 (w);
#endif
    memcpy (p, &w, sizeof (w));
}

/** Writes 32 bits in little endian order */
static inline void SetDWLE (void *p, uint32_t dw)
{
#ifdef WORDS_BIGENDIAN
    dw = bswap32 (dw);
#endif
    memcpy (p, &dw, sizeof (dw));
}

/** Writes 64 bits in little endian order */
static inline void SetQWLE (void *p, uint64_t qw)
{
#ifdef WORDS_BIGENDIAN
    qw = bswap64 (qw);
#endif
    memcpy (p, &qw, sizeof (qw));
}

/* */
#define VLC_UNUSED(x) (void)(x)

/* Stuff defined in src/extras/libc.c */

#if defined(_WIN32)
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

#   ifndef O_NONBLOCK
#       define O_NONBLOCK 0
#   endif

/* the mingw32 swab() and win32 _swab() prototypes expect a char* instead of a
   const void* */
#  define swab(a,b,c)  swab((char*) (a), (char*) (b), (c))


#   include <tchar.h>
#endif /* _WIN32 */

typedef struct {
    unsigned num, den;
} vlc_rational_t;

VLC_API bool vlc_ureduce( unsigned *, unsigned *, uint64_t, uint64_t, uint64_t );

#define container_of(ptr, type, member) \
    ((type *)(((char *)(ptr)) - offsetof(type, member)))

VLC_USED VLC_MALLOC
static inline void *vlc_alloc(size_t count, size_t size)
{
    return mul_overflow(count, size, &size) ? NULL : malloc(size);
}

/*****************************************************************************
 * I18n stuff
 *****************************************************************************/
VLC_API char *vlc_gettext( const char *msgid ) VLC_FORMAT_ARG(1);
VLC_API char *vlc_ngettext( const char *s, const char *p, unsigned long n ) VLC_FORMAT_ARG(1) VLC_FORMAT_ARG(2);

#define vlc_pgettext( ctx, id ) \
        vlc_pgettext_aux( ctx "\004" id, id )

VLC_FORMAT_ARG(2)
static inline const char *vlc_pgettext_aux( const char *ctx, const char *id )
{
    const char *tr = vlc_gettext( ctx );
    return (tr == ctx) ? id : tr;
}

/*****************************************************************************
 * Loosy memory allocation functions. Do not use in new code.
 *****************************************************************************/
static inline void *xmalloc(size_t len)
{
    void *ptr = malloc(len);
    if (unlikely(ptr == NULL && len > 0))
        abort();
    return ptr;
}

static inline void *xrealloc(void *ptr, size_t len)
{
    void *nptr = realloc(ptr, len);
    if (unlikely(nptr == NULL && len > 0))
        abort();
    return nptr;
}

static inline void *xcalloc(size_t n, size_t size)
{
    void *ptr = calloc(n, size);
    if (unlikely(ptr == NULL && (n > 0 || size > 0)))
        abort ();
    return ptr;
}

static inline char *xstrdup (const char *str)
{
    char *ptr = strdup (str);
    if (unlikely(ptr == NULL))
        abort ();
    return ptr;
}

/*****************************************************************************
 * libvlc features
 *****************************************************************************/
VLC_API const char * VLC_CompileBy( void ) VLC_USED;
VLC_API const char * VLC_CompileHost( void ) VLC_USED;
VLC_API const char * VLC_Compiler( void ) VLC_USED;

/*****************************************************************************
 * Additional vlc stuff
 *****************************************************************************/
#include "vlc_messages.h"
#include "vlc_objects.h"
#include "vlc_variables.h"
#include "vlc_main.h"
#include "vlc_configuration.h"

#if defined( _WIN32 ) || defined( __OS2__ )
#   define DIR_SEP_CHAR '\\'
#   define DIR_SEP "\\"
#   define PATH_SEP_CHAR ';'
#   define PATH_SEP ";"
#else
#   define DIR_SEP_CHAR '/'
#   define DIR_SEP "/"
#   define PATH_SEP_CHAR ':'
#   define PATH_SEP ":"
#endif

#define LICENSE_MSG \
  _("This program comes with NO WARRANTY, to the extent permitted by " \
    "law.\nYou may redistribute it under the terms of the GNU General " \
    "Public License;\nsee the file named COPYING for details.\n" \
    "Written by the VideoLAN team; see the AUTHORS file.\n")

#endif /* !VLC_COMMON_H */
