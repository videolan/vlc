/*****************************************************************************
 * source.h: Internal input source structures
 *****************************************************************************
 * Copyright (C) 1998-2006 VLC authors and VideoLAN
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

#ifndef LIBVLC_INPUT_SOURCE_H
#define LIBVLC_INPUT_SOURCE_H 1

#include <vlc_common.h>
#include <vlc_tick.h>
#include <vlc_atomic.h>

typedef struct input_title_t input_title_t;

/* input_source_t: gathers all information per input source */
struct input_source_t
{
    vlc_atomic_rc_t rc;

    demux_t  *p_demux; /**< Demux object (most downstream) */
    es_out_t *p_slave_es_out; /**< Slave es out */

    char *str_id;
    int auto_id;
    bool autoselected;

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

    /* sub-fps handling */
    bool b_slave_sub;
    float sub_rate;

    /* */
    vlc_tick_t i_pts_delay;

    /* Read-write protected by es_out.c lock */
    vlc_tick_t i_normal_time;

    bool       b_eof;   /* eof of demuxer */
};

/**
 * Hold the input_source_t
 */
input_source_t *input_source_Hold( input_source_t *in );

/**
 * Release the input_source_t
 */
void input_source_Release( input_source_t *in );

/**
 * Returns the string identifying this input source
 *
 * @return a string id or NULL if the source is the master
 */
const char *input_source_GetStrId( input_source_t *in );

/**
 * Get a new fmt.i_id from the input source
 *
 * This auto id will be relative to this input source. It allows to have stable
 * ids across different playback instances, by not relying on the input source
 * addition order.
 */
int input_source_GetNewAutoId( input_source_t *in );

/**
 * Returns true if a given source should be auto-selected
 */
bool input_source_IsAutoSelected( input_source_t *in );

#endif
