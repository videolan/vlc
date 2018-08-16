/*****************************************************************************
 * event.c: Events
 *****************************************************************************
 * Copyright (C) 2008 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar < fenrir _AT_ videolan _DOT_ org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_input.h>
#include "input_internal.h"
#include "event.h"
#include <assert.h>

static void input_SendEvent( input_thread_t *p_input,
                             const struct vlc_input_event *event )
{
    input_thread_private_t *priv = input_priv(p_input);
    if( priv->events_cb )
        priv->events_cb( p_input, event, priv->events_data );
}

void input_SendEventDead( input_thread_t *p_input )
{
    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_DEAD,
    });
}

void input_SendEventCapabilities( input_thread_t *p_input, int i_capabilities )
{
    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_CAPABILITIES,
        .capabilities = i_capabilities
    });
}
void input_SendEventPosition( input_thread_t *p_input, double f_position,
                              vlc_tick_t i_time )
{
    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_POSITION,
        .position = { f_position, i_time }
    });
}
void input_SendEventLength( input_thread_t *p_input, vlc_tick_t i_length )
{
    input_item_SetDuration( input_priv(p_input)->p_item, i_length );
    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_LENGTH,
        .length = i_length,
    });
}
void input_SendEventStatistics( input_thread_t *p_input )
{
    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_STATISTICS,
    });
}
void input_SendEventRate( input_thread_t *p_input, int i_rate )
{
    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_RATE,
        .rate = (float)INPUT_RATE_DEFAULT / (float)i_rate,
    });
}
void input_SendEventAudioDelay( input_thread_t *p_input, vlc_tick_t i_delay )
{
    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_AUDIO_DELAY,
        .audio_delay = i_delay,
    });
}

void input_SendEventSubtitleDelay( input_thread_t *p_input, vlc_tick_t i_delay )
{
    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_SUBTITLE_DELAY,
        .subtitle_delay = i_delay,
    });
}

/* TODO and file name ? */
void input_SendEventRecord( input_thread_t *p_input, bool b_recording )
{
    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_RECORD,
        .record = b_recording
    });
}

void input_SendEventTitle( input_thread_t *p_input, int i_title )
{
    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_TITLE,
        .title = i_title
    });
}

void input_SendEventSeekpoint( input_thread_t *p_input, int i_title, int i_seekpoint )
{
    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_CHAPTER,
        .chapter = { i_title, i_seekpoint }
    });
}

void input_SendEventSignal( input_thread_t *p_input, double f_quality,
                            double f_strength )
{
    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_SIGNAL,
        .signal = { f_quality, f_strength }
    });
}

void input_SendEventState( input_thread_t *p_input, int i_state )
{
    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_STATE,
        .state = i_state
    });
}

void input_SendEventCache( input_thread_t *p_input, double f_level )
{
    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_CACHE,
        .cache = f_level
    });
}

void input_SendEventMeta( input_thread_t *p_input )
{
    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_ITEM_META,
    });
}

void input_SendEventMetaInfo( input_thread_t *p_input )
{
    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_ITEM_INFO,
    });
}

void input_SendEventMetaEpg( input_thread_t *p_input )
{
    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_ITEM_EPG,
    });
}
/*****************************************************************************
 * Event for es_out.c
 *****************************************************************************/
void input_SendEventProgramAdd( input_thread_t *p_input,
                                int i_program, const char *psz_text )
{
    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_PROGRAM,
        .program = {
            .action = VLC_INPUT_PROGRAM_ADDED,
            .id = i_program,
            .title = psz_text
        }
    });
}
void input_SendEventProgramDel( input_thread_t *p_input, int i_program )
{
    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_PROGRAM, 
        .program = {
            .action = VLC_INPUT_PROGRAM_DELETED,
            .id = i_program
        }
    });
}
void input_SendEventProgramSelect( input_thread_t *p_input, int i_program )
{
    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_PROGRAM, 
        .program = {
            .action = VLC_INPUT_PROGRAM_SELECTED,
            .id = i_program
        }
    });
}
void input_SendEventProgramScrambled( input_thread_t *p_input, int i_group, bool b_scrambled )
{
    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_PROGRAM, 
        .program = {
            .action = VLC_INPUT_PROGRAM_SCRAMBLED,
            .id = i_group,
            .scrambled = b_scrambled 
        }
    });
}

void input_SendEventEsAdd( input_thread_t *p_input, const es_format_t *p_fmt,
                           const char *psz_title)
{
    input_thread_private_t *priv = input_priv(p_input);
    priv->i_last_es_cat = p_fmt->i_cat;
    priv->i_last_es_id = p_fmt->i_id;

    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_ES,
        .es = {
            .action = VLC_INPUT_ES_ADDED,
            .title = psz_title,
            .fmt = p_fmt,
        }
    });
}
void input_SendEventEsUpdate( input_thread_t *p_input, const es_format_t *p_fmt )
{
    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_ES,
        .es = {
            .action = VLC_INPUT_ES_UPDATED,
            .fmt = p_fmt,
        }
    });
}
void input_SendEventEsDel( input_thread_t *p_input,
                           const es_format_t *p_fmt )
{
    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_ES,
        .es = {
            .action = VLC_INPUT_ES_DELETED,
            .fmt = p_fmt,
        }
    });
}
void input_SendEventEsSelect( input_thread_t *p_input,
                              const es_format_t *p_fmt )
{
    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_ES,
        .es = {
            .action = VLC_INPUT_ES_SELECTED,
            .fmt = p_fmt,
        }
    });
}

void input_SendEventEsUnselect( input_thread_t *p_input,
                                const es_format_t *p_fmt )
{
    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_ES,
        .es = {
            .action = VLC_INPUT_ES_UNSELECTED,
            .fmt = p_fmt,
        }
    });
}

void input_SendEventVout( input_thread_t *p_input )
{
    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_VOUT
    });
}

void input_SendEventAout( input_thread_t *p_input )
{
    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_AOUT
    });
}

void input_SendEventBookmark( input_thread_t *p_input )
{
    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_BOOKMARK
    });
}

void input_SendEventParsing( input_thread_t *p_input, input_item_node_t *p_root )
{
    input_SendEvent( p_input, &(struct vlc_input_event) {
        .type = INPUT_EVENT_SUBITEMS,
        .subitems = p_root,
    });
}
