/*****************************************************************************
 * event.h: Input event functions
 *****************************************************************************
 * Copyright (C) 2008 Laurent Aimar
 * $Id$
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

/*****************************************************************************
 * Event for input.c
 *****************************************************************************/
void input_SendEventDead( input_thread_t *p_input );
void input_SendEventPosition( input_thread_t *p_input, double f_position, vlc_tick_t i_time );
void input_SendEventLength( input_thread_t *p_input, vlc_tick_t i_length );
void input_SendEventStatistics( input_thread_t *p_input );
void input_SendEventRate( input_thread_t *p_input, int i_rate );
void input_SendEventCapabilities( input_thread_t *p_input, int capabilities );
void input_SendEventAudioDelay( input_thread_t *p_input, vlc_tick_t i_delay );
void input_SendEventSubtitleDelay( input_thread_t *p_input, vlc_tick_t i_delay );
void input_SendEventRecord( input_thread_t *p_input, bool b_recording );
void input_SendEventTitle( input_thread_t *p_input, int i_title );
void input_SendEventSeekpoint( input_thread_t *p_input, int i_title, int i_seekpoint );
void input_SendEventSignal( input_thread_t *p_input, double f_quality, double f_strength );
void input_SendEventState( input_thread_t *p_input, int i_state );
void input_SendEventCache( input_thread_t *p_input, double f_level );

/* TODO rename Item* */
void input_SendEventMeta( input_thread_t *p_input );
void input_SendEventMetaInfo( input_thread_t *p_input );
void input_SendEventMetaEpg( input_thread_t *p_input );

void input_SendEventParsing( input_thread_t *p_input, input_item_node_t *p_root );

/*****************************************************************************
 * Event for es_out.c
 *****************************************************************************/
void input_SendEventProgramAdd( input_thread_t *p_input,
                                int i_program, const char *psz_text );
void input_SendEventProgramDel( input_thread_t *p_input, int i_program );
void input_SendEventProgramSelect( input_thread_t *p_input, int i_program );
void input_SendEventProgramScrambled( input_thread_t *p_input, int i_group, bool b_scrambled );

void input_SendEventEsDel( input_thread_t *p_input, const es_format_t *fmt );
void input_SendEventEsAdd( input_thread_t *p_input,
                           const es_format_t *fmt, const char *psz_title );
void input_SendEventEsSelect( input_thread_t *p_input, const es_format_t *fmt );
void input_SendEventEsUnselect( input_thread_t *p_input, const es_format_t *fmt );

/*****************************************************************************
 * Event for decoder.c
 *****************************************************************************/
void input_SendEventVout( input_thread_t *p_input );
void input_SendEventAout( input_thread_t *p_input );

/*****************************************************************************
 * Event for control.c/input.c
 *****************************************************************************/
void input_SendEventBookmark( input_thread_t *p_input );

#endif
