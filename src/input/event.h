/*****************************************************************************
 * event.h: Input event functions
 *****************************************************************************
 * Copyright (C) 2008 Laurent Aimar
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ fr>
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

#ifndef LIBVLC_INPUT_EVENT_H
#define LIBVLC_INPUT_EVENT_H 1

#include <vlc_common.h>
#include <vlc_input.h>
#include "input_internal.h"

static inline void input_SendEvent(input_thread_t *p_input,
                                   const struct vlc_input_event *event)
{
    input_thread_private_t *priv = input_priv(p_input);
    if(priv->events_cb)
        priv->events_cb(p_input, event, priv->events_data);
}

/*****************************************************************************
 * Event for input.c
 *****************************************************************************/
static inline void input_SendEventDead(input_thread_t *p_input)
{
    input_SendEvent(p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_DEAD,
    });
}

static inline void input_SendEventCapabilities(input_thread_t *p_input,
                                                int i_capabilities)
{
    input_SendEvent(p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_CAPABILITIES,
        .capabilities = i_capabilities
    });
}

static inline void input_SendEventTimes(input_thread_t *p_input,
                                        double f_position, vlc_tick_t i_time,
                                        vlc_tick_t i_normal_time,
                                        vlc_tick_t i_length)
{
    input_SendEvent(p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_TIMES,
        .times = { f_position, i_time, i_normal_time, i_length }
    });
}

static inline void input_SendEventOutputClock(input_thread_t *p_input,
                                              vlc_es_id_t *id, bool master,
                                              vlc_tick_t system_ts,
                                              vlc_tick_t ts, double rate,
                                              unsigned frame_rate,
                                              unsigned frame_rate_base)
{
    input_SendEvent(p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_OUTPUT_CLOCK,
        .output_clock = { id, master, system_ts, ts, rate,
                          frame_rate, frame_rate_base }
    });
}

static inline void input_SendEventStatistics(input_thread_t *p_input,
                                             const struct input_stats_t *stats)
{
    input_SendEvent(p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_STATISTICS,
        .stats = stats,
    });
}

static inline void input_SendEventRate(input_thread_t *p_input, float rate)
{
    input_SendEvent(p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_RATE,
        .rate = rate,
    });
}

static inline void input_SendEventRecord(input_thread_t *p_input,
                                         bool b_recording)
{
    input_SendEvent(p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_RECORD,
        .record = b_recording
    });
}

static inline void input_SendEventTitle(input_thread_t *p_input,
                                        const struct vlc_input_event_title *title)
{
    input_SendEvent(p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_TITLE,
        .title = *title
    });
}

static inline void input_SendEventSeekpoint(input_thread_t *p_input,
                                            int i_title, int i_seekpoint)
{
    input_SendEvent(p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_CHAPTER,
        .chapter = { i_title, i_seekpoint }
    });
}

static inline void input_SendEventSignal(input_thread_t *p_input,
                                         double f_quality, double f_strength)
{
    input_SendEvent(p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_SIGNAL,
        .signal = { f_quality, f_strength }
    });
}

static inline void input_SendEventState(input_thread_t *p_input, int i_state,
                                        vlc_tick_t state_date)
{
    input_SendEvent(p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_STATE,
        .state = { i_state, state_date, },
    });
}

static inline void input_SendEventCache(input_thread_t *p_input, double f_level)
{
    input_SendEvent(p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_CACHE,
        .cache = f_level
    });
}

static inline void input_SendEventMeta(input_thread_t *p_input)
{
    input_SendEvent(p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_ITEM_META,
    });
}

static inline void input_SendEventMetaInfo(input_thread_t *p_input)
{
    input_SendEvent(p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_ITEM_INFO,
    });
}

static inline void input_SendEventMetaEpg(input_thread_t *p_input)
{
    input_SendEvent(p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_ITEM_EPG,
    });
}

static inline void input_SendEventSubsFPS(input_thread_t *p_input, float fps)
{
    input_SendEvent(p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_SUBS_FPS,
        .subs_fps = fps,
    });
}

/*****************************************************************************
 * Event for es_out.c
 *****************************************************************************/
static inline void input_SendEventProgramAdd(input_thread_t *p_input,
                                int i_program, const char *psz_text)
{
    input_SendEvent(p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_PROGRAM,
        .program = {
            .action = VLC_INPUT_PROGRAM_ADDED,
            .id = i_program,
            .title = psz_text
        }
    });
}
static inline void input_SendEventProgramUpdated(input_thread_t *p_input,
                                int i_program, const char *psz_text)
{
    input_SendEvent(p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_PROGRAM,
        .program = {
            .action = VLC_INPUT_PROGRAM_UPDATED,
            .id = i_program,
            .title = psz_text
        }
    });
}
static inline void input_SendEventProgramDel(input_thread_t *p_input,
                                             int i_program)
{
    input_SendEvent(p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_PROGRAM,
        .program = {
            .action = VLC_INPUT_PROGRAM_DELETED,
            .id = i_program
        }
    });
}
static inline void input_SendEventProgramSelect(input_thread_t *p_input,
                                                int i_program)
{
    input_SendEvent(p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_PROGRAM,
        .program = {
            .action = VLC_INPUT_PROGRAM_SELECTED,
            .id = i_program
        }
    });
}
static inline void input_SendEventProgramScrambled(input_thread_t *p_input,
                                                   int i_group, bool b_scrambled)
{
    input_SendEvent(p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_PROGRAM,
        .program = {
            .action = VLC_INPUT_PROGRAM_SCRAMBLED,
            .id = i_group,
            .scrambled = b_scrambled 
        }
    });
}

static inline void input_SendEventEs(input_thread_t *p_input,
                                     const struct vlc_input_event_es *es_event)
{
    input_SendEvent(p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_ES,
        .es = *es_event,
    });
}

static inline void input_SendEventParsing(input_thread_t *p_input,
                                          input_item_node_t *p_root)
{
    input_SendEvent(p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_SUBITEMS,
        .subitems = p_root,
    });
}

static inline void input_SendEventVbiPage(input_thread_t *p_input, unsigned page)
{
    input_SendEvent(p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_VBI_PAGE,
        .vbi_page = page,
    });
}

static inline void input_SendEventVbiTransparency(input_thread_t *p_input,
                                                  bool transparent)
{
    input_SendEvent(p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_VBI_TRANSPARENCY,
        .vbi_transparent = transparent,
    });
}

/*****************************************************************************
 * Event for resource.c
 *****************************************************************************/
static inline void input_SendEventVout(input_thread_t *p_input,
                                       const struct vlc_input_event_vout *event)
{
    input_SendEvent(p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_VOUT,
        .vout = *event,
    });
}

/*****************************************************************************
 * Event for control.c/input.c
 *****************************************************************************/
static inline void input_SendEventBookmark(input_thread_t *p_input)
{
    input_SendEvent(p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_BOOKMARK
    });
}

#endif
