/*****************************************************************************
 * var.c: object variables for input thread
 *****************************************************************************
 * Copyright (C) 2004-2007 VLC authors and VideoLAN
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_memstream.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "input_internal.h"

/*****************************************************************************
 * Callbacks
 *****************************************************************************/
static int StateCallback   ( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval, void * );
static int RateCallback    ( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval, void * );
static int PositionCallback( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval, void * );
static int TimeCallback    ( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval, void * );
static int TimeOffsetCallback( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval, void * );
static int ProgramCallback ( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval, void * );
static int TitleCallback   ( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval, void * );
static int SeekpointCallback( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval, void * );
static int NavigationCallback( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval, void * );
static int EsVideoCallback ( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval, void * );
static int EsAudioCallback ( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval, void * );
static int EsSpuCallback ( vlc_object_t *p_this, char const *psz_cmd,
                           vlc_value_t oldval, vlc_value_t newval, void * );
static int EsDelayCallback ( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval, void * );

static int BookmarkCallback( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval, void * );

static int RecordCallback( vlc_object_t *p_this, char const *psz_cmd,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data );
static int FrameNextCallback( vlc_object_t *p_this, char const *psz_cmd,
                              vlc_value_t oldval, vlc_value_t newval,
                              void *p_data );
static void input_LegacyVarTitle( input_thread_t *p_input, int i_title );
static void input_LegacyVarNavigation( input_thread_t *p_input );

typedef struct
{
    const char *psz_name;
    vlc_callback_t callback;
} vlc_input_callback_t;

static void InputAddCallbacks( input_thread_t *, const vlc_input_callback_t * );

#ifdef CALLBACK /* For windows */
# undef CALLBACK /* We don't care of this one here */
#endif
/* List all callbacks added by input */
#define CALLBACK(name,cb) { name, cb }
static const vlc_input_callback_t p_input_callbacks[] =
{
    CALLBACK( "state", StateCallback ),
    CALLBACK( "rate", RateCallback ),
    CALLBACK( "position", PositionCallback ),
    CALLBACK( "time", TimeCallback ),
    CALLBACK( "time-offset", TimeOffsetCallback ),
    CALLBACK( "bookmark", BookmarkCallback ),
    CALLBACK( "program", ProgramCallback ),
    CALLBACK( "title", TitleCallback ),
    CALLBACK( "chapter", SeekpointCallback ),
    CALLBACK( "audio-delay", EsDelayCallback ),
    CALLBACK( "spu-delay", EsDelayCallback ),
    CALLBACK( "video-es", EsVideoCallback ),
    CALLBACK( "audio-es", EsAudioCallback ),
    CALLBACK( "spu-es", EsSpuCallback ),
    CALLBACK( "record", RecordCallback ),
    CALLBACK( "frame-next", FrameNextCallback ),

    CALLBACK( NULL, NULL )
};
#undef CALLBACK

static void Trigger( input_thread_t *p_input, int i_type )
{
    var_SetInteger( p_input, "intf-event", i_type );
}
static void VarListAdd( input_thread_t *p_input,
                        const char *psz_variable,
                        int i_value, const char *psz_text )
{
    vlc_value_t val;

    val.i_int = i_value;

    var_Change( p_input, psz_variable, VLC_VAR_ADDCHOICE, val, psz_text );
}
static void VarListDel( input_thread_t *p_input,
                        const char *psz_variable,
                        int i_value )
{
    vlc_value_t val;

    if( i_value >= 0 )
    {
        val.i_int = i_value;
        var_Change( p_input, psz_variable, VLC_VAR_DELCHOICE, val );
    }
    else
    {
        var_Change( p_input, psz_variable, VLC_VAR_CLEARCHOICES );
    }
}
static void VarListSelect( input_thread_t *p_input,
                           const char *psz_variable,
                           int i_value )
{
    vlc_value_t val;

    val.i_int = i_value;
    var_Change( p_input, psz_variable, VLC_VAR_SETVALUE, val );
}
static const char *GetEsVarName( enum es_format_category_e i_cat )
{
    switch( i_cat )
    {
    case VIDEO_ES:
        return "video-es";
    case AUDIO_ES:
        return "audio-es";
    case SPU_ES:
        return "spu-es";
    default:
        return NULL;
    }
}

static void UpdateBookmarksOption( input_thread_t *p_input )
{
    input_thread_private_t *priv = input_priv(p_input);
    struct vlc_memstream vstr;

    vlc_memstream_open( &vstr );
    vlc_memstream_puts( &vstr, "bookmarks=" );

    vlc_mutex_lock( &priv->p_item->lock );
    var_Change( p_input, "bookmark", VLC_VAR_CLEARCHOICES );

    for( int i = 0; i < priv->i_bookmark; i++ )
    {
        seekpoint_t const* sp = priv->pp_bookmark[i];

        /* Add bookmark to choice-list */
        var_Change( p_input, "bookmark", VLC_VAR_ADDCHOICE,
                    (vlc_value_t){ .i_int = i }, sp->psz_name );

        /* Append bookmark to option-buffer */
        /* TODO: escape inappropriate values */
        vlc_memstream_printf( &vstr, "%s{name=%s,time=%.3f}",
            i > 0 ? "," : "", sp->psz_name, ( 1. * sp->i_time_offset ) / CLOCK_FREQ );
    }

    vlc_mutex_unlock( &priv->p_item->lock );

    if( vlc_memstream_close( &vstr ) == VLC_SUCCESS )
    {
        bool b_overwritten = false;

        for( int i = 0; i < priv->p_item->i_options; ++i )
        {
            char** ppsz_option = &priv->p_item->ppsz_options[i];

            if( strncmp( *ppsz_option, "bookmarks=", 10 ) == 0 )
            {
                free( *ppsz_option );
                *ppsz_option = vstr.ptr;
                b_overwritten = true;
            }
        }

        if( !b_overwritten )
        {
            input_item_AddOption( priv->p_item, vstr.ptr,
                                  VLC_INPUT_OPTION_UNIQUE );
            free( vstr.ptr );
        }
    }
}

void input_LegacyEvents( input_thread_t *p_input, void *user_data,
                         const struct vlc_input_event *event )
{
    (void) user_data;
    vlc_value_t val;

    switch( event->type )
    {
        case INPUT_EVENT_STATE:
            val.i_int = event->state;
            var_Change( p_input, "state", VLC_VAR_SETVALUE, val );
            break;
        case INPUT_EVENT_DEAD:
            break;
        case INPUT_EVENT_RATE:
            val.f_float = event->rate;
            var_Change( p_input, "rate", VLC_VAR_SETVALUE, val );
            break;
        case INPUT_EVENT_CAPABILITIES:
            var_SetBool( p_input, "can-seek",
                         event->capabilities & VLC_INPUT_CAPABILITIES_SEEKABLE );
            var_SetBool( p_input, "can-pause",
                         event->capabilities & VLC_INPUT_CAPABILITIES_PAUSEABLE );
            var_SetBool( p_input, "can-rate",
                         event->capabilities & VLC_INPUT_CAPABILITIES_CHANGE_RATE );
            var_SetBool( p_input, "can-rewind",
                         event->capabilities & VLC_INPUT_CAPABILITIES_REWINDABLE );
            var_SetBool( p_input, "can-record",
                         event->capabilities & VLC_INPUT_CAPABILITIES_RECORDABLE );
            break;
        case INPUT_EVENT_POSITION:
            val.f_float = event->position.percentage;
            var_Change( p_input, "position", VLC_VAR_SETVALUE, val );
            val.i_int = event->position.ms;
            var_Change( p_input, "time", VLC_VAR_SETVALUE, val );
            break;
        case INPUT_EVENT_LENGTH:
            /* FIXME ugly + what about meta change event ? */
            if( var_GetInteger( p_input, "length" ) == event->length )
                return;
            val.i_int = event->length;
            var_Change( p_input, "length", VLC_VAR_SETVALUE, val );
            break;
        case INPUT_EVENT_TITLE:
            val.i_int = event->title;
            var_Change( p_input, "title", VLC_VAR_SETVALUE, val );
            input_LegacyVarNavigation( p_input );
            input_LegacyVarTitle( p_input, event->title );
            break;
        case INPUT_EVENT_CHAPTER:
            /* "chapter" */
            val.i_int = event->chapter.seekpoint;
            var_Change( p_input, "chapter", VLC_VAR_SETVALUE, val );

            /* "title %2u" */
            char psz_title[sizeof ("title ") + 3 * sizeof (int)];
            sprintf( psz_title, "title %2u", event->chapter.title );
            var_Change( p_input, psz_title, VLC_VAR_SETVALUE, val );
            break;
        case INPUT_EVENT_PROGRAM:
            switch (event->program.action)
            {
                case VLC_INPUT_PROGRAM_ADDED:
                    VarListAdd( p_input, "program", event->program.id,
                                event->program.title );
                    break;
                case VLC_INPUT_PROGRAM_DELETED:
                    VarListDel( p_input, "program", event->program.id );
                    break;
                case VLC_INPUT_PROGRAM_SELECTED:
                    VarListSelect( p_input, "program", event->program.id );
                    break;
                case VLC_INPUT_PROGRAM_SCRAMBLED:
                    if( var_GetInteger( p_input, "program" ) != event->program.id  )
                        return;
                    var_SetBool( p_input, "program-scrambled", event->program.scrambled );
                    break;
            }
            break;
        case INPUT_EVENT_ES:
            switch (event->es.action)
            {
                case VLC_INPUT_ES_ADDED:
                {
                    const char *varname = GetEsVarName( event->es.cat );
                    if( varname )
                        VarListAdd( p_input, varname, event->es.id,
                                    event->es.title );
                    break;
                }
                case VLC_INPUT_ES_DELETED:
                {
                    const char *varname = GetEsVarName( event->es.cat );
                    if( varname )
                        VarListDel( p_input, varname, event->es.id );
                    break;
                }
                case VLC_INPUT_ES_SELECTED:
                {
                    const char *varname = GetEsVarName( event->es.cat );
                    if( varname )
                        VarListSelect( p_input, varname, event->es.id );
                    break;
                }
            }
            break;
        case INPUT_EVENT_TELETEXT:
            switch (event->teletext.action)
            {
                case VLC_INPUT_TELETEXT_ADDED:
                    VarListAdd( p_input, "teletext-es", event->teletext.id,
                                event->teletext.title );
                    break;
                case VLC_INPUT_TELETEXT_DELETED:
                    VarListDel( p_input, "teletext-es", event->teletext.id );
                    break;
                case VLC_INPUT_TELETEXT_SELECTED:
                    VarListSelect( p_input, "teletext-es", event->teletext.id );
                    break;
            }
            break;
        case INPUT_EVENT_RECORD:
            val.b_bool = event->record;
            var_Change( p_input, "record", VLC_VAR_SETVALUE, val );
            break;
        case INPUT_EVENT_ITEM_META:
            break;
        case INPUT_EVENT_ITEM_INFO:
            break;
        case INPUT_EVENT_ITEM_EPG:
            break;
        case INPUT_EVENT_STATISTICS:
            break;
        case INPUT_EVENT_SIGNAL:
            val.f_float = event->signal.quality;
            var_Change( p_input, "signal-quality", VLC_VAR_SETVALUE, val );
            val.f_float = event->signal.strength;
            var_Change( p_input, "signal-strength", VLC_VAR_SETVALUE, val );
            break;
        case INPUT_EVENT_AUDIO_DELAY:
            val.i_int = event->audio_delay;
            var_Change( p_input, "audio-delay", VLC_VAR_SETVALUE, val );
            break;
        case INPUT_EVENT_SUBTITLE_DELAY:
            val.i_int = event->subtitle_delay;
            var_Change( p_input, "spu-delay", VLC_VAR_SETVALUE, val );
            break;
        case INPUT_EVENT_BOOKMARK:
            UpdateBookmarksOption( p_input );
            break;
        case INPUT_EVENT_CACHE:
            val.f_float = event->cache;
            var_Change( p_input, "cache", VLC_VAR_SETVALUE, val );
            break;
        case INPUT_EVENT_AOUT:
            break;
        case INPUT_EVENT_VOUT:
            break;
    }
    Trigger( p_input, event->type );
}

/*****************************************************************************
 * input_LegacyVarInit:
 * Create all control object variables with their callbacks
 *****************************************************************************/
void input_LegacyVarInit ( input_thread_t *p_input )
{
    vlc_value_t val;

    var_Create( p_input, "can-seek", VLC_VAR_BOOL );
    var_SetBool( p_input, "can-seek", true ); /* Fixed later*/

    var_Create( p_input, "can-pause", VLC_VAR_BOOL );
    var_SetBool( p_input, "can-pause", true ); /* Fixed later*/

    var_Create( p_input, "can-rate", VLC_VAR_BOOL );
    var_SetBool( p_input, "can-rate", false );

    var_Create( p_input, "can-rewind", VLC_VAR_BOOL );
    var_SetBool( p_input, "can-rewind", false );

    var_Create( p_input, "can-record", VLC_VAR_BOOL );
    var_SetBool( p_input, "can-record", false ); /* Fixed later*/

    var_Create( p_input, "record", VLC_VAR_BOOL );
    var_SetBool( p_input, "record", false );

    var_Create( p_input, "teletext-es", VLC_VAR_INTEGER );
    var_SetInteger( p_input, "teletext-es", -1 );

    var_Create( p_input, "cache", VLC_VAR_FLOAT );
    var_SetFloat( p_input, "cache", 0.0 );

    var_Create( p_input, "signal-quality", VLC_VAR_FLOAT );
    var_SetFloat( p_input, "signal-quality", -1 );

    var_Create( p_input, "signal-strength", VLC_VAR_FLOAT );
    var_SetFloat( p_input, "signal-strength", -1 );

    var_Create( p_input, "program-scrambled", VLC_VAR_BOOL );
    var_SetBool( p_input, "program-scrambled", false );

    /* State */
    var_Create( p_input, "state", VLC_VAR_INTEGER );
    val.i_int = input_priv(p_input)->i_state;
    var_Change( p_input, "state", VLC_VAR_SETVALUE, val );

    var_Create( p_input, "frame-next", VLC_VAR_VOID );

    /* Position */
    var_Create( p_input, "position",  VLC_VAR_FLOAT );

    /* Time */
    var_Create( p_input, "time", VLC_VAR_INTEGER );
    var_Create( p_input, "time-offset", VLC_VAR_INTEGER );    /* relative */

    /* Bookmark */
    var_Create( p_input, "bookmark", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    var_Change( p_input, "bookmark", VLC_VAR_SETTEXT, _("Bookmark") );

    /* Program */
    var_Get( p_input, "program", &val );
    if( val.i_int <= 0 )
        var_Change( p_input, "program", VLC_VAR_DELCHOICE, val );
    var_Change( p_input, "program", VLC_VAR_SETTEXT, _("Program") );

    /* Programs */
    var_Change( p_input, "programs", VLC_VAR_SETTEXT, _("Programs") );

    /* Title */
    var_Create( p_input, "title", VLC_VAR_INTEGER );
    var_Change( p_input, "title", VLC_VAR_SETTEXT, _("Title") );

    /* Chapter */
    var_Create( p_input, "chapter", VLC_VAR_INTEGER );
    var_Change( p_input, "chapter", VLC_VAR_SETTEXT, _("Chapter") );

    /* Delay */
    var_Create( p_input, "audio-delay", VLC_VAR_INTEGER );
    var_SetInteger( p_input, "audio-delay",
                    VLC_TICK_FROM_MS( var_GetInteger( p_input, "audio-desync" ) ) );
    var_Create( p_input, "spu-delay", VLC_VAR_INTEGER );

    val.i_int = -1;
    /* Video ES */
    var_Create( p_input, "video-es", VLC_VAR_INTEGER );
    var_Change( p_input, "video-es", VLC_VAR_SETVALUE, val );
    var_Change( p_input, "video-es", VLC_VAR_SETTEXT, _("Video Track") );

    /* Audio ES */
    var_Create( p_input, "audio-es", VLC_VAR_INTEGER );
    var_Change( p_input, "audio-es", VLC_VAR_SETVALUE, val );
    var_Change( p_input, "audio-es", VLC_VAR_SETTEXT, _("Audio Track") );

    /* Spu ES */
    var_Create( p_input, "spu-es", VLC_VAR_INTEGER );
    var_Change( p_input, "spu-es", VLC_VAR_SETVALUE, val );
    var_Change( p_input, "spu-es", VLC_VAR_SETTEXT, _("Subtitle Track") );

    var_Create( p_input, "spu-choice", VLC_VAR_INTEGER );
    var_SetInteger( p_input, "spu-choice", -1 );

    var_Create( p_input, "length", VLC_VAR_INTEGER );

    var_Create( p_input, "bit-rate", VLC_VAR_INTEGER );
    var_Create( p_input, "sample-rate", VLC_VAR_INTEGER );

    /* Special "intf-event" variable. */
    var_Create( p_input, "intf-event", VLC_VAR_INTEGER );

    /* Add all callbacks
     * XXX we put callback only in non preparsing mode. We need to create the variable
     * unless someone want to check all var_Get/var_Change return value ... */
    if( !input_priv(p_input)->b_preparsing )
        InputAddCallbacks( p_input, p_input_callbacks );
}

/*****************************************************************************
 * input_LegacyVarNavigation:
 *  Create all remaining control object variables
 *****************************************************************************/
static void input_LegacyVarNavigation( input_thread_t *p_input )
{
    /* Create more command variables */
    if( input_priv(p_input)->i_title > 1 )
    {
        if( var_Type( p_input, "next-title" ) == 0 ) {
            var_Create( p_input, "next-title", VLC_VAR_VOID );
            var_Change( p_input, "next-title", VLC_VAR_SETTEXT,
                        _("Next title") );
            var_AddCallback( p_input, "next-title", TitleCallback, NULL );
        }

        if( var_Type( p_input, "prev-title" ) == 0 ) {
            var_Create( p_input, "prev-title", VLC_VAR_VOID );
            var_Change( p_input, "prev-title", VLC_VAR_SETTEXT,
                        _("Previous title") );
            var_AddCallback( p_input, "prev-title", TitleCallback, NULL );
        }

        if( var_Type( p_input, "menu-title" ) == 0 ) {
            var_Create( p_input, "menu-title", VLC_VAR_VOID );
            var_Change( p_input, "menu-title", VLC_VAR_SETTEXT,
                        _("Menu title") );
            var_AddCallback( p_input, "menu-title", TitleCallback, NULL );
        }

        if( var_Type( p_input, "menu-popup" ) == 0 ) {
            var_Create( p_input, "menu-popup", VLC_VAR_VOID );
            var_Change( p_input, "menu-popup", VLC_VAR_SETTEXT,
                        _("Menu popup") );
            var_AddCallback( p_input, "menu-popup", TitleCallback, NULL );
        }
    }

    /* Create titles and chapters */
    var_Change( p_input, "title", VLC_VAR_CLEARCHOICES );

    for( int i = 0; i < input_priv(p_input)->i_title; i++ )
    {
        vlc_value_t val2, text2;
        char title[sizeof("title ") + 3 * sizeof (int)];

        /* Add Navigation entries */
        sprintf( title, "title %2u", i );
        var_Destroy( p_input, title );
        var_Create( p_input, title, VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
        var_AddCallback( p_input, title,
                         NavigationCallback, (void *)(intptr_t)i );

        char psz_length[MSTRTIME_MAX_SIZE + sizeof(" []")];
        if( input_priv(p_input)->title[i]->i_length > 0 )
        {
            strcpy( psz_length, " [" );
            secstotimestr( &psz_length[2], SEC_FROM_VLC_TICK(input_priv(p_input)->title[i]->i_length) );
            strcat( psz_length, "]" );
        }
        else
            psz_length[0] = '\0';

        char *titlestr;
        if( input_priv(p_input)->title[i]->psz_name == NULL ||
            *input_priv(p_input)->title[i]->psz_name == '\0' )
        {
            if( asprintf( &titlestr, _("Title %i%s"),
                          i + input_priv(p_input)->i_title_offset, psz_length ) == -1 )
                continue;
        }
        else
        {
            if( asprintf( &titlestr, "%s%s",
                          input_priv(p_input)->title[i]->psz_name, psz_length ) == -1 )
                continue;
        }

        /* Add title choice */
        val2.i_int = i;
        var_Change( p_input, "title", VLC_VAR_ADDCHOICE, val2,
                    (const char *)titlestr );

        free( titlestr );

        for( int j = 0; j < input_priv(p_input)->title[i]->i_seekpoint; j++ )
        {
            val2.i_int = j;

            if( input_priv(p_input)->title[i]->seekpoint[j]->psz_name == NULL ||
                *input_priv(p_input)->title[i]->seekpoint[j]->psz_name == '\0' )
            {
                /* Default value */
                if( asprintf( &text2.psz_string, _("Chapter %i"),
                          j + input_priv(p_input)->i_seekpoint_offset ) == -1 )
                    continue;
            }
            else
            {
                text2.psz_string =
                    strdup( input_priv(p_input)->title[i]->seekpoint[j]->psz_name );
            }

            var_Change( p_input, title, VLC_VAR_ADDCHOICE, val2,
                        (const char *)text2.psz_string );
            free( text2.psz_string );
        }

    }
}

/*****************************************************************************
 * input_LegacyVarTitle:
 *  Create all variables for a title
 *****************************************************************************/
static void input_LegacyVarTitle( input_thread_t *p_input, int i_title )
{
    const input_title_t *t = input_priv(p_input)->title[i_title];
    vlc_value_t text;
    int  i;

    /* Create/Destroy command variables */
    if( t->i_seekpoint <= 1 )
    {
        var_Destroy( p_input, "next-chapter" );
        var_Destroy( p_input, "prev-chapter" );
    }
    else if( var_Type( p_input, "next-chapter" ) == 0 )
    {
        var_Create( p_input, "next-chapter", VLC_VAR_VOID );
        var_Change( p_input, "next-chapter", VLC_VAR_SETTEXT,
                    _("Next chapter") );
        var_AddCallback( p_input, "next-chapter", SeekpointCallback, NULL );

        var_Create( p_input, "prev-chapter", VLC_VAR_VOID );
        var_Change( p_input, "prev-chapter", VLC_VAR_SETTEXT,
                    _("Previous chapter") );
        var_AddCallback( p_input, "prev-chapter", SeekpointCallback, NULL );
    }

    /* Build chapter list */
    var_Change( p_input, "chapter", VLC_VAR_CLEARCHOICES );
    for( i = 0; i <  t->i_seekpoint; i++ )
    {
        vlc_value_t val;
        val.i_int = i;

        if( t->seekpoint[i]->psz_name == NULL ||
            *t->seekpoint[i]->psz_name == '\0' )
        {
            /* Default value */
            if( asprintf( &text.psz_string, _("Chapter %i"),
                      i + input_priv(p_input)->i_seekpoint_offset ) == -1 )
                continue;
        }
        else
        {
            text.psz_string = strdup( t->seekpoint[i]->psz_name );
        }

        var_Change( p_input, "chapter", VLC_VAR_ADDCHOICE, val,
                    (const char *)text.psz_string );
        free( text.psz_string );
    }
}

/*****************************************************************************
 * input_ConfigVarInit:
 *  Create all config object variables
 *****************************************************************************/
void input_ConfigVarInit ( input_thread_t *p_input )
{
    /* Create Object Variables for private use only */

    if( !input_priv(p_input)->b_preparsing )
    {
        var_Create( p_input, "video", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
        var_Create( p_input, "audio", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
        var_Create( p_input, "spu", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

        var_Create( p_input, "audio-track", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
        var_Create( p_input, "sub-track", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );

        var_Create( p_input, "audio-language",
                    VLC_VAR_STRING|VLC_VAR_DOINHERIT );
        var_Create( p_input, "sub-language",
                    VLC_VAR_STRING|VLC_VAR_DOINHERIT );
        var_Create( p_input, "menu-language",
                    VLC_VAR_STRING|VLC_VAR_DOINHERIT );

        var_Create( p_input, "audio-track-id",
                    VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
        var_Create( p_input, "sub-track-id",
                    VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );

        var_Create( p_input, "sub-file", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
        var_Create( p_input, "sub-autodetect-file", VLC_VAR_BOOL |
                    VLC_VAR_DOINHERIT );
        var_Create( p_input, "sub-autodetect-path", VLC_VAR_STRING |
                    VLC_VAR_DOINHERIT );
        var_Create( p_input, "sub-autodetect-fuzzy", VLC_VAR_INTEGER |
                    VLC_VAR_DOINHERIT );

        var_Create( p_input, "sout", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
        var_Create( p_input, "sout-all",   VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
        var_Create( p_input, "sout-audio", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
        var_Create( p_input, "sout-video", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
        var_Create( p_input, "sout-spu", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
        var_Create( p_input, "sout-keep",  VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

        var_Create( p_input, "input-repeat",
                    VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
        var_Create( p_input, "start-time", VLC_VAR_FLOAT|VLC_VAR_DOINHERIT );
        var_Create( p_input, "stop-time", VLC_VAR_FLOAT|VLC_VAR_DOINHERIT );
        var_Create( p_input, "run-time", VLC_VAR_FLOAT|VLC_VAR_DOINHERIT );
        var_Create( p_input, "input-fast-seek", VLC_VAR_BOOL|VLC_VAR_DOINHERIT );

        var_Create( p_input, "input-slave",
                    VLC_VAR_STRING | VLC_VAR_DOINHERIT );

        var_Create( p_input, "audio-desync",
                    VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
        var_Create( p_input, "cr-average",
                    VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
        var_Create( p_input, "clock-synchro",
                    VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);

        var_Create( p_input, "bookmarks", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
        var_Create( p_input, "programs", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
        var_Create( p_input, "program", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
        var_Create( p_input, "rate", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT );
    }

    /* */
    var_Create( p_input, "input-record-native", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    /* */
    var_Create( p_input, "access", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_input, "demux", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_input, "demux-filter", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_input, "stream-filter", VLC_VAR_STRING | VLC_VAR_DOINHERIT );

    /* Meta */
    var_Create( p_input, "meta-title", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_input, "meta-author", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_input, "meta-artist", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_input, "meta-genre", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_input, "meta-copyright", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    var_Create( p_input, "meta-description", VLC_VAR_STRING|VLC_VAR_DOINHERIT);
    var_Create( p_input, "meta-date", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_input, "meta-url", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
}

/*****************************************************************************
 * Callbacks managements:
 *****************************************************************************/
static void InputAddCallbacks( input_thread_t *p_input,
                               const vlc_input_callback_t *p_callbacks )
{
    int i;
    for( i = 0; p_callbacks[i].psz_name != NULL; i++ )
        var_AddCallback( p_input,
                         p_callbacks[i].psz_name,
                         p_callbacks[i].callback, NULL );
}

/*****************************************************************************
 * All Callbacks:
 *****************************************************************************/
static int StateCallback( vlc_object_t *p_this, char const *psz_cmd,
                          vlc_value_t oldval, vlc_value_t newval,
                          void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(p_data);

    if( newval.i_int == PLAYING_S || newval.i_int == PAUSE_S )
    {
        input_ControlPushHelper( p_input, INPUT_CONTROL_SET_STATE, &newval );
        return VLC_SUCCESS;
    }

    return VLC_EGENERIC;
}

static int RateCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    VLC_UNUSED(oldval); VLC_UNUSED(p_data); VLC_UNUSED(psz_cmd);

    newval.i_int = INPUT_RATE_DEFAULT / newval.f_float;
    input_ControlPushHelper( p_input, INPUT_CONTROL_SET_RATE, &newval );

    return VLC_SUCCESS;
}

static int PositionCallback( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval,
                             void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;

    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(p_data);

    /* Update "length" for better intf behaviour */
    const vlc_tick_t i_length = var_GetInteger( p_input, "length" );
    if( i_length > 0 && newval.f_float >= 0.f && newval.f_float <= 1.f )
    {
        vlc_value_t val;

        val.i_int = i_length * newval.f_float;
        var_Change( p_input, "time", VLC_VAR_SETVALUE, val );
    }

    input_SetPosition( p_input, newval.f_float,
                       var_GetBool( p_input, "input-fast-seek" ) );
    return VLC_SUCCESS;
}

static int TimeCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(p_data);

    /* Update "position" for better intf behaviour */
    const int64_t i_length = var_GetInteger( p_input, "length" );
    if( i_length > 0 && newval.i_int >= 0 && newval.i_int <= i_length )
    {
        vlc_value_t val;

        val.f_float = (double)newval.i_int/(double)i_length;
        var_Change( p_input, "position", VLC_VAR_SETVALUE, val );
        /*
         * Notify the intf that a new event has been occurred.
         * XXX this is a bit hackish but it's the only way to do it now.
         */
        var_SetInteger( p_input, "intf-event", INPUT_EVENT_POSITION );
    }

    input_SetTime( p_input, newval.i_int,
                   var_GetBool( p_input, "input-fast-seek" ) );
    return VLC_SUCCESS;
}

static int TimeOffsetCallback( vlc_object_t *obj, char const *varname,
                               vlc_value_t prev, vlc_value_t cur, void *data )
{
    VLC_UNUSED(varname); VLC_UNUSED(prev); VLC_UNUSED(data);

    int64_t i_time = var_GetInteger( obj, "time" ) + cur.i_int;
    if( i_time < 0 )
        i_time = 0;
    var_SetInteger( obj, "time", i_time );
    return VLC_SUCCESS;
}

static int ProgramCallback( vlc_object_t *p_this, char const *psz_cmd,
                            vlc_value_t oldval, vlc_value_t newval,
                            void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(p_data);

    input_ControlPushHelper( p_input, INPUT_CONTROL_SET_PROGRAM, &newval );

    return VLC_SUCCESS;
}

static int TitleCallback( vlc_object_t *p_this, char const *psz_cmd,
                          vlc_value_t oldval, vlc_value_t newval,
                          void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    vlc_value_t val;
    size_t count;
    VLC_UNUSED(oldval); VLC_UNUSED(p_data);

    if( !strcmp( psz_cmd, "next-title" ) )
    {
        input_ControlPushHelper( p_input, INPUT_CONTROL_SET_TITLE_NEXT, NULL );

        val.i_int = var_GetInteger( p_input, "title" ) + 1;
        var_Change( p_input, "title", VLC_VAR_CHOICESCOUNT, &count );
        if( (size_t)val.i_int < count )
            var_Change( p_input, "title", VLC_VAR_SETVALUE, val );
    }
    else if( !strcmp( psz_cmd, "prev-title" ) )
    {
        input_ControlPushHelper( p_input, INPUT_CONTROL_SET_TITLE_PREV, NULL );

        val.i_int = var_GetInteger( p_input, "title" ) - 1;
        if( val.i_int >= 0 )
            var_Change( p_input, "title", VLC_VAR_SETVALUE, val );
    }
    else if( !strcmp( psz_cmd, "menu-title" ) )
    {
        input_ControlPushHelper( p_input, INPUT_CONTROL_NAV_MENU, NULL );
    }
    else if( !strcmp( psz_cmd, "menu-popup" ) )
    {
        input_ControlPushHelper( p_input, INPUT_CONTROL_NAV_POPUP, NULL );
    }
    else
    {
        input_ControlPushHelper( p_input, INPUT_CONTROL_SET_TITLE, &newval );
    }

    return VLC_SUCCESS;
}

static int SeekpointCallback( vlc_object_t *p_this, char const *psz_cmd,
                              vlc_value_t oldval, vlc_value_t newval,
                              void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    vlc_value_t val;
    size_t count;
    VLC_UNUSED(oldval); VLC_UNUSED(p_data);

    if( !strcmp( psz_cmd, "next-chapter" ) )
    {
        input_ControlPushHelper( p_input, INPUT_CONTROL_SET_SEEKPOINT_NEXT, NULL );

        val.i_int = var_GetInteger( p_input, "chapter" ) + 1;
        var_Change( p_input, "chapter", VLC_VAR_CHOICESCOUNT, &count );
        if( (size_t)val.i_int < count )
            var_Change( p_input, "chapter", VLC_VAR_SETVALUE, val );
    }
    else if( !strcmp( psz_cmd, "prev-chapter" ) )
    {
        input_ControlPushHelper( p_input, INPUT_CONTROL_SET_SEEKPOINT_PREV, NULL );

        val.i_int = var_GetInteger( p_input, "chapter" ) - 1;
        if( val.i_int >= 0 )
            var_Change( p_input, "chapter", VLC_VAR_SETVALUE, val );
    }
    else
    {
        input_ControlPushHelper( p_input, INPUT_CONTROL_SET_SEEKPOINT, &newval );
    }

    return VLC_SUCCESS;
}

static int NavigationCallback( vlc_object_t *p_this, char const *psz_cmd,
                               vlc_value_t oldval, vlc_value_t newval,
                               void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    vlc_value_t     val;
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval);

    /* Issue a title change */
    val.i_int = (intptr_t)p_data;
    input_ControlPushHelper( p_input, INPUT_CONTROL_SET_TITLE, &val );

    var_Change( p_input, "title", VLC_VAR_SETVALUE, val );

    /* And a chapter change */
    input_ControlPushHelper( p_input, INPUT_CONTROL_SET_SEEKPOINT, &newval );

    var_Change( p_input, "chapter", VLC_VAR_SETVALUE, newval );

    return VLC_SUCCESS;
}

static int EsVideoCallback( vlc_object_t *p_this, char const *psz_cmd,
                            vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    VLC_UNUSED( psz_cmd); VLC_UNUSED( oldval ); VLC_UNUSED( p_data );

    if( newval.i_int < 0 )
        newval.i_int = -VIDEO_ES; /* disable video es */

    input_ControlPushHelper( p_input, INPUT_CONTROL_SET_ES, &newval );

    return VLC_SUCCESS;
}

static int EsAudioCallback( vlc_object_t *p_this, char const *psz_cmd,
                            vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    VLC_UNUSED( psz_cmd); VLC_UNUSED( oldval ); VLC_UNUSED( p_data );

    if( newval.i_int < 0 )
        newval.i_int = -AUDIO_ES; /* disable audio es */

    input_ControlPushHelper( p_input, INPUT_CONTROL_SET_ES, &newval );

    return VLC_SUCCESS;
}

static int EsSpuCallback( vlc_object_t *p_this, char const *psz_cmd,
                           vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    VLC_UNUSED( psz_cmd); VLC_UNUSED( oldval ); VLC_UNUSED( p_data );

    if( newval.i_int < 0 )
        newval.i_int = -SPU_ES; /* disable spu es */

    input_ControlPushHelper( p_input, INPUT_CONTROL_SET_ES, &newval );

    return VLC_SUCCESS;
}

static int EsDelayCallback ( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    VLC_UNUSED(oldval); VLC_UNUSED(p_data);

    if( !strcmp( psz_cmd, "audio-delay" ) )
    {
        input_ControlPushHelper( p_input, INPUT_CONTROL_SET_AUDIO_DELAY, &newval );
    }
    else if( !strcmp( psz_cmd, "spu-delay" ) )
    {
        input_ControlPushHelper( p_input, INPUT_CONTROL_SET_SPU_DELAY, &newval );
    }
    return VLC_SUCCESS;
}

static int BookmarkCallback( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval,
                             void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(p_data);

    input_ControlPushHelper( p_input, INPUT_CONTROL_SET_BOOKMARK, &newval );

    return VLC_SUCCESS;
}

static int RecordCallback( vlc_object_t *p_this, char const *psz_cmd,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(p_data);

    input_ControlPushHelper( p_input, INPUT_CONTROL_SET_RECORD_STATE, &newval );

    return VLC_SUCCESS;
}

static int FrameNextCallback( vlc_object_t *p_this, char const *psz_cmd,
                              vlc_value_t oldval, vlc_value_t newval,
                              void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(p_data);
    VLC_UNUSED(newval);

    input_ControlPushHelper( p_input, INPUT_CONTROL_SET_FRAME_NEXT, NULL );

    return VLC_SUCCESS;
}

