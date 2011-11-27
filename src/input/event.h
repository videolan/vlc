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
void input_SendEventAbort( input_thread_t *p_input );
void input_SendEventPosition( input_thread_t *p_input, double f_position, mtime_t i_time );
void input_SendEventLength( input_thread_t *p_input, mtime_t i_length );
void input_SendEventStatistics( input_thread_t *p_input );
void input_SendEventRate( input_thread_t *p_input, int i_rate );
void input_SendEventAudioDelay( input_thread_t *p_input, mtime_t i_delay );
void input_SendEventSubtitleDelay( input_thread_t *p_input, mtime_t i_delay );
void input_SendEventRecord( input_thread_t *p_input, bool b_recording );
void input_SendEventTitle( input_thread_t *p_input, int i_title );
void input_SendEventSeekpoint( input_thread_t *p_input, int i_title, int i_seekpoint );
void input_SendEventSignal( input_thread_t *p_input, double f_quality, double f_strength );
void input_SendEventState( input_thread_t *p_input, int i_state );
void input_SendEventCache( input_thread_t *p_input, double f_level );

/* TODO rename Item* */
void input_SendEventMeta( input_thread_t *p_input );
void input_SendEventMetaInfo( input_thread_t *p_input );
void input_SendEventMetaName( input_thread_t *p_input, const char *psz_name );
void input_SendEventMetaEpg( input_thread_t *p_input );

/*****************************************************************************
 * Event for es_out.c
 *****************************************************************************/
void input_SendEventProgramAdd( input_thread_t *p_input,
                                int i_program, const char *psz_text );
void input_SendEventProgramDel( input_thread_t *p_input, int i_program );
void input_SendEventProgramSelect( input_thread_t *p_input, int i_program );
void input_SendEventProgramScrambled( input_thread_t *p_input, int i_group, bool b_scrambled );

void input_SendEventEsDel( input_thread_t *p_input, int i_cat, int i_id );
void input_SendEventEsAdd( input_thread_t *p_input, int i_cat, int i_id, const char *psz_text );
void input_SendEventEsSelect( input_thread_t *p_input, int i_cat, int i_id ); /* i_id == -1 will unselect */

void input_SendEventTeletextAdd( input_thread_t *p_input,
                                 int i_teletext, const char *psz_text );
void input_SendEventTeletextDel( input_thread_t *p_input, int i_teletext );
void input_SendEventTeletextSelect( input_thread_t *p_input, int i_teletext );

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
