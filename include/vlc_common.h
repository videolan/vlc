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
#if defined( _MSC_VER )
#   pragma warning( disable : 4244 )
#endif

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
#endif

/* Function attributes for compiler warnings */
#ifdef __GNUC__
# define VLC_DEPRECATED __attribute__((deprecated))

# if defined( _WIN32 ) && VLC_GCC_VERSION(4,4)
#  define VLC_FORMAT(x,y) __attribute__ ((format(gnu_printf,x,y)))
# else
#  define VLC_FORMAT(x,y) __attribute__ ((format(printf,x,y)))
# endif
# define VLC_FORMAT_ARG(x) __attribute__ ((format_arg(x)))

# define VLC_MALLOC __attribute__ ((malloc))
# define VLC_NORETURN __attribute__ ((noreturn))

# if VLC_GCC_VERSION(3,4)
#  define VLC_USED __attribute__ ((warn_unused_result))
# else
#  define VLC_USED
# endif

#else
# define VLC_DEPRECATED
# define VLC_FORMAT(x,y)
# define VLC_FORMAT_ARG(x)
# define VLC_MALLOC
# define VLC_NORETURN
# define VLC_USED
#endif


/* Branch prediction */
#ifdef __GNUC__
#   define likely(p)   __builtin_expect(!!(p), 1)
#   define unlikely(p) __builtin_expect(!!(p), 0)
#else
#   define likely(p)   (!!(p))
#   define unlikely(p) (!!(p))
#endif

/* Linkage */
#ifdef __cplusplus
# define VLC_EXTERN extern "C"
#else
# define VLC_EXTERN
#endif

#if defined (WIN32) && defined (DLL_EXPORT)
# define VLC_EXPORT __declspec(dllexport)
#elif VLC_GCC_VERSION(4,0)
# define VLC_EXPORT __attribute__((visibility("default")))
#else
# define VLC_EXPORT
#endif

#define VLC_API VLC_EXTERN VLC_EXPORT


/*****************************************************************************
 * Basic types definitions
 *****************************************************************************/
#if defined( WIN32 ) || defined( UNDER_CE )
#   include <malloc.h>
#   ifndef PATH_MAX
#       define PATH_MAX MAX_PATH
#   endif
#endif

#ifdef __SYMBIAN32__
 #include <sys/syslimits.h>
#endif

/* Counter for statistics and profiling */
typedef unsigned long       count_t;

/* Audio volume */
typedef uint16_t            audio_volume_t;

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
typedef int64_t mtime_t;

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

#define vlc_fourcc_to_char( a, b ) \
        vlc_fourcc_to_char( (vlc_fourcc_t)(a), (char *)(b) )

/*****************************************************************************
 * Classes declaration
 *****************************************************************************/

/* Internal types */
typedef struct vlc_list_t vlc_list_t;
typedef struct vlc_object_t vlc_object_t;
typedef struct libvlc_int_t libvlc_int_t;
typedef struct date_t date_t;

/* Playlist */

/* FIXME */
/**
 * Playlist commands
 */
typedef enum {
    PLAYLIST_PLAY,      /**< No arg.                            res=can fail*/
    PLAYLIST_VIEWPLAY,  /**< arg1= playlist_item_t*,*/
                        /**  arg2 = playlist_item_t*          , res=can fail */
    PLAYLIST_PAUSE,     /**< No arg                             res=can fail*/
    PLAYLIST_STOP,      /**< No arg                             res=can fail*/
    PLAYLIST_SKIP,      /**< arg1=int,                          res=can fail*/
} playlist_command_t;


typedef struct playlist_t playlist_t;
typedef struct playlist_item_t playlist_item_t;
typedef struct playlist_view_t playlist_view_t;
typedef struct services_discovery_t services_discovery_t;
typedef struct services_discovery_sys_t services_discovery_sys_t;
typedef struct playlist_add_t playlist_add_t;

/* Modules */
typedef struct module_t module_t;
typedef struct module_config_t module_config_t;

typedef struct config_category_t config_category_t;

/* Input */
typedef struct input_thread_t input_thread_t;
typedef struct input_item_t input_item_t;
typedef struct input_item_node_t input_item_node_t;
typedef struct access_t access_t;
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
typedef struct aout_fifo_t aout_fifo_t;
typedef struct aout_input_t aout_input_t;
typedef struct block_t aout_buffer_t;
typedef audio_format_t audio_sample_format_t;

/* Video */
typedef struct vout_thread_t vout_thread_t;

typedef video_format_t video_frame_format_t;
typedef struct picture_t picture_t;
typedef struct picture_sys_t picture_sys_t;

/* Subpictures */
typedef struct spu_t spu_t;
typedef struct subpicture_t subpicture_t;
typedef struct subpicture_sys_t subpicture_sys_t;
typedef struct subpicture_region_t subpicture_region_t;

typedef struct image_handler_t image_handler_t;

/* Stream output */
typedef struct sout_instance_t sout_instance_t;
typedef struct sout_instance_sys_t sout_instance_sys_t;

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
typedef struct virtual_socket_t v_socket_t;
typedef struct vlc_acl_t vlc_acl_t;
typedef struct vlc_url_t vlc_url_t;

/* Misc */
typedef struct iso639_lang_t iso639_lang_t;

/* block */
typedef struct block_t      block_t;
typedef struct block_fifo_t block_fifo_t;

/* httpd */
typedef struct httpd_t          httpd_t;
typedef struct httpd_host_t     httpd_host_t;
typedef struct httpd_url_t      httpd_url_t;
typedef struct httpd_client_t   httpd_client_t;
typedef struct httpd_callback_sys_t httpd_callback_sys_t;
typedef struct httpd_message_t  httpd_message_t;
typedef int    (*httpd_callback_t)( httpd_callback_sys_t *, httpd_client_t *, httpd_message_t *answer, const httpd_message_t *query );
typedef struct httpd_file_t     httpd_file_t;
typedef struct httpd_file_sys_t httpd_file_sys_t;
typedef int (*httpd_file_callback_t)( httpd_file_sys_t *, httpd_file_t *, uint8_t *psz_request, uint8_t **pp_data, int *pi_data );
typedef struct httpd_handler_t  httpd_handler_t;
typedef struct httpd_handler_sys_t httpd_handler_sys_t;
typedef int (*httpd_handler_callback_t)( httpd_handler_sys_t *, httpd_handler_t *, char *psz_url, uint8_t *psz_request, int i_type, uint8_t *p_in, int i_in, char *psz_remote_addr, char *psz_remote_host, uint8_t **pp_data, int *pi_data );
typedef struct httpd_redirect_t httpd_redirect_t;
typedef struct httpd_stream_t httpd_stream_t;

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

/* osdmenu */
typedef struct osd_menu_t   osd_menu_t;
typedef struct osd_state_t  osd_state_t;
typedef struct osd_event_t  osd_event_t;
typedef struct osd_button_t osd_button_t;
typedef struct osd_menu_state_t osd_menu_state_t;

/* VLM */
typedef struct vlm_t         vlm_t;
typedef struct vlm_message_t vlm_message_t;

/* misc */
typedef struct vlc_meta_t    vlc_meta_t;
typedef struct input_stats_t input_stats_t;

/* Update */
typedef struct update_t update_t;
typedef struct update_iterator_t update_iterator_t;

/* Meta engine */
typedef struct meta_engine_t meta_engine_t;

/* stat/lstat/fstat */
#ifdef WIN32
# include <sys/stat.h>

# ifndef UNDER_CE
struct _stati64;
#  define stat _stati64
#  define fstat _fstati64
#endif

/* You should otherwise use vlc_stat and vlc_lstat. */
#else
struct stat;
#endif

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
    vlc_object_t *  p_object;
    vlc_list_t *    p_list;
    mtime_t         i_time;
    struct { int32_t x; int32_t y; } coords;

} vlc_value_t;

/**
 * VLC list structure
 */
struct vlc_list_t
{
    int             i_count;
    vlc_value_t *   p_values;
    int *           pi_types;

};

/*****************************************************************************
 * Error values (shouldn't be exposed)
 *****************************************************************************/
#define VLC_SUCCESS         -0                                   /* No error */
#define VLC_ENOMEM          -1                          /* Not enough memory */
#define VLC_ETIMEOUT        -3                                    /* Timeout */

#define VLC_ENOMOD         -10                           /* Module not found */

#define VLC_ENOOBJ         -20                           /* Object not found */

#define VLC_ENOVAR         -30                         /* Variable not found */
#define VLC_EBADVAR        -31                         /* Bad variable value */

#define VLC_ENOITEM        -40                           /**< Item not found */

#define VLC_EEXIT         -255                             /* Program exited */
#define VLC_EEXITSUCCESS  -999                /* Program exited successfully */
#define VLC_EGENERIC      -666                              /* Generic error */

/*****************************************************************************
 * Variable callbacks
 *****************************************************************************/
typedef int ( * vlc_callback_t ) ( vlc_object_t *,      /* variable's object */
                                   char const *,            /* variable name */
                                   vlc_value_t,                 /* old value */
                                   vlc_value_t,                 /* new value */
                                   void * );                /* callback data */

/*****************************************************************************
 * OS-specific headers and thread types
 *****************************************************************************/
#if defined( WIN32 ) || defined( UNDER_CE )
/* WIN32_LEAN_AND_MEAN is needed to be able to include winsock2.h because else,
 * windows.h will also include winsock.h and declarations will conflict */
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#endif

#ifdef __OS2__
#   define OS2EMX_PLAIN_CHAR
#   define INCL_BASE
#   define INCL_PM
#   include <os2.h>
#endif

#include "vlc_mtime.h"
#include "vlc_threads.h"

/**
 * Memory storage space for an atom. Never access it directly.
 */
typedef union
{
    volatile uintptr_t u;
    volatile intptr_t  s;
} vlc_atomic_t;

/*****************************************************************************
 * Common structure members
 *****************************************************************************/

/* VLC_COMMON_MEMBERS : members common to all basic vlc objects */
#define VLC_COMMON_MEMBERS                                                  \
/** \name VLC_COMMON_MEMBERS                                                \
 * these members are common for all vlc objects                             \
 */                                                                         \
/**@{*/                                                                     \
    const char *psz_object_type;                                            \
                                                                            \
    /* Messages header */                                                   \
    char *psz_header;                                                       \
    int  i_flags;                                                           \
                                                                            \
    /* Object properties */                                                 \
    volatile bool b_die;                   /**< set by the outside */ \
    bool b_force;      /**< set by the outside (eg. module_need()) */ \
                                                                            \
    /* Stuff related to the libvlc structure */                             \
    libvlc_int_t *p_libvlc;                  /**< (root of all evil) - 1 */ \
                                                                            \
    vlc_object_t *  p_parent;                            /**< our parent */ \
                                                                            \
/**@}*/                                                                     \

/* VLC_OBJECT: attempt at doing a clever cast */
#if VLC_GCC_VERSION(4,0)
# ifndef __cplusplus
#  define VLC_OBJECT( x ) \
    __builtin_choose_expr( \
        __builtin_offsetof(__typeof__(*(x)), psz_object_type), \
        (void)0 /* screw you */, \
        (vlc_object_t *)(x))
# else
#  define VLC_OBJECT( x ) \
    ((vlc_object_t *)(x) \
      + 0 * __builtin_offsetof(__typeof__(*(x)), psz_object_type))
# endif
#else
# define VLC_OBJECT( x ) ((vlc_object_t *)(x))
#endif

typedef struct gc_object_t
{
    vlc_atomic_t    refs;
    void          (*pf_destructor) (struct gc_object_t *);
} gc_object_t;

/**
 * These members are common to all objects that wish to be garbage-collected.
 */
#define VLC_GC_MEMBERS gc_object_t vlc_gc_data;

VLC_API void * vlc_gc_init(gc_object_t *, void (*)(gc_object_t *));
VLC_API void * vlc_hold(gc_object_t *);
VLC_API void vlc_release(gc_object_t *);

#define vlc_gc_init( a,b ) vlc_gc_init( &(a)->vlc_gc_data, (b) )
#define vlc_gc_incref( a ) vlc_hold( &(a)->vlc_gc_data )
#define vlc_gc_decref( a ) vlc_release( &(a)->vlc_gc_data )
#define vlc_priv( gc, t ) ((t *)(((char *)(gc)) - offsetof(t, vlc_gc_data)))

/*****************************************************************************
 * Macros and inline functions
 *****************************************************************************/

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
static inline unsigned clz (unsigned x)
{
#if VLC_GCC_VERSION(3,4)
    return __builtin_clz (x);
#else
    unsigned i = sizeof (x) * 8;

    while (x)
    {
        x = x >> 1;
        i--;
    }
    return i;
#endif
}

#define clz8( x ) (clz(x) - ((sizeof(unsigned) - sizeof (uint8_t)) * 8))
#define clz16( x ) (clz(x) - ((sizeof(unsigned) - sizeof (uint16_t)) * 8))
/* XXX: this assumes that int is 32-bits or more */
#define clz32( x ) (clz(x) - ((sizeof(unsigned) - sizeof (uint32_t)) * 8))

/** Bit weight */
VLC_USED
static inline unsigned popcount (unsigned x)
{
#if VLC_GCC_VERSION(3,4)
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

#ifdef __OS2__
#   undef bswap16
#   undef bswap32
#   undef bswap64
#endif

/** Byte swap (16 bits) */
VLC_USED
static inline uint16_t bswap16 (uint16_t x)
{
    return (x << 8) | (x >> 8);
}

/** Byte swap (32 bits) */
VLC_USED
static inline uint32_t bswap32 (uint32_t x)
{
#if VLC_GCC_VERSION(4,3)
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
static inline uint64_t bswap64 (uint64_t x)
{
#if VLC_GCC_VERSION(4,3)
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
    return ((x & 0x00000000000000FFLLU) << 56)
         | ((x & 0x000000000000FF00LLU) << 40)
         | ((x & 0x0000000000FF0000LLU) << 24)
         | ((x & 0x00000000FF000000LLU) <<  8)
         | ((x & 0x000000FF00000000LLU) >>  8)
         | ((x & 0x0000FF0000000000LLU) >> 24)
         | ((x & 0x00FF000000000000LLU) >> 40)
         | ((x & 0xFF00000000000000LLU) >> 56);
#endif
}


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

#if defined(WIN32) || defined(UNDER_CE)
/* win32, cl and icl support */
#   if defined( _MSC_VER ) || !defined( __MINGW32__ )
#       define __attribute__(x)
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
            /* for wx compatibility typedef long off_t; */
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

#   include <tchar.h>
#endif

VLC_API bool vlc_ureduce( unsigned *, unsigned *, uint64_t, uint64_t, uint64_t );

/* Aligned memory allocator */
#ifdef __APPLE__
#include <AvailabilityMacros.h>
#endif

#ifdef WIN32
# include <malloc.h>
# define vlc_memalign(align, size) (__mingw_aligned_malloc(size, align))
# define vlc_free(base)            (__mingw_aligned_free(base))
#elif defined(__APPLE__) && !defined(MAC_OS_X_VERSION_10_6)
static inline void *vlc_memalign(size_t align, size_t size)
{
    long diff;
    void *ptr;

    ptr = malloc(size+align);
    if(!ptr)
        return ptr;
    diff = ((-(long)ptr - 1)&(align-1)) + 1;
    ptr  = (char*)ptr + diff;
    ((char*)ptr)[-1]= diff;
    return ptr;
}

static void vlc_free(void *ptr)
{
    if (ptr)
        free((char*)ptr - ((char*)ptr)[-1]);
}
#elif defined(__ANDROID__)
# define vlc_memalign(align, size) memalign(align, size)
# define vlc_free(base) free(base)
#else
static inline void *vlc_memalign(size_t align, size_t size)
{
    void *base;
    if (unlikely(posix_memalign(&base, align, size)))
        base = NULL;
    return base;
}
# define vlc_free(base) free(base)
#endif

VLC_API void vlc_tdestroy( void *, void (*)(void *) );

/* Fast large memory copy and memory set */
VLC_API void * vlc_memcpy( void *, const void *, size_t );
#define vlc_memset memset

/*****************************************************************************
 * I18n stuff
 *****************************************************************************/
VLC_API char * vlc_gettext( const char *msgid ) VLC_FORMAT_ARG(1);

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
static inline void *xmalloc (size_t len)
{
    void *ptr = malloc (len);
    if (unlikely (ptr == NULL))
        abort ();
    return ptr;
}

static inline void *xrealloc (void *ptr, size_t len)
{
    void *nptr = realloc (ptr, len);
    if (unlikely (nptr == NULL))
        abort ();
    return nptr;
}

static inline void *xcalloc (size_t n, size_t size)
{
    void *ptr = calloc (n, size);
    if (unlikely (ptr == NULL))
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

#if defined( WIN32 ) || defined( UNDER_CE ) || defined( __SYMBIAN32__ ) || defined( __OS2__ )
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
