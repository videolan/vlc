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

/* */
static void Trigger( input_thread_t *, int i_type );
static void VarListAdd( input_thread_t *,
                        const char *psz_variable, int i_event,
                        int i_value, const char *psz_text );
static void VarListDel( input_thread_t *,
                        const char *psz_variable, int i_event,
                        int i_value );
static void VarListSelect( input_thread_t *,
                           const char *psz_variable, int i_event,
                           int i_value );

/*****************************************************************************
 * Event for input.c
 *****************************************************************************/
void input_SendEventDead( input_thread_t *p_input )
{
    p_input->b_dead = true;

    Trigger( p_input, INPUT_EVENT_DEAD );
}
void input_SendEventAbort( input_thread_t *p_input )
{
    Trigger( p_input, INPUT_EVENT_ABORT );
}

void input_SendEventPosition( input_thread_t *p_input, double f_position, mtime_t i_time )
{
    vlc_value_t val;

    /* */
    val.f_float = f_position;
    var_Change( p_input, "position", VLC_VAR_SETVALUE, &val, NULL );

    /* */
    val.i_time = i_time;
    var_Change( p_input, "time", VLC_VAR_SETVALUE, &val, NULL );

    Trigger( p_input, INPUT_EVENT_POSITION );
}
void input_SendEventLength( input_thread_t *p_input, mtime_t i_length )
{
    vlc_value_t val;

    /* FIXME ugly + what about meta change event ? */
    if( var_GetTime( p_input, "length" ) == i_length )
        return;

    input_item_SetDuration( p_input->p->p_item, i_length );

    val.i_time = i_length;
    var_Change( p_input, "length", VLC_VAR_SETVALUE, &val, NULL );

    Trigger( p_input, INPUT_EVENT_LENGTH );
}
void input_SendEventStatistics( input_thread_t *p_input )
{
    Trigger( p_input, INPUT_EVENT_STATISTICS );
}
void input_SendEventRate( input_thread_t *p_input, int i_rate )
{
    vlc_value_t val;

    val.f_float = (float)INPUT_RATE_DEFAULT / (float)i_rate;
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

    /* "chapter" */
    val.i_int = i_seekpoint;
    var_Change( p_input, "chapter", VLC_VAR_SETVALUE, &val, NULL );

    /* "title %2i" */
    char psz_title[10];
    snprintf( psz_title, sizeof(psz_title), "title %2i", i_title );
    var_Change( p_input, psz_title, VLC_VAR_SETVALUE, &val, NULL );

    /* */
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
}

void input_SendEventCache( input_thread_t *p_input, double f_level )
{
    vlc_value_t val;

    val.f_float = f_level;
    var_Change( p_input, "cache", VLC_VAR_SETVALUE, &val, NULL );

    Trigger( p_input, INPUT_EVENT_CACHE );
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
    vlc_event_send( &p_input->p->p_item->event_manager, &event );
}

void input_SendEventMetaInfo( input_thread_t *p_input )
{
    Trigger( p_input, INPUT_EVENT_ITEM_INFO );

    /* FIXME remove this ugliness */
    vlc_event_t event;

    event.type = vlc_InputItemInfoChanged;
    vlc_event_send( &p_input->p->p_item->event_manager, &event );
}

void input_SendEventMetaName( input_thread_t *p_input, const char *psz_name )
{
    Trigger( p_input, INPUT_EVENT_ITEM_NAME );

    /* FIXME remove this ugliness */
    vlc_event_t event;

    event.type = vlc_InputItemNameChanged;
    event.u.input_item_name_changed.new_name = psz_name;
    vlc_event_send( &p_input->p->p_item->event_manager, &event );
}

void input_SendEventMetaEpg( input_thread_t *p_input )
{
    Trigger( p_input, INPUT_EVENT_ITEM_EPG );
}
/*****************************************************************************
 * Event for es_out.c
 *****************************************************************************/
void input_SendEventProgramAdd( input_thread_t *p_input,
                                int i_program, const char *psz_text )
{
    VarListAdd( p_input, "program", INPUT_EVENT_PROGRAM, i_program, psz_text );
}
void input_SendEventProgramDel( input_thread_t *p_input, int i_program )
{
    VarListDel( p_input, "program", INPUT_EVENT_PROGRAM, i_program );
}
void input_SendEventProgramSelect( input_thread_t *p_input, int i_program )
{
    VarListSelect( p_input, "program", INPUT_EVENT_PROGRAM, i_program );
}
void input_SendEventProgramScrambled( input_thread_t *p_input, int i_group, bool b_scrambled )
{
    if( var_GetInteger( p_input, "program" ) != i_group )
        return;

    var_SetBool( p_input, "program-scrambled", b_scrambled );
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
void input_SendEventEsAdd( input_thread_t *p_input, int i_cat, int i_id, const char *psz_text )
{
    if( i_cat != UNKNOWN_ES )
        VarListAdd( p_input, GetEsVarName( i_cat ), INPUT_EVENT_ES,
                    i_id, psz_text );
}
void input_SendEventEsDel( input_thread_t *p_input, int i_cat, int i_id )
{
    if( i_cat != UNKNOWN_ES )
        VarListDel( p_input, GetEsVarName( i_cat ), INPUT_EVENT_ES, i_id );
}
/* i_id == -1 will unselect */
void input_SendEventEsSelect( input_thread_t *p_input, int i_cat, int i_id )
{
    if( i_cat != UNKNOWN_ES )
        VarListSelect( p_input, GetEsVarName( i_cat ), INPUT_EVENT_ES, i_id );
}

void input_SendEventTeletextAdd( input_thread_t *p_input,
                                 int i_teletext, const char *psz_text )
{
    VarListAdd( p_input, "teletext-es", INPUT_EVENT_TELETEXT, i_teletext, psz_text );
}
void input_SendEventTeletextDel( input_thread_t *p_input, int i_teletext )
{
    VarListDel( p_input, "teletext-es", INPUT_EVENT_TELETEXT, i_teletext );
}
void input_SendEventTeletextSelect( input_thread_t *p_input, int i_teletext )
{
    VarListSelect( p_input, "teletext-es", INPUT_EVENT_TELETEXT, i_teletext );
}

void input_SendEventVout( input_thread_t *p_input )
{
    Trigger( p_input, INPUT_EVENT_VOUT );
}

void input_SendEventAout( input_thread_t *p_input )
{
    Trigger( p_input, INPUT_EVENT_AOUT );
}

/*****************************************************************************
 * Event for control.c/input.c
 *****************************************************************************/
void input_SendEventBookmark( input_thread_t *p_input )
{
    Trigger( p_input, INPUT_EVENT_BOOKMARK );
}

/*****************************************************************************
 *
 *****************************************************************************/
static void Trigger( input_thread_t *p_input, int i_type )
{
    var_SetInteger( p_input, "intf-event", i_type );
}
static void VarListAdd( input_thread_t *p_input,
                        const char *psz_variable, int i_event,
                        int i_value, const char *psz_text )
{
    vlc_value_t val;
    vlc_value_t text;

    val.i_int = i_value;
    text.psz_string = (char*)psz_text;

    var_Change( p_input, psz_variable, VLC_VAR_ADDCHOICE,
                &val, psz_text ? &text : NULL );

    Trigger( p_input, i_event );
}
static void VarListDel( input_thread_t *p_input,
                        const char *psz_variable, int i_event,
                        int i_value )
{
    vlc_value_t val;

    if( i_value >= 0 )
    {
        val.i_int = i_value;
        var_Change( p_input, psz_variable, VLC_VAR_DELCHOICE, &val, NULL );
    }
    else
    {
        var_Change( p_input, psz_variable, VLC_VAR_CLEARCHOICES, &val, NULL );
    }

    Trigger( p_input, i_event );
}
static void VarListSelect( input_thread_t *p_input,
                           const char *psz_variable, int i_event,
                           int i_value )
{
    vlc_value_t val;

    val.i_int = i_value;
    var_Change( p_input, psz_variable, VLC_VAR_SETVALUE, &val, NULL );

    Trigger( p_input, i_event );
}


