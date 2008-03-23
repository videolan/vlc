/*****************************************************************************
 * vlm.c: VLM interface plugin
 *****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Simon Latapie <garf@videolan.org>
 *          Laurent Aimar <fenrir@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
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

#include <vlc/vlc.h>

#include <stdio.h>
#include <ctype.h>                                              /* tolower() */
#include <assert.h>

#include <vlc_vlm.h>

#ifdef ENABLE_VLM

#ifndef WIN32
#   include <sys/time.h>                                   /* gettimeofday() */
#endif

#ifdef HAVE_TIME_H
#   include <time.h>                                              /* ctime() */
#   include <sys/timeb.h>                                         /* ftime() */
#endif

#include <vlc_input.h>
#include "input_internal.h"
#include <vlc_stream.h>
#include "vlm_internal.h"
#include <vlc_vod.h>
#include <vlc_charset.h>
#include <vlc_sout.h>
#include "../stream_output/stream_output.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/

/* ugly kludge to avoid "null format string" warnings,
 * even if we handle NULL format string in vlm_MessageNew() */
static const char *vlm_NULL = NULL;

static void vlm_Destructor( vlm_t *p_vlm );

/* */
static int vlm_ControlInternal( vlm_t *, int, ... );

/* */
static vlm_message_t *vlm_Show( vlm_t *, vlm_media_sys_t *, vlm_schedule_sys_t *, const char * );

static vlm_schedule_sys_t *vlm_ScheduleSearch( vlm_t *, const char * );

static char *Save( vlm_t * );
static int Load( vlm_t *, char * );

static int ExecuteCommand( vlm_t *, const char *, vlm_message_t ** );

static int Manage( vlc_object_t * );

static vlm_schedule_sys_t *vlm_ScheduleNew( vlm_t *vlm, const char *psz_name );
static void vlm_ScheduleDelete( vlm_t *vlm, vlm_schedule_sys_t *sched );
static int vlm_ScheduleSetup( vlm_schedule_sys_t *schedule, const char *psz_cmd,
                              const char *psz_value );

static int vlm_MediaVodControl( void *, vod_media_t *, const char *, int, va_list );

/* */
static vlm_media_sys_t *vlm_MediaSearch( vlm_t *, const char *);

/*****************************************************************************
 * vlm_New:
 *****************************************************************************/
vlm_t *__vlm_New ( vlc_object_t *p_this )
{
    vlc_value_t lockval;
    vlm_t *p_vlm = NULL;
    char *psz_vlmconf;

    /* Avoid multiple creation */
    if( var_Create( p_this->p_libvlc, "vlm_mutex", VLC_VAR_MUTEX ) ||
        var_Get( p_this->p_libvlc, "vlm_mutex", &lockval ) )
        return NULL;

    vlc_mutex_lock( lockval.p_address );

    p_vlm = vlc_object_find( p_this, VLC_OBJECT_VLM, FIND_ANYWHERE );
    if( p_vlm )
    {
        vlc_object_yield( p_vlm );
        vlc_mutex_unlock( lockval.p_address );
        return p_vlm;
    }

    msg_Dbg( p_this, "creating VLM" );

    p_vlm = vlc_object_create( p_this, VLC_OBJECT_VLM );
    if( !p_vlm )
    {
        vlc_mutex_unlock( lockval.p_address );
        return NULL;
    }

    vlc_mutex_init( p_this->p_libvlc, &p_vlm->lock );
    p_vlm->i_id = 1;
    TAB_INIT( p_vlm->i_media, p_vlm->media );
    TAB_INIT( p_vlm->i_schedule, p_vlm->schedule );
    p_vlm->i_vod = 0;
    p_vlm->p_vod = NULL;
    vlc_object_attach( p_vlm, p_this->p_libvlc );

    if( vlc_thread_create( p_vlm, "vlm thread",
                           Manage, VLC_THREAD_PRIORITY_LOW, VLC_FALSE ) )
    {
        vlc_mutex_destroy( &p_vlm->lock );
        vlc_object_release( p_vlm );
        return NULL;
    }

    /* Load our configuration file */
    psz_vlmconf = var_CreateGetString( p_vlm, "vlm-conf" );
    if( psz_vlmconf && *psz_vlmconf )
    {
        vlm_message_t *p_message = NULL;
        char *psz_buffer = NULL;

        msg_Dbg( p_this, "loading VLM configuration" );
        asprintf(&psz_buffer, "load %s", psz_vlmconf );
        if( psz_buffer )
        {
            msg_Dbg( p_this, psz_buffer );
            if( vlm_ExecuteCommand( p_vlm, psz_buffer, &p_message ) )
                msg_Warn( p_this, "error while loading the configuration file" );

            vlm_MessageDelete(p_message);
            free(psz_buffer);
        }
    }
    free(psz_vlmconf);

    vlc_object_set_destructor( p_vlm, (vlc_destructor_t)vlm_Destructor );
    vlc_mutex_unlock( lockval.p_address );

    return p_vlm;
}

/*****************************************************************************
 * vlm_Delete:
 *****************************************************************************/
void vlm_Delete( vlm_t *p_vlm )
{
    vlc_object_release( p_vlm );
}

/*****************************************************************************
 * vlm_Destructor:
 *****************************************************************************/
static void vlm_Destructor( vlm_t *p_vlm )
{
    vlc_object_kill( p_vlm );
    vlc_thread_join( p_vlm );

    vlm_ControlInternal( p_vlm, VLM_CLEAR_MEDIAS );
    TAB_CLEAN( p_vlm->i_media, p_vlm->media );

    vlm_ControlInternal( p_vlm, VLM_CLEAR_SCHEDULES );
    TAB_CLEAN( p_vlm->schedule, p_vlm->schedule );

    vlc_mutex_destroy( &p_vlm->lock );
}

/*****************************************************************************
 * vlm_ExecuteCommand:
 *****************************************************************************/
int vlm_ExecuteCommand( vlm_t *p_vlm, const char *psz_command,
                        vlm_message_t **pp_message)
{
    int i_result;

    vlc_mutex_lock( &p_vlm->lock );
    i_result = ExecuteCommand( p_vlm, psz_command, pp_message );
    vlc_mutex_unlock( &p_vlm->lock );

    return i_result;
}


static const char quotes[] = "\"'";
/**
 * FindCommandEnd: look for the end of a possibly quoted string
 * @return NULL on mal-formatted string,
 * pointer past the last character otherwise.
 */
static const char *FindCommandEnd( const char *psz_sent )
{
    char c, quote = 0;

    while( (c = *psz_sent) != '\0' )
    {
        if( !quote )
        {
            if( strchr(quotes,c) )   // opening quote
                quote = c;
            else if( isspace(c) )         // non-escaped space
                return psz_sent;
            else if( c == '\\' )
            {
                psz_sent++;         // skip escaped character
                if( *psz_sent == '\0' )
                    return psz_sent;
            }
        }
        else
        {
            if( c == quote )         // non-escaped matching quote
                quote = 0;
            else if( (quote == '"') && (c == '\\') )
            {
                psz_sent++;         // skip escaped character
                if (*psz_sent == '\0')
                    return NULL;    // error, closing quote missing
            }
        }
        psz_sent++;
    }

    // error (NULL) if we could not find a matching quote
    return quote ? NULL : psz_sent;
}


/**
 * Unescape a nul-terminated string.
 * Note that in and out can be identical.
 *
 * @param out output buffer (at least <strlen (in) + 1> characters long)
 * @param in nul-terminated string to be unescaped
 *
 * @return 0 on success, -1 on error.
 */
static int Unescape( char *out, const char *in )
{
    char c, quote = 0;

    while( (c = *in++) != '\0' )
    {
        if( !quote )
        {
            if (strchr(quotes,c))   // opening quote
            {
                quote = c;
                continue;
            }
            else if( c == '\\' )
            {
                switch (c = *in++)
                {
                    case '"':
                    case '\'':
                    case '\\':
                        *out++ = c;
                        continue;

                    case '\0':
                        *out = '\0';
                        return 0;
                }
                if( isspace(c) )
                {
                    *out++ = c;
                    continue;
                }
                /* None of the special cases - copy the backslash */
                *out++ = '\\';
            }
        }
        else
        {
            if( c == quote )         // non-escaped matching quote
            {
                quote = 0;
                continue;
            }
            if( (quote == '"') && (c == '\\') )
            {
                switch( c = *in++ )
                {
                    case '"':
                    case '\\':
                        *out++ = c;
                        continue;

                    case '\0':   // should never happen
                        *out = '\0';
                        return -1;
                }
                /* None of the special cases - copy the backslash */
                *out++ = '\\';
            }
        }
        *out++ = c;
    }

    *out = '\0';
    return 0;
}


/*****************************************************************************
 * ExecuteCommand: The main state machine
 *****************************************************************************
 * Execute a command which ends with '\0' (string)
 *****************************************************************************/
static int ExecuteSyntaxError( const char *psz_cmd, vlm_message_t **pp_status )
{
    *pp_status = vlm_MessageNew( psz_cmd, "Wrong command syntax" );
    return VLC_EGENERIC;
}

static vlc_bool_t ExecuteIsMedia( vlm_t *p_vlm, const char *psz_name )
{
    int64_t id;

    if( !psz_name || vlm_ControlInternal( p_vlm, VLM_GET_MEDIA_ID, psz_name, &id ) )
        return VLC_FALSE;
    return VLC_TRUE;
}
static vlc_bool_t ExecuteIsSchedule( vlm_t *p_vlm, const char *psz_name )
{
    if( !psz_name || !vlm_ScheduleSearch( p_vlm, psz_name ) )
        return VLC_FALSE;
    return VLC_TRUE;
}

static int ExecuteDel( vlm_t *p_vlm, const char *psz_name, vlm_message_t **pp_status )
{
    vlm_media_sys_t *p_media;
    vlm_schedule_sys_t *p_schedule;

    p_media = vlm_MediaSearch( p_vlm, psz_name );
    p_schedule = vlm_ScheduleSearch( p_vlm, psz_name );

    if( p_schedule != NULL )
    {
        vlm_ScheduleDelete( p_vlm, p_schedule );
    }
    else if( p_media != NULL )
    {
        vlm_ControlInternal( p_vlm, VLM_DEL_MEDIA, p_media->cfg.id );
    }
    else if( !strcmp(psz_name, "media") )
    {
        vlm_ControlInternal( p_vlm, VLM_CLEAR_MEDIAS );
    }
    else if( !strcmp(psz_name, "schedule") )
    {
        vlm_ControlInternal( p_vlm, VLM_CLEAR_SCHEDULES );
    }
    else if( !strcmp(psz_name, "all") )
    {
        vlm_ControlInternal( p_vlm, VLM_CLEAR_MEDIAS );
        vlm_ControlInternal( p_vlm, VLM_CLEAR_SCHEDULES );
    }
    else
    {
        *pp_status = vlm_MessageNew( "del", "%s: media unknown", psz_name );
        return VLC_EGENERIC;
    }

    *pp_status = vlm_MessageNew( "del", vlm_NULL );
    return VLC_SUCCESS;
}

static int ExecuteShow( vlm_t *p_vlm, const char *psz_name, vlm_message_t **pp_status )
{
    vlm_media_sys_t *p_media;
    vlm_schedule_sys_t *p_schedule;

    if( !psz_name )
    {
        *pp_status = vlm_Show( p_vlm, NULL, NULL, NULL );
        return VLC_SUCCESS;
    }

    p_media = vlm_MediaSearch( p_vlm, psz_name );
    p_schedule = vlm_ScheduleSearch( p_vlm, psz_name );

    if( p_schedule != NULL )
        *pp_status = vlm_Show( p_vlm, NULL, p_schedule, NULL );
    else if( p_media != NULL )
        *pp_status = vlm_Show( p_vlm, p_media, NULL, NULL );
    else
        *pp_status = vlm_Show( p_vlm, NULL, NULL, psz_name );

    return VLC_SUCCESS;
}

static int ExecuteHelp( vlm_message_t *p_status )
{
    vlm_message_t *message_child;

#define MessageAdd( a ) \
        vlm_MessageAdd( p_status, vlm_MessageNew( a, vlm_NULL ) );
#define MessageAddChild( a ) \
        vlm_MessageAdd( message_child, vlm_MessageNew( a, vlm_NULL ) );

    p_status = vlm_MessageNew( "help", vlm_NULL );

    message_child = MessageAdd( "Commands Syntax:" );
    MessageAddChild( "new (name) vod|broadcast|schedule [properties]" );
    MessageAddChild( "setup (name) (properties)" );
    MessageAddChild( "show [(name)|media|schedule]" );
    MessageAddChild( "del (name)|all|media|schedule" );
    MessageAddChild( "control (name) [instance_name] (command)" );
    MessageAddChild( "save (config_file)" );
    MessageAddChild( "export" );
    MessageAddChild( "load (config_file)" );

    message_child = MessageAdd( "Media Proprieties Syntax:" );
    MessageAddChild( "input (input_name)" );
    MessageAddChild( "inputdel (input_name)|all" );
    MessageAddChild( "inputdeln input_number" );
    MessageAddChild( "output (output_name)" );
    MessageAddChild( "option (option_name)[=value]" );
    MessageAddChild( "enabled|disabled" );
    MessageAddChild( "loop|unloop (broadcast only)" );
    MessageAddChild( "mux (mux_name)" );

    message_child = MessageAdd( "Schedule Proprieties Syntax:" );
    MessageAddChild( "enabled|disabled" );
    MessageAddChild( "append (command_until_rest_of_the_line)" );
    MessageAddChild( "date (year)/(month)/(day)-(hour):(minutes):"
                     "(seconds)|now" );
    MessageAddChild( "period (years_aka_12_months)/(months_aka_30_days)/"
                     "(days)-(hours):(minutes):(seconds)" );
    MessageAddChild( "repeat (number_of_repetitions)" );

    message_child = MessageAdd( "Control Commands Syntax:" );
    MessageAddChild( "play [input_number]" );
    MessageAddChild( "pause" );
    MessageAddChild( "stop" );
    MessageAddChild( "seek [+-](percentage) | [+-](seconds)s | [+-](miliseconds)ms" );

    return VLC_SUCCESS;
}

static int ExecuteControl( vlm_t *p_vlm, const char *psz_name, const int i_arg, char ** ppsz_arg, vlm_message_t **pp_status )
{
    vlm_media_sys_t *p_media;
    const char *psz_control = NULL;
    const char *psz_instance = NULL;
    const char *psz_argument = NULL;
    int i_index;
    int i_result;

    if( !ExecuteIsMedia( p_vlm, psz_name ) )
    {
        *pp_status = vlm_MessageNew( "control", "%s: media unknown", psz_name );
        return VLC_EGENERIC;
    }

    assert( i_arg > 0 );

#define IS(txt) ( !strcmp( ppsz_arg[i_index], (txt) ) )
    i_index = 0;
    if( !IS("play") && !IS("stop") && !IS("pause") && !IS("seek") )
    {
        i_index = 1;
        psz_instance = ppsz_arg[0];

        if( i_index >= i_arg || ( !IS("play") && !IS("stop") && !IS("pause") && !IS("seek") ) )
            return ExecuteSyntaxError( "control", pp_status );
    }
#undef IS
    psz_control = ppsz_arg[i_index];

    if( i_index+1 < i_arg )
        psz_argument = ppsz_arg[i_index+1];

    p_media = vlm_MediaSearch( p_vlm, psz_name );
    assert( p_media );

    if( !strcmp( psz_control, "play" ) )
    {
        int i_input_index = 0;
        int i;

        if( ( psz_argument && sscanf(psz_argument, "%d", &i) == 1 ) && i > 0 && i-1 < p_media->cfg.i_input )
        {
            i_input_index = i-1;
        }
        else if( psz_argument )
        {
            int j;
            vlm_media_t *p_cfg = &p_media->cfg;
            for ( j=0; j < p_cfg->i_input; j++)
            {
                if( !strcmp( p_cfg->ppsz_input[j], psz_argument ) )
                {
                    i_input_index = j;
                    break;
                }
            }
        }

        if( p_media->cfg.b_vod )
            i_result = vlm_ControlInternal( p_vlm, VLM_START_MEDIA_VOD_INSTANCE, p_media->cfg.id, psz_instance, i_input_index, NULL );    // we should get here now
        else
            i_result = vlm_ControlInternal( p_vlm, VLM_START_MEDIA_BROADCAST_INSTANCE, p_media->cfg.id, psz_instance, i_input_index );
    }
    else if( !strcmp( psz_control, "seek" ) )
    {
        if( psz_argument )
        {
            vlc_bool_t b_relative;
            if( psz_argument[0] == '+' || psz_argument[0] == '-' )
                b_relative = VLC_TRUE;
            else
                b_relative = VLC_FALSE;

            if( strstr( psz_argument, "ms" ) || strstr( psz_argument, "s" ) )
            {
                /* Time (ms or s) */
                int64_t i_new_time;

                if( strstr( psz_argument, "ms" ) )
                    i_new_time =  1000 * (int64_t)atoi( psz_argument );
                else
                    i_new_time = 1000000 * (int64_t)atoi( psz_argument );

                if( b_relative )
                {
                    int64_t i_time = 0;
                    vlm_ControlInternal( p_vlm, VLM_GET_MEDIA_INSTANCE_TIME, p_media->cfg.id, psz_instance, &i_time );
                    i_new_time += i_time;
                }
                if( i_new_time < 0 )
                    i_new_time = 0;
                i_result = vlm_ControlInternal( p_vlm, VLM_SET_MEDIA_INSTANCE_TIME, p_media->cfg.id, psz_instance, i_new_time );
            }
            else
            {
                /* Percent */
                double d_new_position = i18n_atof( psz_argument ) / 100.0;

                if( b_relative )
                {
                    double d_position = 0.0;

                    vlm_ControlInternal( p_vlm, VLM_GET_MEDIA_INSTANCE_POSITION, p_media->cfg.id, psz_instance, &d_position );
                    d_new_position += d_position;
                }
                if( d_new_position < 0.0 )
                    d_new_position = 0.0;
                else if( d_new_position > 1.0 )
                    d_new_position = 1.0;
                i_result = vlm_ControlInternal( p_vlm, VLM_SET_MEDIA_INSTANCE_POSITION, p_media->cfg.id, psz_instance, d_new_position );
            }
        }
        else
        {
            i_result = VLC_EGENERIC;
        }
    }
    else if( !strcmp( psz_control, "rewind" ) )
    {
        if( psz_argument )
        {
            const double d_scale = i18n_atof( psz_argument );
            double d_position;

            vlm_ControlInternal( p_vlm, VLM_GET_MEDIA_INSTANCE_POSITION, p_media->cfg.id, psz_instance, &d_position );
            d_position -= (d_scale / 1000.0);
            if( d_position < 0.0 )
                d_position = 0.0;
            i_result = vlm_ControlInternal( p_vlm, VLM_SET_MEDIA_INSTANCE_POSITION, p_media->cfg.id, psz_instance, d_position );
        }
        else
        {
            i_result = VLC_EGENERIC;
        }
    }
    else if( !strcmp( psz_control, "forward" ) )
    {
        if( psz_argument )
        {
            const double d_scale = i18n_atof( psz_argument );
            double d_position;

            vlm_ControlInternal( p_vlm, VLM_GET_MEDIA_INSTANCE_POSITION, p_media->cfg.id, psz_instance, &d_position );
            d_position += (d_scale / 1000.0);
            if( d_position > 1.0 )
                d_position = 1.0;
            i_result = vlm_ControlInternal( p_vlm, VLM_SET_MEDIA_INSTANCE_POSITION, p_media->cfg.id, psz_instance, d_position );

        }
        else
        {
            i_result = VLC_EGENERIC;
        }
    }
    else if( !strcmp( psz_control, "stop" ) )
    {
        i_result = vlm_ControlInternal( p_vlm, VLM_STOP_MEDIA_INSTANCE, p_media->cfg.id, psz_instance );
    }
    else if( !strcmp( psz_control, "pause" ) )
    {
        i_result = vlm_ControlInternal( p_vlm, VLM_PAUSE_MEDIA_INSTANCE, p_media->cfg.id, psz_instance );
    }
    else
    {
        i_result = VLC_EGENERIC;
    }

    if( i_result )
    {
        *pp_status = vlm_MessageNew( "control", "unknown error" );
        return VLC_SUCCESS;
    }
    *pp_status = vlm_MessageNew( "control", vlm_NULL );
    return VLC_SUCCESS;
}

static int ExecuteExport( vlm_t *p_vlm, vlm_message_t **pp_status )
{
    char *psz_export = Save( p_vlm );

    *pp_status = vlm_MessageNew( "export", psz_export );
    free( psz_export );
    return VLC_SUCCESS;
}

static int ExecuteSave( vlm_t *p_vlm, const char *psz_file, vlm_message_t **pp_status )
{
    FILE *f = utf8_fopen( psz_file, "wt" );
    char *psz_save;

    if( !f )
        goto error;

    psz_save = Save( p_vlm );
    if( psz_save == NULL )
    {
        fclose( f );
        goto error;
    }
    fwrite( psz_save, strlen( psz_save ), 1, f );
    free( psz_save );
    fclose( f );

    *pp_status = vlm_MessageNew( "save", vlm_NULL );
    return VLC_SUCCESS;

error:
    *pp_status = vlm_MessageNew( "save", "Unable to save to file" );
    return VLC_EGENERIC;
}

static int ExecuteLoad( vlm_t *p_vlm, const char *psz_url, vlm_message_t **pp_status )
{
    stream_t *p_stream = stream_UrlNew( p_vlm, psz_url );
    int64_t i_size;
    char *psz_buffer;

    if( !p_stream )
    {
        *pp_status = vlm_MessageNew( "load", "Unable to load from file" );
        return VLC_EGENERIC;
    }

    /* FIXME needed ? */
    if( stream_Seek( p_stream, 0 ) != 0 )
    {
        stream_Delete( p_stream );

        *pp_status = vlm_MessageNew( "load", "Read file error" );
        return VLC_EGENERIC;
    }

    i_size = stream_Size( p_stream );

    psz_buffer = malloc( i_size + 1 );
    if( !psz_buffer )
    {
        stream_Delete( p_stream );

        *pp_status = vlm_MessageNew( "load", "Read file error" );
        return VLC_EGENERIC;
    }

    stream_Read( p_stream, psz_buffer, i_size );
    psz_buffer[i_size] = '\0';

    stream_Delete( p_stream );

    if( Load( p_vlm, psz_buffer ) )
    {
        free( psz_buffer );

        *pp_status = vlm_MessageNew( "load", "Error while loading file" );
        return VLC_EGENERIC;
    }

    free( psz_buffer );

    *pp_status = vlm_MessageNew( "load", vlm_NULL );
    return VLC_SUCCESS;
}

static int ExecuteScheduleProperty( vlm_t *p_vlm, vlm_schedule_sys_t *p_schedule, vlc_bool_t b_new,
                                    const int i_property, char *ppsz_property[], vlm_message_t **pp_status )
{
    const char *psz_cmd = b_new ? "new" : "setup";
    int i;

    for( i = 0; i < i_property; i++ )
    {
        if( !strcmp( ppsz_property[i], "enabled" ) ||
            !strcmp( ppsz_property[i], "disabled" ) )
        {
            vlm_ScheduleSetup( p_schedule, ppsz_property[i], NULL );
        }
        else if( !strcmp( ppsz_property[i], "append" ) )
        {
            char *psz_line;
            int j;
            /* Beware: everything behind append is considered as
             * command line */

            if( ++i >= i_property )
                break;

            psz_line = strdup( ppsz_property[i] );
            for( j = i+1; j < i_property; j++ )
            {
                psz_line = realloc( psz_line, strlen(psz_line) + strlen(ppsz_property[j]) + 1 + 1 );
                strcat( psz_line, " " );
                strcat( psz_line, ppsz_property[j] );
            }

            vlm_ScheduleSetup( p_schedule, "append", psz_line );
            break;
        }
        else
        {
            if( i + 1 >= i_property )
            {
                if( b_new )
                    vlm_ScheduleDelete( p_vlm, p_schedule );
                return ExecuteSyntaxError( psz_cmd, pp_status );
            }

            vlm_ScheduleSetup( p_schedule, ppsz_property[i], ppsz_property[i+1] );
            i++;
        }
    }
    *pp_status = vlm_MessageNew( psz_cmd, vlm_NULL );
    return VLC_SUCCESS;
}

static int ExecuteMediaProperty( vlm_t *p_vlm, int64_t id, vlc_bool_t b_new,
                                 const int i_property, char *ppsz_property[], vlm_message_t **pp_status )
{
    const char *psz_cmd = b_new ? "new" : "setup";
    vlm_media_t *p_cfg = NULL;
    int i_result;
    int i;

#undef ERROR
#undef MISSING
#define ERROR( txt ) do { *pp_status = vlm_MessageNew( psz_cmd, txt); goto error; } while(0)
    if( vlm_ControlInternal( p_vlm, VLM_GET_MEDIA, id, &p_cfg ) )
        ERROR( "unknown media" );

#define MISSING(cmd) do { if( !psz_value ) ERROR( "missing argument for " cmd ); } while(0)
    for( i = 0; i < i_property; i++ )
    {
        const char *psz_option = ppsz_property[i];
        const char *psz_value = i+1 < i_property ? ppsz_property[i+1] :  NULL;

        if( !strcmp( psz_option, "enabled" ) )
        {
            p_cfg->b_enabled = VLC_TRUE;
        }
        else if( !strcmp( psz_option, "disabled" ) )
        {
            p_cfg->b_enabled = VLC_FALSE;
        }
        else if( !strcmp( psz_option, "input" ) )
        {
            MISSING( "input" );
            TAB_APPEND( p_cfg->i_input, p_cfg->ppsz_input, strdup(psz_value) );
            i++;
        }
        else if( !strcmp( psz_option, "inputdel" ) && psz_value && !strcmp( psz_value, "all" ) )
        {
            while( p_cfg->i_input > 0 )
                TAB_REMOVE( p_cfg->i_input, p_cfg->ppsz_input, p_cfg->ppsz_input[0] );
            i++;
        }
        else if( !strcmp( psz_option, "inputdel" ) )
        {
            int j;

            MISSING( "inputdel" );

            for( j = 0; j < p_cfg->i_input; j++ )
            {
                if( !strcmp( p_cfg->ppsz_input[j], psz_value ) )
                {
                    TAB_REMOVE( p_cfg->i_input, p_cfg->ppsz_input, p_cfg->ppsz_input[j] );
                    break;
                }
            }
            i++;
        }
        else if( !strcmp( psz_option, "inputdeln" ) )
        {
            int i_index;

            MISSING( "inputdeln" );
 
            i_index = atoi( psz_value );
            if( i_index > 0 && i_index <= p_cfg->i_input )
                TAB_REMOVE( p_cfg->i_input, p_cfg->ppsz_input, p_cfg->ppsz_input[i_index-1] );
            i++;
        }
        else if( !strcmp( psz_option, "output" ) )
        {
            MISSING( "output" );

            free( p_cfg->psz_output );
            p_cfg->psz_output = *psz_value ? strdup( psz_value ) : NULL;
            i++;
        }
        else if( !strcmp( psz_option, "option" ) )
        {
            MISSING( "option" );

            TAB_APPEND( p_cfg->i_option, p_cfg->ppsz_option, strdup( psz_value ) );
            i++;
        }
        else if( !strcmp( psz_option, "loop" ) )
        {
            if( p_cfg->b_vod )
                ERROR( "invalid loop option for vod" );
            p_cfg->broadcast.b_loop = VLC_TRUE;
        }
        else if( !strcmp( psz_option, "unloop" ) )
        {
            if( p_cfg->b_vod )
                ERROR( "invalid unloop option for vod" );
            p_cfg->broadcast.b_loop = VLC_FALSE;
        }
        else if( !strcmp( psz_option, "mux" ) )
        {
            MISSING( "mux" );
            if( !p_cfg->b_vod )
                ERROR( "invalid mux option for broadcast" );

            free( p_cfg->vod.psz_mux );
            p_cfg->vod.psz_mux = *psz_value ? strdup( psz_value ) : NULL;
            i++;
        }
        else
        {
            fprintf( stderr, "PROP: name=%s unknown\n", psz_option );
            ERROR( "Wrong command syntax" );
        }
    }
#undef MISSING
#undef ERROR

    /* */
    i_result = vlm_ControlInternal( p_vlm, VLM_CHANGE_MEDIA, p_cfg );
    vlm_media_Delete( p_cfg );

    *pp_status = vlm_MessageNew( psz_cmd, vlm_NULL );
    return i_result;

error:
    if( p_cfg )
    {
        if( b_new )
            vlm_ControlInternal( p_vlm, VLM_DEL_MEDIA, p_cfg->id );
        vlm_media_Delete( p_cfg );
    }
    return VLC_EGENERIC;
}

static int ExecuteNew( vlm_t *p_vlm, const char *psz_name, const char *psz_type, const int i_property, char *ppsz_property[], vlm_message_t **pp_status )
{
    /* Check name */
    if( !strcmp( psz_name, "all" ) || !strcmp( psz_name, "media" ) || !strcmp( psz_name, "schedule" ) )
    {
        *pp_status = vlm_MessageNew( "new", "\"all\", \"media\" and \"schedule\" are reserved names" );
        return VLC_EGENERIC;
    }
    if( ExecuteIsMedia( p_vlm, psz_name ) || ExecuteIsSchedule( p_vlm, psz_name ) )
    {
        *pp_status = vlm_MessageNew( "new", "%s: Name already in use", psz_name );
        return VLC_EGENERIC;
    }
    /* */
    if( !strcmp( psz_type, "schedule" ) )
    {
        vlm_schedule_sys_t *p_schedule = vlm_ScheduleNew( p_vlm, psz_name );
        if( !p_schedule )
        {
            *pp_status = vlm_MessageNew( "new", "could not create schedule" );
            return VLC_EGENERIC;
        }
        return ExecuteScheduleProperty( p_vlm, p_schedule, VLC_TRUE, i_property, ppsz_property, pp_status );
    }
    else if( !strcmp( psz_type, "vod" ) || !strcmp( psz_type, "broadcast" ) )
    {
        vlm_media_t cfg;
        int64_t id;

        vlm_media_Init( &cfg );
        cfg.psz_name = strdup( psz_name );
        cfg.b_vod = !strcmp( psz_type, "vod" );

        if( vlm_ControlInternal( p_vlm, VLM_ADD_MEDIA, &cfg, &id ) )
        {
            vlm_media_Clean( &cfg );
            *pp_status = vlm_MessageNew( "new", "could not create media" );
            return VLC_EGENERIC;
        }
        vlm_media_Clean( &cfg );
        return ExecuteMediaProperty( p_vlm, id, VLC_TRUE, i_property, ppsz_property, pp_status );
    }
    else
    {
        *pp_status = vlm_MessageNew( "new", "%s: Choose between vod, broadcast or schedule", psz_type );
        return VLC_EGENERIC;
    }
}

static int ExecuteSetup( vlm_t *p_vlm, const char *psz_name, const int i_property, char *ppsz_property[], vlm_message_t **pp_status )
{
    if( ExecuteIsSchedule( p_vlm, psz_name ) )
    {
        vlm_schedule_sys_t *p_schedule = vlm_ScheduleSearch( p_vlm, psz_name );
        return ExecuteScheduleProperty( p_vlm, p_schedule, VLC_FALSE, i_property, ppsz_property, pp_status );
    }
    else if( ExecuteIsMedia( p_vlm, psz_name ) )
    {
        int64_t id;
        if( vlm_ControlInternal( p_vlm, VLM_GET_MEDIA_ID, psz_name, &id ) )
            goto error;
        return ExecuteMediaProperty( p_vlm, id, VLC_FALSE, i_property, ppsz_property, pp_status );
    }

error:
    *pp_status = vlm_MessageNew( "setup", "%s unknown", psz_name );
    return VLC_EGENERIC;
}

static int ExecuteCommand( vlm_t *p_vlm, const char *psz_command,
                           vlm_message_t **pp_message )
{
    size_t i_command = 0;
    char buf[strlen (psz_command) + 1], *psz_buf = buf;
    char *ppsz_command[3+sizeof (buf) / 2];
    vlm_message_t *p_message = NULL;

    /* First, parse the line and cut it */
    while( *psz_command != '\0' )
    {
        const char *psz_temp;

        if(isspace (*psz_command))
        {
            psz_command++;
            continue;
        }

        /* support for comments */
        if( i_command == 0 && *psz_command == '#')
        {
            p_message = vlm_MessageNew( "", vlm_NULL );
            goto success;
        }

        psz_temp = FindCommandEnd( psz_command );

        if( psz_temp == NULL )
        {
            p_message = vlm_MessageNew( "Incomplete command", psz_command );
            goto error;
        }

        assert (i_command < (sizeof (ppsz_command) / sizeof (ppsz_command[0])));

        ppsz_command[i_command] = psz_buf;
        memcpy (psz_buf, psz_command, psz_temp - psz_command);
        psz_buf[psz_temp - psz_command] = '\0';

        Unescape (psz_buf, psz_buf);

        i_command++;
        psz_buf += psz_temp - psz_command + 1;
        psz_command = psz_temp;

        assert (buf + sizeof (buf) >= psz_buf);
    }

    /*
     * And then Interpret it
     */

#define IF_EXECUTE( name, check, cmd ) if( !strcmp(ppsz_command[0], name ) ) { if( (check) ) goto syntax_error;  if( (cmd) ) goto error; goto success; }
    if( i_command == 0 )
    {
        p_message = vlm_MessageNew( "", vlm_NULL );
        goto success;
    }
    else IF_EXECUTE( "del",     (i_command != 2),   ExecuteDel(p_vlm, ppsz_command[1], &p_message) )
    else IF_EXECUTE( "show",    (i_command > 2),    ExecuteShow(p_vlm, i_command > 1 ? ppsz_command[1] : NULL, &p_message) )
    else IF_EXECUTE( "help",    (i_command != 1),   ExecuteHelp( p_message) )
    else IF_EXECUTE( "control", (i_command < 3),    ExecuteControl(p_vlm, ppsz_command[1], i_command - 2, &ppsz_command[2], &p_message) )
    else IF_EXECUTE( "save",    (i_command != 2),   ExecuteSave(p_vlm, ppsz_command[1], &p_message) )
    else IF_EXECUTE( "export",  (i_command != 1),   ExecuteExport(p_vlm, &p_message) )
    else IF_EXECUTE( "load",    (i_command != 2),   ExecuteLoad(p_vlm, ppsz_command[1], &p_message) )
    else IF_EXECUTE( "new",     (i_command < 3),    ExecuteNew(p_vlm, ppsz_command[1], ppsz_command[2], i_command-3, &ppsz_command[3], &p_message) )
    else IF_EXECUTE( "setup",   (i_command < 2),    ExecuteSetup(p_vlm, ppsz_command[1], i_command-2, &ppsz_command[2], &p_message) )
    else
    {
        p_message = vlm_MessageNew( ppsz_command[0], "Unknown command" );
        goto error;
    }
#undef IF_EXECUTE

success:
    *pp_message = p_message;
    return VLC_SUCCESS;

syntax_error:
    return ExecuteSyntaxError( ppsz_command[0], pp_message );

error:
    *pp_message = p_message;
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Media handling
 *****************************************************************************/
vlm_media_sys_t *vlm_MediaSearch( vlm_t *vlm, const char *psz_name )
{
    int i;

    for( i = 0; i < vlm->i_media; i++ )
    {
        if( strcmp( psz_name, vlm->media[i]->cfg.psz_name ) == 0 )
            return vlm->media[i];
    }

    return NULL;
}

/*****************************************************************************
 * Schedule handling
 *****************************************************************************/
static int64_t vlm_Date(void)
{
#ifdef WIN32
    struct timeb tm;
    ftime( &tm );
    return ((int64_t)tm.time) * 1000000 + ((int64_t)tm.millitm) * 1000;
#else
    struct timeval tv_date;

    /* gettimeofday() cannot fail given &tv_date is a valid address */
    (void)gettimeofday( &tv_date, NULL );
    return (mtime_t) tv_date.tv_sec * 1000000 + (mtime_t) tv_date.tv_usec;
#endif
}

static vlm_schedule_sys_t *vlm_ScheduleNew( vlm_t *vlm, const char *psz_name )
{
    vlm_schedule_sys_t *p_sched = malloc( sizeof( vlm_schedule_sys_t ) );

    if( !p_sched )
    {
        return NULL;
    }

    if( !psz_name )
    {
        return NULL;
    }

    p_sched->psz_name = strdup( psz_name );
    p_sched->b_enabled = VLC_FALSE;
    p_sched->i_command = 0;
    p_sched->command = NULL;
    p_sched->i_date = 0;
    p_sched->i_period = 0;
    p_sched->i_repeat = -1;

    TAB_APPEND( vlm->i_schedule, vlm->schedule, p_sched );

    return p_sched;
}

/* for now, simple delete. After, del with options (last arg) */
static void vlm_ScheduleDelete( vlm_t *vlm, vlm_schedule_sys_t *sched )
{
    if( sched == NULL ) return;

    TAB_REMOVE( vlm->i_schedule, vlm->schedule, sched );

    if( vlm->i_schedule == 0 ) free( vlm->schedule );
    free( sched->psz_name );
    while( sched->i_command )
    {
        char *psz_cmd = sched->command[0];
        TAB_REMOVE( sched->i_command, sched->command, psz_cmd );
        free( psz_cmd );
    }
    free( sched );
}

static vlm_schedule_sys_t *vlm_ScheduleSearch( vlm_t *vlm, const char *psz_name )
{
    int i;

    for( i = 0; i < vlm->i_schedule; i++ )
    {
        if( strcmp( psz_name, vlm->schedule[i]->psz_name ) == 0 )
        {
            return vlm->schedule[i];
        }
    }

    return NULL;
}

/* Ok, setup schedule command will be able to support only one (argument value) at a time  */
static int vlm_ScheduleSetup( vlm_schedule_sys_t *schedule, const char *psz_cmd,
                       const char *psz_value )
{
    if( !strcmp( psz_cmd, "enabled" ) )
    {
        schedule->b_enabled = VLC_TRUE;
    }
    else if( !strcmp( psz_cmd, "disabled" ) )
    {
        schedule->b_enabled = VLC_FALSE;
    }
#if !defined( UNDER_CE )
    else if( !strcmp( psz_cmd, "date" ) )
    {
        struct tm time;
        const char *p;
        time_t date;

        time.tm_sec = 0;         /* seconds */
        time.tm_min = 0;         /* minutes */
        time.tm_hour = 0;        /* hours */
        time.tm_mday = 0;        /* day of the month */
        time.tm_mon = 0;         /* month */
        time.tm_year = 0;        /* year */
        time.tm_wday = 0;        /* day of the week */
        time.tm_yday = 0;        /* day in the year */
        time.tm_isdst = -1;       /* daylight saving time */

        /* date should be year/month/day-hour:minutes:seconds */
        p = strchr( psz_value, '-' );

        if( !strcmp( psz_value, "now" ) )
        {
            schedule->i_date = 0;
        }
        else if( (p == NULL) && sscanf( psz_value, "%d:%d:%d", &time.tm_hour,
                                        &time.tm_min, &time.tm_sec ) != 3 )
                                        /* it must be a hour:minutes:seconds */
        {
            return 1;
        }
        else
        {
            unsigned i,j,k;

            switch( sscanf( p + 1, "%u:%u:%u", &i, &j, &k ) )
            {
                case 1:
                    time.tm_sec = i;
                    break;
                case 2:
                    time.tm_min = i;
                    time.tm_sec = j;
                    break;
                case 3:
                    time.tm_hour = i;
                    time.tm_min = j;
                    time.tm_sec = k;
                    break;
                default:
                    return 1;
            }

            switch( sscanf( psz_value, "%d/%d/%d", &i, &j, &k ) )
            {
                case 1:
                    time.tm_mday = i;
                    break;
                case 2:
                    time.tm_mon = i - 1;
                    time.tm_mday = j;
                    break;
                case 3:
                    time.tm_year = i - 1900;
                    time.tm_mon = j - 1;
                    time.tm_mday = k;
                    break;
                default:
                    return 1;
            }

            date = mktime( &time );
            schedule->i_date = ((mtime_t) date) * 1000000;
        }
    }
    else if( !strcmp( psz_cmd, "period" ) )
    {
        struct tm time;
        const char *p;
        const char *psz_time = NULL, *psz_date = NULL;
        time_t date;
        unsigned i,j,k;

        /* First, if date or period are modified, repeat should be equal to -1 */
        schedule->i_repeat = -1;

        time.tm_sec = 0;         /* seconds */
        time.tm_min = 0;         /* minutes */
        time.tm_hour = 0;        /* hours */
        time.tm_mday = 0;        /* day of the month */
        time.tm_mon = 0;         /* month */
        time.tm_year = 0;        /* year */
        time.tm_wday = 0;        /* day of the week */
        time.tm_yday = 0;        /* day in the year */
        time.tm_isdst = -1;       /* daylight saving time */

        /* date should be year/month/day-hour:minutes:seconds */
        p = strchr( psz_value, '-' );
        if( p )
        {
            psz_date = psz_value;
            psz_time = p + 1;
        }
        else
        {
            psz_time = psz_value;
        }

        switch( sscanf( psz_time, "%u:%u:%u", &i, &j, &k ) )
        {
            case 1:
                time.tm_sec = i;
                break;
            case 2:
                time.tm_min = i;
                time.tm_sec = j;
                break;
            case 3:
                time.tm_hour = i;
                time.tm_min = j;
                time.tm_sec = k;
                break;
            default:
                return 1;
        }
        if( psz_date )
        {
            switch( sscanf( psz_date, "%u/%u/%u", &i, &j, &k ) )
            {
                case 1:
                    time.tm_mday = i;
                    break;
                case 2:
                    time.tm_mon = i;
                    time.tm_mday = j;
                    break;
                case 3:
                    time.tm_year = i;
                    time.tm_mon = j;
                    time.tm_mday = k;
                    break;
                default:
                    return 1;
            }
        }

        /* ok, that's stupid... who is going to schedule streams every 42 years ? */
        date = (((( time.tm_year * 12 + time.tm_mon ) * 30 + time.tm_mday ) * 24 + time.tm_hour ) * 60 + time.tm_min ) * 60 + time.tm_sec ;
        schedule->i_period = ((mtime_t) date) * 1000000;
    }
#endif /* UNDER_CE */
    else if( !strcmp( psz_cmd, "repeat" ) )
    {
        int i;

        if( sscanf( psz_value, "%d", &i ) == 1 )
        {
            schedule->i_repeat = i;
        }
        else
        {
            return 1;
        }
    }
    else if( !strcmp( psz_cmd, "append" ) )
    {
        char *command = strdup( psz_value );

        TAB_APPEND( schedule->i_command, schedule->command, command );
    }
    else
    {
        return 1;
    }
    return 0;
}

/*****************************************************************************
 * Message handling functions
 *****************************************************************************/
vlm_message_t *vlm_MessageNew( const char *psz_name,
                               const char *psz_format, ... )
{
    vlm_message_t *p_message;
    va_list args;

    if( !psz_name ) return NULL;

    p_message = malloc( sizeof(vlm_message_t) );
    if( !p_message)
    {
        return NULL;
    }

    p_message->psz_value = 0;

    if( psz_format )
    {
        va_start( args, psz_format );
        if( vasprintf( &p_message->psz_value, psz_format, args ) == -1 )
        {
            va_end( args );
            free( p_message );
            return NULL;
        }
        va_end( args );
    }

    p_message->psz_name = strdup( psz_name );
    p_message->i_child = 0;
    p_message->child = NULL;

    return p_message;
}

void vlm_MessageDelete( vlm_message_t *p_message )
{
    free( p_message->psz_name );
    free( p_message->psz_value );
    while( p_message->i_child-- )
        vlm_MessageDelete( p_message->child[p_message->i_child] );
    free( p_message->child );
    free( p_message );
}

/* Add a child */
vlm_message_t *vlm_MessageAdd( vlm_message_t *p_message,
                               vlm_message_t *p_child )
{
    if( p_message == NULL ) return NULL;

    if( p_child )
    {
        TAB_APPEND( p_message->i_child, p_message->child, p_child );
    }

    return p_child;
}

/*****************************************************************************
 * Misc utility functions
 *****************************************************************************/
static vlm_message_t *vlm_ShowMedia( vlm_media_sys_t *p_media )
{
    vlm_media_t *p_cfg = &p_media->cfg;
    vlm_message_t *p_msg;
    vlm_message_t *p_msg_sub;
    int i;

    p_msg = vlm_MessageNew( p_cfg->psz_name, vlm_NULL );
    vlm_MessageAdd( p_msg,
                    vlm_MessageNew( "type", p_cfg->b_vod ? "vod" : "broadcast" ) );
    vlm_MessageAdd( p_msg,
                    vlm_MessageNew( "enabled", p_cfg->b_enabled ? "yes" : "no" ) );

    if( p_cfg->b_vod )
        vlm_MessageAdd( p_msg,
                        vlm_MessageNew( "mux", p_cfg->vod.psz_mux ) );
    else
        vlm_MessageAdd( p_msg,
                        vlm_MessageNew( "loop", p_cfg->broadcast.b_loop ? "yes" : "no" ) );

    p_msg_sub = vlm_MessageAdd( p_msg, vlm_MessageNew( "inputs", vlm_NULL ) );
    for( i = 0; i < p_cfg->i_input; i++ )
    {
        char *psz_tmp;
        asprintf( &psz_tmp, "%d", i+1 );
        vlm_MessageAdd( p_msg_sub,
                        vlm_MessageNew( psz_tmp, p_cfg->ppsz_input[i] ) );
        free( psz_tmp );
    }

    vlm_MessageAdd( p_msg,
                    vlm_MessageNew( "output", p_cfg->psz_output ? p_cfg->psz_output : "" ) );

    p_msg_sub = vlm_MessageAdd( p_msg, vlm_MessageNew( "options", vlm_NULL ) );
    for( i = 0; i < p_cfg->i_option; i++ )
        vlm_MessageAdd( p_msg_sub, vlm_MessageNew( p_cfg->ppsz_option[i], vlm_NULL ) );

    p_msg_sub = vlm_MessageAdd( p_msg, vlm_MessageNew( "instances", vlm_NULL ) );
    for( i = 0; i < p_media->i_instance; i++ )
    {
        vlm_media_instance_sys_t *p_instance = p_media->instance[i];
        vlc_value_t val;
        vlm_message_t *p_msg_instance;
        char *psz_tmp;

        val.i_int = END_S;
        if( p_instance->p_input )
            var_Get( p_instance->p_input, "state", &val );

        p_msg_instance = vlm_MessageAdd( p_msg_sub, vlm_MessageNew( "instance" , vlm_NULL ) );

        vlm_MessageAdd( p_msg_instance,
                        vlm_MessageNew( "name" , p_instance->psz_name ? p_instance->psz_name : "default" ) );
        vlm_MessageAdd( p_msg_instance,
                        vlm_MessageNew( "state",
                            val.i_int == PLAYING_S ? "playing" :
                            val.i_int == PAUSE_S ? "paused" :
                            "stopped" ) );

        /* FIXME should not do that this way */
        if( p_instance->p_input )
        {
#define APPEND_INPUT_INFO( a, format, type ) \
            asprintf( &psz_tmp, format, \
                      var_Get ## type( p_instance->p_input, a ) ); \
            vlm_MessageAdd( p_msg_instance, vlm_MessageNew( a, psz_tmp ) ); \
            free( psz_tmp );
            APPEND_INPUT_INFO( "position", "%f", Float );
            APPEND_INPUT_INFO( "time", I64Fi, Time );
            APPEND_INPUT_INFO( "length", I64Fi, Time );
            APPEND_INPUT_INFO( "rate", "%d", Integer );
            APPEND_INPUT_INFO( "title", "%d", Integer );
            APPEND_INPUT_INFO( "chapter", "%d", Integer );
            APPEND_INPUT_INFO( "seekable", "%d", Bool );
        }
#undef APPEND_INPUT_INFO
        asprintf( &psz_tmp, "%d", p_instance->i_index + 1 );
        vlm_MessageAdd( p_msg_instance, vlm_MessageNew( "playlistindex", psz_tmp ) );
        free( psz_tmp );
    }
    return p_msg;
}

static vlm_message_t *vlm_Show( vlm_t *vlm, vlm_media_sys_t *media,
                                vlm_schedule_sys_t *schedule,
                                const char *psz_filter )
{
    if( media != NULL )
    {
        vlm_message_t *p_msg = vlm_MessageNew( "show", vlm_NULL );
        if( p_msg )
            vlm_MessageAdd( p_msg, vlm_ShowMedia( media ) );
        return p_msg;
    }

    else if( schedule != NULL )
    {
        int i;
        vlm_message_t *msg;
        vlm_message_t *msg_schedule;
        vlm_message_t *msg_child;
        char buffer[100];

        msg = vlm_MessageNew( "show", vlm_NULL );
        msg_schedule =
            vlm_MessageAdd( msg, vlm_MessageNew( schedule->psz_name, vlm_NULL ) );

        vlm_MessageAdd( msg_schedule, vlm_MessageNew("type", "schedule") );

        vlm_MessageAdd( msg_schedule,
                        vlm_MessageNew( "enabled", schedule->b_enabled ?
                                        "yes" : "no" ) );

#if !defined( UNDER_CE )
        if( schedule->i_date != 0 )
        {
            struct tm date;
            time_t i_time = (time_t)( schedule->i_date / 1000000 );
            char *psz_date;

            localtime_r( &i_time, &date);
            asprintf( &psz_date, "%d/%d/%d-%d:%d:%d",
                      date.tm_year + 1900, date.tm_mon + 1, date.tm_mday,
                      date.tm_hour, date.tm_min, date.tm_sec );

            vlm_MessageAdd( msg_schedule,
                            vlm_MessageNew( "date", psz_date ) );
            free( psz_date );
        }
        else
        {
            vlm_MessageAdd( msg_schedule, vlm_MessageNew("date", "now") );
        }

        if( schedule->i_period != 0 )
        {
            time_t i_time = (time_t) ( schedule->i_period / 1000000 );
            struct tm date;

            date.tm_sec = (int)( i_time % 60 );
            i_time = i_time / 60;
            date.tm_min = (int)( i_time % 60 );
            i_time = i_time / 60;
            date.tm_hour = (int)( i_time % 24 );
            i_time = i_time / 24;
            date.tm_mday = (int)( i_time % 30 );
            i_time = i_time / 30;
            /* okay, okay, months are not always 30 days long */
            date.tm_mon = (int)( i_time % 12 );
            i_time = i_time / 12;
            date.tm_year = (int)i_time;

            sprintf( buffer, "%d/%d/%d-%d:%d:%d", date.tm_year, date.tm_mon,
                     date.tm_mday, date.tm_hour, date.tm_min, date.tm_sec);

            vlm_MessageAdd( msg_schedule, vlm_MessageNew("period", buffer) );
        }
        else
        {
            vlm_MessageAdd( msg_schedule, vlm_MessageNew("period", "0") );
        }
#endif /* UNDER_CE */

        sprintf( buffer, "%d", schedule->i_repeat );
        vlm_MessageAdd( msg_schedule, vlm_MessageNew( "repeat", buffer ) );

        msg_child =
            vlm_MessageAdd( msg_schedule, vlm_MessageNew("commands", vlm_NULL ) );

        for( i = 0; i < schedule->i_command; i++ )
        {
           vlm_MessageAdd( msg_child,
                           vlm_MessageNew( schedule->command[i], vlm_NULL ) );
        }

        return msg;

    }

    else if( psz_filter && !strcmp( psz_filter, "media" ) )
    {
        vlm_message_t *p_msg;
        vlm_message_t *p_msg_child;
        int i_vod = 0, i_broadcast = 0;
        int i;
        char *psz_count;

        for( i = 0; i < vlm->i_media; i++ )
        {
            if( vlm->media[i]->cfg.b_vod )
                i_vod++;
            else
                i_broadcast++;
        }

        asprintf( &psz_count, "( %d broadcast - %d vod )", i_broadcast, i_vod);

        p_msg = vlm_MessageNew( "show", vlm_NULL );
        p_msg_child = vlm_MessageAdd( p_msg, vlm_MessageNew( "media", psz_count ) );
        free( psz_count );

        for( i = 0; i < vlm->i_media; i++ )
            vlm_MessageAdd( p_msg_child, vlm_ShowMedia( vlm->media[i] ) );

        return p_msg;
    }

    else if( psz_filter && !strcmp( psz_filter, "schedule" ) )
    {
        int i;
        vlm_message_t *msg;
        vlm_message_t *msg_child;

        msg = vlm_MessageNew( "show", vlm_NULL );
        msg_child = vlm_MessageAdd( msg, vlm_MessageNew( "schedule", vlm_NULL ) );

        for( i = 0; i < vlm->i_schedule; i++ )
        {
            vlm_schedule_sys_t *s = vlm->schedule[i];
            vlm_message_t *msg_schedule;
            mtime_t i_time, i_next_date;

            msg_schedule = vlm_MessageAdd( msg_child,
                                           vlm_MessageNew( s->psz_name, vlm_NULL ) );
            vlm_MessageAdd( msg_schedule,
                            vlm_MessageNew( "enabled", s->b_enabled ?
                                            "yes" : "no" ) );

            /* calculate next date */
            i_time = vlm_Date();
            i_next_date = s->i_date;

            if( s->i_period != 0 )
            {
                int j = 0;
                while( s->i_date + j * s->i_period <= i_time &&
                       s->i_repeat > j )
                {
                    j++;
                }

                i_next_date = s->i_date + j * s->i_period;
            }

            if( i_next_date > i_time )
            {
                time_t i_date = (time_t) (i_next_date / 1000000) ;

#if !defined( UNDER_CE )
#ifdef HAVE_CTIME_R
                char psz_date[500];
                ctime_r( &i_date, psz_date );
#else
                char *psz_date = ctime( &i_date );
#endif

                vlm_MessageAdd( msg_schedule,
                                vlm_MessageNew( "next launch", psz_date ) );
#endif
            }
        }

        return msg;
    }

    else if( ( psz_filter == NULL ) && ( media == NULL ) && ( schedule == NULL ) )
    {
        vlm_message_t *show1 = vlm_Show( vlm, NULL, NULL, "media" );
        vlm_message_t *show2 = vlm_Show( vlm, NULL, NULL, "schedule" );

        vlm_MessageAdd( show1, show2->child[0] );

        /* We must destroy the parent node "show" of show2
         * and not the children */
        free( show2->psz_name );
        free( show2 );

        return show1;
    }

    else
    {
        return vlm_MessageNew( "show", vlm_NULL );
    }
}

/*****************************************************************************
 * Config handling functions
 *****************************************************************************/
static int Load( vlm_t *vlm, char *file )
{
    char *pf = file;
    int  i_line = 1;

    while( *pf != '\0' )
    {
        vlm_message_t *message = NULL;
        int i_end = 0;

        while( pf[i_end] != '\n' && pf[i_end] != '\0' && pf[i_end] != '\r' )
        {
            i_end++;
        }

        if( pf[i_end] == '\r' || pf[i_end] == '\n' )
        {
            pf[i_end] = '\0';
            i_end++;
            if( pf[i_end] == '\n' ) i_end++;
        }

        if( *pf && ExecuteCommand( vlm, pf, &message ) )
        {
            if( message )
            {
                if( message->psz_value )
                    msg_Err( vlm, "Load error on line %d: %s: %s",
                             i_line, message->psz_name, message->psz_value );
                vlm_MessageDelete( message );
            }
            return 1;
        }
        if( message ) vlm_MessageDelete( message );

        pf += i_end;
        i_line++;
    }

    return 0;
}

static char *Save( vlm_t *vlm )
{
    char *save = NULL;
    char psz_header[] = "\n"
                        "# VLC media player VLM command batch\n"
                        "# http://www.videolan.org/vlc/\n\n" ;
    char *p;
    int i,j;
    int i_length = strlen( psz_header );

    for( i = 0; i < vlm->i_media; i++ )
    {
        vlm_media_sys_t *media = vlm->media[i];
        vlm_media_t *p_cfg = &media->cfg;

        if( p_cfg->b_vod )
            i_length += strlen( "new * vod " ) + strlen(p_cfg->psz_name);
        else
            i_length += strlen( "new * broadcast " ) + strlen(p_cfg->psz_name);

        if( p_cfg->b_enabled == VLC_TRUE )
            i_length += strlen( "enabled" );
        else
            i_length += strlen( "disabled" );

        if( !p_cfg->b_vod && p_cfg->broadcast.b_loop == VLC_TRUE )
            i_length += strlen( " loop\n" );
        else
            i_length += strlen( "\n" );

        for( j = 0; j < p_cfg->i_input; j++ )
            i_length += strlen( "setup * input \"\"\n" ) + strlen( p_cfg->psz_name ) + strlen( p_cfg->ppsz_input[j] );

        if( p_cfg->psz_output != NULL )
            i_length += strlen( "setup * output \n" ) + strlen(p_cfg->psz_name) + strlen(p_cfg->psz_output);

        for( j = 0; j < p_cfg->i_option; j++ )
            i_length += strlen("setup * option \n") + strlen(p_cfg->psz_name) + strlen(p_cfg->ppsz_option[j]);

        if( p_cfg->b_vod && p_cfg->vod.psz_mux )
            i_length += strlen("setup * mux \n") + strlen(p_cfg->psz_name) + strlen(p_cfg->vod.psz_mux);
    }

    for( i = 0; i < vlm->i_schedule; i++ )
    {
        vlm_schedule_sys_t *schedule = vlm->schedule[i];

        i_length += strlen( "new  schedule " ) + strlen( schedule->psz_name );

        if( schedule->b_enabled == VLC_TRUE )
        {
            i_length += strlen( "date //-:: enabled\n" ) + 14;
        }
        else
        {
            i_length += strlen( "date //-:: disabled\n" ) + 14;
        }


        if( schedule->i_period != 0 )
        {
            i_length += strlen( "setup  " ) + strlen( schedule->psz_name ) +
                strlen( "period //-::\n" ) + 14;
        }

        if( schedule->i_repeat >= 0 )
        {
            char buffer[12];

            sprintf( buffer, "%d", schedule->i_repeat );
            i_length += strlen( "setup  repeat \n" ) +
                strlen( schedule->psz_name ) + strlen( buffer );
        }
        else
        {
            i_length++;
        }

        for( j = 0; j < schedule->i_command; j++ )
        {
            i_length += strlen( "setup  append \n" ) +
                strlen( schedule->psz_name ) + strlen( schedule->command[j] );
        }

    }

    /* Don't forget the '\0' */
    i_length++;
    /* now we have the length of save */

    p = save = malloc( i_length );
    if( !save ) return NULL;
    *save = '\0';

    p += sprintf( p, "%s", psz_header );

    /* finally we can write in it */
    for( i = 0; i < vlm->i_media; i++ )
    {
        vlm_media_sys_t *media = vlm->media[i];
        vlm_media_t *p_cfg = &media->cfg;

        if( p_cfg->b_vod )
            p += sprintf( p, "new %s vod ", p_cfg->psz_name );
        else
            p += sprintf( p, "new %s broadcast ", p_cfg->psz_name );

        if( p_cfg->b_enabled )
            p += sprintf( p, "enabled" );
        else
            p += sprintf( p, "disabled" );

        if( !p_cfg->b_vod && p_cfg->broadcast.b_loop )
            p += sprintf( p, " loop\n" );
        else
            p += sprintf( p, "\n" );

        for( j = 0; j < p_cfg->i_input; j++ )
            p += sprintf( p, "setup %s input \"%s\"\n", p_cfg->psz_name, p_cfg->ppsz_input[j] );

        if( p_cfg->psz_output )
            p += sprintf( p, "setup %s output %s\n", p_cfg->psz_name, p_cfg->psz_output );

        for( j = 0; j < p_cfg->i_option; j++ )
            p += sprintf( p, "setup %s option %s\n", p_cfg->psz_name, p_cfg->ppsz_option[j] );

        if( p_cfg->b_vod && p_cfg->vod.psz_mux )
            p += sprintf( p, "setup %s mux %s\n", p_cfg->psz_name, p_cfg->vod.psz_mux );
    }

    /* and now, the schedule scripts */
#if !defined( UNDER_CE )
    for( i = 0; i < vlm->i_schedule; i++ )
    {
        vlm_schedule_sys_t *schedule = vlm->schedule[i];
        struct tm date;
        time_t i_time = (time_t) ( schedule->i_date / 1000000 );

        localtime_r( &i_time, &date);
        p += sprintf( p, "new %s schedule ", schedule->psz_name);

        if( schedule->b_enabled == VLC_TRUE )
        {
            p += sprintf( p, "date %d/%d/%d-%d:%d:%d enabled\n",
                          date.tm_year + 1900, date.tm_mon + 1, date.tm_mday,
                          date.tm_hour, date.tm_min, date.tm_sec );
        }
        else
        {
            p += sprintf( p, "date %d/%d/%d-%d:%d:%d disabled\n",
                          date.tm_year + 1900, date.tm_mon + 1, date.tm_mday,
                          date.tm_hour, date.tm_min, date.tm_sec);
        }

        if( schedule->i_period != 0 )
        {
            p += sprintf( p, "setup %s ", schedule->psz_name );

            i_time = (time_t) ( schedule->i_period / 1000000 );

            date.tm_sec = (int)( i_time % 60 );
            i_time = i_time / 60;
            date.tm_min = (int)( i_time % 60 );
            i_time = i_time / 60;
            date.tm_hour = (int)( i_time % 24 );
            i_time = i_time / 24;
            date.tm_mday = (int)( i_time % 30 );
            i_time = i_time / 30;
            /* okay, okay, months are not always 30 days long */
            date.tm_mon = (int)( i_time % 12 );
            i_time = i_time / 12;
            date.tm_year = (int)i_time;

            p += sprintf( p, "period %d/%d/%d-%d:%d:%d\n",
                          date.tm_year, date.tm_mon, date.tm_mday,
                          date.tm_hour, date.tm_min, date.tm_sec);
        }

        if( schedule->i_repeat >= 0 )
        {
            p += sprintf( p, "setup %s repeat %d\n",
                          schedule->psz_name, schedule->i_repeat );
        }
        else
        {
            p += sprintf( p, "\n" );
        }

        for( j = 0; j < schedule->i_command; j++ )
        {
            p += sprintf( p, "setup %s append %s\n",
                          schedule->psz_name, schedule->command[j] );
        }

    }
#endif /* UNDER_CE */

    return save;
}

/*****************************************************************************
 *
 *****************************************************************************/
static int vlm_MediaVodControl( void *p_private, vod_media_t *p_vod_media,
                                const char *psz_id, int i_query, va_list args )
{
    vlm_t *vlm = (vlm_t *)p_private;
    int i, i_ret;
    const char *psz;
    int64_t id;

    if( !p_private || !p_vod_media )
        return VLC_EGENERIC;

    vlc_mutex_lock( &vlm->lock );

    /* Find media id */
    for( i = 0, id = -1; i < vlm->i_media; i++ )
    {
        if( p_vod_media == vlm->media[i]->vod.p_media )
        {
            id = vlm->media[i]->cfg.id;
            break;
        }
    }
    if( id == -1 )
    {
        vlc_mutex_unlock( &vlm->lock );
        return VLC_EGENERIC;
    }

    switch( i_query )
    {
    case VOD_MEDIA_PLAY:
        psz = (const char *)va_arg( args, const char * );
        i_ret = vlm_ControlInternal( vlm, VLM_START_MEDIA_VOD_INSTANCE, id, psz_id, 0, psz );
        break;

    case VOD_MEDIA_PAUSE:
        i_ret = vlm_ControlInternal( vlm, VLM_PAUSE_MEDIA_INSTANCE, id, psz_id );
        break;

    case VOD_MEDIA_STOP:
        i_ret = vlm_ControlInternal( vlm, VLM_STOP_MEDIA_INSTANCE, id, psz_id );
        break;

    case VOD_MEDIA_SEEK:
    {
        double d_position = (double)va_arg( args, double );
        i_ret = vlm_ControlInternal( vlm, VLM_SET_MEDIA_INSTANCE_POSITION, id, psz_id, d_position/100.0 );
        break;
    }

    case VOD_MEDIA_REWIND:
    {
        double d_scale = (double)va_arg( args, double );
        double d_position;

        vlm_ControlInternal( vlm, VLM_GET_MEDIA_INSTANCE_POSITION, id, psz_id, &d_position );
        d_position -= (d_scale / 1000.0);
        if( d_position < 0.0 )
            d_position = 0.0;
        i_ret = vlm_ControlInternal( vlm, VLM_SET_MEDIA_INSTANCE_POSITION, id, psz_id, d_position );
        break;
    }

    case VOD_MEDIA_FORWARD:
    {
        double d_scale = (double)va_arg( args, double );
        double d_position;

        vlm_ControlInternal( vlm, VLM_GET_MEDIA_INSTANCE_POSITION, id, psz_id, &d_position );
        d_position += (d_scale / 1000.0);
        if( d_position > 1.0 )
            d_position = 1.0;
        i_ret = vlm_ControlInternal( vlm, VLM_SET_MEDIA_INSTANCE_POSITION, id, psz_id, d_position );
        break;
    }

    default:
        i_ret = VLC_EGENERIC;
        break;
    }

    vlc_mutex_unlock( &vlm->lock );

    return i_ret;
}


/*****************************************************************************
 * Manage:
 *****************************************************************************/
static int Manage( vlc_object_t* p_object )
{
    vlm_t *vlm = (vlm_t*)p_object;
    int i, j;
    mtime_t i_lastcheck;
    mtime_t i_time;

    i_lastcheck = vlm_Date();

    while( !vlm->b_die )
    {
        char **ppsz_scheduled_commands = NULL;
        int    i_scheduled_commands = 0;

        vlc_mutex_lock( &vlm->lock );

        /* destroy the inputs that wants to die, and launch the next input */
        for( i = 0; i < vlm->i_media; i++ )
        {
            vlm_media_sys_t *p_media = vlm->media[i];

            for( j = 0; j < p_media->i_instance; )
            {
                vlm_media_instance_sys_t *p_instance = p_media->instance[j];

                if( p_instance->p_input && ( p_instance->p_input->b_eof || p_instance->p_input->b_error ) )
                {
                    int i_new_input_index;

                    /* */
                    i_new_input_index = p_instance->i_index + 1;
                    if( !p_media->cfg.b_vod && p_media->cfg.broadcast.b_loop && i_new_input_index >= p_media->cfg.i_input )
                        i_new_input_index = 0;

                    /* FIXME implement multiple input with VOD */
                    if( p_media->cfg.b_vod || i_new_input_index >= p_media->cfg.i_input )
                        vlm_ControlInternal( vlm, VLM_STOP_MEDIA_INSTANCE, p_media->cfg.id, p_instance->psz_name );
                    else
                        vlm_ControlInternal( vlm, VLM_START_MEDIA_BROADCAST_INSTANCE, p_media->cfg.id, p_instance->psz_name, i_new_input_index );

                    j = 0;
                }
                else
                {
                    j++;
                }
            }
        }

        /* scheduling */
        i_time = vlm_Date();

        for( i = 0; i < vlm->i_schedule; i++ )
        {
            mtime_t i_real_date = vlm->schedule[i]->i_date;

            if( vlm->schedule[i]->b_enabled == VLC_TRUE )
            {
                if( vlm->schedule[i]->i_date == 0 ) // now !
                {
                    vlm->schedule[i]->i_date = (i_time / 1000000) * 1000000 ;
                    i_real_date = i_time;
                }
                else if( vlm->schedule[i]->i_period != 0 )
                {
                    int j = 0;
                    while( vlm->schedule[i]->i_date + j *
                           vlm->schedule[i]->i_period <= i_lastcheck &&
                           ( vlm->schedule[i]->i_repeat > j ||
                             vlm->schedule[i]->i_repeat == -1 ) )
                    {
                        j++;
                    }

                    i_real_date = vlm->schedule[i]->i_date + j *
                        vlm->schedule[i]->i_period;
                }

                if( i_real_date <= i_time && i_real_date > i_lastcheck )
                {
                    for( j = 0; j < vlm->schedule[i]->i_command; j++ )
                    {
                        TAB_APPEND( i_scheduled_commands,
                                    ppsz_scheduled_commands,
                                    strdup(vlm->schedule[i]->command[j] ) );
                    }
                }
            }
        }
        while( i_scheduled_commands )
        {
            vlm_message_t *message = NULL;
            char *psz_command = ppsz_scheduled_commands[0];
            ExecuteCommand( vlm, psz_command,&message );

            /* for now, drop the message */
            vlm_MessageDelete( message );
            TAB_REMOVE( i_scheduled_commands,
                        ppsz_scheduled_commands,
                        psz_command );
            free( psz_command );
        }

        i_lastcheck = i_time;

        vlc_mutex_unlock( &vlm->lock );

        msleep( 100000 );
    }

    return VLC_SUCCESS;
}

/* New API
 */
/*
typedef struct
{
    struct
    {
        int i_connection_count;
        int i_connection_active;
    } vod;
    struct
    {
        int        i_count;
        vlc_bool_t b_playing;
        int        i_playing_index;
    } broadcast;

} vlm_media_status_t;
*/

/* */
static vlm_media_sys_t *vlm_ControlMediaGetById( vlm_t *p_vlm, int64_t id )
{
    int i;

    for( i = 0; i < p_vlm->i_media; i++ )
    {
        if( p_vlm->media[i]->cfg.id == id )
            return p_vlm->media[i];
    }
    return NULL;
}
static vlm_media_sys_t *vlm_ControlMediaGetByName( vlm_t *p_vlm, const char *psz_name )
{
    int i;

    for( i = 0; i < p_vlm->i_media; i++ )
    {
        if( !strcmp( p_vlm->media[i]->cfg.psz_name, psz_name ) )
            return p_vlm->media[i];
    }
    return NULL;
}
static int vlm_MediaDescriptionCheck( vlm_t *p_vlm, vlm_media_t *p_cfg )
{
    int i;

    if( !p_cfg || !p_cfg->psz_name ||
        !strcmp( p_cfg->psz_name, "all" ) || !strcmp( p_cfg->psz_name, "media" ) || !strcmp( p_cfg->psz_name, "schedule" ) )
        return VLC_EGENERIC;

    for( i = 0; i < p_vlm->i_media; i++ )
    {
        if( p_vlm->media[i]->cfg.id == p_cfg->id )
            continue;
        if( !strcmp( p_vlm->media[i]->cfg.psz_name, p_cfg->psz_name ) )
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}


/* Called after a media description is changed/added */
static int vlm_OnMediaUpdate( vlm_t *p_vlm, vlm_media_sys_t *p_media )
{
    vlm_media_t *p_cfg = &p_media->cfg;
    /* Check if we need to create/delete a vod media */
    if( p_cfg->b_vod )
    {
        if( !p_cfg->b_enabled && p_media->vod.p_media )
        {
            p_vlm->p_vod->pf_media_del( p_vlm->p_vod, p_media->vod.p_media );
            p_media->vod.p_media = NULL;
        }
        else if( p_cfg->b_enabled && !p_media->vod.p_media && p_cfg->i_input )
        {
            /* Pre-parse the input */
            input_thread_t *p_input;
            char *psz_output;
            char *psz_header;
            char *psz_dup;
            int i;

            input_ItemClean( &p_media->vod.item );
            input_ItemInit( VLC_OBJECT(p_vlm), &p_media->vod.item );

            if( p_cfg->psz_output )
                asprintf( &psz_output, "%s:description", p_cfg->psz_output );
            else
                asprintf( &psz_output, "#description" );

            p_media->vod.item.psz_uri = strdup( p_cfg->ppsz_input[0] );

            asprintf( &psz_dup, "sout=%s", psz_output);
            input_ItemAddOption( &p_media->vod.item, psz_dup );
            free( psz_dup );
            for( i = 0; i < p_cfg->i_option; i++ )
                input_ItemAddOption( &p_media->vod.item,
                                     p_cfg->ppsz_option[i] );
            input_ItemAddOption( &p_media->vod.item, "no-sout-keep" );

            asprintf( &psz_header, _("Media: %s"), p_cfg->psz_name );

            if( (p_input = input_CreateThreadExtended( p_vlm, &p_media->vod.item, psz_header, NULL ) ) )
            {
                while( !p_input->b_eof && !p_input->b_error )
                    msleep( 100000 );

                input_StopThread( p_input );
                vlc_object_release( p_input );
            }
            free( psz_output );
            free( psz_header );

            if( p_cfg->vod.psz_mux )
            {
                input_item_t item;
                es_format_t es, *p_es = &es;
                char fourcc[5];

                sprintf( fourcc, "%4.4s", p_cfg->vod.psz_mux );
                fourcc[0] = tolower(fourcc[0]); fourcc[1] = tolower(fourcc[1]);
                fourcc[2] = tolower(fourcc[2]); fourcc[3] = tolower(fourcc[3]);

                item = p_media->vod.item;
                item.i_es = 1;
                item.es = &p_es;
                es_format_Init( &es, VIDEO_ES, *((int *)fourcc) );

                p_media->vod.p_media =
                    p_vlm->p_vod->pf_media_new( p_vlm->p_vod, p_cfg->psz_name, &item );
            }
            else
            {
                p_media->vod.p_media =
                    p_vlm->p_vod->pf_media_new( p_vlm->p_vod, p_cfg->psz_name, &p_media->vod.item );
            }
        }
    }
    else
    {
        /* TODO start media if needed */
    }

    /* TODO add support of var vlm_media_broadcast/vlm_media_vod */

    return VLC_SUCCESS;
}
static int vlm_ControlMediaChange( vlm_t *p_vlm, vlm_media_t *p_cfg )
{
    vlm_media_sys_t *p_media = vlm_ControlMediaGetById( p_vlm, p_cfg->id );

    /* */
    if( !p_media || vlm_MediaDescriptionCheck( p_vlm, p_cfg ) )
        return VLC_EGENERIC;
    if( ( p_media->cfg.b_vod && !p_cfg->b_vod ) || ( !p_media->cfg.b_vod && p_cfg->b_vod ) )
        return VLC_EGENERIC;

    if( 0 )
    {
        /* TODO check what are the changes being done (stop instance if needed) */
    }

    vlm_media_Clean( &p_media->cfg );
    vlm_media_Copy( &p_media->cfg, p_cfg );

    return vlm_OnMediaUpdate( p_vlm, p_media );
}

static int vlm_ControlMediaAdd( vlm_t *p_vlm, vlm_media_t *p_cfg, int64_t *p_id )
{
    vlm_media_sys_t *p_media;

    if( vlm_MediaDescriptionCheck( p_vlm, p_cfg ) || vlm_ControlMediaGetByName( p_vlm, p_cfg->psz_name ) )
    {
        msg_Err( p_vlm, "invalid media description" );
        return VLC_EGENERIC;
    }
    /* Check if we need to load the VOD server */
    if( p_cfg->b_vod && !p_vlm->i_vod )
    {
        p_vlm->p_vod = vlc_object_create( p_vlm, VLC_OBJECT_VOD );
        vlc_object_attach( p_vlm->p_vod, p_vlm );
        p_vlm->p_vod->p_module = module_Need( p_vlm->p_vod, "vod server", 0, 0 );
        if( !p_vlm->p_vod->p_module )
        {
            msg_Err( p_vlm, "cannot find vod server" );
            vlc_object_detach( p_vlm->p_vod );
            vlc_object_release( p_vlm->p_vod );
            p_vlm->p_vod = 0;
            return VLC_EGENERIC;
        }

        p_vlm->p_vod->p_data = p_vlm;
        p_vlm->p_vod->pf_media_control = vlm_MediaVodControl;
    }

    p_media = malloc( sizeof( vlm_media_sys_t ) );
    if( !p_media )
    {
        msg_Err( p_vlm, "out of memory" );
        return VLC_ENOMEM;
    }
    memset( p_media, 0, sizeof(vlm_media_sys_t) );

    if( p_cfg->b_vod )
        p_vlm->i_vod++;

    vlm_media_Copy( &p_media->cfg, p_cfg );
    p_media->cfg.id = p_vlm->i_id++;
    /* FIXME do we do something here if enabled is true ? */

    input_ItemInit( VLC_OBJECT(p_vlm), &p_media->vod.item );

    p_media->vod.p_media = NULL;
    TAB_INIT( p_media->i_instance, p_media->instance );

    /* */
    TAB_APPEND( p_vlm->i_media, p_vlm->media, p_media );

    if( p_id )
        *p_id = p_media->cfg.id;

    return vlm_OnMediaUpdate( p_vlm, p_media );
}

static int vlm_ControlMediaDel( vlm_t *p_vlm, int64_t id )
{
    vlm_media_sys_t *p_media = vlm_ControlMediaGetById( p_vlm, id );

    if( !p_media )
        return VLC_EGENERIC;

    while( p_media->i_instance > 0 )
        vlm_ControlInternal( p_vlm, VLM_STOP_MEDIA_INSTANCE, id, p_media->instance[0]->psz_name );

    if( p_media->cfg.b_vod )
    {
        p_media->cfg.b_enabled = VLC_FALSE;
        vlm_OnMediaUpdate( p_vlm, p_media );
        p_vlm->i_vod--;
    }

    vlm_media_Clean( &p_media->cfg );

    input_ItemClean( &p_media->vod.item );

    TAB_REMOVE( p_vlm->i_media, p_vlm->media, p_media );

    free( p_media );

    /* Check if we need to unload the VOD server */
    if( p_vlm->p_vod && p_vlm->i_vod <= 0 )
    {
        module_Unneed( p_vlm->p_vod, p_vlm->p_vod->p_module );
        vlc_object_detach( p_vlm->p_vod );
        vlc_object_release( p_vlm->p_vod );
        p_vlm->p_vod = NULL;
    }
    return VLC_SUCCESS;
}

static int vlm_ControlMediaGets( vlm_t *p_vlm, vlm_media_t ***ppp_dsc, int *pi_dsc )
{
    vlm_media_t **pp_dsc;
    int                     i_dsc;
    int i;

    TAB_INIT( i_dsc, pp_dsc );
    for( i = 0; i < p_vlm->i_media; i++ )
    {
        vlm_media_t *p_dsc = vlm_media_Duplicate( &p_vlm->media[i]->cfg );
        TAB_APPEND( i_dsc, pp_dsc, p_dsc );
    }

    *ppp_dsc = pp_dsc;
    *pi_dsc = i_dsc;

    return VLC_SUCCESS;
}
static int vlm_ControlMediaClear( vlm_t *p_vlm )
{
    while( p_vlm->i_media > 0 )
        vlm_ControlMediaDel( p_vlm, p_vlm->media[0]->cfg.id );

    return VLC_SUCCESS;
}
static int vlm_ControlMediaGet( vlm_t *p_vlm, int64_t id, vlm_media_t **pp_dsc )
{
    vlm_media_sys_t *p_media = vlm_ControlMediaGetById( p_vlm, id );
    if( !p_media )
        return VLC_EGENERIC;

    *pp_dsc = vlm_media_Duplicate( &p_media->cfg );
    return VLC_SUCCESS;
}
static int vlm_ControlMediaGetId( vlm_t *p_vlm, const char *psz_name, int64_t *p_id )
{
    vlm_media_sys_t *p_media = vlm_ControlMediaGetByName( p_vlm, psz_name );
    if( !p_media )
        return VLC_EGENERIC;

    *p_id = p_media->cfg.id;
    return VLC_SUCCESS;
}

static vlm_media_instance_sys_t *vlm_ControlMediaInstanceGetByName( vlm_media_sys_t *p_media, const char *psz_id )
{
    int i;

    for( i = 0; i < p_media->i_instance; i++ )
    {
        const char *psz = p_media->instance[i]->psz_name;
        if( ( psz == NULL && psz_id == NULL ) ||
            ( psz && psz_id && !strcmp( psz, psz_id ) ) )
            return p_media->instance[i];
    }
    return NULL;
}
static vlm_media_instance_sys_t *vlm_MediaInstanceNew( vlm_t *p_vlm, const char *psz_name )
{
    vlm_media_instance_sys_t *p_instance = malloc( sizeof(vlm_media_instance_sys_t) );
    if( !p_instance )
        return NULL;

    memset( p_instance, 0, sizeof(vlm_media_instance_sys_t) );

    p_instance->psz_name = NULL;
    if( psz_name )
        p_instance->psz_name = strdup( psz_name );

    input_ItemInit( VLC_OBJECT(p_vlm), &p_instance->item );

    p_instance->i_index = 0;
    p_instance->b_sout_keep = VLC_FALSE;
    p_instance->p_input = NULL;
    p_instance->p_sout = NULL;

    return p_instance;
}
static void vlm_MediaInstanceDelete( vlm_media_instance_sys_t *p_instance )
{
    if( p_instance->p_input )
    {
        input_StopThread( p_instance->p_input );
        p_instance->p_sout = input_DetachSout( p_instance->p_input );
        vlc_object_release( p_instance->p_input );
    }
    if( p_instance->p_sout )
        sout_DeleteInstance( p_instance->p_sout );

    input_ItemClean( &p_instance->item );
    free( p_instance->psz_name );
    free( p_instance );
}


static int vlm_ControlMediaInstanceStart( vlm_t *p_vlm, int64_t id, const char *psz_id, int i_input_index, const char *psz_vod_output )
{
    vlm_media_sys_t *p_media = vlm_ControlMediaGetById( p_vlm, id );
    vlm_media_instance_sys_t *p_instance;
    char *psz_log;

    if( !p_media || !p_media->cfg.b_enabled || p_media->cfg.i_input <= 0 )
        return VLC_EGENERIC;

    /* TODO support multiple input for VOD with sout-keep ? */

    if( ( p_media->cfg.b_vod && !psz_vod_output ) || ( !p_media->cfg.b_vod && psz_vod_output ) )
        return VLC_EGENERIC;

    if( i_input_index < 0 || i_input_index >= p_media->cfg.i_input )
        return VLC_EGENERIC;

    p_instance = vlm_ControlMediaInstanceGetByName( p_media, psz_id );
    if( !p_instance )
    {
        vlm_media_t *p_cfg = &p_media->cfg;
        const char *psz_keep;
        int i;

        p_instance = vlm_MediaInstanceNew( p_vlm, psz_id );
        if( !p_instance )
            return VLC_ENOMEM;

        if( p_cfg->psz_output != NULL || psz_vod_output != NULL )
        {
            char *psz_buffer;
            asprintf( &psz_buffer, "sout=%s%s%s",
                      p_cfg->psz_output ? p_cfg->psz_output : "",
                      (p_cfg->psz_output && psz_vod_output) ? ":" : psz_vod_output ? "#" : "",
                      psz_vod_output ? psz_vod_output : "" );
            input_ItemAddOption( &p_instance->item, psz_buffer );
            free( psz_buffer );
        }

        for( i = 0; i < p_cfg->i_option; i++ )
        {
            if( !strcmp( p_cfg->ppsz_option[i], "sout-keep" ) )
                p_instance->b_sout_keep = VLC_TRUE;
            else if( !strcmp( p_cfg->ppsz_option[i], "nosout-keep" ) || !strcmp( p_cfg->ppsz_option[i], "no-sout-keep" ) )
                p_instance->b_sout_keep = VLC_FALSE;
            else
                input_ItemAddOption( &p_instance->item, p_cfg->ppsz_option[i] );
        }
        /* We force the right sout-keep value (avoid using the sout-keep from the global configuration)
         * FIXME implement input list for VOD (need sout-keep)
         * */
        if( !p_cfg->b_vod && p_instance->b_sout_keep )
            psz_keep = "sout-keep";
        else
            psz_keep = "no-sout-keep";
        input_ItemAddOption( &p_instance->item, psz_keep );

        TAB_APPEND( p_media->i_instance, p_media->instance, p_instance );
    }

    /* Stop old instance */
    if( p_instance->p_input )
    {
        if( p_instance->i_index == i_input_index &&
            !p_instance->p_input->b_eof && !p_instance->p_input->b_error )
        {
            if( var_GetInteger( p_instance->p_input, "state" ) == PAUSE_S )
                var_SetInteger( p_instance->p_input, "state",  PLAYING_S );
            return VLC_SUCCESS;
        }

        input_StopThread( p_instance->p_input );
        p_instance->p_sout = input_DetachSout( p_instance->p_input );
        vlc_object_release( p_instance->p_input );
        if( !p_instance->b_sout_keep && p_instance->p_sout )
        {
            sout_DeleteInstance( p_instance->p_sout );
            p_instance->p_sout = NULL;
        }
    }

    /* Start new one */
    p_instance->i_index = i_input_index;
    input_item_SetURI( &p_instance->item, p_media->cfg.ppsz_input[p_instance->i_index] ) ;

    asprintf( &psz_log, _("Media: %s"), p_media->cfg.psz_name );
    p_instance->p_input = input_CreateThreadExtended( p_vlm, &p_instance->item, psz_log, p_instance->p_sout );
    if( !p_instance->p_input )
    {
        TAB_REMOVE( p_media->i_instance, p_media->instance, p_instance );
        vlm_MediaInstanceDelete( p_instance );
    }
    free( psz_log );

    return VLC_SUCCESS;
}

static int vlm_ControlMediaInstanceStop( vlm_t *p_vlm, int64_t id, const char *psz_id )
{
    vlm_media_sys_t *p_media = vlm_ControlMediaGetById( p_vlm, id );
    vlm_media_instance_sys_t *p_instance;

    if( !p_media )
        return VLC_EGENERIC;

    p_instance = vlm_ControlMediaInstanceGetByName( p_media, psz_id );
    if( !p_instance )
        return VLC_EGENERIC;

    TAB_REMOVE( p_media->i_instance, p_media->instance, p_instance );

    vlm_MediaInstanceDelete( p_instance );

    return VLC_SUCCESS;
}
static int vlm_ControlMediaInstancePause( vlm_t *p_vlm, int64_t id, const char *psz_id )
{
    vlm_media_sys_t *p_media = vlm_ControlMediaGetById( p_vlm, id );
    vlm_media_instance_sys_t *p_instance;
    int i_state;

    if( !p_media )
        return VLC_EGENERIC;

    p_instance = vlm_ControlMediaInstanceGetByName( p_media, psz_id );
    if( !p_instance || !p_instance->p_input )
        return VLC_EGENERIC;

    /* Toggle pause state */
    i_state = var_GetInteger( p_instance->p_input, "state" );
    if( i_state == PAUSE_S )
        var_SetInteger( p_instance->p_input, "state", PLAYING_S );
    else if( i_state == PLAYING_S )
        var_SetInteger( p_instance->p_input, "state", PAUSE_S );
    return VLC_SUCCESS;
}
static int vlm_ControlMediaInstanceGetTimePosition( vlm_t *p_vlm, int64_t id, const char *psz_id, int64_t *pi_time, double *pd_position )
{
    vlm_media_sys_t *p_media = vlm_ControlMediaGetById( p_vlm, id );
    vlm_media_instance_sys_t *p_instance;

    if( !p_media )
        return VLC_EGENERIC;

    p_instance = vlm_ControlMediaInstanceGetByName( p_media, psz_id );
    if( !p_instance || !p_instance->p_input )
        return VLC_EGENERIC;

    if( pi_time )
        *pi_time = var_GetTime( p_instance->p_input, "time" );
    if( pd_position )
        *pd_position = var_GetFloat( p_instance->p_input, "position" );
    return VLC_SUCCESS;
}
static int vlm_ControlMediaInstanceSetTimePosition( vlm_t *p_vlm, int64_t id, const char *psz_id, int64_t i_time, double d_position )
{
    vlm_media_sys_t *p_media = vlm_ControlMediaGetById( p_vlm, id );
    vlm_media_instance_sys_t *p_instance;

    if( !p_media )
        return VLC_EGENERIC;

    p_instance = vlm_ControlMediaInstanceGetByName( p_media, psz_id );
    if( !p_instance || !p_instance->p_input )
        return VLC_EGENERIC;

    if( i_time >= 0 )
        return var_SetTime( p_instance->p_input, "time", i_time );
    else if( d_position >= 0 && d_position <= 100 )
        return var_SetFloat( p_instance->p_input, "position", d_position );
    return VLC_EGENERIC;
}

static int vlm_ControlMediaInstanceGets( vlm_t *p_vlm, int64_t id, vlm_media_instance_t ***ppp_idsc, int *pi_instance )
{
    vlm_media_sys_t *p_media = vlm_ControlMediaGetById( p_vlm, id );
    vlm_media_instance_t **pp_idsc;
    int                              i_idsc;
    int i;

    if( !p_media )
        return VLC_EGENERIC;

    TAB_INIT( i_idsc, pp_idsc );
    for( i = 0; i < p_media->i_instance; i++ )
    {
        vlm_media_instance_sys_t *p_instance = p_media->instance[i];
        vlm_media_instance_t *p_idsc = vlm_media_instance_New();

        if( p_instance->psz_name )
            p_idsc->psz_name = strdup( p_instance->psz_name );
        if( p_instance->p_input )
        {
            p_idsc->i_time = var_GetTime( p_instance->p_input, "time" );
            p_idsc->i_length = var_GetTime( p_instance->p_input, "length" );
            p_idsc->d_position = var_GetFloat( p_instance->p_input, "position" );
            if( var_GetInteger( p_instance->p_input, "state" ) == PAUSE_S )
                p_idsc->b_paused = VLC_TRUE;
            p_idsc->i_rate = var_GetInteger( p_instance->p_input, "rate" );
        }

        TAB_APPEND( i_idsc, pp_idsc, p_idsc );
    }
    *ppp_idsc = pp_idsc;
    *pi_instance = i_idsc;
    return VLC_SUCCESS;
}
static int vlm_ControlMediaInstanceClear( vlm_t *p_vlm, int64_t id )
{
    vlm_media_sys_t *p_media = vlm_ControlMediaGetById( p_vlm, id );

    if( !p_media )
        return VLC_EGENERIC;

    while( p_media->i_instance > 0 )
        vlm_ControlMediaInstanceStop( p_vlm, id, p_media->instance[0]->psz_name );

    return VLC_SUCCESS;
}

static int vlm_ControlScheduleClear( vlm_t *p_vlm )
{
    while( p_vlm->i_schedule > 0 )
        vlm_ScheduleDelete( p_vlm, p_vlm->schedule[0] );

    return VLC_SUCCESS;
}
static int vlm_vaControlInternal( vlm_t *p_vlm, int i_query, va_list args )
{
    vlm_media_t *p_dsc;
    vlm_media_t **pp_dsc;
    vlm_media_t ***ppp_dsc;
    vlm_media_instance_t ***ppp_idsc;
    const char *psz_id;
    const char *psz_vod;
    int64_t *p_id;
    int64_t id;
    int i_int;
    int *pi_int;

    int64_t *pi_i64;
    int64_t i_i64;
    double *pd_double;
    double d_double;

    switch( i_query )
    {
    /* Media control */
    case VLM_GET_MEDIAS:
        ppp_dsc = (vlm_media_t ***)va_arg( args, vlm_media_t *** );
        pi_int = (int *)va_arg( args, int * );
        return vlm_ControlMediaGets( p_vlm, ppp_dsc, pi_int );

    case VLM_CLEAR_MEDIAS:
        return vlm_ControlMediaClear( p_vlm );

    case VLM_CHANGE_MEDIA:
        p_dsc = (vlm_media_t*)va_arg( args, vlm_media_t * );
        return vlm_ControlMediaChange( p_vlm, p_dsc );

    case VLM_ADD_MEDIA:
        p_dsc = (vlm_media_t*)va_arg( args, vlm_media_t * );
        p_id = (int64_t*)va_arg( args, int64_t * );
        return vlm_ControlMediaAdd( p_vlm, p_dsc, p_id );

    case VLM_DEL_MEDIA:
        id = (int64_t)va_arg( args, int64_t );
        return vlm_ControlMediaDel( p_vlm, id );

    case VLM_GET_MEDIA:
        id = (int64_t)va_arg( args, int64_t );
        pp_dsc = (vlm_media_t **)va_arg( args, vlm_media_t ** );
        return vlm_ControlMediaGet( p_vlm, id, pp_dsc );

    case VLM_GET_MEDIA_ID:
        psz_id = (const char*)va_arg( args, const char * );
        p_id = (int64_t*)va_arg( args, int64_t * );
        return vlm_ControlMediaGetId( p_vlm, psz_id, p_id );


    /* Media instance control */
    case VLM_GET_MEDIA_INSTANCES:
        id = (int64_t)va_arg( args, int64_t );
        ppp_idsc = (vlm_media_instance_t ***)va_arg( args, vlm_media_instance_t *** );
        pi_int = (int *)va_arg( args, int *);
        return vlm_ControlMediaInstanceGets( p_vlm, id, ppp_idsc, pi_int );

    case VLM_CLEAR_MEDIA_INSTANCES:
        id = (int64_t)va_arg( args, int64_t );
        return vlm_ControlMediaInstanceClear( p_vlm, id );


    case VLM_START_MEDIA_BROADCAST_INSTANCE:
        id = (int64_t)va_arg( args, int64_t );
        psz_id = (const char*)va_arg( args, const char* );
        i_int = (int)va_arg( args, int );
        return vlm_ControlMediaInstanceStart( p_vlm, id, psz_id, i_int, NULL );

    case VLM_START_MEDIA_VOD_INSTANCE:
        id = (int64_t)va_arg( args, int64_t );
        psz_id = (const char*)va_arg( args, const char* );
        i_int = (int)va_arg( args, int );
        psz_vod = (const char*)va_arg( args, const char* );
        if( !psz_vod )
            return VLC_EGENERIC;
        return vlm_ControlMediaInstanceStart( p_vlm, id, psz_id, i_int, psz_vod );

    case VLM_STOP_MEDIA_INSTANCE:
        id = (int64_t)va_arg( args, int64_t );
        psz_id = (const char*)va_arg( args, const char* );
        return vlm_ControlMediaInstanceStop( p_vlm, id, psz_id );

    case VLM_PAUSE_MEDIA_INSTANCE:
        id = (int64_t)va_arg( args, int64_t );
        psz_id = (const char*)va_arg( args, const char* );
        return vlm_ControlMediaInstancePause( p_vlm, id, psz_id );

    case VLM_GET_MEDIA_INSTANCE_TIME:
        id = (int64_t)va_arg( args, int64_t );
        psz_id = (const char*)va_arg( args, const char* );
        pi_i64 = (int64_t*)va_arg( args, int64_t * );
        return vlm_ControlMediaInstanceGetTimePosition( p_vlm, id, psz_id, pi_i64, NULL );
    case VLM_GET_MEDIA_INSTANCE_POSITION:
        id = (int64_t)va_arg( args, int64_t );
        psz_id = (const char*)va_arg( args, const char* );
        pd_double = (double*)va_arg( args, double* );
        return vlm_ControlMediaInstanceGetTimePosition( p_vlm, id, psz_id, NULL, pd_double );

    case VLM_SET_MEDIA_INSTANCE_TIME:
        id = (int64_t)va_arg( args, int64_t );
        psz_id = (const char*)va_arg( args, const char* );
        i_i64 = (int64_t)va_arg( args, int64_t);
        return vlm_ControlMediaInstanceSetTimePosition( p_vlm, id, psz_id, i_i64, -1 );
    case VLM_SET_MEDIA_INSTANCE_POSITION:
        id = (int64_t)va_arg( args, int64_t );
        psz_id = (const char*)va_arg( args, const char* );
        d_double = (double)va_arg( args, double );
        return vlm_ControlMediaInstanceSetTimePosition( p_vlm, id, psz_id, -1, d_double );

    case VLM_CLEAR_SCHEDULES:
        return vlm_ControlScheduleClear( p_vlm );

    default:
        msg_Err( p_vlm, "unknown VLM query" );
        return VLC_EGENERIC;
    }
}
static int vlm_ControlInternal( vlm_t *p_vlm, int i_query, ... )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );
    i_result = vlm_vaControlInternal( p_vlm, i_query, args );
    va_end( args );

    return i_result;
}

int vlm_Control( vlm_t *p_vlm, int i_query, ... )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );

    vlc_mutex_lock( &p_vlm->lock );
    i_result = vlm_vaControlInternal( p_vlm, i_query, args );
    vlc_mutex_unlock( &p_vlm->lock );

    va_end( args );

    return i_result;
}

#else /* ENABLE_VLM */

/* We just define an empty wrapper */
vlm_t *__vlm_New( vlc_object_t *a )
{
    msg_Err( a, "VideoLAN manager support is disabled" );
    return NULL;
}

void vlm_Delete( vlm_t *a )
{
    (void)a;
}

int vlm_ExecuteCommand( vlm_t *a, const char *b, vlm_message_t **c )
{
    abort();
}

vlm_message_t *vlm_MessageNew( const char *psz_name,
                               const char *psz_format, ... )
{
    (void)psz_name; (void)psz_format;
    return NULL;
}

vlm_message_t *vlm_MessageAdd( vlm_message_t *p_message,
                               vlm_message_t *p_child )
{
    abort();
}

void vlm_MessageDelete( vlm_message_t *a )
{
    (void)a;
}

int vlm_Control( vlm_t *p_vlm, int i_query, ... )
{
    (void)p_vlm; (void)i_query;
    return VLC_EGENERIC;
}

#endif /* ENABLE_VLM */
