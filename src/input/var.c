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
static int ProgramCallback ( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval, void * );
static int TitleCallback   ( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval, void * );
static int SeekpointCallback( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval, void * );
static int NavigationCallback( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval, void * );
static int ESCallback      ( vlc_object_t *p_this, char const *psz_cmd,
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

typedef struct
{
    const char *psz_name;
    vlc_callback_t callback;
} vlc_input_callback_t;

static void InputAddCallbacks( input_thread_t *, const vlc_input_callback_t * );
static void InputDelCallbacks( input_thread_t *, const vlc_input_callback_t * );

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
    CALLBACK( "position-offset", PositionCallback ),
    CALLBACK( "time", TimeCallback ),
    CALLBACK( "time-offset", TimeCallback ),
    CALLBACK( "bookmark", BookmarkCallback ),
    CALLBACK( "program", ProgramCallback ),
    CALLBACK( "title", TitleCallback ),
    CALLBACK( "chapter", SeekpointCallback ),
    CALLBACK( "audio-delay", EsDelayCallback ),
    CALLBACK( "spu-delay", EsDelayCallback ),
    CALLBACK( "video-es", ESCallback ),
    CALLBACK( "audio-es", ESCallback ),
    CALLBACK( "spu-es", ESCallback ),
    CALLBACK( "record", RecordCallback ),
    CALLBACK( "frame-next", FrameNextCallback ),

    CALLBACK( NULL, NULL )
};
static const vlc_input_callback_t p_input_navigation_callbacks[] =
{
    CALLBACK( "next-title", TitleCallback ),
    CALLBACK( "prev-title", TitleCallback ),

    CALLBACK( NULL, NULL )
};
static const vlc_input_callback_t p_input_title_callbacks[] =
{
    CALLBACK( "next-chapter", SeekpointCallback ),
    CALLBACK( "prev-chapter", SeekpointCallback ),

    CALLBACK( NULL, NULL )
};
#undef CALLBACK

/*****************************************************************************
 * input_ControlVarInit:
 *  Create all control object variables with their callbacks
 *****************************************************************************/
void input_ControlVarInit ( input_thread_t *p_input )
{
    vlc_value_t val, text;

    /* State */
    var_Create( p_input, "state", VLC_VAR_INTEGER );
    val.i_int = p_input->p->i_state;
    var_Change( p_input, "state", VLC_VAR_SETVALUE, &val, NULL );

    /* Rate */
    var_Create( p_input, "rate", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT );

    var_Create( p_input, "frame-next", VLC_VAR_VOID );

    /* Position */
    var_Create( p_input, "position",  VLC_VAR_FLOAT );
    var_Create( p_input, "position-offset",  VLC_VAR_FLOAT );
    val.f_float = 0.0;
    var_Change( p_input, "position", VLC_VAR_SETVALUE, &val, NULL );

    /* Time */
    var_Create( p_input, "time",  VLC_VAR_TIME );
    var_Create( p_input, "time-offset",  VLC_VAR_TIME );    /* relative */
    val.i_time = 0;
    var_Change( p_input, "time", VLC_VAR_SETVALUE, &val, NULL );

    /* Bookmark */
    var_Create( p_input, "bookmark", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE |
                VLC_VAR_ISCOMMAND );
    val.psz_string = _("Bookmark");
    var_Change( p_input, "bookmark", VLC_VAR_SETTEXT, &val, NULL );

    /* Program */
    var_Create( p_input, "program", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE |
                VLC_VAR_DOINHERIT );
    var_Get( p_input, "program", &val );
    if( val.i_int <= 0 )
        var_Change( p_input, "program", VLC_VAR_DELCHOICE, &val, NULL );
    text.psz_string = _("Program");
    var_Change( p_input, "program", VLC_VAR_SETTEXT, &text, NULL );

    /* Programs */
    var_Create( p_input, "programs", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    text.psz_string = _("Programs");
    var_Change( p_input, "programs", VLC_VAR_SETTEXT, &text, NULL );

    /* Title */
    var_Create( p_input, "title", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
    text.psz_string = _("Title");
    var_Change( p_input, "title", VLC_VAR_SETTEXT, &text, NULL );

    /* Chapter */
    var_Create( p_input, "chapter", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
    text.psz_string = _("Chapter");
    var_Change( p_input, "chapter", VLC_VAR_SETTEXT, &text, NULL );

    /* Navigation The callback is added after */
    var_Create( p_input, "navigation", VLC_VAR_VARIABLE | VLC_VAR_HASCHOICE );
    text.psz_string = _("Navigation");
    var_Change( p_input, "navigation", VLC_VAR_SETTEXT, &text, NULL );

    /* Delay */
    var_Create( p_input, "audio-delay", VLC_VAR_TIME );
    val.i_time = INT64_C(1000) * var_GetInteger( p_input, "audio-desync" );
    var_Change( p_input, "audio-delay", VLC_VAR_SETVALUE, &val, NULL );
    var_Create( p_input, "spu-delay", VLC_VAR_TIME );
    val.i_time = 0;
    var_Change( p_input, "spu-delay", VLC_VAR_SETVALUE, &val, NULL );

    /* Video ES */
    var_Create( p_input, "video-es", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
    text.psz_string = _("Video Track");
    var_Change( p_input, "video-es", VLC_VAR_SETTEXT, &text, NULL );

    /* Audio ES */
    var_Create( p_input, "audio-es", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
    text.psz_string = _("Audio Track");
    var_Change( p_input, "audio-es", VLC_VAR_SETTEXT, &text, NULL );

    /* Spu ES */
    var_Create( p_input, "spu-es", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
    text.psz_string = _("Subtitle Track");
    var_Change( p_input, "spu-es", VLC_VAR_SETTEXT, &text, NULL );

    /* Special read only objects variables for intf */
    var_Create( p_input, "bookmarks", VLC_VAR_STRING | VLC_VAR_DOINHERIT );

    var_Create( p_input, "length",  VLC_VAR_TIME );
    val.i_time = 0;
    var_Change( p_input, "length", VLC_VAR_SETVALUE, &val, NULL );

    var_Create( p_input, "bit-rate", VLC_VAR_INTEGER );
    var_Create( p_input, "sample-rate", VLC_VAR_INTEGER );

    if( !p_input->b_preparsing )
    {
        /* Special "intf-event" variable. */
        var_Create( p_input, "intf-event", VLC_VAR_INTEGER );
    }

    /* Add all callbacks
     * XXX we put callback only in non preparsing mode. We need to create the variable
     * unless someone want to check all var_Get/var_Change return value ... */
    if( !p_input->b_preparsing )
        InputAddCallbacks( p_input, p_input_callbacks );
}

/*****************************************************************************
 * input_ControlVarStop:
 *****************************************************************************/
void input_ControlVarStop( input_thread_t *p_input )
{
    if( !p_input->b_preparsing )
        InputDelCallbacks( p_input, p_input_callbacks );

    if( p_input->p->i_title > 0 )
    {
        char name[sizeof("title ") + 5 ];
        int i;

        InputDelCallbacks( p_input, p_input_navigation_callbacks );
        InputDelCallbacks( p_input, p_input_title_callbacks );

        for( i = 0; i < p_input->p->i_title; i++ )
        {
            snprintf( name, sizeof(name), "title %2i", i );
            var_DelCallback( p_input, name, NavigationCallback, (void *)(intptr_t)i );
        }
    }
}

/*****************************************************************************
 * input_ControlVarNavigation:
 *  Create all remaining control object variables
 *****************************************************************************/
void input_ControlVarNavigation( input_thread_t *p_input )
{
    vlc_value_t val, text;
    int  i;

    /* Create more command variables */
    if( p_input->p->i_title > 1 )
    {
        var_Create( p_input, "next-title", VLC_VAR_VOID );
        text.psz_string = _("Next title");
        var_Change( p_input, "next-title", VLC_VAR_SETTEXT, &text, NULL );
        var_AddCallback( p_input, "next-title", TitleCallback, NULL );

        var_Create( p_input, "prev-title", VLC_VAR_VOID );
        text.psz_string = _("Previous title");
        var_Change( p_input, "prev-title", VLC_VAR_SETTEXT, &text, NULL );
        var_AddCallback( p_input, "prev-title", TitleCallback, NULL );
    }

    /* Create title and navigation */
    val.psz_string = malloc( sizeof("title ") + 5 );
    if( !val.psz_string )
        return;

    for( i = 0; i < p_input->p->i_title; i++ )
    {
        vlc_value_t val2, text2;
        int j;

        /* Add Navigation entries */
        sprintf( val.psz_string,  "title %2i", i );
        var_Destroy( p_input, val.psz_string );
        var_Create( p_input, val.psz_string,
                    VLC_VAR_INTEGER|VLC_VAR_HASCHOICE|VLC_VAR_ISCOMMAND );
        var_AddCallback( p_input, val.psz_string,
                         NavigationCallback, (void *)(intptr_t)i );

        char psz_length[MSTRTIME_MAX_SIZE + sizeof(" []")] = "";
        if( p_input->p->title[i]->i_length > 0 )
        {
            strcpy( psz_length, " [" );
            secstotimestr( &psz_length[2], p_input->p->title[i]->i_length / CLOCK_FREQ );
            strcat( psz_length, "]" );
        }

        if( p_input->p->title[i]->psz_name == NULL ||
            *p_input->p->title[i]->psz_name == '\0' )
        {
            if( asprintf( &text.psz_string, _("Title %i%s"),
                          i + p_input->p->i_title_offset, psz_length ) == -1 )
                continue;
        }
        else
        {
            if( asprintf( &text.psz_string, "%s%s",
                          p_input->p->title[i]->psz_name, psz_length ) == -1 )
                continue;
        }
        var_Change( p_input, "navigation", VLC_VAR_ADDCHOICE, &val, &text );

        /* Add title choice */
        val2.i_int = i;
        var_Change( p_input, "title", VLC_VAR_ADDCHOICE, &val2, &text );

        free( text.psz_string );

        for( j = 0; j < p_input->p->title[i]->i_seekpoint; j++ )
        {
            val2.i_int = j;

            if( p_input->p->title[i]->seekpoint[j]->psz_name == NULL ||
                *p_input->p->title[i]->seekpoint[j]->psz_name == '\0' )
            {
                /* Default value */
                if( asprintf( &text2.psz_string, _("Chapter %i"),
                          j + p_input->p->i_seekpoint_offset ) == -1 )
                    continue;
            }
            else
            {
                text2.psz_string =
                    strdup( p_input->p->title[i]->seekpoint[j]->psz_name );
            }

            var_Change( p_input, val.psz_string, VLC_VAR_ADDCHOICE,
                        &val2, &text2 );
            free( text2.psz_string );
        }

    }
    free( val.psz_string );
}

/*****************************************************************************
 * input_ControlVarTitle:
 *  Create all variables for a title
 *****************************************************************************/
void input_ControlVarTitle( input_thread_t *p_input, int i_title )
{
    input_title_t *t = p_input->p->title[i_title];
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
        text.psz_string = _("Next chapter");
        var_Change( p_input, "next-chapter", VLC_VAR_SETTEXT, &text, NULL );
        var_AddCallback( p_input, "next-chapter", SeekpointCallback, NULL );

        var_Create( p_input, "prev-chapter", VLC_VAR_VOID );
        text.psz_string = _("Previous chapter");
        var_Change( p_input, "prev-chapter", VLC_VAR_SETTEXT, &text, NULL );
        var_AddCallback( p_input, "prev-chapter", SeekpointCallback, NULL );
    }

    /* Build chapter list */
    var_Change( p_input, "chapter", VLC_VAR_CLEARCHOICES, NULL, NULL );
    for( i = 0; i <  t->i_seekpoint; i++ )
    {
        vlc_value_t val;
        val.i_int = i;

        if( t->seekpoint[i]->psz_name == NULL ||
            *t->seekpoint[i]->psz_name == '\0' )
        {
            /* Default value */
            if( asprintf( &text.psz_string, _("Chapter %i"),
                      i + p_input->p->i_seekpoint_offset ) == -1 )
                continue;
        }
        else
        {
            text.psz_string = strdup( t->seekpoint[i]->psz_name );
        }

        var_Change( p_input, "chapter", VLC_VAR_ADDCHOICE, &val, &text );
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

    if( !p_input->b_preparsing )
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
    }

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

    var_Create( p_input, "signal-quality", VLC_VAR_FLOAT );
    var_SetFloat( p_input, "signal-quality", -1 );

    var_Create( p_input, "signal-strength", VLC_VAR_FLOAT );
    var_SetFloat( p_input, "signal-strength", -1 );

    var_Create( p_input, "program-scrambled", VLC_VAR_BOOL );
    var_SetBool( p_input, "program-scrambled", false );

    var_Create( p_input, "cache", VLC_VAR_FLOAT );
    var_SetFloat( p_input, "cache", 0.0 );

    /* */
    var_Create( p_input, "input-record-native", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    /* */
    var_Create( p_input, "access", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_input, "demux", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
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

static void InputDelCallbacks( input_thread_t *p_input,
                               const vlc_input_callback_t *p_callbacks )
{
    int i;
    for( i = 0; p_callbacks[i].psz_name != NULL; i++ )
        var_DelCallback( p_input,
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
        input_ControlPush( p_input, INPUT_CONTROL_SET_STATE, &newval );
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
    input_ControlPush( p_input, INPUT_CONTROL_SET_RATE, &newval );

    return VLC_SUCCESS;
}

static int PositionCallback( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval,
                             void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    VLC_UNUSED(oldval); VLC_UNUSED(p_data);

    if( !strcmp( psz_cmd, "position-offset" ) )
    {
        float f_position = var_GetFloat( p_input, "position" ) + newval.f_float;
        if( f_position < 0.0 )
            f_position = 0.0;
        else if( f_position > 1.0 )
            f_position = 1.0;
        var_SetFloat( p_this, "position", f_position );
    }
    else
    {
        /* Update "length" for better intf behavour */
        const mtime_t i_length = var_GetTime( p_input, "length" );
        if( i_length > 0 && newval.f_float >= 0.0 && newval.f_float <= 1.0 )
        {
            vlc_value_t val;

            val.i_time = i_length * newval.f_float;
            var_Change( p_input, "time", VLC_VAR_SETVALUE, &val, NULL );
        }

        /* */
        input_ControlPush( p_input, INPUT_CONTROL_SET_POSITION, &newval );
    }
    return VLC_SUCCESS;
}

static int TimeCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    VLC_UNUSED(oldval); VLC_UNUSED(p_data);

    if( !strcmp( psz_cmd, "time-offset" ) )
    {
        mtime_t i_time = var_GetTime( p_input, "time" ) + newval.i_time;
        if( i_time < 0 )
            i_time = 0;
        var_SetTime( p_this, "time", i_time );
    }
    else
    {
        /* Update "position" for better intf behavour */
        const mtime_t i_length = var_GetTime( p_input, "length" );
        if( i_length > 0 && newval.i_time >= 0 && newval.i_time <= i_length )
        {
            vlc_value_t val;

            val.f_float = (double)newval.i_time/(double)i_length;
            var_Change( p_input, "position", VLC_VAR_SETVALUE, &val, NULL );
            /*
             * Notify the intf that a new event has been occurred.
             * XXX this is a bit hackish but it's the only way to do it now.
             */
            var_SetInteger( p_input, "intf-event", INPUT_EVENT_POSITION );
        }

        /* */
        input_ControlPush( p_input, INPUT_CONTROL_SET_TIME, &newval );
    }
    return VLC_SUCCESS;
}

static int ProgramCallback( vlc_object_t *p_this, char const *psz_cmd,
                            vlc_value_t oldval, vlc_value_t newval,
                            void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(p_data);

    input_ControlPush( p_input, INPUT_CONTROL_SET_PROGRAM, &newval );

    return VLC_SUCCESS;
}

static int TitleCallback( vlc_object_t *p_this, char const *psz_cmd,
                          vlc_value_t oldval, vlc_value_t newval,
                          void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    vlc_value_t val, count;
    VLC_UNUSED(oldval); VLC_UNUSED(p_data);

    if( !strcmp( psz_cmd, "next-title" ) )
    {
        input_ControlPush( p_input, INPUT_CONTROL_SET_TITLE_NEXT, NULL );

        val.i_int = var_GetInteger( p_input, "title" ) + 1;
        var_Change( p_input, "title", VLC_VAR_CHOICESCOUNT, &count, NULL );
        if( val.i_int < count.i_int )
            var_Change( p_input, "title", VLC_VAR_SETVALUE, &val, NULL );
    }
    else if( !strcmp( psz_cmd, "prev-title" ) )
    {
        input_ControlPush( p_input, INPUT_CONTROL_SET_TITLE_PREV, NULL );

        val.i_int = var_GetInteger( p_input, "title" ) - 1;
        if( val.i_int >= 0 )
            var_Change( p_input, "title", VLC_VAR_SETVALUE, &val, NULL );
    }
    else
    {
        input_ControlPush( p_input, INPUT_CONTROL_SET_TITLE, &newval );
    }

    return VLC_SUCCESS;
}

static int SeekpointCallback( vlc_object_t *p_this, char const *psz_cmd,
                              vlc_value_t oldval, vlc_value_t newval,
                              void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    vlc_value_t val, count;
    VLC_UNUSED(oldval); VLC_UNUSED(p_data);

    if( !strcmp( psz_cmd, "next-chapter" ) )
    {
        input_ControlPush( p_input, INPUT_CONTROL_SET_SEEKPOINT_NEXT, NULL );

        val.i_int = var_GetInteger( p_input, "chapter" ) + 1;
        var_Change( p_input, "chapter", VLC_VAR_CHOICESCOUNT, &count, NULL );
        if( val.i_int < count.i_int )
            var_Change( p_input, "chapter", VLC_VAR_SETVALUE, &val, NULL );
    }
    else if( !strcmp( psz_cmd, "prev-chapter" ) )
    {
        input_ControlPush( p_input, INPUT_CONTROL_SET_SEEKPOINT_PREV, NULL );

        val.i_int = var_GetInteger( p_input, "chapter" ) - 1;
        if( val.i_int >= 0 )
            var_Change( p_input, "chapter", VLC_VAR_SETVALUE, &val, NULL );
    }
    else
    {
        input_ControlPush( p_input, INPUT_CONTROL_SET_SEEKPOINT, &newval );
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
    input_ControlPush( p_input, INPUT_CONTROL_SET_TITLE, &val );

    var_Change( p_input, "title", VLC_VAR_SETVALUE, &val, NULL );

    /* And a chapter change */
    input_ControlPush( p_input, INPUT_CONTROL_SET_SEEKPOINT, &newval );

    var_Change( p_input, "chapter", VLC_VAR_SETVALUE, &newval, NULL );

    return VLC_SUCCESS;
}

static int ESCallback( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    VLC_UNUSED(oldval); VLC_UNUSED(p_data);

    if( newval.i_int < 0 )
    {
        vlc_value_t v;
        /* Hack */
        if( !strcmp( psz_cmd, "audio-es" ) )
            v.i_int = -AUDIO_ES;
        else if( !strcmp( psz_cmd, "video-es" ) )
            v.i_int = -VIDEO_ES;
        else if( !strcmp( psz_cmd, "spu-es" ) )
            v.i_int = -SPU_ES;
        else
            v.i_int = 0;
        if( v.i_int != 0 )
            input_ControlPush( p_input, INPUT_CONTROL_SET_ES, &v );
    }
    else
    {
        input_ControlPush( p_input, INPUT_CONTROL_SET_ES, &newval );
    }

    return VLC_SUCCESS;
}

static int EsDelayCallback ( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    VLC_UNUSED(oldval); VLC_UNUSED(p_data);

    if( !strcmp( psz_cmd, "audio-delay" ) )
    {
        input_ControlPush( p_input, INPUT_CONTROL_SET_AUDIO_DELAY, &newval );
    }
    else if( !strcmp( psz_cmd, "spu-delay" ) )
    {
        input_ControlPush( p_input, INPUT_CONTROL_SET_SPU_DELAY, &newval );
    }
    return VLC_SUCCESS;
}

static int BookmarkCallback( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval,
                             void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(p_data);

    input_ControlPush( p_input, INPUT_CONTROL_SET_BOOKMARK, &newval );

    return VLC_SUCCESS;
}

static int RecordCallback( vlc_object_t *p_this, char const *psz_cmd,
                           vlc_value_t oldval, vlc_value_t newval,
                           void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(p_data);

    input_ControlPush( p_input, INPUT_CONTROL_SET_RECORD_STATE, &newval );

    return VLC_SUCCESS;
}

static int FrameNextCallback( vlc_object_t *p_this, char const *psz_cmd,
                              vlc_value_t oldval, vlc_value_t newval,
                              void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(p_data);
    VLC_UNUSED(newval);

    input_ControlPush( p_input, INPUT_CONTROL_SET_FRAME_NEXT, NULL );

    return VLC_SUCCESS;
}

