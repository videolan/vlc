/*****************************************************************************
 * var.c: object variables for input thread
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>
#include <vlc/vlc.h>
#include <vlc/input.h>

#include "input_internal.h"

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
void input_ControlVarInit ( input_thread_t * );
void input_ControlVarClean( input_thread_t * );
void input_ControlVarNavigation( input_thread_t * );
void input_ControlVarTitle( input_thread_t *p_input, int i_title );

void input_ConfigVarInit ( input_thread_t *p_input );

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

/*****************************************************************************
 * input_ControlVarInit:
 *  Create all control object variables with their callbacks
 *****************************************************************************/
void input_ControlVarInit ( input_thread_t *p_input )
{
    vlc_value_t val, text;

    /* State */
    var_Create( p_input, "state", VLC_VAR_INTEGER );
    val.i_int = p_input->i_state;
    var_Change( p_input, "state", VLC_VAR_SETVALUE, &val, NULL );
    var_AddCallback( p_input, "state", StateCallback, NULL );

    /* Rate */
    var_Create( p_input, "rate", VLC_VAR_INTEGER );
    val.i_int = p_input->i_rate;
    var_Change( p_input, "rate", VLC_VAR_SETVALUE, &val, NULL );
    var_AddCallback( p_input, "rate", RateCallback, NULL );

    var_Create( p_input, "rate-slower", VLC_VAR_VOID );
    var_AddCallback( p_input, "rate-slower", RateCallback, NULL );

    var_Create( p_input, "rate-faster", VLC_VAR_VOID );
    var_AddCallback( p_input, "rate-faster", RateCallback, NULL );

    /* Position */
    var_Create( p_input, "position",  VLC_VAR_FLOAT );
    var_Create( p_input, "position-offset",  VLC_VAR_FLOAT );
    val.f_float = 0.0;
    var_Change( p_input, "position", VLC_VAR_SETVALUE, &val, NULL );
    var_AddCallback( p_input, "position", PositionCallback, NULL );
    var_AddCallback( p_input, "position-offset", PositionCallback, NULL );

    /* Time */
    var_Create( p_input, "time",  VLC_VAR_TIME );
    var_Create( p_input, "time-offset",  VLC_VAR_TIME );    /* relative */
    val.i_time = 0;
    var_Change( p_input, "time", VLC_VAR_SETVALUE, &val, NULL );
    var_AddCallback( p_input, "time", TimeCallback, NULL );
    var_AddCallback( p_input, "time-offset", TimeCallback, NULL );

    /* Bookmark */
    var_Create( p_input, "bookmark", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE |
                VLC_VAR_ISCOMMAND );
    val.psz_string = _("Bookmark");
    var_Change( p_input, "bookmark", VLC_VAR_SETTEXT, &val, NULL );
    var_AddCallback( p_input, "bookmark", BookmarkCallback, NULL );

    /* Program */
    var_Create( p_input, "program", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE |
                VLC_VAR_DOINHERIT );
    var_Get( p_input, "program", &val );
    if( val.i_int <= 0 )
        var_Change( p_input, "program", VLC_VAR_DELCHOICE, &val, NULL );
    text.psz_string = _("Program");
    var_Change( p_input, "program", VLC_VAR_SETTEXT, &text, NULL );
    var_AddCallback( p_input, "program", ProgramCallback, NULL );

    /* Programs */
    var_Create( p_input, "programs", VLC_VAR_LIST | VLC_VAR_DOINHERIT );
    text.psz_string = _("Programs");
    var_Change( p_input, "programs", VLC_VAR_SETTEXT, &text, NULL );

    /* Title */
    var_Create( p_input, "title", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
    text.psz_string = _("Title");
    var_Change( p_input, "title", VLC_VAR_SETTEXT, &text, NULL );
    var_AddCallback( p_input, "title", TitleCallback, NULL );

    /* Chapter */
    var_Create( p_input, "chapter", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
    text.psz_string = _("Chapter");
    var_Change( p_input, "chapter", VLC_VAR_SETTEXT, &text, NULL );
    var_AddCallback( p_input, "chapter", SeekpointCallback, NULL );

    /* Navigation The callback is added after */
    var_Create( p_input, "navigation", VLC_VAR_VARIABLE | VLC_VAR_HASCHOICE );
    text.psz_string = _("Navigation");
    var_Change( p_input, "navigation", VLC_VAR_SETTEXT, &text, NULL );

    /* Delay */
    var_Create( p_input, "audio-delay", VLC_VAR_TIME );
    val.i_time = 0;
    var_Change( p_input, "audio-delay", VLC_VAR_SETVALUE, &val, NULL );
    var_AddCallback( p_input, "audio-delay", EsDelayCallback, NULL );
    var_Create( p_input, "spu-delay", VLC_VAR_TIME );
    val.i_time = 0;
    var_Change( p_input, "spu-delay", VLC_VAR_SETVALUE, &val, NULL );
    var_AddCallback( p_input, "spu-delay", EsDelayCallback, NULL );

    /* Video ES */
    var_Create( p_input, "video-es", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
    text.psz_string = _("Video Track");
    var_Change( p_input, "video-es", VLC_VAR_SETTEXT, &text, NULL );
    var_AddCallback( p_input, "video-es", ESCallback, NULL );

    /* Audio ES */
    var_Create( p_input, "audio-es", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
    text.psz_string = _("Audio Track");
    var_Change( p_input, "audio-es", VLC_VAR_SETTEXT, &text, NULL );
    var_AddCallback( p_input, "audio-es", ESCallback, NULL );

    /* Spu ES */
    var_Create( p_input, "spu-es", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE );
    text.psz_string = _("Subtitles Track");
    var_Change( p_input, "spu-es", VLC_VAR_SETTEXT, &text, NULL );
    var_AddCallback( p_input, "spu-es", ESCallback, NULL );

    /* Special read only objects variables for intf */
    var_Create( p_input, "bookmarks", VLC_VAR_STRING | VLC_VAR_DOINHERIT );

    var_Create( p_input, "length",  VLC_VAR_TIME );
    val.i_time = 0;
    var_Change( p_input, "length", VLC_VAR_SETVALUE, &val, NULL );

    /* Special "intf-change" variable, it allows intf to set up a callback
     * to be notified of some changes.
     * TODO list all changes warn by this callbacks */
    var_Create( p_input, "intf-change", VLC_VAR_BOOL );
    var_SetBool( p_input, "intf-change", VLC_TRUE );

   /* item-change variable */
    var_Create( p_input, "item-change", VLC_VAR_INTEGER );
}

/*****************************************************************************
 * input_ControlVarClean:
 *****************************************************************************/
void input_ControlVarClean( input_thread_t *p_input )
{
    var_Destroy( p_input, "state" );
    var_Destroy( p_input, "rate" );
    var_Destroy( p_input, "rate-slower" );
    var_Destroy( p_input, "rate-faster" );
    var_Destroy( p_input, "position" );
    var_Destroy( p_input, "position-offset" );
    var_Destroy( p_input, "time" );
    var_Destroy( p_input, "time-offset" );

    var_Destroy( p_input, "audio-delay" );
    var_Destroy( p_input, "spu-delay" );

    var_Destroy( p_input, "bookmark" );

    var_Destroy( p_input, "program" );
    if( p_input->i_title > 1 )
    {
        /* TODO Destroy sub navigation var ? */

        var_Destroy( p_input, "next-title" );
        var_Destroy( p_input, "prev-title" );
    }
    if( p_input->i_title > 0 )
    {
        /* FIXME title > 0 doesn't mean current title has more than 1 seekpoint */
        var_Destroy( p_input, "next-chapter" );
        var_Destroy( p_input, "prev-chapter" );
    }
    var_Destroy( p_input, "title" );
    var_Destroy( p_input, "chapter" );
    var_Destroy( p_input, "navigation" );

    var_Destroy( p_input, "video-es" );
    var_Destroy( p_input, "audio-es" );
    var_Destroy( p_input, "spu-es" );

    var_Destroy( p_input, "bookmarks" );
    var_Destroy( p_input, "length" );

    var_Destroy( p_input, "intf-change" );
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
    if( p_input->i_title > 1 )
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
    for( i = 0; i < p_input->i_title; i++ )
    {
        vlc_value_t val2, text, text2;
        int j;

        /* Add Navigation entries */
        sprintf( val.psz_string,  "title %2i", i );
        var_Destroy( p_input, val.psz_string );
        var_Create( p_input, val.psz_string,
                    VLC_VAR_INTEGER|VLC_VAR_HASCHOICE|VLC_VAR_ISCOMMAND );
        var_AddCallback( p_input, val.psz_string,
                         NavigationCallback, (void *)i );

        if( p_input->title[i]->psz_name == NULL ||
            *p_input->title[i]->psz_name == '\0' )
        {
            asprintf( &text.psz_string, _("Title %i"),
                      i + p_input->i_title_offset );
        }
        else
        {
            text.psz_string = strdup( p_input->title[i]->psz_name );
        }
        var_Change( p_input, "navigation", VLC_VAR_ADDCHOICE, &val, &text );

        /* Add title choice */
        val2.i_int = i;
        var_Change( p_input, "title", VLC_VAR_ADDCHOICE, &val2, &text );

        free( text.psz_string );

        for( j = 0; j < p_input->title[i]->i_seekpoint; j++ )
        {
            val2.i_int = j;

            if( p_input->title[i]->seekpoint[j]->psz_name == NULL ||
                *p_input->title[i]->seekpoint[j]->psz_name == '\0' )
            {
                /* Default value */
                asprintf( &text2.psz_string, _("Chapter %i"),
                          j + p_input->i_seekpoint_offset );
            }
            else
            {
                text2.psz_string =
                    strdup( p_input->title[i]->seekpoint[j]->psz_name );
            }

            var_Change( p_input, val.psz_string, VLC_VAR_ADDCHOICE,
                        &val2, &text2 );
            if( text2.psz_string ) free( text2.psz_string );
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
    input_title_t *t = p_input->title[i_title];
    vlc_value_t val;
    int  i;

    /* Create/Destroy command variables */
    if( t->i_seekpoint <= 1 )
    {
        var_Destroy( p_input, "next-chapter" );
        var_Destroy( p_input, "prev-chapter" );
    }
    else if( var_Get( p_input, "next-chapter", &val ) != VLC_SUCCESS )
    {
        vlc_value_t text;

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
        vlc_value_t text;
        val.i_int = i;

        if( t->seekpoint[i]->psz_name == NULL ||
            *t->seekpoint[i]->psz_name == '\0' )
        {
            /* Default value */
            asprintf( &text.psz_string, _("Chapter %i"),
                      i + p_input->i_seekpoint_offset );
        }
        else
        {
            text.psz_string = strdup( t->seekpoint[i]->psz_name );
        }

        var_Change( p_input, "chapter", VLC_VAR_ADDCHOICE, &val, &text );
        if( text.psz_string ) free( text.psz_string );
    }
}

/*****************************************************************************
 * input_ConfigVarInit:
 *  Create all config object variables
 *****************************************************************************/
void input_ConfigVarInit ( input_thread_t *p_input )
{
    vlc_value_t val;

    /* Create Object Variables for private use only */
    var_Create( p_input, "video", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_input, "audio", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_input, "spu", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    var_Create( p_input, "audio-track", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
    var_Create( p_input, "sub-track", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );

    var_Create( p_input, "audio-language", VLC_VAR_STRING|VLC_VAR_DOINHERIT );
    var_Create( p_input, "sub-language", VLC_VAR_STRING|VLC_VAR_DOINHERIT );

    var_Create( p_input, "audio-track-id", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
    var_Create( p_input, "sub-track-id", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );

    var_Create( p_input, "sub-file", VLC_VAR_FILE | VLC_VAR_DOINHERIT );
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

    var_Create( p_input, "input-repeat", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );

    var_Create( p_input, "start-time", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
    var_Create( p_input, "stop-time", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );

    var_Create( p_input, "minimize-threads", VLC_VAR_BOOL|VLC_VAR_DOINHERIT );

    var_Create( p_input, "demuxed-id3", VLC_VAR_BOOL ); /* FIXME beurk */
    val.b_bool = VLC_FALSE;
    var_Change( p_input, "demuxed-id3", VLC_VAR_SETVALUE, &val, NULL );

    var_Create( p_input, "audio-desync", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    var_Create( p_input, "cr-average", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_input, "clock-synchro", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);

    var_Create( p_input, "seekable", VLC_VAR_BOOL );
    val.b_bool = VLC_TRUE; /* Fixed later*/
    var_Change( p_input, "seekable", VLC_VAR_SETVALUE, &val, NULL );

    var_Create( p_input, "input-slave", VLC_VAR_STRING | VLC_VAR_DOINHERIT );

    /* */
    var_Create( p_input, "access-filter", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_input, "access", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_input, "demux", VLC_VAR_STRING | VLC_VAR_DOINHERIT );

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
 * All Callbacks:
 *****************************************************************************/
static int StateCallback( vlc_object_t *p_this, char const *psz_cmd,
                          vlc_value_t oldval, vlc_value_t newval,
                          void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;


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

    /* Problem with this way: the "rate" variable is update after the input thread do the change */
    if( !strcmp( psz_cmd, "rate-slower" ) )
    {
        input_ControlPush( p_input, INPUT_CONTROL_SET_RATE_SLOWER, NULL );
    }
    else if( !strcmp( psz_cmd, "rate-faster" ) )
    {
        input_ControlPush( p_input, INPUT_CONTROL_SET_RATE_FASTER, NULL );
    }
    else
    {
        input_ControlPush( p_input, INPUT_CONTROL_SET_RATE, &newval );
    }

    return VLC_SUCCESS;
}

static int PositionCallback( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval,
                             void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    vlc_value_t val, length;

    if( !strcmp( psz_cmd, "position-offset" ) )
    {
        input_ControlPush( p_input, INPUT_CONTROL_SET_POSITION_OFFSET, &newval );

        val.f_float = var_GetFloat( p_input, "position" ) + newval.f_float;
        if( val.f_float < 0.0 ) val.f_float = 0.0;
        if( val.f_float > 1.0 ) val.f_float = 1.0;
        var_Change( p_input, "position", VLC_VAR_SETVALUE, &val, NULL );
    }
    else
    {
        val.f_float = newval.f_float;
        input_ControlPush( p_input, INPUT_CONTROL_SET_POSITION, &newval );
    }

    /* Update "position" for better intf behavour */
    var_Get( p_input, "length", &length );
    if( length.i_time > 0 && val.f_float >= 0.0 && val.f_float <= 1.0 )
    {
        val.i_time = length.i_time * val.f_float;
        var_Change( p_input, "time", VLC_VAR_SETVALUE, &val, NULL );
    }

    return VLC_SUCCESS;
}

static int TimeCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    vlc_value_t val, length;

    if( !strcmp( psz_cmd, "time-offset" ) )
    {
        input_ControlPush( p_input, INPUT_CONTROL_SET_TIME_OFFSET, &newval );
        val.i_time = var_GetTime( p_input, "time" ) + newval.i_time;
        if( val.i_time < 0 ) val.i_time = 0;
        /* TODO maybe test against i_length ? */
        var_Change( p_input, "time", VLC_VAR_SETVALUE, &val, NULL );
    }
    else
    {
        val.i_time = newval.i_time;
        input_ControlPush( p_input, INPUT_CONTROL_SET_TIME, &newval );
    }

    /* Update "position" for better intf behavour */
    var_Get( p_input, "length", &length );
    if( length.i_time > 0 && val.i_time >= 0 && val.i_time <= length.i_time )
    {
        val.f_float = (double)val.i_time/(double)length.i_time;
        var_Change( p_input, "position", VLC_VAR_SETVALUE, &val, NULL );
    }

    return VLC_SUCCESS;
}

static int ProgramCallback( vlc_object_t *p_this, char const *psz_cmd,
                            vlc_value_t oldval, vlc_value_t newval,
                            void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;

    input_ControlPush( p_input, INPUT_CONTROL_SET_PROGRAM, &newval );

    return VLC_SUCCESS;
}

static int TitleCallback( vlc_object_t *p_this, char const *psz_cmd,
                          vlc_value_t oldval, vlc_value_t newval,
                          void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    vlc_value_t val, count;

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

    /* Issue a title change */
    val.i_int = (int)p_data;
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
                             vlc_value_t oldval, vlc_value_t newval, void *p )
{
    input_thread_t *p_input = (input_thread_t*)p_this;


    if( !strcmp( psz_cmd, "audio-delay" ) )
    {
        /*Change i_pts_delay to make sure es are decoded in time*/
        if (newval.i_int < 0 || oldval.i_int < 0 )
        {
            p_input->i_pts_delay -= newval.i_int - oldval.i_int;
        }
        input_ControlPush( p_input, INPUT_CONTROL_SET_AUDIO_DELAY, &newval );
    }
    else if( !strcmp( psz_cmd, "spu-delay" ) )
        input_ControlPush( p_input, INPUT_CONTROL_SET_SPU_DELAY, &newval );
    return VLC_SUCCESS;
}

static int BookmarkCallback( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval,
                             void *p_data )
{
    input_thread_t *p_input = (input_thread_t*)p_this;

    input_ControlPush( p_input, INPUT_CONTROL_SET_BOOKMARK, &newval );

    return VLC_SUCCESS;
}
