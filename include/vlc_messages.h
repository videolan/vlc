/*****************************************************************************
 * messages.h: messages interface
 * This library provides basic functions for threads to interact with user
 * interface, such as message output.
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001, 2002 the VideoLAN team
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
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

#include <stdarg.h>

int vlc_mutex_lock(  vlc_mutex_t * ) ;
int vlc_mutex_unlock(  vlc_mutex_t * ) ;

/**
 * \defgroup messages Messages
 * This library provides basic functions for threads to interact with user
 * interface, such as message output.
 *
 * @{
 */


/**
 * Store a single message.
 */
typedef struct
{
    int     i_type;                             /**< message type, see below */
    int     i_object_id;
    int     i_object_type;
    char *  psz_module;
    char *  psz_msg;                            /**< the message itself */
    char *  psz_header;                         /**< Additional header */

    mtime_t date;                               /**< Message date */
} msg_item_t;

/* Message types */
/** standard messages */
#define VLC_MSG_INFO  0
/** error messages */
#define VLC_MSG_ERR   1
/** warning messages */
#define VLC_MSG_WARN  2
/** debug messages */
#define VLC_MSG_DBG   3

#define MSG_QUEUE_NORMAL 0
#define MSG_QUEUE_HTTPD_ACCESS 1

/**
 * Store all data requiered by messages interfaces.
 */
struct msg_bank_t
{
    vlc_mutex_t             lock;
    int                     i_queues;
    msg_queue_t           **pp_queues;
};

struct msg_queue_t
{
    int                     i_id;

    /** Message queue lock */
    vlc_mutex_t             lock;
    vlc_bool_t              b_overflow;

    /* Message queue */
    msg_item_t              msg[VLC_MSG_QSIZE];           /**< message queue */
    int i_start;
    int i_stop;

    /* Subscribers */
    int i_sub;
    msg_subscription_t **pp_sub;

    /* Logfile for WinCE */
#ifdef UNDER_CE
    FILE *logfile;
#endif
};

/**
 * Used by interface plugins which subscribe to the message bank.
 */
struct msg_subscription_t
{
    int   i_start;
    int*  pi_stop;

    msg_item_t*  p_msg;
    vlc_mutex_t* p_lock;
};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
VLC_EXPORT( void, __msg_Generic, ( vlc_object_t *, int, int, const char *, const char *, ... ) ATTRIBUTE_FORMAT( 5, 6 ) );
VLC_EXPORT( void, __msg_GenericVa, ( vlc_object_t *, int, int, const char *, const char *, va_list args ) );
#define msg_GenericVa(a, b, c, d, e,f) __msg_GenericVa(VLC_OBJECT(a), b, c, d, e,f)
VLC_EXPORT( void, __msg_Info,    ( vlc_object_t *, const char *, ... ) ATTRIBUTE_FORMAT( 2, 3 ) );
VLC_EXPORT( void, __msg_Err,     ( vlc_object_t *, const char *, ... ) ATTRIBUTE_FORMAT( 2, 3 ) );
VLC_EXPORT( void, __msg_Warn,    ( vlc_object_t *, const char *, ... ) ATTRIBUTE_FORMAT( 2, 3 ) );
VLC_EXPORT( void, __msg_Dbg,    ( vlc_object_t *, const char *, ... ) ATTRIBUTE_FORMAT( 2, 3 ) );

#ifdef HAVE_VARIADIC_MACROS

#   define msg_Info( p_this, psz_format, args... ) \
      __msg_Generic( VLC_OBJECT(p_this), MSG_QUEUE_NORMAL,VLC_MSG_INFO, MODULE_STRING, \
                     psz_format, ## args )

#   define msg_Err( p_this, psz_format, args... ) \
      __msg_Generic( VLC_OBJECT(p_this), MSG_QUEUE_NORMAL, VLC_MSG_ERR, MODULE_STRING, \
                     psz_format, ## args )

#   define msg_Warn( p_this, psz_format, args... ) \
      __msg_Generic( VLC_OBJECT(p_this), MSG_QUEUE_NORMAL, VLC_MSG_WARN, MODULE_STRING, \
                     psz_format, ## args )

#   define msg_Dbg( p_this, psz_format, args... ) \
      __msg_Generic( VLC_OBJECT(p_this), MSG_QUEUE_NORMAL, VLC_MSG_DBG, MODULE_STRING, \
                     psz_format, ## args )

#elif defined(_MSC_VER) /* To avoid warnings and even errors with c++ files */

inline void msg_Info( void *p_this, const char *psz_format, ... )
{
  va_list ap;
  va_start( ap, psz_format );
  __msg_GenericVa( ( vlc_object_t *)p_this, MSG_QUEUE_NORMAL,VLC_MSG_INFO, MODULE_STRING,
                   psz_format, ap );
  va_end(ap);
}
inline void msg_Err( void *p_this, const char *psz_format, ... )
{
  va_list ap;
  va_start( ap, psz_format );
  __msg_GenericVa( ( vlc_object_t *)p_this,MSG_QUEUE_NORMAL, VLC_MSG_ERR, MODULE_STRING,
                   psz_format, ap );
  va_end(ap);
}
inline void msg_Warn( void *p_this, const char *psz_format, ... )
{
  va_list ap;
  va_start( ap, psz_format );
  __msg_GenericVa( ( vlc_object_t *)p_this, MSG_QUEUE_NORMAL, VLC_MSG_WARN, MODULE_STRING,
                   psz_format, ap );
  va_end(ap);
}
inline void msg_Dbg( void *p_this, const char *psz_format, ... )
{
  va_list ap;
  va_start( ap, psz_format );
  __msg_GenericVa( ( vlc_object_t *)p_this, MSG_QUEUE_NORMAL, VLC_MSG_DBG, MODULE_STRING,
                   psz_format, ap );
  va_end(ap);
}

#else /* _MSC_VER */

#   define msg_Info __msg_Info
#   define msg_Err __msg_Err
#   define msg_Warn __msg_Warn
#   define msg_Dbg __msg_Dbg

#endif /* HAVE_VARIADIC_MACROS */

#define msg_Create(a) __msg_Create(VLC_OBJECT(a))
#define msg_Flush(a) __msg_Flush(VLC_OBJECT(a))
#define msg_Destroy(a) __msg_Destroy(VLC_OBJECT(a))
void __msg_Create  ( vlc_object_t * );
void __msg_Flush   ( vlc_object_t * );
void __msg_Destroy ( vlc_object_t * );

#define msg_Subscribe(a,b) __msg_Subscribe(VLC_OBJECT(a),b)
#define msg_Unsubscribe(a,b) __msg_Unsubscribe(VLC_OBJECT(a),b)
VLC_EXPORT( msg_subscription_t*, __msg_Subscribe, ( vlc_object_t *, int ) );
VLC_EXPORT( void, __msg_Unsubscribe, ( vlc_object_t *, msg_subscription_t * ) );


/**
 * @}
 */

/**
 * \defgroup statistics Statistics
 *
 * @{
 */

enum
{
    STATS_LAST,
    STATS_COUNTER,
    STATS_MAX,
    STATS_MIN,
    STATS_DERIVATIVE
};

struct counter_sample_t
{
    vlc_value_t value;
    mtime_t     date;
};

struct counter_t
{
    char              * psz_name;
    int                 i_source_object;
    int                 i_compute_type;
    int                 i_type;
    int                 i_samples;
    counter_sample_t ** pp_samples;

    mtime_t             update_interval;
    mtime_t             last_update;
};

struct stats_handler_t
{
    VLC_COMMON_MEMBERS

    int         i_counters;
    counter_t **pp_counters;
};

VLC_EXPORT( void, stats_HandlerDestroy, (stats_handler_t*) );

#define stats_Update( a,b,c) __stats_Update( VLC_OBJECT( a ), b, c )
VLC_EXPORT( int, __stats_Update, (vlc_object_t*, char *, vlc_value_t) );
#define stats_Create( a,b,c,d ) __stats_Create( VLC_OBJECT(a), b, c, d )
VLC_EXPORT( int, __stats_Create, (vlc_object_t*, char *, int, int) );
#define stats_Get( a,b,c,d ) __stats_Create( VLC_OBJECT(a), b, c, d )
VLC_EXPORT( int, __stats_Get, (vlc_object_t*, int, char *, vlc_value_t*) );
#define stats_CounterGet( a,b,c) __stats_CounterGet( VLC_OBJECT(a), b, c )
VLC_EXPORT( counter_t*, __stats_CounterGet, (vlc_object_t*, int, char * ) );

#define stats_GetInteger( a,b,c,d ) __stats_GetInteger( VLC_OBJECT(a), b, c, d )
static inline int __stats_GetInteger( vlc_object_t *p_obj, int i_id,
                                      char *psz_name, int *value )
{
    vlc_value_t val;
    int i_ret = __stats_Get( p_obj, i_id, psz_name, &val );
    *value = val.i_int;
    return i_ret;
}

#define stats_GetFloat(a,b,c,d ) __stats_GetFloat( VLC_OBJECT(a), b, c, d )
static inline int __stats_GetFloat( vlc_object_t *p_obj, int i_id,
                                    char *psz_name, float *value )
{
    vlc_value_t val;
    int i_ret = __stats_Get( p_obj, i_id, psz_name, &val );
    *value = val.f_float;
    return i_ret;
}
#define stats_UpdateInteger( a,b,c ) __stats_UpdateInteger( VLC_OBJECT(a),b,c )
static inline int __stats_UpdateInteger( vlc_object_t *p_obj, char *psz_name,
                                         int i )
{
    vlc_value_t val;
    val.i_int = i;
    return __stats_Update( p_obj, psz_name, val );
}
#define stats_UpdateFloat( a,b,c ) __stats_UpdateFloat( VLC_OBJECT(a),b,c )
static inline int __stats_UpdateFloat( vlc_object_t *p_obj, char *psz_name,
                                         float f )
{
    vlc_value_t val;
    val.f_float = f;
    return __stats_Update( p_obj, psz_name, val );
}

struct input_stats_t
{

    vlc_mutex_t         lock;

    /* Input */
    int i_read_packets;
    int i_read_bytes;
    float f_input_bitrate;
    float f_average_input_bitrate;

    /* Demux */
    int i_demux_read_packets;
    int i_demux_read_bytes;
    float f_demux_bitrate;
    float f_average_demux_bitrate;

    /* Decoders */
    int i_decoded_audio;
    int i_decoded_video;

    /* Vout */
    int i_displayed_pictures;
    int i_lost_pictures;

    /* Sout */
    int i_sent_packets;
    int i_sent_bytes;
    float f_send_bitrate;

    /* Aout */
    int i_played_abuffers;
    int i_lost_abuffers;
};

VLC_EXPORT( void, stats_ComputeInputStats, (input_thread_t*, input_stats_t*) );
VLC_EXPORT( void, stats_ReinitInputStats, (input_stats_t *) );
VLC_EXPORT( void, stats_DumpInputStats, (input_stats_t *) );
