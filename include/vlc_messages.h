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

#ifndef VLC_MESSAGES_H_
#define VLC_MESSAGES_H_

/**
 * \file
 * This file defines structures and functions to handle messages and statistics gathering
 */

#include <stdarg.h>

/**
 * \defgroup messages Messages
 * This library provides basic functions for threads to interact with user
 * interface, such as message output.
 *
 * @{
 */

/**
 * Store a single message sent to user.
 */
typedef struct
{
    int     i_type;                             /**< message type, see below */
    uintptr_t   i_object_id;
    const char *psz_object_type;
    char *  psz_module;
    char *  psz_msg;                            /**< the message itself */
    char *  psz_header;                         /**< Additional header */

    mtime_t date;                               /**< Message date */
    gc_object_t vlc_gc_data;
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

static inline msg_item_t *msg_Hold (msg_item_t *msg)
{
    vlc_hold (&msg->vlc_gc_data);
    return msg;
}

static inline void msg_Release (msg_item_t *msg)
{
    vlc_release (&msg->vlc_gc_data);
}

/**
 * Used by interface plugins which subscribe to the message bank.
 */
typedef struct msg_subscription_t msg_subscription_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
VLC_EXPORT( void, __msg_Generic, ( vlc_object_t *, int, const char *, const char *, ... ) LIBVLC_FORMAT( 4, 5 ) );
VLC_EXPORT( void, __msg_GenericVa, ( vlc_object_t *, int, const char *, const char *, va_list args ) );
#define msg_GenericVa(a, b, c, d, e) __msg_GenericVa(VLC_OBJECT(a), b, c, d, e)

#define msg_Info( p_this, ... ) \
      __msg_Generic( VLC_OBJECT(p_this), VLC_MSG_INFO, \
                     MODULE_STRING, __VA_ARGS__ )
#define msg_Err( p_this, ... ) \
      __msg_Generic( VLC_OBJECT(p_this), VLC_MSG_ERR, \
                     MODULE_STRING, __VA_ARGS__ )
#define msg_Warn( p_this, ... ) \
      __msg_Generic( VLC_OBJECT(p_this), VLC_MSG_WARN, \
                     MODULE_STRING, __VA_ARGS__ )
#define msg_Dbg( p_this, ... ) \
      __msg_Generic( VLC_OBJECT(p_this), VLC_MSG_DBG, \
                     MODULE_STRING, __VA_ARGS__ )

typedef struct msg_cb_data_t msg_cb_data_t;

/**
 * Message logging callback signature.
 * Accepts one private data pointer, the message, and an overrun counter.
 */
typedef void (*msg_callback_t) (msg_cb_data_t *, msg_item_t *, unsigned);

VLC_EXPORT( msg_subscription_t*, msg_Subscribe, ( libvlc_int_t *, msg_callback_t, msg_cb_data_t * ) );
VLC_EXPORT( void, msg_Unsubscribe, ( msg_subscription_t * ) );

/* Enable or disable a certain object debug messages */
#define msg_EnableObjectPrinting(a,b) __msg_EnableObjectPrinting(VLC_OBJECT(a),b)
#define msg_DisableObjectPrinting(a,b) __msg_DisableObjectPrinting(VLC_OBJECT(a),b)
VLC_EXPORT( void, __msg_EnableObjectPrinting, ( vlc_object_t *, char * psz_object ) );
VLC_EXPORT( void, __msg_DisableObjectPrinting, ( vlc_object_t *, char * psz_object ) );

/**
 * @}
 */

/**
 * \defgroup statistics Statistics
 *
 * @{
 */

/****************************
 * Generic stats stuff
 ****************************/
enum
{
    STATS_LAST,
    STATS_COUNTER,
    STATS_MAX,
    STATS_MIN,
    STATS_DERIVATIVE,
    STATS_TIMER
};

struct counter_sample_t
{
    vlc_value_t value;
    mtime_t     date;
};

struct counter_t
{
    unsigned int        i_id;
    char              * psz_name;
    int                 i_type;
    void              * p_obj;
    int                 i_compute_type;
    int                 i_samples;
    counter_sample_t ** pp_samples;

    mtime_t             update_interval;
    mtime_t             last_update;
};

enum
{
    STATS_INPUT_BITRATE,
    STATS_READ_BYTES,
    STATS_READ_PACKETS,
    STATS_DEMUX_READ,
    STATS_DEMUX_BITRATE,
    STATS_PLAYED_ABUFFERS,
    STATS_LOST_ABUFFERS,
    STATS_DECODED_AUDIO,
    STATS_DECODED_VIDEO,
    STATS_DECODED_SUB,
    STATS_CLIENT_CONNECTIONS,
    STATS_ACTIVE_CONNECTIONS,
    STATS_SOUT_SENT_PACKETS,
    STATS_SOUT_SENT_BYTES,
    STATS_SOUT_SEND_BITRATE,
    STATS_DISPLAYED_PICTURES,
    STATS_LOST_PICTURES,

    STATS_TIMER_PLAYLIST_BUILD,
    STATS_TIMER_ML_LOAD,
    STATS_TIMER_ML_DUMP,
    STATS_TIMER_INTERACTION,
    STATS_TIMER_PREPARSE,
    STATS_TIMER_INPUT_LAUNCHING,
    STATS_TIMER_MODULE_NEED,
    STATS_TIMER_VIDEO_FRAME_ENCODING,
    STATS_TIMER_AUDIO_FRAME_ENCODING,

    STATS_TIMER_SKINS_PLAYTREE_IMAGE,
};

#define stats_Update(a,b,c) __stats_Update( VLC_OBJECT(a), b, c )
VLC_EXPORT( int, __stats_Update, (vlc_object_t*, counter_t *, vlc_value_t, vlc_value_t *) );
#define stats_CounterCreate(a,b,c) __stats_CounterCreate( VLC_OBJECT(a), b, c )
VLC_EXPORT( counter_t *, __stats_CounterCreate, (vlc_object_t*, int, int) );
#define stats_Get(a,b,c) __stats_Get( VLC_OBJECT(a), b, c)
VLC_EXPORT( int, __stats_Get, (vlc_object_t*, counter_t *, vlc_value_t*) );

VLC_EXPORT (void, stats_CounterClean, (counter_t * ) );

#define stats_GetInteger(a,b,c) __stats_GetInteger( VLC_OBJECT(a), b, c )
static inline int __stats_GetInteger( vlc_object_t *p_obj, counter_t *p_counter,
                                      int *value )
{
    int i_ret;
    vlc_value_t val; val.i_int = 0;
    if( !p_counter ) return VLC_EGENERIC;
    i_ret = __stats_Get( p_obj, p_counter, &val );
    *value = val.i_int;
    return i_ret;
}

#define stats_GetFloat(a,b,c) __stats_GetFloat( VLC_OBJECT(a), b, c )
static inline int __stats_GetFloat( vlc_object_t *p_obj, counter_t *p_counter,
                                    float *value )
{
    int i_ret;
    vlc_value_t val; val.f_float = 0.0;
    if( !p_counter ) return VLC_EGENERIC;
    i_ret = __stats_Get( p_obj, p_counter, &val );
    *value = val.f_float;
    return i_ret;
}
#define stats_UpdateInteger(a,b,c,d) __stats_UpdateInteger( VLC_OBJECT(a),b,c,d )
static inline int __stats_UpdateInteger( vlc_object_t *p_obj,counter_t *p_co,
                                         int i, int *pi_new )
{
    int i_ret;
    vlc_value_t val;
    vlc_value_t new_val; new_val.i_int = 0;
    if( !p_co ) return VLC_EGENERIC;
    val.i_int = i;
    i_ret = __stats_Update( p_obj, p_co, val, &new_val );
    if( pi_new )
        *pi_new = new_val.i_int;
    return i_ret;
}
#define stats_UpdateFloat(a,b,c,d) __stats_UpdateFloat( VLC_OBJECT(a),b,c,d )
static inline int __stats_UpdateFloat( vlc_object_t *p_obj, counter_t *p_co,
                                       float f, float *pf_new )
{
    vlc_value_t val;
    int i_ret;
    vlc_value_t new_val;new_val.f_float = 0.0;
    if( !p_co ) return VLC_EGENERIC;
    val.f_float = f;
    i_ret =  __stats_Update( p_obj, p_co, val, &new_val );
    if( pf_new )
        *pf_new = new_val.f_float;
    return i_ret;
}

/******************
 * Input stats
 ******************/
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

/********************
 * Global stats
 *******************/
struct global_stats_t
{
    vlc_mutex_t lock;

    float f_input_bitrate;
    float f_demux_bitrate;
    float f_output_bitrate;

    int i_http_clients;
};

#define stats_ComputeGlobalStats(a,b) __stats_ComputeGlobalStats( VLC_OBJECT(a),b)
VLC_EXPORT( void, __stats_ComputeGlobalStats, (vlc_object_t*,global_stats_t*));


/*********
 * Timing
 ********/
#define stats_TimerStart(a,b,c) __stats_TimerStart( VLC_OBJECT(a), b,c )
#define stats_TimerStop(a,b) __stats_TimerStop( VLC_OBJECT(a), b )
#define stats_TimerDump(a,b) __stats_TimerDump( VLC_OBJECT(a), b )
#define stats_TimersDumpAll(a) __stats_TimersDumpAll( VLC_OBJECT(a) )
VLC_EXPORT( void,__stats_TimerStart, (vlc_object_t*, const char *, unsigned int ) );
VLC_EXPORT( void,__stats_TimerStop, (vlc_object_t*, unsigned int) );
VLC_EXPORT( void,__stats_TimerDump, (vlc_object_t*, unsigned int) );
VLC_EXPORT( void,__stats_TimersDumpAll, (vlc_object_t*) );
#define stats_TimersCleanAll(a) __stats_TimersCleanAll( VLC_OBJECT(a) )
VLC_EXPORT( void, __stats_TimersCleanAll, (vlc_object_t * ) );

#define stats_TimerClean(a,b) __stats_TimerClean( VLC_OBJECT(a), b )
VLC_EXPORT( void, __stats_TimerClean, (vlc_object_t *, unsigned int ) );

#endif
