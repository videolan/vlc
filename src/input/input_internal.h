/*****************************************************************************
 * input_internal.h: Internal input structures
 *****************************************************************************
 * Copyright (C) 1998-2006 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#ifndef LIBVLC_INPUT_INTERNAL_H
#define LIBVLC_INPUT_INTERNAL_H 1

#include <stddef.h>
#include <stdatomic.h>

#include <vlc_access.h>
#include <vlc_demux.h>
#include <vlc_input.h>
#include <vlc_viewpoint.h>
#include <libvlc.h>
#include "input_interface.h"
#include "misc/interrupt.h"

struct input_stats;

/*****************************************************************************
 *  Private input fields
 *****************************************************************************/

#define INPUT_CONTROL_FIFO_SIZE    100

/* input_source_t: gathers all information per input source */
typedef struct
{
    struct vlc_common_members obj;

    demux_t  *p_demux; /**< Demux object (most downstream) */

    /* Title infos for that input */
    bool         b_title_demux; /* Titles/Seekpoints provided by demux */
    int          i_title;
    input_title_t **title;

    int i_title_offset;
    int i_seekpoint_offset;

    int i_title_start;
    int i_title_end;
    int i_seekpoint_start;
    int i_seekpoint_end;

    /* Properties */
    bool b_can_pause;
    bool b_can_pace_control;
    bool b_can_rate_control;
    bool b_can_stream_record;
    bool b_rescale_ts;
    double f_fps;

    /* */
    vlc_tick_t i_pts_delay;

    bool       b_eof;   /* eof of demuxer */

} input_source_t;

typedef union
{
    vlc_value_t val;
    vlc_viewpoint_t viewpoint;
    struct {
        bool b_fast_seek;
        vlc_tick_t i_val;
    } time;
    struct {
        bool b_fast_seek;
        float f_val;
    } pos;
} input_control_param_t;

typedef struct
{
    int         i_type;
    input_control_param_t param;
} input_control_t;

/** Private input fields */
typedef struct input_thread_private_t
{
    struct input_thread_t input;

    input_thread_events_cb events_cb;
    void *events_data;

    /* Global properties */
    bool        b_preparsing;
    bool        b_can_pause;
    bool        b_can_rate_control;
    bool        b_can_pace_control;

    /* Current state */
    int         i_state;
    bool        is_running;
    bool        is_stopped;
    bool        b_recording;
    int         i_rate;

    /* Playtime configuration and state */
    vlc_tick_t  i_start;    /* :start-time,0 by default */
    vlc_tick_t  i_stop;     /* :stop-time, 0 if none */
    vlc_tick_t  i_time;     /* Current time */

    /* Output */
    bool            b_out_pace_control; /* XXX Move it ot es_sout ? */
    sout_instance_t *p_sout;            /* Idem ? */
    es_out_t        *p_es_out;
    es_out_t        *p_es_out_display;
    vlc_viewpoint_t viewpoint;
    bool            viewpoint_changed;
    vlc_renderer_item_t *p_renderer;

    /* Title infos FIXME multi-input (not easy) ? */
    int          i_title;
    const input_title_t **title;

    int i_title_offset;
    int i_seekpoint_offset;

    /* User bookmarks FIXME won't be easy with multiples input */
    seekpoint_t bookmark;
    int         i_bookmark;
    seekpoint_t **pp_bookmark;

    /* Input attachment */
    int i_attachment;
    input_attachment_t **attachment;
    const demux_t **attachment_demux;

    /* Main input properties */

    /* Input item */
    input_item_t   *p_item;

    /* Main source */
    input_source_t *master;
    /* Slave sources (subs, and others) */
    int            i_slave;
    input_source_t **slave;

    /* Last ES added */
    enum es_format_category_e i_last_es_cat;
    int                       i_last_es_id;

    /* Resources */
    input_resource_t *p_resource;
    input_resource_t *p_resource_private;

    /* Stats counters */
    struct input_stats *stats;

    /* Buffer of pending actions */
    vlc_mutex_t lock_control;
    vlc_cond_t  wait_control;
    int i_control;
    input_control_t control[INPUT_CONTROL_FIFO_SIZE];

    vlc_thread_t thread;
    vlc_interrupt_t interrupt;
} input_thread_private_t;

static inline input_thread_private_t *input_priv(input_thread_t *input)
{
    return container_of(input, input_thread_private_t, input);
}

/***************************************************************************
 * Internal control helpers
 ***************************************************************************/
enum input_control_e
{
    INPUT_CONTROL_SET_STATE,

    INPUT_CONTROL_SET_RATE,

    INPUT_CONTROL_SET_POSITION,

    INPUT_CONTROL_SET_TIME,

    INPUT_CONTROL_SET_PROGRAM,

    INPUT_CONTROL_SET_TITLE,
    INPUT_CONTROL_SET_TITLE_NEXT,
    INPUT_CONTROL_SET_TITLE_PREV,

    INPUT_CONTROL_SET_SEEKPOINT,
    INPUT_CONTROL_SET_SEEKPOINT_NEXT,
    INPUT_CONTROL_SET_SEEKPOINT_PREV,

    INPUT_CONTROL_SET_BOOKMARK,

    INPUT_CONTROL_NAV_ACTIVATE, // NOTE: INPUT_CONTROL_NAV_* values must be
    INPUT_CONTROL_NAV_UP,       // contiguous and in the same order as
    INPUT_CONTROL_NAV_DOWN,     // INPUT_NAV_* and DEMUX_NAV_*.
    INPUT_CONTROL_NAV_LEFT,
    INPUT_CONTROL_NAV_RIGHT,
    INPUT_CONTROL_NAV_POPUP,
    INPUT_CONTROL_NAV_MENU,

    INPUT_CONTROL_SET_ES,
    INPUT_CONTROL_RESTART_ES,

    INPUT_CONTROL_SET_VIEWPOINT,    // new absolute viewpoint
    INPUT_CONTROL_SET_INITIAL_VIEWPOINT, // set initial viewpoint (generally from video)
    INPUT_CONTROL_UPDATE_VIEWPOINT, // update viewpoint relative to current

    INPUT_CONTROL_SET_AUDIO_DELAY,
    INPUT_CONTROL_SET_SPU_DELAY,

    INPUT_CONTROL_ADD_SLAVE,

    INPUT_CONTROL_SET_RECORD_STATE,

    INPUT_CONTROL_SET_FRAME_NEXT,

    INPUT_CONTROL_SET_RENDERER,
};

/* Internal helpers */

void input_ControlPush( input_thread_t *, int, input_control_param_t * );

/* XXX for string value you have to allocate it before calling
 * input_ControlPushHelper
 */
static inline void input_ControlPushHelper( input_thread_t *p_input, int i_type, vlc_value_t *val )
{
    if( val != NULL )
    {
        input_control_param_t param = { .val = *val };
        input_ControlPush( p_input, i_type, &param );
    }
    else
    {
        input_ControlPush( p_input, i_type, NULL );
    }
}

bool input_Stopped( input_thread_t * );

/* Bound pts_delay */
#define INPUT_PTS_DELAY_MAX VLC_TICK_FROM_SEC(60)

/**********************************************************************
 * Item metadata
 **********************************************************************/
/* input_ExtractAttachmentAndCacheArt:
 *  Be careful: p_item lock will be taken! */
void input_ExtractAttachmentAndCacheArt( input_thread_t *, const char *name );

/***************************************************************************
 * Internal prototypes
 ***************************************************************************/

/* var.c */

void input_ConfigVarInit ( input_thread_t * );

/* Subtitles */
int subtitles_Detect( input_thread_t *, char *, const char *, input_item_slave_t ***, int * );
int subtitles_Filter( const char *);

/* meta.c */
void vlc_audio_replay_gain_MergeFromMeta( audio_replay_gain_t *p_dst,
                                          const vlc_meta_t *p_meta );

/* stats.c */
typedef struct input_rate_t
{
    vlc_mutex_t lock;
    uintmax_t updates;
    uintmax_t value;
    struct
    {
        uintmax_t  value;
        vlc_tick_t date;
    } samples[2];
} input_rate_t;

struct input_stats {
    input_rate_t input_bitrate;
    input_rate_t demux_bitrate;
    atomic_uintmax_t demux_corrupted;
    atomic_uintmax_t demux_discontinuity;
    atomic_uintmax_t decoded_audio;
    atomic_uintmax_t decoded_video;
    atomic_uintmax_t played_abuffers;
    atomic_uintmax_t lost_abuffers;
    atomic_uintmax_t displayed_pictures;
    atomic_uintmax_t lost_pictures;
};

struct input_stats *input_stats_Create(void);
void input_stats_Destroy(struct input_stats *);
void input_rate_Add(input_rate_t *, uintmax_t);
void input_stats_Compute(struct input_stats *, input_stats_t*);

#endif
