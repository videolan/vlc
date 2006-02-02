/*****************************************************************************
 * common.h: common definitions
 * Collection of useful common types and macros definitions
 *****************************************************************************
 * Copyright (C) 1998-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@via.ecp.fr>
 *          Vincent Seguin <seguin@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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
 * \file
 * This file is a collection of common definitions and types
 */

/*****************************************************************************
 * Required vlc headers
 *****************************************************************************/
#if defined( __BORLANDC__ )
#   undef PACKAGE
#endif

#include "config.h"

#if defined(PACKAGE)
#   undef PACKAGE_NAME
#   define PACKAGE_NAME PACKAGE
#endif
#if defined(VERSION)
#   undef PACKAGE_VERSION
#   define PACKAGE_VERSION VERSION
#endif

#if defined( __BORLANDC__ )
#   undef HAVE_VARIADIC_MACROS
#   undef HAVE_STDINT_H
#   undef HAVE_INTTYPES_H
#   undef off_t
#elif defined( _MSC_VER )
#   pragma warning( disable : 4244 )
#endif

#include "vlc_config.h"
#include "modules_inner.h"

/*****************************************************************************
 * Required system headers
 *****************************************************************************/
#include <stdlib.h>
#include <stdarg.h>

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
    typedef uint32_t            uintptr_t;
    typedef int32_t             intptr_t;
#endif

typedef uint8_t                 byte_t;

/* Systems that don't have stdint.h may not define INT64_MIN and
   INT64_MAX */
#ifndef INT64_MIN
#define INT64_MIN (-9223372036854775807LL-1)
#endif
#ifndef INT64_MAX
#define INT64_MAX (9223372036854775807LL)
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

#if defined( WIN32 ) || defined( UNDER_CE )
#   include <malloc.h>
#   ifndef PATH_MAX
#       define PATH_MAX MAX_PATH
#   endif
#endif

#if (defined( WIN32 ) || defined( UNDER_CE )) && !defined( _SSIZE_T_ )
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

/*****************************************************************************
 * Classes declaration
 *****************************************************************************/

/* Internal types */
typedef struct libvlc_t libvlc_t;
typedef struct vlc_t vlc_t;
typedef struct variable_t variable_t;
typedef struct date_t date_t;

/* Messages */
typedef struct msg_bank_t msg_bank_t;
typedef struct msg_queue_t msg_queue_t;
typedef struct msg_subscription_t msg_subscription_t;

/* Playlist */

/* FIXME */
/**
 * Playlist commands
 */
typedef enum {
    PLAYLIST_PLAY,      /**< No arg.                            res=can fail*/
    PLAYLIST_AUTOPLAY,  /**< No arg.                            res=cant fail*/
    PLAYLIST_VIEWPLAY,  /**< arg1= int, arg2= playlist_item_t*,*/
                        /**  arg3 = playlist_item_t*          , res=can fail */
    PLAYLIST_ITEMPLAY,  /** <arg1 = playlist_item_t *         , res=can fail */
    PLAYLIST_PAUSE,     /**< No arg                             res=can fail*/
    PLAYLIST_STOP,      /**< No arg                             res=can fail*/
    PLAYLIST_SKIP,      /**< arg1=int,                          res=can fail*/
    PLAYLIST_GOTO,      /**< arg1=int                           res=can fail */
    PLAYLIST_VIEWGOTO   /**< arg1=int                           res=can fail */
} playlist_command_t;


typedef struct playlist_t playlist_t;
typedef struct playlist_item_t playlist_item_t;
typedef struct playlist_view_t playlist_view_t;
typedef struct playlist_export_t playlist_export_t;
typedef struct services_discovery_t services_discovery_t;
typedef struct services_discovery_sys_t services_discovery_sys_t;
typedef struct playlist_add_t playlist_add_t;
typedef struct playlist_preparse_t playlist_preparse_t;

/* Modules */
typedef struct module_bank_t module_bank_t;
typedef struct module_t module_t;
typedef struct module_config_t module_config_t;
typedef struct module_symbols_t module_symbols_t;
typedef struct module_cache_t module_cache_t;

typedef struct config_category_t config_category_t;

/* Interface */
typedef struct intf_thread_t intf_thread_t;
typedef struct intf_sys_t intf_sys_t;
typedef struct intf_console_t intf_console_t;
typedef struct intf_msg_t intf_msg_t;
typedef struct interaction_t interaction_t;
typedef struct interaction_dialog_t interaction_dialog_t;
typedef struct user_widget_t user_widget_t;

/* Input */
typedef struct input_thread_t input_thread_t;
typedef struct input_thread_sys_t input_thread_sys_t;
typedef struct input_item_t input_item_t;
typedef struct access_t access_t;
typedef struct access_sys_t access_sys_t;
typedef struct stream_t     stream_t;
typedef struct stream_sys_t stream_sys_t;
typedef struct demux_t  demux_t;
typedef struct demux_sys_t demux_sys_t;
typedef struct es_out_t     es_out_t;
typedef struct es_out_id_t  es_out_id_t;
typedef struct es_out_sys_t es_out_sys_t;
typedef struct es_descriptor_t es_descriptor_t;
typedef struct seekpoint_t seekpoint_t;
typedef struct info_t info_t;
typedef struct info_category_t info_category_t;

/* Format */
typedef struct audio_format_t audio_format_t;
typedef struct video_format_t video_format_t;
typedef struct subs_format_t subs_format_t;
typedef struct es_format_t es_format_t;
typedef struct video_palette_t video_palette_t;

/* Audio */
typedef struct aout_instance_t aout_instance_t;
typedef struct aout_sys_t aout_sys_t;
typedef struct aout_fifo_t aout_fifo_t;
typedef struct aout_input_t aout_input_t;
typedef struct aout_buffer_t aout_buffer_t;
typedef audio_format_t audio_sample_format_t;
typedef struct audio_date_t audio_date_t;
typedef struct aout_filter_t aout_filter_t;

/* Video */
typedef struct vout_thread_t vout_thread_t;
typedef struct vout_sys_t vout_sys_t;
typedef struct vout_synchro_t vout_synchro_t;
typedef struct chroma_sys_t chroma_sys_t;

typedef video_format_t video_frame_format_t;
typedef struct picture_t picture_t;
typedef struct picture_sys_t picture_sys_t;
typedef struct picture_heap_t picture_heap_t;

/* Subpictures */
typedef struct spu_t spu_t;
typedef struct subpicture_t subpicture_t;
typedef struct subpicture_sys_t subpicture_sys_t;
typedef struct subpicture_region_t subpicture_region_t;
typedef struct text_style_t text_style_t;

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

typedef struct sout_cfg_t       sout_cfg_t;
typedef struct sap_session_t    sap_session_t;
typedef struct sap_address_t sap_address_t;
typedef struct session_descriptor_t session_descriptor_t;
typedef struct announce_method_t announce_method_t;
typedef struct announce_handler_t announce_handler_t;
typedef struct sap_handler_t sap_handler_t;

/* Decoders */
typedef struct decoder_t      decoder_t;
typedef struct decoder_sys_t  decoder_sys_t;

/* Encoders */
typedef struct encoder_t      encoder_t;
typedef struct encoder_sys_t  encoder_sys_t;

/* Filters */
typedef struct filter_t filter_t;
typedef struct filter_sys_t filter_sys_t;

/* Network */
typedef struct network_socket_t network_socket_t;
typedef struct virtual_socket_t v_socket_t;
typedef struct sockaddr sockaddr;
typedef struct addrinfo addrinfo;
typedef struct vlc_acl_t vlc_acl_t;

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
typedef int    (*httpd_callback_t)( httpd_callback_sys_t *, httpd_client_t *, httpd_message_t *answer, httpd_message_t *query );
typedef struct httpd_file_t     httpd_file_t;
typedef struct httpd_file_sys_t httpd_file_sys_t;
typedef int (*httpd_file_callback_t)( httpd_file_sys_t *, httpd_file_t *, uint8_t *psz_request, uint8_t **pp_data, int *pi_data );
typedef struct httpd_handler_t  httpd_handler_t;
typedef struct httpd_handler_sys_t httpd_handler_sys_t;
typedef int (*httpd_handler_callback_t)( httpd_handler_sys_t *, httpd_handler_t *, char *psz_url, uint8_t *psz_request, int i_type, uint8_t *p_in, int i_in, char *psz_remote_addr, char *psz_remote_host, uint8_t **pp_data, int *pi_data );
typedef struct httpd_redirect_t httpd_redirect_t;
typedef struct httpd_stream_t httpd_stream_t;

/* TLS support */
typedef struct tls_t tls_t;
typedef struct tls_server_t tls_server_t;
typedef struct tls_session_t tls_session_t;

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

/* opengl */
typedef struct opengl_t     opengl_t;
typedef struct opengl_sys_t opengl_sys_t;

/* osdmenu */
typedef struct osd_menu_t   osd_menu_t;
typedef struct osd_state_t  osd_state_t;
typedef struct osd_event_t  osd_event_t;
typedef struct osd_button_t osd_button_t;
typedef struct osd_menu_state_t osd_menu_state_t;

/* VLM */
typedef struct vlm_t         vlm_t;
typedef struct vlm_message_t vlm_message_t;
typedef struct vlm_media_t   vlm_media_t;
typedef struct vlm_schedule_t vlm_schedule_t;

/* divers */
typedef struct vlc_meta_t    vlc_meta_t;

/* Stats */
typedef struct counter_t     counter_t;
typedef struct counter_sample_t counter_sample_t;
typedef struct stats_handler_t stats_handler_t;
typedef struct input_stats_t input_stats_t;
typedef struct global_stats_t global_stats_t;

/* Update */
typedef struct update_t update_t;
typedef struct update_iterator_t update_iterator_t;

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
#   if defined( UNDER_CE )
#      define IS_WINNT 0
#   else
#      define IS_WINNT ( GetVersion() < 0x80000000 )
#   endif
#endif

#include "vlc_threads.h"

/*****************************************************************************
 * Common structure members
 *****************************************************************************/

/* VLC_COMMON_MEMBERS : members common to all basic vlc objects */
#define VLC_COMMON_MEMBERS                                                  \
/** \name VLC_COMMON_MEMBERS                                                \
 * these members are common for all vlc objects                             \
 */                                                                         \
/**@{*/                                                                     \
    int   i_object_id;                                                      \
    int   i_object_type;                                                    \
    char *psz_object_type;                                                  \
    char *psz_object_name;                                                  \
                                                                            \
    /* Messages header */                                                   \
    char *psz_header;                                                       \
    int  i_flags;                                                           \
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
    volatile vlc_bool_t b_error;                  /**< set by the object */ \
    volatile vlc_bool_t b_die;                   /**< set by the outside */ \
    volatile vlc_bool_t b_dead;                   /**< set by the object */ \
    volatile vlc_bool_t b_attached;               /**< set by the object */ \
    vlc_bool_t b_force;      /**< set by the outside (eg. module_Need()) */ \
                                                                            \
    /* Object variables */                                                  \
    vlc_mutex_t     var_lock;                                               \
    int             i_vars;                                                 \
    variable_t *    p_vars;                                                 \
                                                                            \
    /* Stuff related to the libvlc structure */                             \
    libvlc_t *      p_libvlc;                      /**< root of all evil */ \
    vlc_t *         p_vlc;                   /**< (root of all evil) - 1 */ \
                                                                            \
    volatile int    i_refcount;                         /**< usage count */ \
    vlc_object_t *  p_parent;                            /**< our parent */ \
    vlc_object_t ** pp_children;                       /**< our children */ \
    volatile int    i_children;                                             \
                                                                            \
    /* Private data */                                                      \
    void *          p_private;                                              \
                                                                            \
    /** Just a reminder so that people don't cast garbage */                \
    int be_sure_to_add_VLC_COMMON_MEMBERS_to_struct;                        \
/**@}*/                                                                     \

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

static int64_t GCD( int64_t a, int64_t b )
{
    if( b ) return GCD( b, a % b );
    else return a;
}

/* Dynamic array handling: realloc array, move data, increment position */
#if defined( _MSC_VER ) && _MSC_VER < 1300 && !defined( UNDER_CE )
#   define VLCCVP (void**) /* Work-around for broken compiler */
#else
#   define VLCCVP
#endif
#define INSERT_ELEM( p_ar, i_oldsize, i_pos, elem )                           \
    do                                                                        \
    {                                                                         \
        if( !i_oldsize ) (p_ar) = NULL;                                       \
        (p_ar) = VLCCVP realloc( p_ar, ((i_oldsize) + 1) * sizeof(*(p_ar)) ); \
        if( (i_oldsize) - (i_pos) )                                           \
        {                                                                     \
            memmove( (p_ar) + (i_pos) + 1, (p_ar) + (i_pos),                  \
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


#define TAB_APPEND( count, tab, p )             \
    if( (count) > 0 )                           \
    {                                           \
        (tab) = realloc( tab, sizeof( void ** ) * ( (count) + 1 ) ); \
    }                                           \
    else                                        \
    {                                           \
        (tab) = malloc( sizeof( void ** ) );    \
    }                                           \
    (tab)[count] = (p);        \
    (count)++

#define TAB_FIND( count, tab, p, index )        \
    {                                           \
        int _i_;                                \
        (index) = -1;                           \
        for( _i_ = 0; _i_ < (count); _i_++ )    \
        {                                       \
            if( (tab)[_i_] == (p) )  \
            {                                   \
                (index) = _i_;                  \
                break;                          \
            }                                   \
        }                                       \
    }

#define TAB_REMOVE( count, tab, p )             \
    {                                           \
        int _i_index_;                          \
        TAB_FIND( count, tab, p, _i_index_ );   \
        if( _i_index_ >= 0 )                    \
        {                                       \
            if( (count) > 1 )                     \
            {                                   \
                memmove( ((void**)(tab) + _i_index_),    \
                         ((void**)(tab) + _i_index_+1),  \
                         ( (count) - _i_index_ - 1 ) * sizeof( void* ) );\
            }                                   \
            (count)--;                          \
            if( (count) == 0 )                  \
            {                                   \
                free( tab );                    \
                (tab) = NULL;                   \
            }                                   \
        }                                       \
    }

/* MSB (big endian)/LSB (little endian) conversions - network order is always
 * MSB, and should be used for both network communications and files. Note that
 * byte orders other than little and big endians are not supported, but only
 * the VAX seems to have such exotic properties. */
static inline uint16_t U16_AT( void const * _p )
{
    uint8_t * p = (uint8_t *)_p;
    return ( ((uint16_t)p[0] << 8) | p[1] );
}
static inline uint32_t U32_AT( void const * _p )
{
    uint8_t * p = (uint8_t *)_p;
    return ( ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
              | ((uint32_t)p[2] << 8) | p[3] );
}
static inline uint64_t U64_AT( void const * _p )
{
    uint8_t * p = (uint8_t *)_p;
    return ( ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48)
              | ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32)
              | ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16)
              | ((uint64_t)p[6] << 8) | p[7] );
}

static inline uint16_t GetWLE( void const * _p )
{
    uint8_t * p = (uint8_t *)_p;
    return ( ((uint16_t)p[1] << 8) | p[0] );
}
static inline uint32_t GetDWLE( void const * _p )
{
    uint8_t * p = (uint8_t *)_p;
    return ( ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16)
              | ((uint32_t)p[1] << 8) | p[0] );
}
static inline uint64_t GetQWLE( void const * _p )
{
    uint8_t * p = (uint8_t *)_p;
    return ( ((uint64_t)p[7] << 56) | ((uint64_t)p[6] << 48)
              | ((uint64_t)p[5] << 40) | ((uint64_t)p[4] << 32)
              | ((uint64_t)p[3] << 24) | ((uint64_t)p[2] << 16)
              | ((uint64_t)p[1] << 8) | p[0] );
}

#define GetWBE( p )     U16_AT( p )
#define GetDWBE( p )    U32_AT( p )
#define GetQWBE( p )    U64_AT( p )

/* Helper writer functions */
#define SetWLE( p, v ) _SetWLE( (uint8_t*)p, v)
static inline void _SetWLE( uint8_t *p, uint16_t i_dw )
{
    p[1] = ( i_dw >>  8 )&0xff;
    p[0] = ( i_dw       )&0xff;
}

#define SetDWLE( p, v ) _SetDWLE( (uint8_t*)p, v)
static inline void _SetDWLE( uint8_t *p, uint32_t i_dw )
{
    p[3] = ( i_dw >> 24 )&0xff;
    p[2] = ( i_dw >> 16 )&0xff;
    p[1] = ( i_dw >>  8 )&0xff;
    p[0] = ( i_dw       )&0xff;
}
#define SetQWLE( p, v ) _SetQWLE( (uint8_t*)p, v)
static inline void _SetQWLE( uint8_t *p, uint64_t i_qw )
{
    SetDWLE( p,   i_qw&0xffffffff );
    SetDWLE( p+4, ( i_qw >> 32)&0xffffffff );
}
#define SetWBE( p, v ) _SetWBE( (uint8_t*)p, v)
static inline void _SetWBE( uint8_t *p, uint16_t i_dw )
{
    p[0] = ( i_dw >>  8 )&0xff;
    p[1] = ( i_dw       )&0xff;
}

#define SetDWBE( p, v ) _SetDWBE( (uint8_t*)p, v)
static inline void _SetDWBE( uint8_t *p, uint32_t i_dw )
{
    p[0] = ( i_dw >> 24 )&0xff;
    p[1] = ( i_dw >> 16 )&0xff;
    p[2] = ( i_dw >>  8 )&0xff;
    p[3] = ( i_dw       )&0xff;
}
#define SetQWBE( p, v ) _SetQWBE( (uint8_t*)p, v)
static inline void _SetQWBE( uint8_t *p, uint64_t i_qw )
{
    SetDWBE( p+4,   i_qw&0xffffffff );
    SetDWBE( p, ( i_qw >> 32)&0xffffffff );
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

#if !defined(HAVE_VASPRINTF) || defined(__APPLE__) || defined(SYS_BEOS)
#   define vasprintf vlc_vasprintf
    VLC_EXPORT( int, vlc_vasprintf, (char **, const char *, va_list ) );
#elif !defined(__PLUGIN__)
#   define vlc_vasprintf NULL
#endif

#if !defined(HAVE_ASPRINTF) || defined(__APPLE__) || defined(SYS_BEOS)
#   define asprintf vlc_asprintf
    VLC_EXPORT( int, vlc_asprintf, (char **, const char *, ... ) );
#elif !defined(__PLUGIN__)
#   define vlc_asprintf NULL
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

#ifndef HAVE_STRTOF
#   ifdef HAVE_STRTOD
#       define strtof strtod
#   endif
#endif

#ifndef HAVE_ATOLL
#   define atoll vlc_atoll
    VLC_EXPORT( int64_t, vlc_atoll, ( const char *nptr ) );
#elif !defined(__PLUGIN__)
#   define vlc_atoll NULL
#endif

#ifndef HAVE_STRTOLL
#   define strtoll vlc_strtoll
    VLC_EXPORT( int64_t, vlc_strtoll, ( const char *nptr, char **endptr, int base ) );
#elif !defined(__PLUGIN__)
#   define vlc_strtoll NULL
#endif

#ifndef HAVE_SCANDIR
#   define scandir vlc_scandir
#   define alphasort vlc_alphasort
    struct dirent;
    VLC_EXPORT( int, vlc_scandir, ( const char *name, struct dirent ***namelist, int (*filter) ( const struct dirent * ), int (*compar) ( const struct dirent **, const struct dirent ** ) ) );
    VLC_EXPORT( int, vlc_alphasort, ( const struct dirent **a, const struct dirent **b ) );
#elif !defined(__PLUGIN__)
#   define vlc_scandir NULL
#   define vlc_alphasort NULL
#endif

#ifndef HAVE_GETENV
#   define getenv vlc_getenv
    VLC_EXPORT( char *, vlc_getenv, ( const char *name ) );
#elif !defined(__PLUGIN__)
#   define vlc_getenv NULL
#endif

#ifndef HAVE_STRCASECMP
#   ifndef HAVE_STRICMP
#       define strcasecmp vlc_strcasecmp
        VLC_EXPORT( int, vlc_strcasecmp, ( const char *s1, const char *s2 ) );
#   else
#       define strcasecmp stricmp
#       if !defined(__PLUGIN__)
#           define vlc_strcasecmp NULL
#       endif
#   endif
#elif !defined(__PLUGIN__)
#   define vlc_strcasecmp NULL
#endif

#ifndef HAVE_STRNCASECMP
#   ifndef HAVE_STRNICMP
#       define strncasecmp vlc_strncasecmp
        VLC_EXPORT( int, vlc_strncasecmp, ( const char *s1, const char *s2, size_t n ) );
#   else
#       define strncasecmp strnicmp
#       if !defined(__PLUGIN__)
#           define vlc_strncasecmp NULL
#       endif
#   endif
#elif !defined(__PLUGIN__)
#   define vlc_strncasecmp NULL
#endif

#ifndef HAVE_STRCASESTR
#   ifndef HAVE_STRISTR
#       define strcasestr vlc_strcasestr
        VLC_EXPORT( char *, vlc_strcasestr, ( const char *s1, const char *s2 ) );
#   else
#       define strcasestr stristr
#       if !defined(__PLUGIN__)
#           define vlc_strcasestr NULL
#       endif
#   endif
#elif !defined(__PLUGIN__)
#   define vlc_strcasestr NULL
#endif

#ifndef HAVE_DIRENT_H
    typedef void DIR;
#   ifndef FILENAME_MAX
#       define FILENAME_MAX (260)
#   endif
    struct dirent
    {
        long            d_ino;          /* Always zero. */
        unsigned short  d_reclen;       /* Always zero. */
        unsigned short  d_namlen;       /* Length of name in d_name. */
        char            d_name[FILENAME_MAX]; /* File name. */
    };
#   define opendir vlc_opendir
#   define readdir vlc_readdir
#   define closedir vlc_closedir
    VLC_EXPORT( void *, vlc_opendir, ( const char * ) );
    VLC_EXPORT( void *, vlc_readdir, ( void * ) );
    VLC_EXPORT( int, vlc_closedir, ( void * ) );
#else
    struct dirent;  /* forward declaration for vlc_symbols.h */
#   if !defined(__PLUGIN__)
#       define vlc_opendir  NULL
#       define vlc_readdir  NULL
#       define vlc_closedir NULL
#   endif
#endif

    VLC_EXPORT( void *, vlc_opendir_wrapper, ( const char * ) );
    VLC_EXPORT( struct dirent *, vlc_readdir_wrapper, ( void * ) );
    VLC_EXPORT( int, vlc_closedir_wrapper, ( void * ) );

/* Format type specifiers for 64 bits numbers */
#if defined(__CYGWIN32__) || (!defined(WIN32) && !defined(UNDER_CE))
#   if defined(__WORDSIZE) && __WORDSIZE == 64
#       define I64Fd "%ld"
#       define I64Fi "%li"
#       define I64Fo "%lo"
#       define I64Fu "%lu"
#       define I64Fx "%lx"
#       define I64FX "%lX"
#   else
#       define I64Fd "%lld"
#       define I64Fi "%lli"
#       define I64Fo "%llo"
#       define I64Fu "%llu"
#       define I64Fx "%llx"
#       define I64FX "%llX"
#   endif
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
#   if defined(__WORDSIZE) && __WORDSIZE == 64
#       define I64C(x)         x##L
#       define UI64C(x)        x##UL
#   else
#       define I64C(x)         x##LL
#       define UI64C(x)        x##ULL
#   endif
#else
#   define I64C(x)         x##i64
#   define UI64C(x)        x##ui64
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

#   if defined( _MSC_VER ) && !defined( __WXMSW__ )
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

    /* These two are not defined in mingw32 (bug?) */
#   ifndef snprintf
#       define snprintf _snprintf
#   endif
#   ifndef vsnprintf
#       define vsnprintf _vsnprintf
#   endif

#   include <tchar.h>
#endif

VLC_EXPORT( vlc_bool_t, vlc_ureduce, ( unsigned *, unsigned *, uint64_t, uint64_t, uint64_t ) );
VLC_EXPORT( char **, vlc_parse_cmdline, ( const char *, int * ) );

/* vlc_wraptext (defined in src/extras/libc.c) */
#define wraptext vlc_wraptext
VLC_EXPORT( char *, vlc_wraptext, ( const char *, int, vlc_bool_t ) );

/* iconv wrappers (defined in src/extras/libc.c) */
typedef void *vlc_iconv_t;
VLC_EXPORT( vlc_iconv_t, vlc_iconv_open, ( const char *, const char * ) );
VLC_EXPORT( size_t, vlc_iconv, ( vlc_iconv_t, char **, size_t *, char **, size_t * ) );
VLC_EXPORT( int, vlc_iconv_close, ( vlc_iconv_t ) );

/* execve wrapper (defined in src/extras/libc.c) */
VLC_EXPORT( int, __vlc_execve, ( vlc_object_t *p_object, int i_argc, char **pp_argv, char **pp_env, char *psz_cwd, char *p_in, int i_in, char **pp_data, int *pi_data ) );
#define vlc_execve(a,b,c,d,e,f,g,h,i) __vlc_execve(VLC_OBJECT(a),b,c,d,e,f,g,h,i)

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
#define CPU_CAPABILITY_SSE2    (1<<7)
#define CPU_CAPABILITY_ALTIVEC (1<<16)
#define CPU_CAPABILITY_FPU     (1<<31)

/*****************************************************************************
 * I18n stuff
 *****************************************************************************/
VLC_EXPORT( char *, vlc_dgettext, ( const char *package, const char *msgid ) );

#if defined( ENABLE_NLS ) && \
     (defined(MODULE_NAME_IS_gnome)||defined(MODULE_NAME_IS_gnome_main)||\
      defined(MODULE_NAME_IS_gnome2)||defined(MODULE_NAME_IS_gnome2_main)||\
      defined(MODULE_NAME_IS_pda))
    /* Declare nothing: gnome.h will do it for us */
#elif defined( ENABLE_NLS )
#   if defined( HAVE_INCLUDED_GETTEXT )
#       include "libintl.h"
#   else
#       include <libintl.h>
#   endif
#   undef _
#   define _(String) vlc_dgettext (PACKAGE_NAME, String)
#   define N_(String) ((char*)(String))
#else
#   define _(String) ((char*)(String))
#   define N_(String) ((char*)(String))
#endif

/*****************************************************************************
 * libvlc features
 *****************************************************************************/
VLC_EXPORT( const char *, VLC_Version, ( void ) );
VLC_EXPORT( const char *, VLC_CompileBy, ( void ) );
VLC_EXPORT( const char *, VLC_CompileHost, ( void ) );
VLC_EXPORT( const char *, VLC_CompileDomain, ( void ) );
VLC_EXPORT( const char *, VLC_Compiler, ( void ) );
VLC_EXPORT( const char *, VLC_Changeset, ( void ) );
VLC_EXPORT( const char *, VLC_Error, ( int ) );

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

