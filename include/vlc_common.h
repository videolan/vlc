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

#ifndef VLC_COMMON_H
# define VLC_COMMON_H 1

/**
 * \defgroup vlc VLC plug-in programming interface
 * \file
 * \ingroup vlc
 * This file is a collection of common definitions and types
 */

/*****************************************************************************
 * Required vlc headers
 *****************************************************************************/
#include "vlc_config.h"

/*****************************************************************************
 * Required system headers
 *****************************************************************************/
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <stddef.h>

#ifndef __cplusplus
# include <stdbool.h>
#endif

/**
 * \defgroup cext C programming language extensions
 * \ingroup vlc
 *
 * This section defines a number of macros and inline functions extending the
 * C language. Most extensions are implemented by GCC and LLVM/Clang, and have
 * unoptimized fallbacks for other C11/C++11 conforming compilers.
 * @{
 */
#ifdef __GNUC__
# define VLC_GCC_VERSION(maj,min) \
    ((__GNUC__ > (maj)) || (__GNUC__ == (maj) && __GNUC_MINOR__ >= (min)))
#else
/** GCC version check */
# define VLC_GCC_VERSION(maj,min) (0)
#endif

/* Function attributes for compiler warnings */
#if defined __has_attribute
# if __has_attribute(warning)
#  define VLC_WARN_CALL(w)  VLC_NOINLINE_FUNC __attribute__((warning((w))))
# else
#  define VLC_WARN_CALL(w)
# endif

# if __has_attribute(error)
#  define VLC_ERROR_CALL(e)  VLC_NOINLINE_FUNC __attribute__((error((e))))
# else
#  define VLC_ERROR_CALL(e)
# endif

# if __has_attribute(unused)
#  define VLC_UNUSED_FUNC  __attribute__((unused))
# else
#  define VLC_UNUSED_FUNC
# endif

# if __has_attribute(noinline)
#  define VLC_NOINLINE_FUNC  __attribute__((noinline))
# else
#  define VLC_NOINLINE_FUNC
# endif

# if __has_attribute(deprecated)
#  define VLC_DEPRECATED  __attribute__((deprecated))
# else
/**
 * Deprecated functions or compound members annotation
 *
 * Use this macro in front of a function declaration or compound member
 * within a compound type declaration.
 * The compiler may emit a warning every time the function or member is used.
 *
 * Use \ref VLC_DEPRECATED_ENUM instead for enumeration members.
 */
#  define VLC_DEPRECATED
# endif

# if __has_attribute(malloc)
#  define VLC_MALLOC  __attribute__((malloc))
# else
/**
 * Heap allocated result function annotation
 *
 * Use this macro to annotate a function that returns a pointer to memory that
 * cannot alias any other valid pointer.
 *
 * This is primarily used for functions that return a pointer to heap-allocated
 * memory, but it can be used for other applicable purposes.
 *
 * \warning Do not use this annotation if the returned pointer can in any way
 * alias a valid pointer at the time the function exits. This could lead to
 * very weird memory corruption bugs.
 */
#  define VLC_MALLOC
# endif

# if __has_attribute(warn_unused_result)
#  define VLC_USED  __attribute__((warn_unused_result))
# else
/**
 * Used result function annotation
 *
 * Use this macro to annotate a function whose result must be used.
 *
 * There are several cases where this is useful:
 * - If a function has no side effects (or no useful side effects), such that
 *   the only useful purpose of calling said function is to obtain its
 *   return value.
 * - If ignoring the function return value would lead to a resource leak
 *   (including but not limited to a memory leak).
 * - If a function cannot be used correctly without checking its return value.
 *   For instance, if the function can fail at any time.
 *
 * The compiler may warn if the return value of a function call is ignored.
 */
#  define VLC_USED
# endif
#elif defined(_MSC_VER)
# define VLC_USED _Check_return_
// # define VLC_MALLOC __declspec(allocator)
# define VLC_MALLOC
// # define VLC_DEPRECATED __declspec(deprecated)
# define VLC_DEPRECATED
#else // !GCC && !MSVC
# define VLC_USED
# define VLC_MALLOC
# define VLC_DEPRECATED
#endif


#ifdef __GNUC__
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

#else
/**
 * Deprecated enum member annotation
 *
 * Use this macro after an enumerated type member declaration.
 * The compiler may emit a warning every time the enumeration member is used.
 *
 * See also \ref VLC_DEPRECATED.
 */
# define VLC_DEPRECATED_ENUM

/**
 * String format function annotation
 *
 * Use this macro after a function prototype/declaration if the function
 * expects a standard C format string. This helps compiler diagnostics.
 *
 * @param x the position (starting from 1) of the format string argument
 * @param y the first position (also starting from 1) of the variable arguments
 *          following the format string (usually but not always \p x+1).
 */
# define VLC_FORMAT(x,y)

/**
 * Format string translation function annotation
 *
 * Use this macro after a function prototype/declaration if the function
 * expects a format string as input and returns another format string as output
 * to another function.
 *
 * This is primarily intended for localization functions such as gettext().
 */
# define VLC_FORMAT_ARG(x)
#endif

#if defined (__ELF__) || defined (__MACH__) || defined (__wasm__)
# define VLC_WEAK __attribute__((weak))
#else
/**
 * Weak symbol annotation
 *
 * Use this macro before an external identifier \b definition to mark it as a
 * weak symbol. A weak symbol can be overridden by another symbol of the same
 * name at the link time.
 */
# define VLC_WEAK
#endif

/* Branch prediction */
#if defined (__GNUC__) || defined (__clang__)
# define likely(p)     __builtin_expect(!!(p), 1)
# define unlikely(p)   __builtin_expect(!!(p), 0)
# define unreachable() __builtin_unreachable()
#elif defined(_MSC_VER)
# define likely(p)     (!!(p))
# define unlikely(p)   (!!(p))
# define unreachable() (__assume(0))
#else
/**
 * Predicted true condition
 *
 * This macro indicates that the condition is expected most often true.
 * The compiler may optimize the code assuming that this condition is usually
 * met.
 */
# define likely(p)     (!!(p))

/**
 * Predicted false condition
 *
 * This macro indicates that the condition is expected most often false.
 * The compiler may optimize the code assuming that this condition is rarely
 * met.
 */
# define unlikely(p)   (!!(p))

/**
 * Impossible branch
 *
 * This macro indicates that the branch cannot be reached at run-time, and
 * represents undefined behaviour.
 * The compiler may optimize the code assuming that the call flow will never
 * logically reach the point where this macro is expanded.
 *
 * See also \ref vlc_assert_unreachable.
 */
# define unreachable() ((void)0)
#endif

/**
 * Impossible branch assertion
 *
 * This macro asserts that the branch cannot be reached at run-time.
 *
 * If the branch is reached in a debug build, it will trigger an assertion
 * failure and abnormal program termination.
 *
 * If the branch is reached in a non-debug build, this macro is equivalent to
 * \ref unreachable and the behaviour is undefined.
 */
#define vlc_assert_unreachable() (vlc_assert(!"unreachable"), unreachable())

/**
 * Run-time assertion
 *
 * This macro performs a run-time assertion if C assertions are enabled
 * and the following preprocessor symbol is defined:
 * @verbatim LIBVLC_INTERNAL_ @endverbatim
 * That restriction ensures that assertions in public header files are not
 * unwittingly <i>leaked</i> to externally-compiled plug-ins
 * including those header files.
 *
 * Within the LibVLC code base, this is exactly the same as assert(), which can
 * and probably should be used directly instead.
 */
#ifdef LIBVLC_INTERNAL_
# define vlc_assert(pred) assert(pred)
#else
# define vlc_assert(pred) ((void)0)
#endif

/* Linkage */
#ifdef __cplusplus
# define VLC_EXTERN extern "C"
#else
# define VLC_EXTERN
#endif

#if defined (_WIN32) && defined (VLC_DLL_EXPORT)
# define VLC_EXPORT __declspec(dllexport)
#elif defined (__GNUC__)
# define VLC_EXPORT __attribute__((visibility("default")))
#else
# define VLC_EXPORT
#endif

/**
 * Exported API call annotation
 *
 * This macro is placed before a function declaration to indicate that the
 * function is an API call of the LibVLC plugin API.
 */
#define VLC_API VLC_EXTERN VLC_EXPORT

/** @} */

/*****************************************************************************
 * Basic types definitions
 *****************************************************************************/
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
typedef struct vlc_object_t vlc_object_t;
typedef struct libvlc_int_t libvlc_int_t;
typedef struct date_t date_t;

/* Playlist */

typedef struct services_discovery_t services_discovery_t;
typedef struct vlc_renderer_discovery_t vlc_renderer_discovery_t;
typedef struct vlc_renderer_item_t vlc_renderer_item_t;

/* Modules */
typedef struct module_t module_t;
typedef struct module_config_t module_config_t;

typedef struct config_category_t config_category_t;

/* Input */
typedef struct input_item_t input_item_t;
typedef struct input_item_node_t input_item_node_t;
typedef struct input_source_t input_source_t;
typedef struct stream_t     stream_t;
typedef struct stream_t demux_t;
typedef struct es_out_t     es_out_t;
typedef struct es_out_id_t  es_out_id_t;
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
typedef struct vlc_es_id_t vlc_es_id_t;

/* Audio */
typedef struct audio_output audio_output_t;
typedef audio_format_t audio_sample_format_t;

/* Video */
typedef struct vout_thread_t vout_thread_t;
typedef struct vlc_viewpoint_t vlc_viewpoint_t;

typedef video_format_t video_frame_format_t;
typedef struct picture_t picture_t;

/* Subpictures */
typedef struct spu_t spu_t;
typedef struct subpicture_t subpicture_t;
typedef struct subpicture_region_t subpicture_region_t;

typedef struct image_handler_t image_handler_t;

/* Stream output */
typedef struct sout_input_t sout_input_t;
typedef struct sout_packetizer_input_t sout_packetizer_input_t;

typedef struct sout_access_out_t sout_access_out_t;

typedef struct sout_mux_t sout_mux_t;

typedef struct sout_stream_t    sout_stream_t;

typedef struct config_chain_t       config_chain_t;
typedef struct session_descriptor_t session_descriptor_t;

/* Decoders */
typedef struct decoder_t         decoder_t;

/* Encoders */
typedef struct encoder_t      encoder_t;

/* Filters */
typedef struct filter_t filter_t;

/* Network */
typedef struct vlc_url_t vlc_url_t;

/* Misc */
typedef struct iso639_lang_t iso639_lang_t;

/* block */
typedef struct vlc_frame_t  block_t;
typedef struct vlc_fifo_t vlc_fifo_t;
typedef struct vlc_fifo_t block_fifo_t;

/* Hashing */
typedef struct vlc_hash_md5_ctx vlc_hash_md5_t;

/* XML */
typedef struct xml_t xml_t;
typedef struct xml_reader_t xml_reader_t;

/* vod server */
typedef struct vod_t     vod_t;
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
    struct { int32_t x; int32_t y; } coords;

} vlc_value_t;

/**
 * \defgroup errors Error codes
 * \ingroup cext
 * @{
 */
/** No error */
#define VLC_SUCCESS        0
/** Unspecified error */
#define VLC_EGENERIC       (-2 * (1 << (sizeof (int) * 8 - 2))) /* INT_MIN */
/** Not enough memory */
#define VLC_ENOMEM         (-ENOMEM)
/** Timeout */
#define VLC_ETIMEOUT       (-ETIMEDOUT)
/** Not found */
#define VLC_ENOENT         (-ENOENT)
/** Bad variable value */
#define VLC_EINVAL         (-EINVAL)
/** Operation forbidden */
#define VLC_EACCES         (-EACCES)
/** Operation not supported */
#define VLC_ENOTSUP        (-ENOTSUP)

/** @} */

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

#include "vlc_tick.h"
#include "vlc_threads.h"

/**
 * \defgroup intops Integer operations
 * \ingroup cext
 *
 * Common integer functions.
 * @{
 */
/* __MAX and __MIN: self explanatory */
#ifndef __MAX
#   define __MAX(a, b)   ( ((a) > (b)) ? (a) : (b) )
#endif
#ifndef __MIN
#   define __MIN(a, b)   ( ((a) < (b)) ? (a) : (b) )
#endif

/* clip v in [min, max] */
#define VLC_CLIP(v, min, max)    __MIN(__MAX((v), (min)), (max))

/**
 * Make integer v a multiple of align
 *
 * \note align must be a power of 2
 */
VLC_USED
static inline size_t vlc_align(size_t v, size_t align)
{
    return (v + (align - 1)) & ~(align - 1);
}

#if defined __has_attribute
# if __has_attribute(diagnose_if)
static inline size_t vlc_align(size_t v, size_t align)
    __attribute__((diagnose_if(((align & (align - 1)) || (align == 0)),
        "align must be power of 2", "error")));
# endif
#endif

/** Greatest common divisor */
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

/**
 * \defgroup bitops Bit operations
 * @{
 */

#define VLC_INT_FUNC(basename) \
        VLC_INT_FUNC_TYPE(basename, unsigned, ) \
        VLC_INT_FUNC_TYPE(basename, unsigned long, l) \
        VLC_INT_FUNC_TYPE(basename, unsigned long long, ll)

#if defined (__GNUC__) || defined (__clang__)
# define VLC_INT_FUNC_TYPE(basename,type,suffix) \
VLC_USED static inline int vlc_##basename##suffix(type x) \
{ \
    return __builtin_##basename##suffix(x); \
}

VLC_INT_FUNC(clz)
#else
VLC_USED static inline int vlc_clzll(unsigned long long x)
{
    int i = sizeof (x) * 8;

    while (x)
    {
        x >>= 1;
        i--;
    }
    return i;
}

VLC_USED static inline int vlc_clzl(unsigned long x)
{
    return vlc_clzll(x) - ((sizeof (long long) - sizeof (long)) * 8);
}

VLC_USED static inline int vlc_clz(unsigned x)
{
    return vlc_clzll(x) - ((sizeof (long long) - sizeof (int)) * 8);
}

VLC_USED static inline int vlc_ctz_generic(unsigned long long x)
{
    unsigned i = sizeof (x) * 8;

    while (x)
    {
        x <<= 1;
        i--;
    }
    return i;
}

VLC_USED static inline int vlc_parity_generic(unsigned long long x)
{
    for (unsigned i = 4 * sizeof (x); i > 0; i /= 2)
        x ^= x >> i;
    return x & 1;
}

VLC_USED static inline int vlc_popcount_generic(unsigned long long x)
{
    int count = 0;
    while (x)
    {
        count += x & 1;
        x = x >> 1;
    }
    return count;
}

# define VLC_INT_FUNC_TYPE(basename,type,suffix) \
VLC_USED static inline int vlc_##basename##suffix(type x) \
{ \
    return vlc_##basename##_generic(x); \
}
#endif

VLC_INT_FUNC(ctz)
VLC_INT_FUNC(parity)
VLC_INT_FUNC(popcount)

#ifndef __cplusplus
# define VLC_INT_GENERIC(func,x) \
    _Generic((x), \
        unsigned char:      func(x), \
          signed char:      func(x), \
        unsigned short:     func(x), \
          signed short:     func(x), \
        unsigned int:       func(x), \
          signed int:       func(x), \
        unsigned long:      func##l(x), \
          signed long:      func##l(x), \
        unsigned long long: func##ll(x), \
          signed long long: func##ll(x))

/**
 * Count leading zeroes
 *
 * This function counts the number of consecutive zero (clear) bits
 * down from the highest order bit in an unsigned integer.
 *
 * \param x a non-zero integer
 * \note This macro assumes that CHAR_BIT equals 8.
 * \warning By definition, the result depends on the (width of the) type of x.
 * \return The number of leading zero bits in x.
 */
# define clz(x) \
    _Generic((x), \
        unsigned char: (vlc_clz(x) - (sizeof (unsigned) - 1) * 8), \
        unsigned short: (vlc_clz(x) \
        - (sizeof (unsigned) - sizeof (unsigned short)) * 8), \
        unsigned: vlc_clz(x), \
        unsigned long: vlc_clzl(x), \
        unsigned long long: vlc_clzll(x))

/**
 * Count trailing zeroes
 *
 * This function counts the number of consecutive zero bits
 * up from the lowest order bit in an unsigned integer.
 *
 * \param x a non-zero integer
 * \note This function assumes that CHAR_BIT equals 8.
 * \return The number of trailing zero bits in x.
 */
# define ctz(x) VLC_INT_GENERIC(vlc_ctz, x)

/**
 * Parity
 *
 * This function determines the parity of an integer.
 * \retval 0 if x has an even number of set bits.
 * \retval 1 if x has an odd number of set bits.
 */
# define parity(x) VLC_INT_GENERIC(vlc_parity, x)

/**
 * Bit weight / population count
 *
 * This function counts the number of non-zero bits in an integer.
 *
 * \return The count of non-zero bits.
 */
# define vlc_popcount(x) \
    _Generic((x), \
        signed char:  vlc_popcount((unsigned char)(x)), \
        signed short: vlc_popcount((unsigned short)(x)), \
        default: VLC_INT_GENERIC(vlc_popcount ,x))
#else
VLC_USED static inline int vlc_popcount(unsigned char x)
{
    return vlc_popcount((unsigned)x);
}

VLC_USED static inline int vlc_popcount(unsigned short x)
{
    return vlc_popcount((unsigned)x);
}

VLC_USED static inline int vlc_popcount(unsigned long x)
{
    return vlc_popcountl(x);
}

VLC_USED static inline int vlc_popcount(unsigned long long x)
{
    return vlc_popcountll(x);
}
#endif

/** Byte swap (16 bits) */
VLC_USED
static inline uint16_t vlc_bswap16(uint16_t x)
{
    return (x << 8) | (x >> 8);
}

/** Byte swap (32 bits) */
VLC_USED
static inline uint32_t vlc_bswap32(uint32_t x)
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
static inline uint64_t vlc_bswap64(uint64_t x)
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
/** @} */

/**
 * \defgroup overflow Overflowing arithmetic
 * @{
 */
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
/**
 * Overflowing addition
 *
 * Converts \p a and \p b to the type of \p *r.
 * Then computes the sum of both conversions while checking for overflow.
 * Finally stores the result in \p *r.
 *
 * \param a an integer
 * \param b an integer
 * \param r a pointer to an integer [OUT]
 * \retval false The sum did not overflow.
 * \retval true The sum overflowed.
 */
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
/**
 * Overflowing multiplication
 *
 * Converts \p a and \p b to the type of \p *r.
 * Then computes the product of both conversions while checking for overflow.
 * Finally stores the result in \p *r.
 *
 * \param a an integer
 * \param b an integer
 * \param r a pointer to an integer [OUT]
 * \retval false The product did not overflow.
 * \retval true The product overflowed.
 */
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
/** @} */
/** @} */

/* Free and set set the variable to NULL */
#define FREENULL(a) do { free( a ); a = NULL; } while(0)

#define EMPTY_STR(str) (!str || !*str)

#include <vlc_arrays.h>

/* MSB (big endian)/LSB (little endian) conversions - network order is always
 * MSB, and should be used for both network communications and files. */

#ifdef WORDS_BIGENDIAN
# define hton16(i) ((uint16_t)(i))
# define hton32(i) ((uint32_t)(i))
# define hton64(i) ((uint64_t)(i))
#else
# define hton16(i) vlc_bswap16(i)
# define hton32(i) vlc_bswap32(i)
# define hton64(i) vlc_bswap64(i)
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
    x = vlc_bswap16 (x);
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
    x = vlc_bswap32 (x);
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
    x = vlc_bswap64 (x);
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
    w = vlc_bswap16 (w);
#endif
    memcpy (p, &w, sizeof (w));
}

/** Writes 32 bits in little endian order */
static inline void SetDWLE (void *p, uint32_t dw)
{
#ifdef WORDS_BIGENDIAN
    dw = vlc_bswap32 (dw);
#endif
    memcpy (p, &dw, sizeof (dw));
}

/** Writes 64 bits in little endian order */
static inline void SetQWLE (void *p, uint64_t qw)
{
#ifdef WORDS_BIGENDIAN
    qw = vlc_bswap64 (qw);
#endif
    memcpy (p, &qw, sizeof (qw));
}

/* */
#define VLC_UNUSED(x) (void)(x)

/* Stuff defined in src/extras/libc.c */

#if defined(_WIN32)
/* several type definitions */
#   ifndef O_NONBLOCK
#       define O_NONBLOCK 0
#   endif

/* the mingw32 swab() and win32 _swab() prototypes expect a char* instead of a
   const void* */
#  define swab(a,b,c)  _swab((char*) (a), (char*) (b), (c))

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

VLC_USED
static inline void *vlc_reallocarray(void *ptr, size_t count, size_t size)
{
    return mul_overflow(count, size, &size) ? NULL : realloc(ptr, size);
}

/*****************************************************************************
 * I18n stuff
 *****************************************************************************/
VLC_API const char *vlc_gettext(const char *msgid) VLC_FORMAT_ARG(1);
VLC_API const char *vlc_ngettext(const char *s, const char *p, unsigned long n)
VLC_FORMAT_ARG(1) VLC_FORMAT_ARG(2);

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

#if defined(__cplusplus) || defined(_MSC_VER)
#define ARRAY_STATIC_SIZE
#else
#define ARRAY_STATIC_SIZE  static
#endif

#endif /* !VLC_COMMON_H */
