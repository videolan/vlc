/*****************************************************************************
 * event.c: Events
 *****************************************************************************
 * Copyright (C) 2008 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar < fenrir _AT_ videolan _DOT_ org>
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

static void Trigger( input_thread_t *p_input, int i_type );

/*****************************************************************************
 * Event for input.c
 *****************************************************************************/
void input_SendEventTimes( input_thread_t *p_input,
                           double f_position, mtime_t i_time, mtime_t i_length )
{
    vlc_value_t val;

    /* */
    val.f_float = f_position;
    var_Change( p_input, "position", VLC_VAR_SETVALUE, &val, NULL );

    /* */
    val.i_time = i_time;
    var_Change( p_input, "time", VLC_VAR_SETVALUE, &val, NULL );

	/* FIXME ugly + what about meta change event ? */
    if( var_GetTime( p_input, "length" ) != i_length )
        input_item_SetDuration( p_input->p->input.p_item, i_length );
    val.i_time = i_length;
    var_Change( p_input, "length", VLC_VAR_SETVALUE, &val, NULL );

    Trigger( p_input, INPUT_EVENT_TIMES );
}
void input_SendEventStatistics( input_thread_t *p_input )
{
    Trigger( p_input, INPUT_EVENT_STATISTICS );
}
void input_SendEventRate( input_thread_t *p_input, int i_rate )
{
	vlc_value_t val;

	val.i_int = i_rate;
	var_Change( p_input, "rate", VLC_VAR_SETVALUE, &val, NULL );

    Trigger( p_input, INPUT_EVENT_RATE );
}
void input_SendEventAudioDelay( input_thread_t *p_input, mtime_t i_delay )
{
	vlc_value_t val;

	val.i_time = i_delay;
	var_Change( p_input, "audio-delay", VLC_VAR_SETVALUE, &val, NULL );

    Trigger( p_input, INPUT_EVENT_AUDIO_DELAY );
}

void input_SendEventSubtitleDelay( input_thread_t *p_input, mtime_t i_delay )
{
	vlc_value_t val;

	val.i_time = i_delay;
	var_Change( p_input, "spu-delay", VLC_VAR_SETVALUE, &val, NULL );

    Trigger( p_input, INPUT_EVENT_SUBTITLE_DELAY );
}

/* TODO and file name ? */
void input_SendEventRecord( input_thread_t *p_input, bool b_recording )
{
	vlc_value_t val;

	val.b_bool = b_recording;
	var_Change( p_input, "record", VLC_VAR_SETVALUE, &val, NULL );

    Trigger( p_input, INPUT_EVENT_RECORD );
}

void input_SendEventTitle( input_thread_t *p_input, int i_title )
{
	vlc_value_t val;

	val.i_int = i_title;
	var_Change( p_input, "title", VLC_VAR_SETVALUE, &val, NULL );

	input_ControlVarTitle( p_input, i_title );

    Trigger( p_input, INPUT_EVENT_TITLE );
}

void input_SendEventSeekpoint( input_thread_t *p_input, int i_title, int i_seekpoint )
{
	vlc_value_t val;

	VLC_UNUSED( i_title );
	val.i_int = i_seekpoint;
	var_Change( p_input, "chapter", VLC_VAR_SETVALUE, &val, NULL );

    Trigger( p_input, INPUT_EVENT_CHAPTER );
}

void input_SendEventSignal( input_thread_t *p_input, double f_quality, double f_strength )
{
    vlc_value_t val;

    val.f_float = f_quality;
	var_Change( p_input, "signal-quality", VLC_VAR_SETVALUE, &val, NULL );

    val.f_float = f_strength;
	var_Change( p_input, "signal-strength", VLC_VAR_SETVALUE, &val, NULL );

    Trigger( p_input, INPUT_EVENT_SIGNAL );
}

void input_SendEventState( input_thread_t *p_input, int i_state )
{
    vlc_value_t val;

    val.i_int = i_state;
    var_Change( p_input, "state", VLC_VAR_SETVALUE, &val, NULL );

    Trigger( p_input, INPUT_EVENT_STATE );

	/* FIXME remove this ugliness */
    vlc_event_t event;

    event.type = vlc_InputStateChanged;
    event.u.input_state_changed.new_state = i_state;
    vlc_event_send( &p_input->p->event_manager, &event );
}

/* FIXME: review them because vlc_event_send might be
 * moved inside input_item* functions.
 */
void input_SendEventMeta( input_thread_t *p_input )
{
    Trigger( p_input, INPUT_EVENT_ITEM_META );

	/* FIXME remove this ugliness ? */
	vlc_event_t event;

	event.type = vlc_InputItemMetaChanged;
	event.u.input_item_meta_changed.meta_type = vlc_meta_ArtworkURL;
	vlc_event_send( &p_input->p->input.p_item->event_manager, &event );
}

void input_SendEventMetaInfo( input_thread_t *p_input )
{
    Trigger( p_input, INPUT_EVENT_ITEM_INFO );

	/* FIXME remove this ugliness */
    vlc_event_t event;

    event.type = vlc_InputItemInfoChanged;
    vlc_event_send( &p_input->p->input.p_item->event_manager, &event );
}

void input_SendEventMetaName( input_thread_t *p_input, const char *psz_name )
{
    Trigger( p_input, INPUT_EVENT_ITEM_NAME );

	/* FIXME remove this ugliness */
    vlc_event_t event;

    event.type = vlc_InputItemNameChanged;
    event.u.input_item_name_changed.new_name = psz_name;
    vlc_event_send( &p_input->p->input.p_item->event_manager, &event );
}

/*****************************************************************************
 * Event for es_out.c
 *****************************************************************************/
void input_SendEventProgramAdd( input_thread_t *p_input,
                                int i_program, const char *psz_text )
{
    vlc_value_t val;
    vlc_value_t text;

    val.i_int = i_program;
    text.psz_string = (char*)psz_text;

    var_Change( p_input, "program", VLC_VAR_ADDCHOICE,
                &val, psz_text ? &text : NULL );

    Trigger( p_input, INPUT_EVENT_PROGRAM );
}
void input_SendEventProgramDel( input_thread_t *p_input, int i_program )
{
    vlc_value_t val;

    val.i_int = i_program;
    var_Change( p_input, "program", VLC_VAR_DELCHOICE, &val, NULL );

    Trigger( p_input, INPUT_EVENT_PROGRAM );
}
void input_SendEventProgramSelect( input_thread_t *p_input, int i_program )
{
    vlc_value_t val;

    val.i_int = i_program;
    var_Change( p_input, "program", VLC_VAR_SETVALUE, &val, NULL );

    Trigger( p_input, INPUT_EVENT_PROGRAM );
}

static const char *GetEsVarName( int i_cat )
{
    switch( i_cat )
    {
    case VIDEO_ES:
        return "video-es";
    case AUDIO_ES:
        return "audio-es";
    default:
        assert( i_cat == SPU_ES );
        return "spu-es";
    }
}
void input_SendEventEsDel( input_thread_t *p_input, int i_cat, int i_id )
{
    vlc_value_t val;

    if( i_id >= 0 )
    {
        val.i_int = i_id;
        var_Change( p_input, GetEsVarName( i_cat ), VLC_VAR_DELCHOICE, &val, NULL );
    }
    else
    {
        var_Change( p_input, GetEsVarName( i_cat ), VLC_VAR_CLEARCHOICES, NULL, NULL );
    }

    Trigger( p_input, INPUT_EVENT_ES );
}
void input_SendEventEsAdd( input_thread_t *p_input, int i_cat, int i_id, const char *psz_text )
{
    vlc_value_t val;
    vlc_value_t text;

    val.i_int = i_id;
    text.psz_string = (char*)psz_text;

    var_Change( p_input, GetEsVarName( i_cat ), VLC_VAR_ADDCHOICE,
                &val, psz_text ? &text : NULL );

    Trigger( p_input, INPUT_EVENT_ES );
}

/* i_id == -1 will unselect */
void input_SendEventEsSelect( input_thread_t *p_input, int i_cat, int i_id )
{
    vlc_value_t val;

    val.i_int = i_id;
    var_Change( p_input, GetEsVarName( i_cat ), VLC_VAR_SETVALUE, &val, NULL );

    Trigger( p_input, INPUT_EVENT_ES );

    /* FIXME to remove this ugliness */
    vlc_event_t event;
    event.type = vlc_InputSelectedStreamChanged;
    vlc_event_send( &p_input->p->event_manager, &event );
}

void input_SendEventTeletext( input_thread_t *p_input, int i_id )
{
    vlc_value_t val;

    val.i_int = i_id;
    var_Change( p_input, "teletext-es", VLC_VAR_SETVALUE, &val, NULL );

    Trigger( p_input, INPUT_EVENT_TELETEXT );
}

void input_SendEventVout( input_thread_t *p_input )
{
    Trigger( p_input, INPUT_EVENT_VOUT );
}

/*****************************************************************************
 *
 *****************************************************************************/
static void Trigger( input_thread_t *p_input, int i_type )
{
    var_SetInteger( p_input, "intf-event", i_type );
}
