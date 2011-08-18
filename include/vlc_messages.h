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

/** Message types */
enum msg_item_type
{
    VLC_MSG_INFO=0, /**< Important information */
    VLC_MSG_ERR,    /**< Error */
    VLC_MSG_WARN,   /**< Warning */
    VLC_MSG_DBG,    /**< Debug */
};

/**
 * Log message
 */
typedef struct
{
    unsigned    i_type;  /**< Message type, see @ref msg_item_type */
    uintptr_t   i_object_id; /**< Emitter (temporaly) unique object ID or 0 */
    const char *psz_object_type; /**< Emitter object type name */
    const char *psz_module; /**< Emitter module (source code) */
    const char *psz_header; /**< Additional header (used by VLM media) */
    char       *psz_msg; /**< Message text */
} msg_item_t;

VLC_MALLOC VLC_USED
static inline msg_item_t *msg_Copy (const msg_item_t *msg)
{
    msg_item_t *copy = (msg_item_t *)xmalloc (sizeof (*copy));
    copy->i_type = msg->i_type;
    copy->i_object_id = msg->i_object_id;
    copy->psz_object_type = msg->psz_object_type;
    copy->psz_module = strdup (msg->psz_module);
    copy->psz_msg = strdup (msg->psz_msg);
    copy->psz_header = msg->psz_header ? strdup (msg->psz_header) : NULL;
    return copy;
}

static inline void msg_Free (msg_item_t *msg)
{
    free ((char *)msg->psz_module);
    free ((char *)msg->psz_header);
    free (msg->psz_msg);
    free (msg);
}

/**
 * Used by interface plugins which subscribe to the message bank.
 */
typedef struct msg_subscription_t msg_subscription_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
VLC_API void msg_Generic( vlc_object_t *, int, const char *, const char *, ... ) VLC_FORMAT( 4, 5 );
VLC_API void msg_GenericVa( vlc_object_t *, int, const char *, const char *, va_list args );
#define msg_GenericVa(a, b, c, d, e) msg_GenericVa(VLC_OBJECT(a), b, c, d, e)

#define msg_Info( p_this, ... ) \
        msg_Generic( VLC_OBJECT(p_this), VLC_MSG_INFO, \
                     MODULE_STRING, __VA_ARGS__ )
#define msg_Err( p_this, ... ) \
        msg_Generic( VLC_OBJECT(p_this), VLC_MSG_ERR, \
                     MODULE_STRING, __VA_ARGS__ )
#define msg_Warn( p_this, ... ) \
        msg_Generic( VLC_OBJECT(p_this), VLC_MSG_WARN, \
                     MODULE_STRING, __VA_ARGS__ )
#define msg_Dbg( p_this, ... ) \
        msg_Generic( VLC_OBJECT(p_this), VLC_MSG_DBG, \
                     MODULE_STRING, __VA_ARGS__ )

typedef struct msg_cb_data_t msg_cb_data_t;

/**
 * Message logging callback signature.
 * Accepts one private data pointer, the message, and an overrun counter.
 */
typedef void (*msg_callback_t) (msg_cb_data_t *, const msg_item_t *);

VLC_API msg_subscription_t* msg_Subscribe( libvlc_int_t *, msg_callback_t, msg_cb_data_t * ) VLC_USED;
VLC_API void msg_Unsubscribe( msg_subscription_t * );

/* Enable or disable a certain object debug messages */
VLC_API void msg_EnableObjectPrinting( vlc_object_t *, const char * psz_object );
#define msg_EnableObjectPrinting(a,b) msg_EnableObjectPrinting(VLC_OBJECT(a),b)
VLC_API void msg_DisableObjectPrinting( vlc_object_t *, const char * psz_object );
#define msg_DisableObjectPrinting(a,b) msg_DisableObjectPrinting(VLC_OBJECT(a),b)


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
    STATS_DEMUX_CORRUPTED,
    STATS_DEMUX_DISCONTINUITY,
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

/*********
 * Timing
 ********/
VLC_API void stats_TimerStart(vlc_object_t*, const char *, unsigned int );
VLC_API void stats_TimerStop(vlc_object_t*, unsigned int);
VLC_API void stats_TimerDump(vlc_object_t*, unsigned int);
VLC_API void stats_TimersDumpAll(vlc_object_t*);
#define stats_TimerStart(a,b,c) stats_TimerStart( VLC_OBJECT(a), b,c )
#define stats_TimerStop(a,b) stats_TimerStop( VLC_OBJECT(a), b )
#define stats_TimerDump(a,b) stats_TimerDump( VLC_OBJECT(a), b )
#define stats_TimersDumpAll(a) stats_TimersDumpAll( VLC_OBJECT(a) )

VLC_API void stats_TimersCleanAll(vlc_object_t * );
#define stats_TimersCleanAll(a) stats_TimersCleanAll( VLC_OBJECT(a) )

VLC_API void stats_TimerClean(vlc_object_t *, unsigned int );
#define stats_TimerClean(a,b) stats_TimerClean( VLC_OBJECT(a), b )

/**
 * @}
 */
#endif
