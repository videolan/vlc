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

#include <vlc_access.h>
#include <vlc_demux.h>
#include <vlc_input.h>
#include <libvlc.h>
#include "input_interface.h"

/*****************************************************************************
 *  Private input fields
 *****************************************************************************/

#define INPUT_CONTROL_FIFO_SIZE    100

/* input_source_t: gathers all information per input source */
typedef struct
{
    /* Access/Stream/Demux plugins */
    access_t *p_access VLC_DEPRECATED;
    stream_t *p_stream;
    demux_t  *p_demux;

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

    /* */
    int64_t i_pts_delay;

    bool       b_eof;   /* eof of demuxer */

} input_source_t;

typedef struct
{
    int         i_type;
    vlc_value_t val;
} input_control_t;

/** Private input fields */
struct input_thread_private_t
{
    /* Global properties */
    double      f_fps;
    int         i_state;
    bool        b_can_pause;
    bool        b_can_rate_control;
    bool        b_can_pace_control;

    /* Current state */
    bool        b_recording;
    int         i_rate;

    /* Playtime configuration and state */
    int64_t     i_start;    /* :start-time,0 by default */
    int64_t     i_stop;     /* :stop-time, 0 if none */
    int64_t     i_run;      /* :run-time, 0 if none */
    int64_t     i_time;     /* Current time */
    bool        b_fast_seek;/* :input-fast-seek */

    /* Output */
    bool            b_out_pace_control; /* XXX Move it ot es_sout ? */
    sout_instance_t *p_sout;            /* Idem ? */
    es_out_t        *p_es_out;
    es_out_t        *p_es_out_display;

    /* Title infos FIXME multi-input (not easy) ? */
    int          i_title;
    input_title_t **title;

    int i_title_offset;
    int i_seekpoint_offset;

    /* User bookmarks FIXME won't be easy with multiples input */
    seekpoint_t bookmark;
    int         i_bookmark;
    seekpoint_t **pp_bookmark;

    /* Input attachment */
    int i_attachment;
    input_attachment_t **attachment;

    /* Main input properties */

    /* Input item */
    input_item_t   *p_item;

    /* Main source */
    input_source_t input;
    /* Slave sources (subs, and others) */
    int            i_slave;
    input_source_t **slave;

    /* Resources */
    input_resource_t *p_resource;
    input_resource_t *p_resource_private;

    /* Stats counters */
    struct {
        counter_t *p_read_packets;
        counter_t *p_read_bytes;
        counter_t *p_input_bitrate;
        counter_t *p_demux_read;
        counter_t *p_demux_bitrate;
        counter_t *p_demux_corrupted;
        counter_t *p_demux_discontinuity;
        counter_t *p_decoded_audio;
        counter_t *p_decoded_video;
        counter_t *p_decoded_sub;
        counter_t *p_sout_sent_packets;
        counter_t *p_sout_sent_bytes;
        counter_t *p_sout_send_bitrate;
        counter_t *p_played_abuffers;
        counter_t *p_lost_abuffers;
        counter_t *p_displayed_pictures;
        counter_t *p_lost_pictures;
        vlc_mutex_t counters_lock;
    } counters;

    /* Buffer of pending actions */
    vlc_mutex_t lock_control;
    vlc_cond_t  wait_control;
    int i_control;
    input_control_t control[INPUT_CONTROL_FIFO_SIZE];

    bool b_abort;
    bool is_running;
    vlc_thread_t thread;
};

/***************************************************************************
 * Internal control helpers
 ***************************************************************************/
enum input_control_e
{
    INPUT_CONTROL_SET_DIE,

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

    INPUT_CONTROL_SET_ES,
    INPUT_CONTROL_RESTART_ES,

    INPUT_CONTROL_SET_AUDIO_DELAY,
    INPUT_CONTROL_SET_SPU_DELAY,

    INPUT_CONTROL_ADD_SLAVE,

    INPUT_CONTROL_ADD_SUBTITLE,

    INPUT_CONTROL_SET_RECORD_STATE,

    INPUT_CONTROL_SET_FRAME_NEXT,
};

/* Internal helpers */

/* XXX for string value you have to allocate it before calling
 * input_ControlPush
 */
void input_ControlPush( input_thread_t *, int i_type, vlc_value_t * );

/* Bound pts_delay */
#define INPUT_PTS_DELAY_MAX INT64_C(60000000)

/**********************************************************************
 * Item metadata
 **********************************************************************/
/* input_ExtractAttachmentAndCacheArt:
 *  Becarefull; p_item lock HAS to be taken */
void input_ExtractAttachmentAndCacheArt( input_thread_t *p_input );

/***************************************************************************
 * Internal prototypes
 ***************************************************************************/

/* var.c */
void input_ControlVarInit ( input_thread_t * );
void input_ControlVarStop( input_thread_t * );
void input_ControlVarNavigation( input_thread_t * );
void input_ControlVarTitle( input_thread_t *, int i_title );

void input_ConfigVarInit ( input_thread_t * );

/* Subtitles */
char **subtitles_Detect( input_thread_t *, char* path, const char *fname );
int subtitles_Filter( const char *);

/* input.c */
void input_SplitMRL( const char **, const char **, const char **,
                     const char **, char * );

/* meta.c */
void vlc_audio_replay_gain_MergeFromMeta( audio_replay_gain_t *p_dst,
                                          const vlc_meta_t *p_meta );

#endif
